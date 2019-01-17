/*
 *---------------------------------------------------------------------------
 * vlogFanout vlog_input [vlog_output]
 *
 * vlogFanout[.c] parses a structural verilog netlist.  The fanout is analyzed,
 * and fanout of each gate is counted.  A value to parameterize the driving
 * cell will be output.
 * Fanouts exceeding a maximum are broken into (possibly hierarchical)
 * buffer trees.  Eventually, a critical path can be identified, and the
 * gates sized to improve it.
 *
 * Original:  fanout.c by Steve Beccue
 * New: vlogFanout.c by Tim Edwards.
 *     changes: 1) Input and output format changed from RTL verilog to BDNET
 *		 for compatibility with the existing digital design flow.
 *	        2) Gate format changed to facilitate entering IBM data
 *		3) Code changed/optimized in too many ways to list here.
 *
 * Update 4/8/2013:
 *	Removing dependence upon the naming convention of the original
 *	technology used with this tool.  Instead, the format of the naming
 *	convention is passed to the tool using "-s <separator>", and parsing
 *	the remaining name for unique identifiers.
 *
 * Update 10/8/2013:
 *	Changed input file format from BDNET to BLIF
 *
 * Update 10/21/2013:
 *	Removed most of the dependencies on fixed-length arrays
 *
 * Update 5/13/2015:
 *	Added hash functions, which have been on the "to do" list for a
 *	while.  Greatly speeds up the processing, especially for large
 *	netlists.
 *
 *	Replaced the gate.cfg file parser with the liberty file parser
 *	from the "liberty2tech" code, which is no longer needed.  The
 *	gate.cfg format was essentially useless as it failed to track
 *	which pins in a cell are inputs and which are outputs.
 *
 *	Moving the clock tree generator into this module, because I have
 *	figured out a way to do this in one stage instead of two, using
 *	the swap_group capability of the graywolf placement tool to find
 *	the optimal groupings.
 *
 * Update 5/7/2018:
 *	Separated clock buffers from other buffers.  Identify clock inputs
 *	on flops and latches and trace back to a common clock pin or pins.
 *
 * Update 6/15/2018:
 *	Finally got around to doing dynamic string allocation on the input,
 *	which avoids issues of having I/O lines that are arbitrarily long
 *	crashing the program.
 *
 * Update 11/26/2018:
 *	Reworked the entire tool to operate on an input verilog netlist
 *	instead of a BLIF file, given the limitations of the BLIF format
 *	and the need to completely rewrite the BLIF handling scripts in
 *	order to maintain use of a badly outdated format.  Also, modified
 *	the code so that iterative calls are run inside vlogFanout instead
 *	of requiring vlogFanout to be run multiple times, so that the
 *	liberty file and netlist do not have to be re-read on each pass.
 *
 * Update 1/17/2019:
 *	Added handling of pins which are buses.
 *---------------------------------------------------------------------------
 *
 * Revision 1.3  2008/09/09 21:24:30  steve_beccue
 * changed gate strengths in code.  Inserted inverters.  2nd pass on
 * insert inverters does not work.
 *
 * Revision 1.2  2008/09/04 14:25:59  steve_beccue
 * added helpmessage and id
 *
 *---------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* for getopt() */
#include <string.h>
#include <ctype.h>		/* for isdigit() */
#include <math.h>

#include "hash.h"		/* for hash table functions */
#include "readliberty.h"	/* liberty file database */
#include "readverilog.h"	/* verilog parser */

#define  FALSE	     0
#define  TRUE        1
#define  MAXLINE     512

char *Inputfname;
char *Outputfname;
char *Buffername = NULL;
char *Clkbufname = NULL;
char *buf_in_pin = NULL;
char *clkbuf_in_pin = NULL;
char *buf_out_pin = NULL;
char *clkbuf_out_pin = NULL;
char *Ignorepath = NULL;
char SuffixIsNumeric;
int  GatePrintFlag = 0;
int  NodePrintFlag = 0;
int  VerboseFlag = 0;
int  skip_eol = 0;

int  Topfanout = 0;
int  Inputfanout = 0;
double Topload = 0.0;
double Inputload = 0.0;
double Topratio = 0.0;
int    Changed_count = 0;	// number of gates changed
int    Buffer_count = 0;	// number of buffers added
int stren_err_counter = 0;
double MaxOverload = 0.0;

int    MaxFanout  = 16;		// Maximum fanout per node allowed without
				// additional buffering.
double MaxLatency = 1000.0;	// Maximum variable latency (ps) for which we
				// are solving.  Represents the largest delay
				// allowed for any gate caused by total load
				// capacitance.  This is empirically derived.
double MaxOutputCap = 30.0;	// Maximum capacitance for an output node (fF).
				// Outputs should be able to drive this much
				// capacitance within MaxLatency time (ps).
double WireCap = 10.0;		// Base capacitance for an output node, estimate
				// of average wire capacitance (fF).

struct Gatelist {
    char   *gatename;
    Cell   *gatecell;		
    char   *suffix;		// points to position in gatename, not allocated
    char   *separator;
    int	   num_inputs;
    double Cint;
    double delay;
    double strength;
} Gatelist_;

struct hashtable Gatehash;

struct Nodelist {
    char   ignore;
    char   *nodename;
    struct Gatelist *outputgate;
    double outputgatestrength;
    int    type;
    int    clock;
    int    num_inputs;
    double total_load;
    double ratio;                // drive strength to total_load ratio
    // For net buffer trees
    int	   num_buf;		// Number of buffers to split the net
    int	   curcount;		// Active count for fanout buffering trees
} Nodelist_;

struct hashtable Nodehash;
struct hashtable Bushash;

struct Bus {
    int imax;
    int imin;
} Bus_;

struct Drivelist {
    char *Separator;	// Separator (e.g., "X")
    char *DriveType;	// Suffix name (e.g., "1")
    int NgatesIn;	// Number of gates with this suffix in input
    int NgatesOut;	// Number of gates with this suffix in output
} Drivelist_;

struct hashtable Drivehash;

struct Baselist {
    char *BaseName;		// gate base name (e.g., "INV")
    int Ndrives;		// number of different drive types for this gate
    struct Gatelist **gates;	// list of pointers to gates with
} Baselist_;

struct hashtable Basehash;

enum states_ {NONE, INPUTS, OUTPUTS, GATENAME, PINNAME, INPUTNODE, CLOCKNODE,
	OUTPUTNODE, ENDMODEL, ERROR};
enum nodetype_ {UNKNOWN, INPUT, CLOCK, OUTPUT, INPUTPIN, OUTPUTPIN, INOUTPIN};

int read_gate_file(char *gate_file_name, char *separator);
void read_ignore_file(char *ignore_file_name);
struct Gatelist *GatelistAlloc();
struct Nodelist *NodelistAlloc();
struct Drivelist *DrivelistAlloc();
struct Baselist *BaselistAlloc();
void showgatelist(void);
void helpmessage(void);
struct Nodelist *registernode(char *nodename, int type, struct Gatelist *gl,
		char *pinname);
