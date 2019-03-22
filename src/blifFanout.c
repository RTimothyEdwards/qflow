/*
 *---------------------------------------------------------------------------
 * blifFanout blif_input [blif_output]
 *
 * blifFanout[.c] parses a .blif netlist (typically, but not necessarily,
 * from synthesized verilog).  The fanout is analyzed, and fanout of each gate
 * is counted.  A value to parameterize the driving cell will be output.
 * Fanouts exceeding a maximum are broken into (possibly hierarchical)
 * buffer trees.  Eventually, a critical path can be identified, and the
 * gates sized to improve it.
 *
 * Original:  fanout.c by Steve Beccue
 * New: blifFanout.c by Tim Edwards.
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
   Cell	  *gatecell;		
   char   *suffix;		// points to position in gatename, not allocated
   char	  *separator;
   int	  num_inputs;
   double Cint;
   double delay;
   double strength;
} Gatelist_;

struct hashlist *Gatehash[OBJHASHSIZE];

struct Nodelist {
   char	  ignore;
   char   *nodename;
   struct Gatelist *outputgate;
   double outputgatestrength;
   int    type;
   int    clock;
   int    num_inputs;
   double total_load;
   double ratio;                // drive strength to total_load ratio
   // For net buffer trees
   int	  num_buf;		// Number of buffers to split the net
   int	  curcount;		// Active count for fanout buffering trees
} Nodelist_;

struct hashlist *Nodehash[OBJHASHSIZE];

struct Drivelist {
   char *Separator;	// Separator (e.g., "X")
   char *DriveType;	// Suffix name (e.g., "1")
   int NgatesIn;	// Number of gates with this suffix in input
   int NgatesOut;	// Number of gates with this suffix in output
} Drivelist_;

struct hashlist *Drivehash[OBJHASHSIZE];

struct Baselist {
   char *BaseName;	// gate base name (e.g., "INV")
   int Ndrives;		// number of different drive types for this gate
   struct Gatelist **gates;	// list of pointers to gates with
} Baselist_;

struct hashlist *Basehash[OBJHASHSIZE];

enum states_ {NONE, INPUTS, OUTPUTS, GATENAME, PINNAME, INPUTNODE, CLOCKNODE,
	OUTPUTNODE, ENDMODEL, ERROR};
enum nodetype_ {UNKNOWN, INPUT, CLOCK, OUTPUT, INPUTPIN, OUTPUTPIN};

int read_gate_file(char *gate_file_name, char *separator);
void read_ignore_file(char *ignore_file_name);
struct Gatelist *GatelistAlloc();
struct Nodelist *NodelistAlloc();
struct Drivelist *DrivelistAlloc();
struct Baselist *BaselistAlloc();
void showgatelist(void);
void helpmessage(void);
void registernode(char *nodename, int type, struct Gatelist *gl, char *pinname);
void shownodes(void);
void write_output(int doLoadBalance, FILE *infptr, FILE *outfptr);
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

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

int main (int argc, char *argv[])
{
   int i, j, k, l;
   int state;
   int maxline, curline;
   int libcount;
   int inputcount;
   int gateinputs;
   int gatecount;
   int doLoadBalance = 1;
   int doFanout = 1;
   char *pinname;
   char *libfile, *libsep;
   char *separg = NULL;
   char *test;
   char *s, *t, *comptr;
   FILE *infptr, *outfptr;
   char *line;
   GateRec *Gatepaths = NULL;
   struct Gatelist *gl = NULL;
   struct Nodelist *nl = NULL, *nlmax, *nlimax;
   struct Drivelist *dl = NULL;

   SuffixIsNumeric = TRUE;	// By default, assume numeric suffixes

   /* Note:  To-Do, have an option that sets the case-insensitive */
   /* hash & match functions.					  */

   hashfunc = hash;
   matchfunc = match;

   InitializeHashTable(Nodehash);
   InitializeHashTable(Drivehash);
   InitializeHashTable(Gatehash);
   InitializeHashTable(Basehash);

   fprintf(stdout, "blifFanout for qflow " QFLOW_VERSION "." QFLOW_REVISION "\n");

   while ((i = getopt(argc, argv, "fLSgnhvl:c:b:i:o:p:s:I:F:")) != EOF) {
      switch (i) {
	 case 'b':
	    /* If value is a comma-separated pair, the first is a	*/
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
	    /* list, either by specifying all on one "-p" arguments	*/
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
   infptr = stdin;
   outfptr = stdout;
   i = optind;

   if (i < argc) {
      Inputfname = strdup(argv[i]);
      if (!(infptr = fopen(Inputfname, "r"))) {
	  fprintf(stderr, "blifFanout: Couldn't open %s for reading.\n", Inputfname);
	  return 1;
      }
   }
   i++;
   if (i < argc) {
      Outputfname = strdup(argv[i]);
      if (!(outfptr = fopen(Outputfname, "w"))) {
	 fprintf(stderr, "blifFanout: Couldn't open %s for writing.\n", Outputfname);
	 return 1;
      }
   }
   i++;

   // Make sure we have a valid gate file path
   if (Gatepaths == NULL) {
      fprintf(stderr, "blifFanout: No liberty file(s) specified.\n");
      return 1;
   }
   gatecount = 0;
   for (l = 0; l < libcount; l++) {
      int loccount;
      libfile = Gatepaths[l].path;
      libsep = Gatepaths[l].sep;
      loccount = read_gate_file(libfile, libsep);
      if (loccount == 0)
         fprintf(stderr, "blifFanout:  Warning:  No gates found in file %s!\n",
		libfile);
      gatecount += loccount;
   }

   // Determine if suffix is numeric or alphabetic
   if (gatecount > 0) {
      char *suffix;
      gl = (struct Gatelist *)HashFirst(Gatehash);
      while (gl && gl->suffix == NULL)
	  gl = (struct Gatelist *)HashNext(Gatehash);
      if (gl && gl->suffix && !isdigit(*gl->suffix))
	    SuffixIsNumeric = FALSE;
   }

   if (gatecount == 0) {
      fprintf(stderr, "blifFanout:  No gates found in any input file!\n");
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
	 gl = (struct Gatelist *)HashLookup(Buffername, Gatehash);
	 if (gl == NULL) {
	    fprintf(stderr, "No buffer \"%s\" found in gate list\n", Buffername);
	    fprintf(stderr, "Searching gate list for suitable buffer.\n");
	 }
      }

      if ((gl == NULL) || (Buffername == NULL)) {
	 // Find a suitable buffer
	 gl = (struct Gatelist *)HashFirst(Gatehash);
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
	    gl = (struct Gatelist *)HashNext(Gatehash);
	 }
      }
      else
	 gl = (struct Gatelist *)HashLookup(Buffername, Gatehash);

      if (gl == NULL) {
	 if (Buffername == NULL)
            fprintf(stderr, "blifFanout:  No suitable buffer cell in library.\n");
	 else
            fprintf(stderr, "blifFanout:  Buffer cell %s cannot be found.\n",
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
	 fprintf(stderr, "blifFanout:  Could not parse I/O pins "
			"of buffer cell %s.\n", Buffername);
	 return 1;
      }
   }

   /* If Clkbufname is not defined, make it the same as Buffername */
   if (Clkbufname == NULL) Clkbufname = Buffername;

   pinname = (char *)malloc(1);
   state = NONE;
   maxline = MAXLINE;
   line = (char *)malloc(MAXLINE * sizeof(char));
   while ((s = fgets(line, maxline, infptr)) != NULL) {
      curline = strlen(line);
      while (curline >= (maxline - 1)) {
	 maxline += MAXLINE;
	 line = (char *)realloc(line, maxline * sizeof(char));
	 s = fgets(line + curline, (maxline - curline), infptr);
	 curline = strlen(line);
	 if (s == NULL) break;
      }
      if (s == NULL) break;
      t = strtok(line, " \t=\n");
      while (t) {
	 switch (state) {
	    case GATENAME:
	       gl = (struct Gatelist *)HashLookup(t, Gatehash);
	       if (gl != NULL) {
		  if (VerboseFlag) printf("\n\n%s", t);
		  gateinputs = gl->num_inputs;
		  state = PINNAME;
	       }
	       break;

	    case INPUTS:
	       if (!strcmp(t, ".gate"))
		  state = GATENAME;
	       else if (!strcmp(t, ".outputs"))
		  state = OUTPUTS;
	       else if (t) {
	          if (VerboseFlag) printf("\nInput pin %s", t);
	          registernode(t, INPUTPIN, gl, pinname);
	       }
	       break;

	    case OUTPUTS:
	       if (!strcmp(t, ".gate"))
		  state = GATENAME;
	       else if (t) {
	          if (VerboseFlag) printf("\nOutput pin %s", t);
	          registernode(t, OUTPUTPIN, gl, pinname);
	       }
	       break;

	    case PINNAME:
	       if (!strcmp(t, ".gate")) state = GATENAME;  // new gate
	       else if (!strcmp(t, ".end")) state = ENDMODEL;  // last gate
	       else {
		   int cur_pintype;
		   free(pinname);
		   pinname = strdup(t);
	
		   if (gl == NULL)
		      state = ERROR;
		   else if ((cur_pintype = get_pintype(gl->gatecell, t)) == PIN_OUTPUT)
		      state = OUTPUTNODE;
		   else if (cur_pintype == PIN_INPUT)
		      state = INPUTNODE;
		   else if (cur_pintype == PIN_CLOCK)
		      state = CLOCKNODE;
		   else
		      state = ERROR;	// Probably want error handling here. . .
	       }
	       break;

	    case INPUTNODE:
	       if (VerboseFlag) printf("\nInput node %s", t);
	       registernode(t, INPUT, gl, pinname);
	       state = PINNAME;
	       break;

	    case CLOCKNODE:
	       if (VerboseFlag) printf("\nClock node %s", t);
	       registernode(t, CLOCK, gl, pinname);
	       state = PINNAME;
	       break;

	    case OUTPUTNODE:
	       if (VerboseFlag) printf("\nOutput node %s", t);
	       registernode(t, OUTPUT, gl, pinname);
	       state = PINNAME;
	       break;

	    case ERROR:
	    default:
	       if (t[0] == '#') skip_eol = 1;
	       else if (!strcmp(t, ".gate")) state = GATENAME;
	       else if (!strcmp(t, ".inputs")) state = INPUTS;
	       else if (!strcmp(t, ".outputs")) state = OUTPUTS;
	       break;
	 }
	 if (skip_eol == 1) {
	    skip_eol = 0;
	    break;
	 }
	 t = strtok(NULL, " \t=\n");
	 // Ignore continuation lines
	 if (t && (!strcmp(t, "\\"))) t = strtok(NULL, " \t=\n");
      }
   }
   free(pinname);

   /* get list of nets to ignore, if there is one, and mark nets to ignore */
   if (Ignorepath != NULL) read_ignore_file(Ignorepath);

   if (NodePrintFlag) {
      shownodes();
      return 0;
   }

   /* Show top fanout gate */
   nlmax = NULL;
   nlimax = NULL;
   nl = (struct Nodelist *)HashFirst(Nodehash);
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
      nl = (struct Nodelist *)HashNext(Nodehash);
   }

   if (VerboseFlag) printf("\n");
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

   Buffer_count = 0;
   if (doFanout && ((Topfanout > MaxFanout) || (Inputfanout > MaxFanout))) {

      /* Insert buffer trees */
      nl = (struct Nodelist *)HashFirst(Nodehash);
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
         nl = (struct Nodelist *)HashNext(Nodehash);
      }
   }
   write_output(doLoadBalance, infptr, outfptr);

   fprintf(stderr, "%d gates exceed specified minimum load.\n", stren_err_counter);
   fprintf(stderr, "%d buffers were added.\n", Buffer_count);
   fprintf(stderr, "%d gates were changed.\n", Changed_count);

   fprintf(stderr, "\nGate counts by drive strength:\n\n");
   dl = (struct Drivelist *)HashFirst(Drivehash);
   while (dl != NULL) {
      if (dl->NgatesIn > 0) {
	 fprintf(stderr, "\t\"%s%s\" gates\tIn: %d    \tOut: %d    \t%+d\n",
		dl->Separator, dl->DriveType, dl->NgatesIn,
		dl->NgatesOut, (dl->NgatesOut - dl->NgatesIn));
      }
      dl = (struct Drivelist *)HashNext(Drivehash);
   }
   fprintf(stderr, "\n");

   if (infptr != stdin) fclose(infptr);
   if (outfptr != stdout) fclose(outfptr);

   // Output number of gates changed so we can iterate until this is zero.

   fprintf(stdout, "Number of gates changed: %d\n", Changed_count + Buffer_count);
   return 0;
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
      fprintf(stderr, "blifFanout:  Couldn't open %s as ignore file.\n",
		ignore_file_name);
      fflush(stderr);
      return;
      // This is only a warning.  It will not stop execution of blifFanout
   }

   while ((s = fgets(line, MAXLINE, ignorefptr)) != NULL) {
      // One net name per line
      while (isspace(*s)) s++;
      sp = s;
      while (*sp != '\0' && *sp != '\n' && !isspace(*sp)) sp++;
      *sp = '\0';

      nl = (struct Nodelist *)HashLookup(s, Nodehash);
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
 	HashPtrInstall(gl->gatename, gl, Gatehash);
	gatecount++;

	/* Install prefix in Basehash.  Note that prefix contains the	*/
	/* separator string, if any.					*/

	if ((s = gl->suffix) == NULL)
	    ind = strlen(gl->gatename);
	else
	    ind = (int)(s - gl->gatename);

	ssave = gl->gatename[ind];
	gl->gatename[ind] = '\0';
	bl = (struct Baselist *)HashLookup(gl->gatename, Basehash);
	if (bl == NULL) {
	    bl = BaselistAlloc();
	    HashPtrInstall(gl->gatename, bl, Basehash);
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

   gl = (struct Gatelist *)HashFirst(Gatehash);
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
      gl = (struct Gatelist *)HashNext(Gatehash);
   }
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void registernode(char *nodename, int type, struct Gatelist *gl, char *pinname)
{
   struct Nodelist *nl;
   double pincap;

   nl = (struct Nodelist *)HashLookup(nodename, Nodehash);
   
   if (nl == NULL) {
      nl = NodelistAlloc();
      nl->nodename = strdup(nodename);
      if (type == OUTPUT) nl->outputgate = NULL;
      HashPtrInstall(nodename, nl, Nodehash);
      nl->type = type;
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

   dl = (struct Drivelist *)HashLookup(s, Drivehash);
   if (dl == NULL) {

      // New drive type found

      dl = DrivelistAlloc();
      HashPtrInstall(s, dl, Drivehash);
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

   nl = (struct Nodelist *)HashFirst(Nodehash);
   while (nl != NULL) {
      printf("\n\nnode: %s with %d fanout and %g fF cap",
		nl->nodename, nl->num_inputs, nl->total_load);
      printf("\ndriven by %s, with %g strength.\n",
		nl->outputgate->gatename, nl->outputgatestrength);
      nl = (struct Nodelist *)HashNext(Nodehash);
   }
}

/*
 *---------------------------------------------------------------------------
 *---------------------------------------------------------------------------
 */

void write_output(int doLoadBalance, FILE *infptr, FILE *outfptr)
{
   char *s, *t;
   char *line;
   char *inputline;
   char bufferline[MAXLINE];
   char nodename[MAXLINE];
   char *gateline;
   char *stren, *orig;
   int  firstseen;
   int  state, i;
   int  maxline, curline;
   int  gateinputs;
   int  pincount;
   int  needscorrecting;
   int  hasended;
   int slen;
   int hier;
   char *spos;
   struct Nodelist *nltest;
   struct Gatelist *gl;
   struct Gatelist *glbest, *bbest;
   struct Gatelist *glbuf;
   struct Nodelist *nl;
   struct Drivelist *dl;
   double inv_size;

   Changed_count = 0;
   state = NONE;
   needscorrecting = 0;
   firstseen = 0;
   hasended = 0;

   // Find the gate record corresponding to the buffer name
   glbuf = (struct Gatelist *)HashLookup(Buffername, Gatehash);

   rewind(infptr);

   gateline = (char *)malloc(1);
   gateline[0] = '\0';

   maxline = MAXLINE;
   line = (char *)malloc(MAXLINE * sizeof(char));
   inputline = (char *)malloc(MAXLINE * sizeof(char));

   while ((s = fgets(line, maxline, infptr)) != NULL) {
      curline = strlen(line);
      while (curline >= (maxline - 1)) {
	 maxline += MAXLINE;
	 line = (char *)realloc(line, maxline * sizeof(char));
	 inputline = (char *)realloc(inputline, maxline * sizeof(char));
	 s = fgets(line + curline, (maxline - curline), infptr);
	 curline = strlen(line);
	 if (s == NULL) break;
      }
      if (s == NULL) break;
      strcpy(inputline, line);		// save this for later
      t = strtok(line, " \t=\n");
      while (t) { 
	 switch (state) {
	    case GATENAME:
	       if (firstseen == 0) {

		  /* Insert any added buffers (before 1st gate) */

		  nl = (struct Nodelist *)HashFirst(Nodehash);
		  while (nl != NULL) {
		     for (i = nl->num_buf - 1; i >= 0; i--) {
			hier = 0;
			nltest = nl;
			sprintf(nodename, "%s", nl->nodename);
			while (nltest != NULL) {
			   slen = strlen(nodename);
			   spos = nodename + slen - 1;
			   if (*spos == ']') {
			      /* Avoid downstream problems:		*/
			      /* recast "[X]_bF$bufN" as [X_bF$bufN]"	*/
			      sprintf(spos, "_bF$buf%d]", i);
			   }
			   else if (*spos == ']') {
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

			   nltest = (struct Nodelist *)HashLookup(nodename, Nodehash);
			   if (nltest != NULL) {
			      sprintf(nodename, "%s_hier%d", nl->nodename, hier);
			      hier++;
			   }
			}
			if (nl->clock == TRUE)
			   fprintf(outfptr, ".gate %s %s=%s %s=%s\n",
					Clkbufname, clkbuf_in_pin,
					nl->nodename, clkbuf_out_pin,
					nodename);
			else
			   fprintf(outfptr, ".gate %s %s=%s %s=%s\n",
					Buffername, buf_in_pin,
					nl->nodename, buf_out_pin,
					nodename);
		     }
		     nl = (struct Nodelist *)HashNext(Nodehash);
		  }
		  firstseen = 1;
	       }
	       gl = (struct Gatelist *)HashLookup(t, Gatehash);
	       if (gl == NULL) {
		  fprintf(stderr, "Error:  Gate \"%s\" is used in source "
			"but has no liberty file definition.\n", t);
		  gateinputs = 0;
	       }
	       else
		  gateinputs = gl->num_inputs;
	       needscorrecting = 0;
	       pincount = 0;
	       state = PINNAME;
	       break;
	  
	    case PINNAME:
	       if (!strcmp(t, ".gate")) state = GATENAME;  // new gate
	       else if (!strcmp(t, ".end")) state = ENDMODEL;  // last gate
	       else if (gl == NULL)
		   state = ERROR;
	       else {
		  int cur_pintype;
	          if ((cur_pintype = get_pintype(gl->gatecell, t)) == PIN_OUTPUT) {
		     state = OUTPUTNODE;
		     pincount++;
		  }
		  else if (cur_pintype == PIN_INPUT) {
		     state = INPUTNODE;
		     pincount++;
		  }
		  else if (cur_pintype == PIN_CLOCK) {
		     state = CLOCKNODE;
		     pincount++;
		  }
		  else
		     state = ERROR;	// Probably want error handling here. . .
	       }
	       break;

	    case INPUTNODE: case CLOCKNODE:
	       if (VerboseFlag) printf("\nInput node %s", t);
	       nl = (struct Nodelist *)HashLookup(t, Nodehash);
	       if (nl->num_buf > 0) {
		  hier = 0;
		  nltest = nl;
		  sprintf(nodename, "%s", nl->nodename);
		  while (nltest != NULL) {
		     slen = strlen(nodename);
		     spos = nodename + slen - 1;
		     if (*spos == ']') {
		        /* Avoid downstream problems:		*/
		        /* recast "[X]_bF$bufN" as [X_bF$bufN]"	*/
		        sprintf(spos, "_bF$buf%d]", nl->curcount);
		     }
		     else if (*spos == ']') {
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

		     /* For buffer trees of depth > 1, there will be	*/
		     /* an existing node name wih the _bufN extension	*/
		     /* that is in the node hash table.  If so, then	*/
		     /* add the prefix "_hierM".  Test again in case	*/
		     /* the buffer tree is even deeper, incrementing M	*/
		     /* until the name is unique.			*/

	             nltest = (struct Nodelist *)HashLookup(nodename, Nodehash);
		     if (nltest != NULL) {
			sprintf(nodename, "%s_hier%d", nl->nodename, hier);
		 	hier++;
		     }
		  }

		  nl->curcount--;
		  if (nl->curcount < 0) nl->curcount = nl->num_buf - 1;

		  // Splice the suffix into the original line
		  slen = strlen(nodename);
		  s = strchr(inputline, '=');
		  for (i = 1; i < pincount; i++) s = strchr(s + 1, '=');
		  s++;
		  while (*s == ' ' || *s == '\t') s++;
		  memmove(s + slen - strlen(nl->nodename), s, strlen(s) + 1);
		  strncpy(s, nodename, slen); 
	       }
	       state = PINNAME;
	       break;

	    case OUTPUTNODE:
	       if (VerboseFlag) printf("\nOutput node %s", t);

	       nl = (struct Nodelist *)HashLookup(t, Nodehash);
	       if (doLoadBalance && (nl != NULL)) {
		  if ((nl->ignore == FALSE) && (nl->ratio > 1.0)) {
		     if (VerboseFlag)
			printf("\nGate should be %g times stronger", nl->ratio);
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
		  // Don't attempt to correct gates for which we cannot find a suffix
		  if (orig == NULL) needscorrecting = FALSE;
	       }
	       state = PINNAME;
	       break;

	    case ERROR:
	    default:
	       if (t[0] == '#') skip_eol = 1;
	       if (!strcmp(t, ".gate"))
		  state = GATENAME;
	       else if (!strcmp(t, ".end"))
		  state = ENDMODEL;		
	       break;
	 }
	 if (skip_eol == 1) {
	    skip_eol = 0;
	    break;
	 }
         t = strtok(NULL, " \t=\n");
	 // Ignore continuation lines
	 if (t && (!strcmp(t, "\\"))) t = strtok(NULL, " \t=\n");

         if (state == GATENAME || state == ENDMODEL) {

	    /* dump output for the last gate */

            bufferline[0] = 0;
            if (needscorrecting) {
	       if (glbest == NULL) {      // return val to insert inverters

		  if (VerboseFlag)
	             printf("\nInsert buffers %s - %g\n", s, inv_size);
	          s = strstr(gateline, nl->nodename);	// get output node
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

		  /* If bbest->suffix is NULL, then we will have to break up	*/
		  /* this network.						*/
		  /* Buffer trees will be inserted by downstream tools, after	*/
		  /* analyzing the placement of the network.  This error needs	*/
		  /* to be passed down to those tools. . .			*/

		  if (bbest == NULL) {
		     fprintf(stderr, "Fatal error:  No gates found for %s\n",
				glbuf->gatename);
		     fprintf(stderr, "May need to add information to gate.cfg file\n");
		  }

		  dl = (struct Drivelist *)HashLookup(bbest->suffix, Drivehash);
		  if (dl != NULL) dl->NgatesOut++;

		  /* Recompute size of the gate driving the buffer */
		  sprintf(bufferline, "%s", bbest->gatename);
		  if (nl != NULL) {
		     get_pincap(bbest->gatecell, buf_in_pin, &nl->total_load);
		     if (gl != NULL) {
		        nl->total_load += gl->Cint;
		     }
		  }
		  orig = gl->suffix;
		  glbest = best_size(gl, nl->total_load + WireCap, NULL);

		  sprintf(bufferline, 
			".gate %s %s=%s %s=%s\n",
			bbest->gatename, buf_in_pin, s, buf_out_pin,
			nl->nodename);
	       }
	       if ((gl != NULL) && (gl != glbest)) {
		  s = strstr(gateline, gl->gatename);
	          if (s) {
		     int glen = strlen(gl->gatename);
		     int slen = strlen(glbest->gatename);

		     if (glen != slen) {
			// The size of the gatename increased, so reallocate.
			// Allow additional space for inserted buffer suffix.
			if (slen > glen) {
			    gateline = (char *)realloc(gateline, strlen(gateline)
					+ 10 + slen - glen);
			    s = strstr(gateline, gl->gatename);
			}
			memmove(s + slen, s + glen, strlen(s + glen) + 1);
		     }

		     strncpy(s, glbest->gatename, slen);
	             Changed_count++;

		     /* Adjust the gate count for "in" and "out" types */
		     count_gatetype(gl, 0, -1);
		     count_gatetype(glbest, 0, 1);
		  }
	       }
            }
            else {
	       stren = NULL;
            }

            if (strlen(gateline) > 0) gateline[strlen(gateline) - 1] = 0;
            fprintf(outfptr, "%s\n", gateline);
            fprintf(outfptr, "%s", bufferline); 

            bufferline[0] = '\0';
	    gateline[0] = '\0';		/* Starting a new gate */

	    if (state == ENDMODEL)
	       if (hasended == 0) {
		  fprintf(outfptr, "%s", inputline); 
		  hasended = 1;
	       }
         }
	 else if (state == NONE) {
	    /* Print line and reset so we don't overflow gateline */
	    fprintf(outfptr, "%s", gateline);
	    gateline[0] = '\0';		/* Start a new line */
	 }
      }
      /* Append input line to gate.  Always allow additional space for adding	*/
      /* suffixes to inserted buffers (see above).				*/
      gateline = (char *)realloc(gateline, strlen(gateline) + strlen(inputline) + 10);
      strcat(gateline, inputline);	/* Append input line to gate */
   }
   if (hasended == 0) fprintf(outfptr, ".end\n");
   if (VerboseFlag) printf("\n");
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

   bl = (struct Baselist *)HashLookup(gl->gatename, Basehash);
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
   printf("\nblifFanout:\n\n");
   printf("blifFanout looks at a synthesized BLIF netlist.\n");
   printf("Node fanout is measured, and gate size is adjusted.\n");
   printf("File \"gate.cfg\" is used to describe the RTL gates.\n\n");

   printf("\tUsage: blifFanout [-switches] blif_in [blif_out].\n\n");

   printf("blifFanout returns the number of gate substitutions made.\n");
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

/* end of blifFanout.c */
