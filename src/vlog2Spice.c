//--------------------------------------------------------------
// vlog2Spice
//
// Convert a structural verilog netlist (with power and ground
// nets!) into a SPICE netlist.
//
// Revision 0, 2006-11-11: First release by R. Timothy Edwards.
// Revision 1, 2009-07-13: Minor cleanups by Philipp Klaus Krause.
// Revision 2, 2013-05-10: Modified to take a library of subcell
//		definitions to use for determining port order.
// Revision 3, 2013-10-09: Changed from BDnet2BSpice to
//		blif2BSpice
// Revision 4, 2018-11-28: Changed from blif2BSpice to vlog2Spice
//
//--------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

#include "hash.h"
#include "readverilog.h"

#define LengthOfLine    	16384

#define DO_INCLUDE   0x01
#define DO_DELIMITER 0x02

/* Linked list of names of SPICE libraries to read */
typedef struct _linkedstring *LinkedStringPtr;

typedef struct _linkedstring {
    char *string;
    LinkedStringPtr next;
} LinkedString;

/* Function prototypes */
int write_output(struct cellrec *, LinkedStringPtr, char *, int);
int loc_getline(char s[], int lim, FILE *fp);
void helpmessage(FILE *);

//--------------------------------------------------------

int main (int argc, char *argv[])
{
    int i, result = 0;
    int flags = 0;
    char *eptr;

    char *vloginname = NULL;
    char *spclibname = NULL;
    char *spcoutname = NULL;

    LinkedStringPtr spicelibs = NULL, newspicelib;


    struct cellrec *topcell = NULL;

    while ((i = getopt(argc, argv, "hHidD:l:s:o:")) != EOF) {
	switch (i) {
	    case 'l':
		newspicelib = (LinkedStringPtr)malloc(sizeof(LinkedString));
		newspicelib->string = strdup(optarg);
		newspicelib->next = spicelibs;
		spicelibs = newspicelib;
		break;
	    case 'o':
		spcoutname = strdup(optarg);
		break;
	    case 'i':
		flags |= DO_INCLUDE;
		break;
	    case 'd':
		flags |= DO_DELIMITER;
		break;
	    case 'h':
	    case 'H':
		helpmessage(stdout);
		exit(0);
		break;
	    case 'D':
		eptr = strchr(optarg, '=');
		if (eptr != NULL) {
		    *eptr = '\0';
		    VerilogDefine(optarg, eptr + 1);
		    *eptr = '=';
		}
		else
		    VerilogDefine(optarg, "1");
		break;
	    default:
		fprintf(stderr, "Unknown switch %c\n", (char)i);
		helpmessage(stderr);
		exit(1);
		break;
	}
    }

    if (optind < argc) {
	vloginname = strdup(argv[optind]);
	optind++;
    }
    else {
	fprintf(stderr, "Couldn't find a filename as input\n");
	helpmessage(stderr);
	exit(1);
    }
    optind++;

    topcell = ReadVerilog(vloginname);
    if (topcell != NULL)
	result = write_output(topcell, spicelibs, spcoutname, flags);
    else
	result = 1;	/* Return error code */

    return result;
}

/*--------------------------------------------------------------*/
/* Verilog backslash notation, which is the most absurd syntax	*/
/* in the known universe, is fundamentally incompatible with	*/
/* SPICE.  The ad-hoc solution used by qflow is to replace	*/
/* the trailing space with another backslash such that the	*/
/* name is SPICE-compatible and the original syntax can be	*/
/* recovered when needed.					*/
/*--------------------------------------------------------------*/

void backslash_fix(char *netname)
{
    char *sptr;

    if (*netname == '\\')
	if ((sptr = strchr(netname, ' ')) != NULL)
	    *sptr = '\\';
}

/*--------------------------------------------------------------*/
/* write_output ---  Write the SPICE netlist output		*/
/*								*/
/* ARGS: 							*/
/* RETURNS: 0 on success, 1 on error.				*/
/* SIDE EFFECTS: 						*/
/*--------------------------------------------------------------*/