void shownodes(void);
void resize_gates(struct cellrec *topcell, int doLoadBalance, int doFanout);
void write_output(struct cellrec *topcell, FILE *outfptr, int doLoadBalance,
		int doFanout);
struct Gatelist *best_size(struct Gatelist *gl, double amount, char *overload);
void count_gatetype(struct Gatelist *gl, int num_in, int num_out);

/*
 *---------------------------------------------------------------------------
 * find_suffix ---
 *
 *	Given a gate name, return the part of the name corresponding to the
 *	suffix.  That is, find the last occurrance of the string separator
 *	and return a pointer to the following character.  Note that a NULL
 *	separator means there is no suffix, vs. an emptry string separator,
 *	which means that the suffix encompasses all digits at the end of
 *	the gate name.
 *---------------------------------------------------------------------------
 */

char *find_suffix(char *gatename, char *separator)
{
    char *tsuf, *gptr;
    char *suffix = NULL;

    if (separator == NULL) {
	return NULL;
    }
    else if (*separator == '\0') {
	suffix = gatename + strlen(gatename) - 1;
	while (isdigit(*suffix)) suffix--;
	suffix++;
    }
    else {
	gptr = gatename;
	while ((tsuf = strstr(gptr, separator)) != NULL) {
	    suffix = tsuf;
	    gptr = tsuf + 1;
	}
	if (suffix != NULL)
	    suffix += strlen(separator);
    }
    return suffix;
}

/*
 *---------------------------------------------------------------------------
 * Check if a liberty file function describes a buffer.  Since this is the
 * function of the output pin, it needs only be the input pin (e.g.,
 * func(Q) = "A").  However, some liberty files repeat the output pin (e.g.,
 * func(Q) = "Q = A").  Check for both styles.
 *---------------------------------------------------------------------------
 */

int is_buffer_func(char *func_text, char *pin_in, char *pin_out) {
    char *eqptr, esav, *tptr;

    if (!strcmp(func_text, pin_in)) return 1;

    else if ((eqptr = strchr(func_text, '=')) != NULL) {
	tptr = eqptr + 1;
	while (isspace(*tptr) && (*tptr != '\0')) tptr++;
	while ((eqptr > func_text) && isspace(*(eqptr - 1))) eqptr--;
	esav = *eqptr;
	*eqptr = '\0';
	if (!strcmp(func_text, pin_out) && (*tptr != '\0') &&
			!strcmp(tptr, pin_in)) {
	    *eqptr = esav;
	    return 1;
	}
	*eqptr = esav;
    }
    return 0;
}

typedef struct _gaterec {
    char *path;			/* Path to library */
    char *sep;			/* Gate name separator ("-" if none) */
} GateRec;

/*----------------------------------------------------------------------*/
/* Insert buffers to reduce fanout					*/
/*									*/
/* This is done in two passes to avoid changing the hash entries while	*/
/* iterating through them.  Nodes needed buffering are marked with the	*/
/* number of buffers required.  Then, the nodes are parsed again and	*/
/* buffers are added where marked.					*/
/*----------------------------------------------------------------------*/

void insert_buffers(struct cellrec *topcell)
{
    int i, cidx;
    int hier;
    int slen;
    struct Nodelist *nl = NULL;
    struct Nodelist *nltest;
    char nodename[MAXLINE];
    char instname[MAXLINE];
    char *spos;

    struct Gatelist *glbuf, *clkbuf;
    struct Gatelist *gl;

    struct portrec *port, *newport;
    struct instance *inst, *newinst;

    // Find the gate record corresponding to the buffer name
    glbuf = (struct Gatelist *)HashLookup(Buffername, &Gatehash);

    // Find the gate record corresponding to the clock buffer name
    clkbuf = (struct Gatelist *)HashLookup(Clkbufname, &Gatehash);

    Buffer_count = 0;
    if ((Topfanout > MaxFanout) || (Inputfanout > MaxFanout)) {

	/* Mark nets for inserting buffer trees */

	nl = (struct Nodelist *)HashFirst(&Nodehash);
	while (nl != NULL) {
	    if (nl->ignore == FALSE) {

		// Nets with no driver must be module inputs.  Mainly,
		// this condition rejects power and ground nodes.

		if ((nl->num_inputs > MaxFanout) && ((nl->outputgatestrength != 0.0)
				|| (nl->type == INPUTPIN))) {
		    double d;
		    int stages, n, mfan;

		    // Find number of hierarchical stages (plus one) needed

		    mfan = nl->num_inputs;
		    stages = 1;
		    n = MaxFanout;
		    while (mfan > MaxFanout) {
			mfan = nl->num_inputs / n;
			n *= MaxFanout;
			stages++;
		    }

		    // Find the floor of the number of fanouts per buffer

		    d = pow((double)nl->num_inputs, (double)(1.0 / stages));
		    n = (int)((double)nl->num_inputs / d);

		    // Split network into reasonably balanced groups of
		    // the same (or almost) fanout.

		    nl->num_buf = n;
		    nl->curcount = n - 1;
		    Buffer_count += n;
		}
	    }
	    nl = (struct Nodelist *)HashNext(&Nodehash);
	}
    }

    /* Parse all instances and adjust net names to account for buffer trees.	*/

    for (inst = topcell->instlist; inst; inst = inst->next) {
	/* Check each port and net connection */
	gl = (struct Gatelist *)HashLookup(inst->cellname, &Gatehash);
	if (gl == NULL) continue;
	for (port = inst->portlist; port; port = port->next) {
	    if (port->direction != PORT_OUTPUT) {

		if (VerboseFlag) printf("\nInput node %s", port->net);
		nl = (struct Nodelist *)HashLookup(port->net, &Nodehash);
		if (nl == NULL) {
		    fprintf(stderr, "vlogFanout:  Port net %s not in hash\n", port->net);
		    continue;
		}
		if (nl->num_buf > 0) {
		    hier = 0;
		    nltest = nl;
		    sprintf(nodename, "%s", nl->nodename);
		    while (1) {
	 		slen = strlen(nodename);
			spos = nodename + slen - 1;
			if (*spos == ']') {
			    /* Avoid downstream problems:		*/
			    /* recast "[X]_bF$bufN" as _X_bF$bufN"	*/
			    char *dptr = nodename + slen - 1;
			    while (dptr >= nodename && *dptr != '[') dptr--;
			    if (dptr >= nodename) *dptr = '_';
			    sprintf(spos, "_bF$buf%d", nl->curcount);
			}
			else {
			    spos++;
			    sprintf(spos, "_bF$buf%d", nl->curcount);
			}

			/* For buffer trees of depth > 1, there will be	 */
			/* an existing node name wih the _bufN extension */
			/* that is in the node hash table.  If so, then	 */
			/* add the prefix "_hierM".  Test again in case	 */
			/* the buffer tree is even deeper, incrementing  */
			/* until the name is unique.			 */

			nltest = (struct Nodelist *)HashLookup(nodename, &Nodehash);
			if (nltest == NULL) break;
			if (nltest->outputgate == NULL) break;
			sprintf(nodename, "%s_hier%d", nl->nodename, hier);
			hier++;
		    }

		    /* Increment input count and load cap on new node */
		    registernode(nodename, INPUT, gl, port->name);

		    nl->curcount--;
		    if (nl->curcount < 0) nl->curcount = nl->num_buf - 1;

		    /* Reassign the port's net name */
		    free(port->net);
		    port->net = strdup(nodename);
		}
	    }
	}
    }

    /* Insert any added buffers */

    cidx = 0;
    nl = (struct Nodelist *)HashFirst(&Nodehash);
    while (nl != NULL) {
	for (i = nl->num_buf - 1; i >= 0; i--) {
	    hier = 0;
	    nltest = nl;
	    sprintf(nodename, "%s", nl->nodename);
	    while (1) {
		slen = strlen(nodename);
		spos = nodename + slen - 1;
		if (*spos == ']') {
		    /* Avoid downstream problems:		*/
		    /* recast "[X]_bF$bufN" as _X_bF$bufN"	*/
		    char *dptr = nodename + slen - 1;
		    while (dptr >= nodename && *dptr != '[') dptr--;
		    if (dptr >= nodename) *dptr = '_';
		    sprintf(spos, "_bF$buf%d", i);
		}
		else {
		    spos++;
		    sprintf(spos, "_bF$buf%d", i);
		}

		/* For buffer trees of depth > 1, there will be  */
		/* an existing node name wih the _bufN extension */
		/* that is in the node hash table.  If so, then  */
		/* add the prefix "_hierM".  Test again in case  */
		/* the buffer tree is even deeper, incrementing  */
		/* M until the name is unique.		    */

		nltest = (struct Nodelist *)HashLookup(nodename, &Nodehash);
		if (nltest == NULL) break;
		if (nltest->outputgate == NULL) break;
		sprintf(nodename, "%s_hier%d", nl->nodename, hier);
		hier++;
	    }

	    if (nl->clock == TRUE) {
		/* Prepend clock buffer to instance list */
		newinst = PrependInstance(topcell, Clkbufname);
		sprintf(instname, "%s_insert%d", Clkbufname, cidx);
		newinst->instname = strdup(instname);
		newport = InstPort(newinst, clkbuf_in_pin, nl->nodename);
		newport = InstPort(newinst, clkbuf_out_pin, nodename);

		/* Register the new node name */
		Net(topcell, nodename);
		registernode(nodename, OUTPUT, clkbuf, clkbuf_out_pin);
	    }
	    else {
		/* Prepend regular buffer to instance list */
		newinst = PrependInstance(topcell, Buffername);
		sprintf(instname, "%s_insert%d", Buffername, cidx);
		newinst->instname = strdup(instname);
		newport = InstPort(newinst, buf_in_pin, nl->nodename);
		newport = InstPort(newinst, buf_out_pin, nodename);

		/* Register the new node name */
		Net(topcell, nodename);
		registernode(nodename, OUTPUT, glbuf, buf_out_pin);
	    }
	    cidx++;
	}
	nl->num_inputs = nl->num_buf;
	nl->num_buf = 0;
	nl = (struct Nodelist *)HashNext(&Nodehash);
    }
}

