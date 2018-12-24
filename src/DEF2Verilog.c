//----------------------------------------------------------------
// DEF2Verilog
//----------------------------------------------------------------
// Generate a structural verilog netlist from a DEF file.  This is
// used by qflow to get netlists after all modifications have been
// made to the DEF file, including (but not necessarily limited to)
// insertion of clock trees, insertion of fill, decap, and antenna
// cells;  and routing to antenna taps.  The final DEF is presumed
// to represent the expected post-routing netlist, so this netlist
// is compared against the layout extracted netlist to check for
// possible errors introduced by the router.
//
// Revision 0, 2018-12-1: First release by R. Timothy Edwards.
//
// This program is written in ISO C99.
//----------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>	/* For getopt() */
#include <math.h>
#include <ctype.h>
#include <float.h>

#include "hash.h"
#include "readverilog.h"
#include "readdef.h"

void write_output(struct cellrec *top, char *vlogoutname);
void helpmessage(FILE *outf);

char *VddNet = NULL;
char *GndNet = NULL;

/*--------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    int i, result;
    unsigned char Flags;
    float rscale;
    struct cellrec *topcell;

    char *definname = NULL;
    char *vloginname = NULL;
    char *vlogoutname = NULL;

    while ((i = getopt(argc, argv, "hHv:o:l:p:g:")) != EOF) {
	switch( i ) {
	    case 'h':
	    case 'H':
		helpmessage(stdout);
		exit(0);
		break;
	    case 'v':
		vloginname = strdup(optarg);
		break;
	    case 'l':
		LefRead(optarg);	/* Can be called multiple times */
		break;
	    case 'p':
		VddNet = strdup(optarg);
		break;
	    case 'g':
		GndNet = strdup(optarg);
		break;
	    case 'o':
		vlogoutname = strdup(optarg);
		break;
	    default:
		fprintf(stderr,"Bad switch \"%c\"\n", (char)i);
		helpmessage(stderr);
		return 1;
	}
    }

    if (optind < argc) {
	definname = strdup(argv[optind]);
	optind++;
    }
    else {
	fprintf(stderr, "Couldn't find a filename for DEF input file.\n");
	helpmessage(stderr);
	return 1;
    }
    optind++;

    if (vloginname)
	topcell = ReadVerilog(vloginname);
    else {
	fprintf(stderr, "No verilog file specified (not yet handled).\n");
	return 1;
    }
    result = DefRead(definname, &rscale);
    write_output(topcell, vlogoutname);
    return 0;
}

/* Structure to hold a pointer to a net record and an array bound pair */

struct busData {
    NET net;
    int start;
    int end;
};

/*----------------------------------------------------------------------*/
/* Recursion callback function for each item in the NetTable hash	*/
/*----------------------------------------------------------------------*/

struct nlist *hash_nets(struct hashlist *p, void *cptr)
{
    struct hashtable *NetHash = (struct hashtable *)cptr;
    NET net;
    char *dptr;
    struct busData *bdata;
    int aidx;

    /* Get gate from hash record */
    net = (NET)(p->ptr);

    /* Check for array delimiters */
    dptr = strchr(net->netname, '[');
    if (dptr != NULL) {
	*dptr = '\0';    
	sscanf(dptr + 1, "%d", &aidx);
    }
    else aidx = -1;

    /* Check if record already exists */
    bdata = HashLookup(net->netname, NetHash);  
    if (bdata == NULL) {
	/* Allocate record */
	bdata = (struct busData *)malloc(sizeof(struct busData));
	bdata->start = bdata->end = aidx;
	bdata->net = net;
	HashPtrInstall(net->netname, bdata, NetHash);
    }
    else {
	if (aidx != -1) {
	    if (aidx > bdata->start) bdata->start = aidx;
	    if (aidx < bdata->end) bdata->end = aidx;
	}
    }
    if (dptr != NULL) *dptr = '[';
    return NULL;
}

/*--------------------------------------------------------------------------*/
/* Recursion callback function for each item in the cellrec nets hash table */
/*--------------------------------------------------------------------------*/

struct nlist *output_wires(struct hashlist *p, void *cptr)
{
    struct busData *bdata;
    FILE *outf = (FILE *)cptr;

    bdata = (struct busData *)(p->ptr);

    fprintf(outf, "wire ");
    if (bdata->start >= 0 && bdata->end >= 0) {
	fprintf(outf, "[%d:%d] ", bdata->start, bdata->end);
    }
    fprintf(outf, "%s", p->name);

    /* NOTE:  The output format is fixed with power and ground		*/
    /* specified as wires and set to binary values.  May want		*/
    /* additional command line options for various forms;  otherwise,	*/
    /* vlog2Verilog can translate between forms.			*/
	