int write_output(struct cellrec *topcell, LinkedStringPtr spicelibs,
		char *outname, int flags)
{
    FILE *libfile;
    FILE *outfile;
    char *libname;
    LinkedStringPtr curspicelib;

    struct netrec *net;
    struct instance *inst;
    struct portrec *port;
    struct portrec *newport, *portlist, *lastport;

    int i, j, k, start, end, pcount = 1;
    int result = 0;
    int instidx, insti;

    char *lptr;
    char *sp, *sp2;
    char line[LengthOfLine];

    struct hashtable Libhash;

    if (outname != NULL) {
	outfile = fopen(outname, "w");
	if (outfile == NULL) {
	    fprintf(stderr, "Error:  Couldn't open file %s for writing\n", outname);
	    return 1;
	}
    }
    else
	outfile = stdout;

    /* Initialize SPICE library hash table */
    InitializeHashTable(&Libhash, SMALLHASHSIZE);

    // Read one or more SPICE libraries of subcircuits and use them to define
    // the order of pins that were read from LEF (which is not necessarily in
    // SPICE pin order).

    for (curspicelib = spicelibs; curspicelib; curspicelib = curspicelib->next) {
	libname = curspicelib->string;

	libfile = fopen(libname, "r");
	if (libfile == NULL) {
	    fprintf(stderr, "Couldn't open %s for reading\n", libname);
	    continue;
	}

	/* Read SPICE library of subcircuits, if one is specified.	*/
	/* Retain the name and order of ports passed to each		*/
	/* subcircuit.							*/

	j = 0;
	while (loc_getline(line, sizeof(line), libfile) > 0) {
	    if (!strncasecmp(line, ".subckt", 7)) {
		char *cellname;
		lastport = NULL;
		portlist = NULL;

		/* Read cellname */
		sp = line + 7;
		while (isspace(*sp) && (*sp != '\n')) sp++;
		sp2 = sp;
		while (!isspace(*sp2) && (*sp2 != '\n')) sp2++;
		*sp2 = '\0';

		/* Keep a record of the cellname until we generate the	*/
		/* hash entry for it					*/
		cellname = strdup(sp);

		/* Now fill out the ordered port list */
		sp = sp2 + 1;
		while (isspace(*sp) && (*sp != '\n') && (*sp != '\0')) sp++;
		while (sp) {

		    /* Move string pointer to next port name */

		    if (*sp == '\n' || *sp == '\0') {
			loc_getline(line, sizeof(line), libfile);
			if (*line == '+')
			    sp = line + 1;
			else
			    break;
		    }
		    while (isspace(*sp) && (*sp != '\n')) sp++;

		    /* Terminate port name and advance pointer */
		    sp2 = sp;
		    while (!isspace(*sp2) && (*sp2 != '\n') && (*sp2 != '\0')) sp2++;
		    *sp2 = '\0';

		    /* Add port to list (in forward order) */

		    newport = (struct portrec *)malloc(sizeof(struct portrec));
		    if (portlist == NULL)
			portlist = newport;
		    else
			lastport->next = newport;
		    lastport = newport;
		    newport->name = strdup(sp);
		    newport->net = NULL;
		    newport->direction = 0;
		    newport->next = NULL;

		    sp = sp2 + 1;
		}

		/* Read input to end of subcircuit */

		if (strncasecmp(line, ".ends", 4)) {
		    while (loc_getline(line, sizeof(line), libfile) > 0)
			if (!strncasecmp(line, ".ends", 4))
			    break;
		}

		/* Hash the new port record by cellname */
		HashPtrInstall(cellname, portlist, &Libhash);
		free(cellname);
	    }
	}
	fclose(libfile);
    }

    /* Write output header */
    fprintf(outfile, "*SPICE netlist created from verilog structural netlist module "
			"%s by vlog2Spice (qflow)\n", topcell->name);
    fprintf(outfile, "*This file may contain array delimiters, not for use in simulation.\n");
    fprintf(outfile, "\n");

    /* If flags has DO_INCLUDE then dump the contents of the	*/
    /* libraries.  If 0, then just write a .include line.	*/

    for (curspicelib = spicelibs; curspicelib; curspicelib = curspicelib->next) {
	libname = curspicelib->string;

	if (flags & DO_INCLUDE) {
	    libfile = fopen(libname, "r");
	    if (libfile != NULL) {
		fprintf(outfile, "** Start of included library %s\n", libname);
		/* Write out the subcircuit library file verbatim */
		while (loc_getline(line, sizeof(line), libfile) > 0)
		    fputs(line, outfile);
		fprintf(outfile, "** End of included library %s\n", libname);
		fclose(libfile);
	    }
	}
	else {
	    fprintf(outfile, ".include %s\n", libname);
	}
    }
    fprintf(outfile, "\n");

    /* Generate the subcircuit definition, adding power and ground nets */

    fprintf(outfile, ".subckt %s ", topcell->name);

    for (port = topcell->portlist; port; port = port->next) {
	if ((net = BusHashLookup(port->name, &topcell->nets)) != NULL) {
	    start = net->start;
	    end = net->end;
	}
	else start = end = -1;

	if (start > end) {
	    int tmp;
	    tmp = start;
	    start = end;
	    end = tmp;
	}
	if (start == end) {
	    fprintf(outfile, "%s", port->name);
	    if (pcount++ % 8 == 7) {
		pcount = 0;
		fprintf(outfile, "\n+");
	    }
	    fprintf(outfile, " ");
	}
	else {
	    for (i = start; i <= end; i++) {
		/* Note that use of brackets is not legal SPICE syntax	*/
		/* but suffices for LVS and such.  Output should be	*/
		/* post-processed before using in simulation.		*/
		if (flags & DO_DELIMITER)
		    fprintf(outfile, "%s<%d>", port->name, i);
		else
		    fprintf(outfile, "%s[%d]", port->name, i);

		if (pcount++ % 8 == 7) {
		    pcount = 0;
		    fprintf(outfile, "\n+");
		}
		fprintf(outfile, " ");
	    }
	}
    }
    fprintf(outfile, "\n\n");

    /* Output instances */

    instidx = -1;
    for (inst = topcell->instlist; inst; ) {
	int argcnt;
	struct portrec *libport;

	/* Check if the instance is an array */
	if (inst->arraystart != -1) {
	    if (instidx == -1) {
		instidx = inst->arraystart;
		insti = 0;
	    }
	    else if (inst->arraystart > inst->arrayend) {
		instidx--;
		insti++;
	    }
	    else if (inst->arraystart < inst->arrayend) {
		instidx++;
		insti++;
	    }
	}

	if (inst->arraystart == -1)
	    fprintf(outfile, "X%s ", inst->instname);
	else
	    fprintf(outfile, "X%s[%d] ", inst->instname, instidx);
        pcount = 1;

	/* Search library records for subcircuit */

	portlist = (struct portrec *)HashLookup(inst->cellname, &Libhash);

	/* If no library entry exists, complain about arbitrary port	*/
	/* order, then use the instance's port names to create a port	*/
	/* record entry.						*/

	if (portlist == NULL) {
	    fprintf(stderr, "Warning:  No SPICE subcircuit for %s.  Pin"
			" order will be arbitrary.\n", inst->cellname);
	    lastport = portlist = NULL;
	    for (libport = inst->portlist; libport; libport = libport->next) {
	    	newport = (struct portrec *)malloc(sizeof(struct portrec));
		if (portlist == NULL)
			portlist = newport;
		else
		    lastport->next = newport;
		lastport = newport;
		newport->name = strdup(libport->name);
		newport->net = NULL;
		newport->direction = libport->direction;
		newport->next = NULL;
	    }
	    HashPtrInstall(inst->cellname, portlist, &Libhash);
	}

	/* Output pin connections in the order of the LEF record, which	*/
	/* has been forced to match the port order of the SPICE library	*/
	/* If there is no SPICE library record, output in the order of	*/
	/* the instance, which may or may not be correct.  In such a	*/
	/* case, flag a warning.					*/

	argcnt = 0;
	for (libport = portlist; ; libport = libport->next) {
	    char *dptr, dsave;
	    int idx = 0, is_array = FALSE, match = FALSE;

	    if (portlist != NULL && libport == NULL) break;

	    argcnt++;
	    argcnt %= 8;
	    if (argcnt == 7)
		fprintf(outfile, "\n+ ");

	    dptr = NULL;
	    if (libport) {
		for (dptr = libport->name; *dptr != '\0'; dptr++) {
		    if (*dptr == '[') {
			is_array = TRUE;
			dsave = *dptr;
			*dptr = '\0';
			sscanf(dptr + 1, "%d", &idx);
			break;
		    }
		}
	    }

	    /* Treat arrayed instances like a bit-blasted port */
	    if ((is_array == FALSE) && (inst->arraystart != -1)) {
		is_array = TRUE;
		idx = insti;
	    } 

	    for (port = inst->portlist; port; port = port->next) {
		/* Find the port name in the instance which matches */
		/* the port name in the macro definition.	    */

		if (libport) {
		    if (!strcasecmp(libport->name, port->name)) {
			match = TRUE;
			break;
		    }
		}
		else {
		    match = TRUE;
		    break;
		}
	    }
	    if (!match) {
		char *gptr;

		/* Deal with annoying use of "!" as a global indicator,	*/
		/* which is sometimes used. . .				*/
		gptr = libport->name + strlen(libport->name) - 1;
		if (*gptr == '!') {
		    *gptr = '\0';
		    for (port = inst->portlist; port; port = port->next) {
			if (!strcasecmp(libport->name, port->name)) {
			    match = TRUE;
			    break;
			}
		    }
		    *gptr = '!';
		}
		else {
		    for (port = inst->portlist; port; port = port->next) {
			gptr = port->name + strlen(port->name) - 1;
			if (*gptr == '!') {
			    *gptr = '\0';
			    if (!strcasecmp(libport->name, port->name)) match = TRUE;
			    *gptr = '!';
			    if (match == TRUE) break;
			}
		    }
		}
	    }
	    if (!match) {
		fprintf(stderr, "Error:  Instance %s has no port %s!\n",
			inst->instname, libport->name);
	    }
	    else {
		if (flags & DO_DELIMITER) {
		    char *d1ptr, *d2ptr;
		    if ((d1ptr = strchr(port->net, '[')) != NULL) {
			if ((d2ptr = strchr(d1ptr + 1, ']')) != NULL) {
			    *d1ptr = '<';
			    *d2ptr = '>';
			}
		    }
		}
		if (is_array) {
		    char *portname = port->net;
		    if (*portname == '{') {
			char *epos, ssave;
			int k;

			/* Bus notation "{a, b, c, ... }"	    */
			/* Go to the end and count bits backwards.  */
			/* until reaching the idx'th position.	    */

			/* To be done:  Move GetIndexedNet() from   */
			/* vlog2Verilog.c to readverilog.c and call */
			/* it from here.  It is more complete than  */
			/* this implementation.			    */

			while (*portname != '}' && *portname != '\0') portname++;
			for (k = 0; k < idx; k++) {
			    epos = portname;
			    portname--;
			    while (*portname != ',' && portname > port->net)
				portname--;
			}
			if (*portname == ',') portname++;
			ssave = *epos;
			*epos = '\0';
			backslash_fix(portname);
			fprintf(outfile, "%s", portname);
			*epos = ssave;
		    }
		    else {
			struct netrec wb;

			GetBus(portname, &wb, &topcell->nets);

			if (wb.start < 0) {
			    /* portname is not a bus */
			    backslash_fix(portname);
			    fprintf(outfile, "%s", portname);
			}
			else {
			    int lidx;
			    if (wb.start < wb.end)
				lidx =  wb.start + idx;
			    else
				lidx = wb.start - idx;
			    /* portname is a partial or full bus */
			    dptr = strrchr(portname, '[');
			    if (dptr) *dptr = '\0';
			    backslash_fix(portname);
			    if (flags & DO_DELIMITER)
				fprintf(outfile, "%s<%d>", portname, lidx);
			    else
				fprintf(outfile, "%s[%d]", portname, lidx);
			    if (dptr) *dptr = '[';
			}
		    }
		}
		else {
		    backslash_fix(port->net);
		    fprintf(outfile, "%s", port->net);
		}

		if (pcount++ % 8 == 7) {
		    pcount = 0;
		    fprintf(outfile, "\n+");
		}
		fprintf(outfile, " ");
	    }

	    if (portlist == NULL) {
		fprintf(stdout, "Warning:  No defined subcircuit %s for "
				"instance %s!\n", inst->cellname, inst->instname);
		fprintf(stdout, "Pins will be output in arbitrary order.\n");
		break;
	    }
	    if (dptr != NULL) *dptr = '[';
	}
	fprintf(outfile, "%s\n", inst->cellname);

	if ((inst->arraystart != -1) && (instidx != inst->arrayend)) continue;
	instidx = -1;
	inst = inst->next;
    }
    fprintf(outfile, "\n.ends\n");
    fprintf(outfile, ".end\n");

    if (outname != NULL) fclose(outfile);

    return result;
}