/*
 *---------------------------------------------------------------------------
 * Read a file of nets for which we should ignore fanout.  Typically this
 * would include the power and ground nets, but may include other static
 * nets with non-critical timing.
 *---------------------------------------------------------------------------
 */

void read_ignore_file(char *ignore_file_name)
{
    struct Nodelist *nl;
    FILE *ignorefptr;
    char line[MAXLINE];	/* One net per line, should not need dynamic allocation */
    char *s, *sp;

    if (!(ignorefptr = fopen(ignore_file_name, "r"))) {
	fprintf(stderr, "vlogFanout:  Couldn't open %s as ignore file.\n",
			ignore_file_name);
	fflush(stderr);
	return;
	// This is only a warning.  It will not stop execution of vlogFanout
    }

    while ((s = fgets(line, MAXLINE, ignorefptr)) != NULL) {
	// One net name per line
	while (isspace(*s)) s++;
	sp = s;
	while (*sp != '\0' && *sp != '\n' && !isspace(*sp)) sp++;
	*sp = '\0';

	nl = (struct Nodelist *)HashLookup(s, &Nodehash);
	if (nl != NULL) {
	    nl->ignore = (char)1;
	}
    }
    fclose(ignorefptr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Call read_liberty() to read in cell information from a Liberty file,
 * and then hash the resulting list.
 *
 *---------------------------------------------------------------------------
 */

int read_gate_file(char *gate_file_name, char *separator)
{
    int i, j, k, ind, format = -1;
    int gatecount;
    char *s, *t, *ss, ssave;
    struct Gatelist *gl;
    struct Baselist *bl;
    Cell *cells, *curcell;
    Pin *curpin;

    gatecount = 0;
    cells = read_liberty(gate_file_name, NULL);
    for (curcell = cells; curcell != NULL; curcell = curcell->next) {
	if (curcell->name == NULL) continue;	/* undefined/unused cell */

	gl = GatelistAlloc();
	gl->gatename = strdup(curcell->name);
	gl->suffix = find_suffix(gl->gatename, separator);
	gl->separator = separator;
	gl->gatecell = curcell;

	get_values(curcell, &gl->delay, &gl->Cint);

	gl->num_inputs = 0;
	for (curpin = curcell->pins; curpin; curpin = curpin->next)
	    if (curpin->type == PIN_INPUT || curpin->type == PIN_CLOCK)
		gl->num_inputs++;

	/* The "MaxLatency" is empirically derived.  Since gl->delay	*/
	/* is in ps/fF, and strength is compared directly to total	*/
	/* load capacitance, MaxLatency is effectively in units of ps	*/
	/* and represents the maximum latency due to load capacitance	*/
	/* for any gate in the circuit after making gate strength	*/
	/* substitutions (this does not include internal, constant	*/
	/* delays in each gate).					*/

	// (Diagnostic, for debug)
	// fprintf(stdout, "Parsing cell \"%s\", \"%s\", function \"%s\"\n",
	//	gl->gatename, gl->gatecell->name, gl->gatecell->function);

	gl->strength = MaxLatency / gl->delay;
 	HashPtrInstall(gl->gatename, gl, &Gatehash);
	gatecount++;

	/* Install prefix in Basehash.  Note that prefix contains the	*/
	/* separator string, if any.					*/

	if ((s = gl->suffix) == NULL)
	    ind = strlen(gl->gatename);
	else
	    ind = (int)(s - gl->gatename);

	ssave = gl->gatename[ind];
	gl->gatename[ind] = '\0';
	bl = (struct Baselist *)HashLookup(gl->gatename, &Basehash);
	if (bl == NULL) {
	    bl = BaselistAlloc();
	    HashPtrInstall(gl->gatename, bl, &Basehash);
	    bl->BaseName = strdup(gl->gatename);
	}
	gl->gatename[ind] = ssave;

	// Note:  this code assumes that there are no repeats in
	// the liberty file gate list (there shouldn't be).

	if (bl->Ndrives == 0)
	    bl->gates = (struct Gatelist **)malloc(sizeof(struct Gatelist *));
	else
	    bl->gates = (struct Gatelist **)realloc(bl->gates,
			(bl->Ndrives + 1) * sizeof(struct Gatelist *));
	bl->gates[bl->Ndrives] = gl;
	bl->Ndrives++;
    }
    return gatecount;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

struct Gatelist* GatelistAlloc()
{
    struct Gatelist *gl;

    gl = (struct Gatelist*)malloc(sizeof(struct Gatelist));
    gl->gatename = NULL;
    gl->suffix = NULL;
    gl->num_inputs = 0;
    gl->Cint = 0.0;
    gl->delay = 0.0;
    gl->strength = 0.0;
    return gl;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

struct Drivelist *DrivelistAlloc()
{
    struct Drivelist *dl;

    dl = (struct Drivelist *)malloc(sizeof(struct Drivelist));
    dl->NgatesIn = 0;
    dl->NgatesOut = 0;
    dl->DriveType = NULL;
    dl->Separator = NULL;
    return dl;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

struct Nodelist* NodelistAlloc()
{
    struct Nodelist *nl;

    nl = (struct Nodelist *)malloc(sizeof(struct Nodelist));
    nl->nodename = NULL;
    nl->ignore = FALSE;
    nl->outputgate = NULL;
    nl->outputgatestrength = 0.0;
    nl->type = UNKNOWN;
    nl->total_load = 0.0;
    nl->num_inputs = 0;
    nl->num_buf = 0;		// Tree expansion of node
    nl->curcount = 0;
    nl->clock = FALSE;
    return nl;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

struct Baselist *BaselistAlloc()
{
    struct Baselist *bl;

    bl = (struct Baselist *)malloc(sizeof(struct Baselist));
    bl->BaseName = NULL;
    bl->Ndrives = 0;
    bl->gates = NULL;
    return bl;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void showgatelist(void)
{
    struct Gatelist *gl;
    Cell *curcell;
    Pin *curpin;
    double pincap;

    gl = (struct Gatelist *)HashFirst(&Gatehash);
    while (gl != NULL) {

	printf("\n\ngate: %s with %d inputs and %g drive strength\n",
			gl->gatename, gl->num_inputs, gl->strength);
	printf("%g ", gl->Cint);
     
	curcell = gl->gatecell;
	for (curpin = curcell->pins; curpin; curpin = curpin->next) {
	    if (curpin->type == PIN_INPUT || curpin->type == PIN_CLOCK) {
		get_pincap(curcell, curpin->name, &pincap);
		printf("%g   ", pincap);
 	    }
	}
	gl = (struct Gatelist *)HashNext(&Gatehash);
    }
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

struct Nodelist *registernode(char *nodename, int type, struct Gatelist *gl,
		char *pinname)
{
    struct Nodelist *nl;
    double pincap;
    char *dptr;

    nl = (struct Nodelist *)HashLookup(nodename, &Nodehash);
   
    if (nl == NULL) {
	nl = NodelistAlloc();
	nl->nodename = strdup(nodename);
	if (type == OUTPUT) nl->outputgate = NULL;
	HashPtrInstall(nodename, nl, &Nodehash);
	nl->type = type;
	nl->outputgate = NULL;

	if ((dptr = strchr(nodename, '[')) != NULL) {
	    struct Bus *newbus = (struct Bus *)malloc(sizeof(struct Bus));
	    int idx;
	    *dptr = '\0';
	    sscanf(dptr + 1, "%d", &idx);
	    newbus->imax = newbus->imin = idx; 
	    HashPtrInstall(nodename, newbus, &Bushash);
	    *dptr = '[';
	}
    }
    else {
	if ((dptr = strchr(nodename, '[')) != NULL) {
	    struct Bus *newbus;
	    int idx;
	    *dptr = '\0';
	    newbus = (struct Bus *)HashLookup(nodename, &Bushash);
	    sscanf(dptr + 1, "%d", &idx);
	    if (idx < newbus->imin) newbus->imin = idx; 
	    if (idx > newbus->imax) newbus->imax = idx; 
	    *dptr = '[';
	}
    }

    if (type == OUTPUT) {
	nl->outputgate = gl;
	if (gl != NULL) {
	    nl->outputgatestrength = gl->strength;
	    nl->total_load += gl->Cint;
	    count_gatetype(gl, 1, 1);
	}
    }
    else if (type == INPUT || type == CLOCK) {
	if (gl != NULL) {
	    get_pincap(gl->gatecell, pinname, &pincap);
	    nl->total_load += pincap;
	    nl->num_inputs++;
	}
    }
    if (type == CLOCK) nl->clock = TRUE;

    if ((nl->type != INPUTPIN) && (nl->type != OUTPUTPIN) && (gl == NULL)) {
	fprintf(stderr, "\nError: no output gate for net %s\n", nodename);
	fflush(stderr);
    }
    return nl;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void count_gatetype(struct Gatelist *gl, int num_in, int num_out)
{
    struct Drivelist *dl;
    char *s, *nptr, *tsuf;
    int g;

    if ((s = gl->suffix) == NULL)
	return;

    dl = (struct Drivelist *)HashLookup(s, &Drivehash);
    if (dl == NULL) {

	// New drive type found

	dl = DrivelistAlloc();
	HashPtrInstall(s, dl, &Drivehash);
	dl->DriveType = strdup(s);
	dl->Separator = gl->separator;
    }

    dl->NgatesIn += num_in;	// Number of these gates before processing
    dl->NgatesOut += num_out;	// Number of these gates after processing
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void shownodes(void)
{
    struct Nodelist *nl;
    int i;

    nl = (struct Nodelist *)HashFirst(&Nodehash);
    while (nl != NULL) {
	printf("\n\nnode: %s with %d fanout and %g fF cap",
		nl->nodename, nl->num_inputs, nl->total_load);
	printf("\ndriven by %s, with %g strength.\n",
		nl->outputgate->gatename, nl->outputgatestrength);
	nl = (struct Nodelist *)HashNext(&Nodehash);
    }
}

/*--------------------------------------------------------------------------*/
/* Recursion callback function for each item in the cellrec nets hash table */
/*--------------------------------------------------------------------------*/

struct nlist *output_wires(struct hashlist *p, void *cptr)
{
    struct netrec *net;
    FILE *outf = (FILE *)cptr;

    net = (struct netrec *)(p->ptr);
    
    fprintf(outf, "wire ");
    if (net->start >= 0 && net->end >= 0) {
	fprintf(outf, "[%d:%d] ", net->start, net->end);
    }
    fprintf(outf, "%s ;\n", p->name);
    return NULL;
}

/*----------------------------------------------------------------------*/
/* Recursion callback function for each item in the cellrec properties	*/
/* hash table								*/
/*----------------------------------------------------------------------*/

struct nlist *output_props(struct hashlist *p, void *cptr)
{
    char *propval = (char *)(p->ptr);
    FILE *outf = (FILE *)cptr;

    fprintf(outf, ".%s(%s),\n", p->name, propval);
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 * Resize gates
 *---------------------------------------------------------------------------
 */

void resize_gates(struct cellrec *topcell, int doLoadBalance, int doFanout)
{
    char *s, *t;
    char instname[MAXLINE];
    char *stren, *orig;
    int  gateinputs;
    int  pincount;
    int  needscorrecting;
    int i;
    struct Gatelist *gl;
    struct Gatelist *glbest, *bbest;
    struct Gatelist *glbuf;
    struct Nodelist *nl;
    struct Drivelist *dl;
    double inv_size;

    struct netrec *net;
    struct portrec *port, *newport;
    struct instance *inst, *newinst;
    struct cellrec *sizedcell;
    int cidx;

    Changed_count = 0;
    needscorrecting = 0;

    // Find the gate record corresponding to the buffer name
    glbuf = (struct Gatelist *)HashLookup(Buffername, &Gatehash);

    /* If there are situations where a load is high but the fanout is low,	*/
    /* and insertion of a buffer will reduce the overall delay, then insert a	*/
    /* buffer.									*/

    for (inst = topcell->instlist; inst; inst = inst->next) {
	needscorrecting = FALSE;

	/* Find the gate's output node and determine if the gate is resized */

        gl = (struct Gatelist *)HashLookup(inst->cellname, &Gatehash);
	if (gl == NULL) continue;

	for (port = inst->portlist; port; port = port->next)
	    if (port->direction == PORT_OUTPUT)
		break;

	if (port) {
	    nl = (struct Nodelist *)HashLookup(port->net, &Nodehash);
	    if (doLoadBalance && (nl != NULL)) {
		if ((nl->ignore == FALSE) && (nl->ratio > 1.0)) {
		    if (VerboseFlag)
			printf("\nGate %s (%s) should be %g times stronger",
				inst->instname, inst->cellname, nl->ratio);
		    needscorrecting = TRUE;
		    orig = gl->suffix;
		    glbest = best_size(gl, nl->total_load + WireCap, NULL);
		    if (glbest && VerboseFlag)
			printf("\nGate changed from %s to %s\n", gl->gatename,
				glbest->gatename);
		    inv_size = nl->total_load;
		}

		// Is this node an output pin?  Check required output drive.
		if ((nl->ignore == FALSE) && (nl->type == OUTPUTPIN)) {
		    orig = gl->suffix;
		    glbest = best_size(gl, nl->total_load + MaxOutputCap
				+ WireCap, NULL);
		    if (glbest && (glbest != gl)) {
 			if (doLoadBalance) {
			    needscorrecting = TRUE;
			    if (VerboseFlag)
				printf("\nOutput Gate changed from %s to %s\n",
						gl->gatename, glbest->gatename);
			}
		    }
		}
		// Don't attempt to correct gates for which we cannot
		// find a suffix
		if (orig == NULL) needscorrecting = FALSE;
	    }
	}
	
	/* Write cell name, possibly modified for gate strength */
	if (needscorrecting) {
	    if (glbest == NULL) {      // return val to insert inverters

		if (VerboseFlag)
		    printf("\nInsert buffers %s - %g\n", s, inv_size);

		s = strstr(port->name, nl->nodename);	// get output node
		s = strtok(s, " \\\t");		// strip it clean
		if (*s == '[') {
		    char *p = strchr(s, ']');
		    if (p != NULL)
			strcpy(p, "_bF$buf]\";\n");            // rename it
		    else
			strcat(s, "_bF%buf\";\n");
		}
		else
		    strcat(s, "_bF$buf\";\n");            // rename it

		bbest = best_size(glbuf, inv_size + WireCap, NULL);

		/* If bbest->suffix is NULL, then we will have to break */
		/* up this network.					*/
		/* Buffer trees will be inserted by downstream tools, 	*/
		/* after analyzing the placement of the network.  This	*/
		/* error needs to be passed down to those tools. . .	*/

		if (bbest == NULL) {
		    fprintf(stderr, "Fatal error:  No gates found for %s\n",
				glbuf->gatename);
		}

		dl = (struct Drivelist *)HashLookup(bbest->suffix, &Drivehash);
		if (dl != NULL) dl->NgatesOut++;

		/* Recompute size of the gate driving the buffer */
		if (nl != NULL) {
		    get_pincap(bbest->gatecell, buf_in_pin, &nl->total_load);
		    if (gl != NULL) {
			nl->total_load += gl->Cint;
		    }
		}
		orig = gl->suffix;
		glbest = best_size(gl, nl->total_load + WireCap, NULL);

		/* Prepend buffer to instance list */
		newinst = PrependInstance(topcell, bbest->gatename);
		sprintf(instname, "%s_insert%d", bbest->gatename, cidx);
		newinst->instname = strdup(instname);
		newport = InstPort(newinst, buf_in_pin, s);
		newport = InstPort(newinst, buf_out_pin, nl->nodename);
		cidx++;

		/* Register the new node name */
		if (HashLookup(s, &topcell->nets) == NULL) {
		    Net(topcell, s);
		    registernode(s, INPUT, bbest, buf_in_pin);
		}
	    }
	    if ((gl != NULL) && (gl != glbest)) Changed_count++;
	
	    /* Reassign the instance's cell */
	    free(inst->cellname);
	    inst->cellname = strdup(glbest->gatename);
	}
    }
}

/*
 *---------------------------------------------------------------------------
 * Rewrite the verilog output with resized gates and clock and buffer trees
 *---------------------------------------------------------------------------
 */

void write_output(struct cellrec *topcell, FILE *outfptr, int doLoadBalance,
		int doFanout)
{
    struct netrec *net;
    struct portrec *port;
    struct instance *inst;

    /* Write output module header */
    fprintf(outfptr, "/* Verilog module written by vlogFanout (qflow) */\n");
    if (doFanout)
	fprintf(outfptr, "/* With clock tree generation and fanout reduction */\n");
    if (doLoadBalance)
	fprintf(outfptr, "/* %s gate resizing */\n", (doFanout) ? "and" : "With");
    fprintf(outfptr, "\n");

    fprintf(outfptr, "module %s(\n", topcell->name);
    for (port = topcell->portlist; port; port = port->next) {
	switch(port->direction) {
	    case PORT_INPUT:
		fprintf(outfptr, "    input ");
		break;
	    case PORT_OUTPUT:
		fprintf(outfptr, "    output ");
		break;
	    case PORT_INOUT:
		fprintf(outfptr, "    inout ");
		break;
	}
	net = HashLookup(port->name, &topcell->nets);
	if (net && net->start >= 0 && net->end >= 0) {
	    fprintf(outfptr, "[%d:%d] ", net->start, net->end);
	}
	fprintf(outfptr, "%s", port->name);
	if (port->next) fprintf(outfptr, ",");
	fprintf(outfptr, "\n");
    }
    fprintf(outfptr, ");\n\n");

    /* Declare all wires */
    RecurseHashTablePointer(&topcell->nets, output_wires, outfptr);
    fprintf(outfptr, "\n");

    /* Write instances in the order of the input file */

    for (inst = topcell->instlist; inst; inst = inst->next) {
	int nprops = RecurseHashTable(&inst->propdict, CountHashTableEntries);
	fprintf(outfptr, "%s ", inst->cellname);
	if (nprops > 0) {
	    fprintf(outfptr, "#(\n");
	    RecurseHashTablePointer(&inst->propdict, output_props, outfptr);
	    fprintf(outfptr, ") ");
 	}
	if (inst->cellname)
	    fprintf(outfptr, "%s (\n", inst->instname);
	else 
	    fprintf(outfptr, "vlogFanout:  No cell for instance %s\n", inst->instname);

	/* Write each port and net connection */
	for (port = inst->portlist; port; port = port->next) {
	    fprintf(outfptr, "    .%s(%s)", port->name, port->net); 
	    if (port->next) fprintf(outfptr, ",");
	    fprintf(outfptr, "\n");
	}
	fprintf(outfptr, ");\n\n");
    }

    /* End the module */
    fprintf(outfptr, "endmodule\n");

    fflush(stdout);
}

/*
 *---------------------------------------------------------------------------
 * Return a pointer to the gate with the drive strength that is the
 * minimum necessary to drive the load "amount".
 *
 * If the load exceeds the available gate sizes, then return the maximum
 * strength gate available, and set "overload" to TRUE.  Otherwise, FALSE
 * is returned in "overload".
 *---------------------------------------------------------------------------
 */

struct Gatelist *best_size(struct Gatelist *gl, double amount, char *overload)
{
    char *s;
    char ssave;
    int ind, i;
    double amax = 1.0E10;		// Assuming no gate is this big!
    double gmax = 0.0;
    struct Gatelist *newgl, *glbest = NULL, *glsave = NULL;
    struct Baselist *bl;

    if (overload) *overload = FALSE;
    if ((s = gl->suffix) == NULL) return NULL;
    ind = (int)(s - gl->gatename);	// Compare out to and including the suffix
    ssave = gl->gatename[ind];
    gl->gatename[ind] = '\0';

    bl = (struct Baselist *)HashLookup(gl->gatename, &Basehash);
    gl->gatename[ind] = ssave;

    for (i = 0; bl && (i < bl->Ndrives); i++) {
	newgl = bl->gates[i];
	if (newgl->strength >= gmax) {
	    gmax = newgl->strength;
	    glsave = newgl;
	}
	if (amount <= newgl->strength) {
	    if (newgl->strength < amax) {
		if (newgl->suffix) {
		    glbest = newgl;
		    amax = newgl->strength;
		}
	    }
	}
    }

    if (amax == 1.0E10) {
	double oratio;

	stren_err_counter++;
	if (overload) *overload = TRUE;

	if (glsave != NULL)
	    glbest = glsave;
	else
	    glbest = NULL;

	if (gmax > 0.0) {
	    oratio = (double)(amount / gmax);
	    if (oratio > MaxOverload) {

	        fprintf(stderr, "Warning %d: load of %g is %g times greater "
			"than strongest gate %s\n",
			stren_err_counter, amount, oratio, glsave->gatename);

		if (MaxOverload == 0.0) 
		    fprintf(stderr, "This warning will only be repeated for "
				"larger overload ratios.  Warning count reflects\n"
				"the total number of overloaded nets.\n");

		MaxOverload = oratio;
	    }
	}
    }
    return glbest;
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void helpmessage(void)
{
    printf("\nvlogFanout:\n\n");
    printf("vlogFanout looks at a synthesized BLIF netlist.\n");
    printf("Node fanout is measured, and gate size is adjusted.\n");
    printf("File \"gate.cfg\" is used to describe the RTL gates.\n\n");

    printf("\tUsage: vlogFanout [-switches] vlog_in [vlog_out].\n\n");

    printf("vlogFanout returns the number of gate substitutions made.\n");
    printf("Typically, it will be iterated until convergence (return value 0).\n\n");
    
    printf("valid switches are:\n");
    printf("\t-f\t\tRun gate fanout buffering only (no load balancing)\n");
    printf("\t-L\t\tRun gate load balance optimization only (no fanout buffering)\n");
    printf("\t-g\t\tDebug mode: parse and print the gate.cfg table\n");
    printf("\t-n\t\tDebug mode: parse and print the node list\n");
    printf("\t-v\t\tDebug mode: verbose output\n");
    printf("\t-l latency\tSet the maximum variable latency (ps).  "
		"(value %g, default 1000.0)\n", MaxLatency);
    printf("\t-F value\tSet the maximum fanout per node (value %d, default 16)\n",
		MaxFanout);
    printf("\t-b buffername\tSet the name of a buffer gate\n");
    printf("\t-i pin_name\tSet the name of the buffer gate input pin (used with -b)\n");
    printf("\t-o pin_name\tSet the name of the buffer gate output pin (used with -b)\n");
    printf("\t-s separator\tGate names have \"separator\" before drive strength\n");
    printf("\t-c value\tSet the maximum output capacitance (fF).  "
		"(value %g, default 30.0)\n", MaxOutputCap);
    printf("\t-p filepath\tSpecify an alternate path and filename for gate.cfg\n");
    printf("\t-I filepath\tSpecify a path and filename for list of nets to ignore\n");
    printf("\t-h\t\tprint this help message\n\n");

    printf("This will not work at all for tristate gates.\n");
    printf("Nodes with multiple outputs are assumed to be in parallel.\n");
}

/*
 *---------------------------------------------------------------------------
 * main routine for vlogFanout
 *---------------------------------------------------------------------------
 */

int main (int argc, char *argv[])
{
    int i, j, k, l, iter;
    int state;
    int maxline, curline;
    int libcount;
    int inputcount;
    int gateinputs;
    int gatecount;
    int doLoadBalance = 1;
    int doFanout = 1;
    int nodetype, porttype;
    char netname[MAXLINE];
    char *pinname;
    char *libfile, *libsep;
    char *separg = NULL;
    char *test;
    char *s, *t, *comptr;
    FILE *outfptr;
    char *line;
    GateRec *Gatepaths = NULL;
    struct Gatelist *gl = NULL;
    struct Nodelist *nl = NULL, *nlmax, *nlimax;
    struct Drivelist *dl = NULL;
    struct cellrec *topcell;
    struct portrec *port;
    struct instance *inst;
    struct netrec *net;
    int cur_pintype;

    SuffixIsNumeric = TRUE;	// By default, assume numeric suffixes

    InitializeHashTable(&Nodehash, LARGEHASHSIZE);
    InitializeHashTable(&Bushash, SMALLHASHSIZE);
    InitializeHashTable(&Drivehash, SMALLHASHSIZE);
    InitializeHashTable(&Gatehash, SMALLHASHSIZE);
    InitializeHashTable(&Basehash, SMALLHASHSIZE);

    fprintf(stdout, "vlogFanout for qflow " QFLOW_VERSION "." QFLOW_REVISION "\n");

    while ((i = getopt(argc, argv, "fLSgnhvl:c:b:i:o:p:s:I:F:")) != EOF) {
	switch (i) {
	    case 'b':
		/* If value is a comma-separated pair, the first is a		*/
		/* general purpose buffer and the second is a clock buffer.	*/
		Buffername = strdup(optarg);
		if ((comptr = strchr(Buffername, ',')) != NULL) {
		    *comptr = '\0';
		    Clkbufname = comptr + 1;
		}
		break;
	    case 'i':
		buf_in_pin = strdup(optarg);
		if ((comptr = strchr(buf_in_pin, ',')) != NULL) {
		    *comptr = '\0';
		    clkbuf_in_pin = comptr + 1;
		}
		break;
	    case 'o':
		buf_out_pin = strdup(optarg);
		if ((comptr = strchr(buf_out_pin, ',')) != NULL) {
		    *comptr = '\0';
		    clkbuf_out_pin = comptr + 1;
		}
		break;
	    case 'p':
		/* Allow multiple files to be specified as space-separated	*/
		/* list, either by specifying all on one "-p" arguments		*/
		/* or by passing multiple "-p" arguments.			*/

		if (Gatepaths == NULL) {
		    libcount = 1;
		    Gatepaths = (GateRec *)malloc(sizeof(GateRec));
		    Gatepaths->path = strdup(optarg);
		    Gatepaths->sep = (separg) ? strdup(separg) : NULL;
		}
		else {
		    libcount++;
		    Gatepaths = (GateRec *)realloc(Gatepaths, libcount * sizeof(GateRec));
		    Gatepaths[libcount - 1].path = strdup(optarg);
		    Gatepaths[libcount - 1].sep = (separg) ? strdup(separg) : NULL;
		}
		break;
	    case 'f':	// fanout only
		doLoadBalance = 0;
		break;
	    case 'L':	// load balance only
		doFanout = 0;
		break;
	    case 'I':
		Ignorepath = strdup(optarg);
		break;
	    case 'F':
		MaxFanout = atoi(optarg);
		break;
	    case 'l':
		MaxLatency = atof(optarg);
		break;
	    case 'c':
		MaxOutputCap = atof(optarg);
		break;
	    case 's':
		if (!strcasecmp(optarg, "none")) {
		    if (separg) free(separg);
		    separg = NULL;
		}
		else
		    separg = strdup(optarg);
		break;
	    case 'S':
		if (separg) free(separg);
		separg = NULL;
		break;
            case 'g':
		GatePrintFlag = 1;
		break;
            case 'n':
		NodePrintFlag = 1;
 		break;
            case 'v':
		VerboseFlag = 1;
		break;
            case 'h':
		helpmessage();
		return 3;
		break;
            default:
		break;
	}
    }
    if (separg) free(separg);

    /* If there is only one set of in and out pins, then assume	*/
    /* that the pin names apply to both regular and clock	*/
    /* buffer types.						*/

    if (clkbuf_in_pin == NULL) {
	clkbuf_in_pin = buf_in_pin;
    }
    if (clkbuf_out_pin == NULL) {
	clkbuf_out_pin = buf_out_pin;
    }

    Inputfname = Outputfname = NULL;
    outfptr = stdout;
    i = optind;

    if (i < argc) {
	Inputfname = strdup(argv[i]);
    }
    i++;
    if (i < argc) {
	Outputfname = strdup(argv[i]);
	if (!(outfptr = fopen(Outputfname, "w"))) {
	    fprintf(stderr, "vlogFanout: Couldn't open %s for writing.\n", Outputfname);
	    return 1;
	}
    }
    i++;

    // Make sure we have a valid gate file path
    if (Gatepaths == NULL) {
	fprintf(stderr, "vlogFanout: No liberty file(s) specified.\n");
	return 1;
    }
    gatecount = 0;
    for (l = 0; l < libcount; l++) {
	int loccount;
	libfile = Gatepaths[l].path;
	libsep = Gatepaths[l].sep;
	loccount = read_gate_file(libfile, libsep);
	if (loccount == 0)
	    fprintf(stderr, "vlogFanout:  Warning:  No gates found in file %s!\n",
				libfile);
	gatecount += loccount;
    }

    // Determine if suffix is numeric or alphabetic
    if (gatecount > 0) {
	char *suffix;
	gl = (struct Gatelist *)HashFirst(&Gatehash);
	while (gl && gl->suffix == NULL)
	    gl = (struct Gatelist *)HashNext(&Gatehash);
	if (gl && gl->suffix && !isdigit(*gl->suffix))
	    SuffixIsNumeric = FALSE;
    }

    if (gatecount == 0) {
	fprintf(stderr, "vlogFanout:  No gates found in any input file!\n");
	return 1;
    }
    if (GatePrintFlag) {
	showgatelist();
	return 0;
    }

    if (buf_in_pin == NULL || buf_out_pin == NULL) {
	Pin *curpin;

	gl = (struct Gatelist *)NULL;

	if (Buffername != NULL) {
	    gl = (struct Gatelist *)HashLookup(Buffername, &Gatehash);
	    if (gl == NULL) {
		fprintf(stderr, "No buffer \"%s\" found in gate list\n", Buffername);
		fprintf(stderr, "Searching gate list for suitable buffer.\n");
	    }
	}

	if ((gl == NULL) || (Buffername == NULL)) {
	    // Find a suitable buffer
	    gl = (struct Gatelist *)HashFirst(&Gatehash);
	    while (gl != NULL) {
		Cell *ctest;

		// Find the first gate with one input, one output,
		// and a function string that matches the input pin name.

		ctest = gl->gatecell;
		if (ctest->pins && ctest->pins->next && !ctest->pins->next->next) {
		    if (ctest->pins->type == PIN_INPUT &&
				ctest->pins->next->type == PIN_OUTPUT) {
			if (is_buffer_func(ctest->function, ctest->pins->name,
				ctest->pins->next->name)) {
			    fprintf(stdout, "Using cell \"%s\" for buffers.\n",
				ctest->name);
			    Buffername = strdup(ctest->name);
			    break;
			}
		    }
		    else if (ctest->pins->type == PIN_OUTPUT &&
				ctest->pins->next->type == PIN_INPUT) {
			if (is_buffer_func(ctest->function, ctest->pins->next->name,
				ctest->pins->name)) {
			    fprintf(stdout, "Using cell \"%s\" for buffers.\n",
				ctest->name);
			    Buffername = strdup(ctest->name);
			    break;
			}
		    }
		}
		gl = (struct Gatelist *)HashNext(&Gatehash);
	    }
	}
	else
	    gl = (struct Gatelist *)HashLookup(Buffername, &Gatehash);

	if (gl == NULL) {
	    if (Buffername == NULL)
		fprintf(stderr, "vlogFanout:  No suitable buffer cell in library.\n");
	    else
		fprintf(stderr, "vlogFanout:  Buffer cell %s cannot be found.\n",
			Buffername);
	    return 1;
	}
	for (curpin = gl->gatecell->pins; curpin; curpin = curpin->next) {
	    if (curpin->type == PIN_INPUT) {
		if (buf_in_pin == NULL)
		    buf_in_pin = strdup(curpin->name);
	    }
	    else if (curpin->type == PIN_OUTPUT) {
		if (buf_out_pin == NULL)
		    buf_out_pin = strdup(curpin->name);
	    }
	}
	if (buf_in_pin == NULL || buf_out_pin == NULL) {
	    fprintf(stderr, "vlogFanout:  Could not parse I/O pins "
			"of buffer cell %s.\n", Buffername);
	    return 1;
	}
    }

    /* If Clkbufname is not defined, make it the same as Buffername */
    if (Clkbufname == NULL) Clkbufname = Buffername;

    /* Read the verilog file */
    topcell = ReadVerilog(Inputfname);

    if (topcell == NULL) {
	fprintf(stderr, "vlogFanout:  No module found in file!\n");
	return 1;
    }

    /* Transfer the contents of the verilog top cell into the local database	*/

    for (port = topcell->portlist; port; port = port->next) {
	int locdir;

	// Translate port direction from definitions in readverilog.h to
	// the list of direction definitions in "enum nodetype_" above.

	switch (port->direction) {
	    case PORT_INPUT:
		locdir = INPUTPIN;
		break;
	    case PORT_OUTPUT:
		locdir = OUTPUTPIN;
		break;
	    case PORT_INOUT:
		locdir = INOUTPIN;
		break;
	    default:
		locdir = UNKNOWN;
		break;
	}

	/* Find net corresponding to the port name and bit blast if needed */

	net = BusHashLookup(port->name, &topcell->nets);
	if (net->start > net->end) {
	    for (i = net->end; i <= net->start; i++) {
		sprintf(netname, "%s[%d]", port->name, i);
		registernode(netname, locdir, NULL, NULL);
	    }
	}
	else if (net->start < net->end) {
	    for (i = net->start; i <= net->end; i++) {
		sprintf(netname, "%s[%d]", port->name, i);
		registernode(netname, locdir, NULL, NULL);
	    }
	}
	else {
	    registernode(port->name, locdir, NULL, NULL);
	}
    }

    for (inst = topcell->instlist; inst; inst = inst->next) {
	/* Match the gate record pulled from verilog to the gate list	*/
	/* pulled from the liberty file.				*/

	if (!inst->cellname) {
	    fprintf(stderr, "Error:  Instance %s does not name a corresponding cell!\n",
			inst->instname);
	    continue;
	}
	gl = (struct Gatelist *)HashLookup(inst->cellname, &Gatehash);
	if (gl != NULL) {
	    for (port = inst->portlist; port; port = port->next) {
		cur_pintype = get_pintype(gl->gatecell, port->name);
		switch(cur_pintype) {
		    case PIN_OUTPUT:
			/* Gate output */
			nodetype = OUTPUT;
			porttype = PORT_OUTPUT;
			break;
		    case PIN_INPUT:
			/* Gate input */
			nodetype = INPUT;
			porttype = PORT_INPUT;
			break;
		    case PIN_CLOCK:
			/* Gate clock */
			nodetype = CLOCK;
			porttype = PORT_INPUT;
			break;
		    default:
			/* Unknown */
			nodetype = UNKNOWN;
			porttype = PORT_NONE;
			break;
		}
		registernode(port->net, nodetype, gl, port->name);
		port->direction = porttype;
	    }
	}
    }

    /* get list of nets to ignore, if there is one, and mark nets to ignore */
    if (Ignorepath != NULL) read_ignore_file(Ignorepath);

    if (NodePrintFlag) {
	shownodes();
	return 0;
    }

    /* Apply iterative buffering and resizing */

    iter = 0;
    Changed_count = 1;
    while (Changed_count > 0) {
	iter++;

	/* Show top fanout gate */
	nlmax = NULL;
	nlimax = NULL;
	nl = (struct Nodelist *)HashFirst(&Nodehash);
	while (nl != NULL) {
	    if (nl->outputgatestrength != 0.0) {
		nl->ratio = nl->total_load / nl->outputgatestrength;
	    }
	    if (nl->ignore == FALSE) {
        	if ((nl->num_inputs >= Topfanout) && (nl->outputgatestrength != 0.0)) {
		    Topfanout = nl->num_inputs;
		    nlmax = nl;
		}
		else if ((nl->num_inputs >= Inputfanout) && (nl->type == INPUTPIN)) {
		    Inputfanout = nl->num_inputs;
		    nlimax = nl;
		}
		if ((nl->ratio >= Topratio) && (nl->outputgatestrength != 0.0)) {
		    Topratio = nl->ratio;
		}
		if ((nl->total_load >= Topload) && (nl->outputgatestrength != 0.0)) {
		    Topload = nl->total_load;
		}
		else if ((nl->total_load >= Inputload) && (nl->type == INPUTPIN)) {
		    Inputload = nl->total_load;
		}
	    }
	    nl = (struct Nodelist *)HashNext(&Nodehash);
	}

	if (VerboseFlag) printf("\nIteration %d\n", iter);
	fflush(stdout);

	if (nlmax) {
	    fprintf(stderr, "Top internal fanout is %d (load %g) from node %s,\n"
			"driven by %s with strength %g (fF driven at latency %g)\n",
	 		Topfanout, Topload, nlmax->nodename,
			nlmax->outputgate->gatename,
			nlmax->outputgatestrength,
			MaxLatency);

	    fprintf(stderr, "Top fanout load-to-strength ratio is %g (latency = %g ps)\n",
			Topratio, MaxLatency * Topratio);

	    fprintf(stderr, "Top input node fanout is %d (load %g) from node %s.\n",
	 		Inputfanout, Inputload, nlimax->nodename);
	}

	fprintf(stderr, "%d gates exceed specified minimum load.\n", stren_err_counter);

	if (doFanout) insert_buffers(topcell);
	fprintf(stderr, "%d buffers were added.\n", Buffer_count);

	resize_gates(topcell, doLoadBalance, doFanout);
	fprintf(stderr, "%d gates were changed.\n", Changed_count);
 
	fprintf(stderr, "\nGate counts by drive strength:\n\n");
	dl = (struct Drivelist *)HashFirst(&Drivehash);
	while (dl != NULL) {
	    if (dl->NgatesIn > 0) {
		fprintf(stderr, "\t\"%s%s\" gates\tIn: %d    \tOut: %d    \t%+d\n",
			dl->Separator, dl->DriveType, dl->NgatesIn,
			dl->NgatesOut, (dl->NgatesOut - dl->NgatesIn));
	    }
	    dl = (struct Drivelist *)HashNext(&Drivehash);
	}
	fprintf(stderr, "\n");
    }

    write_output(topcell, outfptr, doLoadBalance, doFanout);
    if (outfptr != stdout) fclose(outfptr);

    // Output number of gates changed so we can iterate until this is zero.

    fprintf(stdout, "Number of gates changed: %d\n", Changed_count + Buffer_count);
    return 0;
}

/* end of vlogFanout.c */