    if (VddNet && (!strcmp(p->name, VddNet)))
	fprintf(outf, " = 1'b1");
    else if (GndNet && (!strcmp(p->name, GndNet)))
	fprintf(outf, " = 1'b0");

    fprintf(outf, " ;\n");
    return NULL;
}

/*----------------------------------------------------------------------*/
/* Recursion callback function for each item in the cellrec properties  */
/* hash table                                                           */
/*----------------------------------------------------------------------*/

struct nlist *output_props(struct hashlist *p, void *cptr)
{
    char *propval = (char *)(p->ptr);
    FILE *outf = (FILE *)cptr;

    fprintf(outf, ".%s(%s),\n", p->name, propval);
    return NULL;
}

/*----------------------------------------------------------------------*/
/* Recursion callback function for each item in InstanceTable		*/
/*----------------------------------------------------------------------*/

struct nlist *output_instances(struct hashlist *p, void *cptr)
{
    GATE gate = (GATE)(p->ptr);
    FILE *outf = (FILE *)cptr;
    NODE node;
    int i;

    /* Ignore pins which are recorded as gates */
    if (gate->gatetype == PinMacro) return NULL;

    fprintf(outf, "%s ", gate->gatetype->gatename);
    fprintf(outf, "%s (\n", gate->gatename);

    /* Write each port and net connection */
    for (i = 0; i < gate->nodes; i++) {
	node = gate->noderec[i];

	/* node may be NULL if power or ground.  Currently not handling */
	/* this in expectation that the output is standard functional	*/
	/* verilog without power and ground.  Should have runtime	*/
	/* options for handling power and ground in various ways.	*/

	if (node) {
	    fprintf(outf, "    .%s(%s)", gate->node[i], node->netname);
	    if (i < gate->nodes - 1) fprintf(outf, ",");
	    fprintf(outf, "\n");
	}
    }
    fprintf(outf, ");\n\n");
    return NULL;
}

/*--------------------------------------------------------------*/
/* write_output							*/
/*								*/
/*         ARGS: 						*/
/*      RETURNS: 1 to OS					*/
/* SIDE EFFECTS: 						*/
/*--------------------------------------------------------------*/

void write_output(struct cellrec *topcell, char *vlogoutname)
{
    FILE *outfptr = stdout;

    struct netrec *net;
    struct portrec *port;
    struct instance *inst;
    GATE gate;

    struct hashtable NetHash;

    if (vlogoutname != NULL) {
	outfptr = fopen(vlogoutname, "w");
	if (outfptr == NULL) {
	    fprintf(stderr, "Error:  Failed to open file %s for writing netlist output\n",
			vlogoutname);
	    return;
	}
    }

    InitializeHashTable(&NetHash, LARGEHASHSIZE);

    /* Write output module header */
    fprintf(outfptr, "/* Verilog module written by DEF2Verilog (qflow) */\n");
    fprintf(outfptr, "module %s (\n", topcell->name);

    /* Output the verilog netlist verbatim through the	list of ports.	*/

    for (port = topcell->portlist; port; port = port->next) {
	if (port->name == NULL) continue;
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

    /* Find all wires and create a hash table */
    /* (To do: cross check against topcell->nets to ensure correct array bounds) */

    RecurseHashTablePointer(&NetTable, hash_nets, &NetHash);
    RecurseHashTablePointer(&NetHash, output_wires, outfptr);
    fprintf(outfptr, "\n");

    /* Write instances in the order found in the DEF file */

    RecurseHashTablePointer(&InstanceTable, output_instances, outfptr);

    /* End the module */
    fprintf(outfptr, "endmodule\n");

    if (vlogoutname != NULL) fclose(outfptr);

    fflush(stdout);
}

/*--------------------------------------------------------------*/
/* C helpmessage - tell user how to use the program		*/
/*								*/
/*         ARGS: 						*/
/*      RETURNS: 1 to OS					*/
/* SIDE EFFECTS: 						*/
/*--------------------------------------------------------------*/

void helpmessage(FILE *outf)
{
    fprintf(outf, "DEF2Verilog [-options] <netlist>\n");
    fprintf(outf, "\n");
    fprintf(outf, "DEF2Verilog converts a DEF file to a verilog structural\n");
    fprintf(outf, "netlist. Output on stdout.\n");
    fprintf(outf, "\n");
    fprintf(outf, "options:\n");
    fprintf(outf, "  -v <path>  Path to verilog file (for I/O list)\n");
    fprintf(outf, "  -l <path>  Path to standard cell LEF file (for macro list)\n");
    fprintf(outf, "  -p <name>  Name of power net\n");
    fprintf(outf, "  -g <name>  Name of ground net\n");
    fprintf(outf, "\n");
    fprintf(outf, "  -h         Print this message\n");    

} /* helpmessage() */