/*--------------------------------------------------------------*/
/*C loc_getline: read a line, return length			*/
/*								*/
/* ARGS: 							*/
/* RETURNS: 1 to OS						*/
/* SIDE EFFECTS: 						*/
/*--------------------------------------------------------------*/

int loc_getline(char *s, int lim, FILE *fp)
{
    int c, i;
	
    i = 0;
    while(--lim > 0 && (c = getc(fp)) != EOF && c != '\n')
	s[i++] = c;
    if (c == '\n')
	s[i++] = c;
    s[i] = '\0';
    if (c == EOF) i = 0; 
    return i;
}

/*--------------------------------------------------------------*/
/*C helpmessage - tell user how to use the program		*/
/*								*/
/*  ARGS: error code (0 = success, 1 = error)			*/
/*  RETURNS: 1 to OS						*/
/*  SIDE EFFECTS: 						*/
/*--------------------------------------------------------------*/

void helpmessage(FILE *fout)
{
    fprintf(fout, "vlog2Spice [-options] netlist \n");
    fprintf(fout, "\n");
    fprintf(fout, "vlog2Spice converts a netlist in verilog format \n");
    fprintf(fout, "to Spice subcircuit format. Output on stdout unless -o option used.\n");
    fprintf(fout, "Input file must be a structural verilog netlist with power and ground.\n");
    fprintf(fout, "\n");
    fprintf(fout, "Options:\n");
    fprintf(fout, "   -h          Print this message\n");    
    fprintf(fout, "   -i          Generate include statement for library, not a dump.\n");
    fprintf(fout, "   -d          Convert array delimiter brackets to angle brackets.\n");
    fprintf(fout, "   -D <key>=<value>  Preregister a verilog definition.\n");
    fprintf(fout, "   -l <path>   Specify path to SPICE library of standard cells.\n");
    fprintf(fout, "   -o <path>   Specify path to output SPICE file.\n");
    fprintf(fout, "\n");

} /* helpmessage() */

