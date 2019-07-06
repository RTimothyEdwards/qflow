/*------------------------------------------------------*/
/* addspacers ---					*/
/*							*/
/*  Tool for adding fill cells and other post-placement	*/
/*  cells and data into a DEF format layout.		*/
/*							*/
/*  Primary functions are:				*/
/*							*/
/*  1) Fill the core area out to the bounding box with	*/
/*     fill cells to make the edges straight and fill	*/
/*     any gaps.					*/
/*							*/
/*  2) Create power stripes and power posts, adding	*/
/*     additional fill under (or near, as available)	*/
/*     the power stripes if requested.			*/
/*							*/
/*  3) Adjust pin positions to match any stretching of	*/
/*     the cell done in (2)				*/
/*							*/
/*  4) Adjust obstruction layers to match any		*/
/*     stretching of the cell done in (2)		*/
/*							*/
/*  5) Write the modified DEF layout			*/
/*							*/
/*  6) Write the modified obstruction file (if		*/
/*     modified).					*/
/*							*/
/*------------------------------------------------------*/
/*							*/
/* This file previously addspacers.tcl until it became	*/
/* painfully obvious that the amount of calculation	*/
/* involved made it a very poor choice to be done by	*/
/* an interpreter.					*/
/*							*/
/*------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>     /* For getopt() */
#include <math.h>
#include <ctype.h>
#include <float.h>

#include "hash.h"
#include "readlef.h"
#include "readdef.h"

/* Flags fields */

#define NOSTRETCH  1
#define OBSTRUCT   2
#define VERBOSE    4
#define FILLWARNED 8

/* Structure used to contain core area bounding box.  Also records  */
/* core site height and width.					    */

typedef struct _corebbox *COREBBOX;

typedef struct _corebbox {
    int llx;	    /* Layout bounds */
    int lly; 
    int urx; 
    int ury; 
    int sitew;	    /* Core site width */
    int siteh;	    /* Core site height */
    int fillmin;    /* Minimum fill cell width */
    int orient;	    /* Orientation of the first (lowest) row */
} corebbox;

/* Structure used to hold the final calculated pitch and width of stripes */

typedef struct _stripeinfo *SINFO;

typedef struct _stripeinfo {
    int width;
    int pitch;
    int offset;
    int stretch;   /* Maximum amount of layout stretching */
    int number;    /* Total number of stripes */
} stripeinfo;

/* List of gates (used to sort fill cells) */
typedef struct filllist_ *FILLLIST;

struct filllist_ {
    FILLLIST next;
    FILLLIST last;
    GATE gate;
    int width;
};

/* Structures for defining a power stripe. */

typedef struct _powerpost *PPOST;

typedef struct _powerpost {
    PPOST   next;
    DSEG    strut;
    LefList viagen;
} powerpost;

typedef struct _powerstripe *PSTRIPE;

typedef struct _powerstripe {
    PSTRIPE	next;	    // One stripe definition centered on X = 0
    PPOST	posts;	    // All posts for one stripe, centered on X = 0
    DSEG	stripe;
    int		offset;	    // Horizontal start of posts
    int		num;	    // Number of stripes
    int		pitch;	    // Spacing between stripes
    char       *name;	    // Net name of power rail
} powerstripe;

/* Hash table of instances hashed by (X, Y) position */
struct hashtable CellPosTable;

/* Forward declarations */
unsigned char check_overcell_capable(unsigned char Flags);
FILLLIST generate_fill(char *fillcellname, float rscale, COREBBOX corearea,
	unsigned char Flags);
SINFO generate_stripefill(char *VddNet, char *GndNet,
	char *stripepat, float stripewidth_t, float stripepitch_t,
	char *fillcellname, float scale, FILLLIST fillcells,
	COREBBOX corearea, unsigned char Flags);
void fix_obstructions(char *definname, SINFO stripevals, float scale,
	unsigned char Flags);
PSTRIPE generate_stripes(SINFO stripevals, FILLLIST fillcells,
	COREBBOX corearea, char *stripepat, char *VddNet, char *GndNet,
	float scale, unsigned char Flags);
void write_output(char *definname, char *defoutname, float scale,
	    COREBBOX corearea, SINFO stripevals, PSTRIPE rails,
	    char *VddNet, char *GndNet, unsigned char Flags);
void helpmessage(FILE *outf);

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    int i, result;
    unsigned char Flags;
    float rscale;
    struct cellrec *topcell;
    COREBBOX corearea;
    FILLLIST fillcells;
    SINFO stripevals;
    PSTRIPE rails;

    char *definname = NULL;
    char *fillcellname = NULL;
    char *defoutname = NULL;

    float stripewidth_t, stripepitch_t;
    static char* default_stripepat = "PG";
    char *stripepat = default_stripepat;

    char *VddNet = NULL;
    char *GndNet = NULL;

    Flags = 0;
    stripewidth_t = stripepitch_t = 0.0;

    while ((i = getopt(argc, argv, "hHvOn:o:l:p:g:f:w:P:s:")) != EOF) {
        switch( i ) {
	    case 'v':
		Flags |= VERBOSE;
		/* Also set global variable used by readlef.c */
		Verbose = 1;
		break;
            case 'h':
            case 'H':
                helpmessage(stdout);
                exit(0);
                break;
	    case 'n':
		Flags |= NOSTRETCH;
		break;
	    case 'O':
		Flags |= OBSTRUCT;
		break;
	    case 'w':
		if (sscanf(optarg, "%g", &stripewidth_t) != 1) {
		    fprintf(stderr, "Cannot read numeric value from \"%s\"\n", optarg);
		}
		break;
	    case 'P':
		if (sscanf(optarg, "%g", &stripepitch_t) != 1) {
		    fprintf(stderr, "Cannot read numeric value from \"%s\"\n", optarg);
		}
		break;
	    case 'o':
	        defoutname = strdup(optarg);
		break;
	    case 'f':
	        fillcellname = strdup(optarg);
		break;
	    case 's':
		if (!strcmp(optarg, "tripe")) {
		    /* Accept the argument "-stripe <width> <pitch> <pattern>"	*/
		    /* that was the option for the old addspacers.tcl script.	*/
		    /* Combines width, pitch, and pattern in one argument.	*/
		    if (sscanf(argv[optind], "%g", &stripewidth_t) != 1) {
			fprintf(stderr, "Cannot read numeric value from \"%s\"\n", argv[optind]);
		    }
		    optind++;
		    if (sscanf(argv[optind], "%g", &stripepitch_t) != 1) {
			fprintf(stderr, "Cannot read numeric value from \"%s\"\n", argv[optind]);
		    }
		    optind++;
		    stripepat = strdup(argv[optind]);
		    optind++;
		}
		else {
		    stripepat = strdup(optarg);
		}
		break;
            case 'l':
                LefRead(optarg);        /* Can be called multiple times */
                break;
            case 'p':
                VddNet = strdup(optarg);
                break;
            case 'g':
                GndNet = strdup(optarg);
                break;
            default:
                fprintf(stderr,"Bad switch \"%c\"\n", (char)i);
                helpmessage(stderr);
                return 1;
        }
    }

    if (optind < argc) {
	char *pptr;

        definname = (char *)malloc(strlen(argv[optind]) + 5);
	strcpy(definname, argv[optind]);
	pptr = strrchr(argv[optind], '.');
	if (pptr == NULL) strcat(definname, ".def");
        optind++;
    }
    else {
        fprintf(stderr, "Couldn't find a filename for DEF input file.\n");
        helpmessage(stderr);
        return 1;
    }
    optind++;

    result = DefRead(definname, &rscale);

    corearea = (COREBBOX)malloc(sizeof(corebbox));

    Flags |= check_overcell_capable(Flags);
    fillcells = generate_fill(fillcellname, rscale, corearea, Flags);
    if (fillcells == NULL) {
	fprintf(stderr, "Failed to parse any fill cells from the standard cell library.\n");
	return 1;
    }
    stripevals = generate_stripefill(VddNet, GndNet, stripepat,
	    stripewidth_t, stripepitch_t, fillcellname, rscale, fillcells,
	    corearea, Flags);
    fix_obstructions(definname, stripevals, rscale, Flags);
    rails = generate_stripes(stripevals, fillcells, corearea, stripepat,
	    VddNet, GndNet, rscale, Flags);
    write_output(definname, defoutname, rscale, corearea, stripevals,
	    rails, VddNet, GndNet, Flags);

    return 0;
}

/*--------------------------------------------------------------*/
/* Find empty spaces in the DEF layout and insert fill macros	*/
/*--------------------------------------------------------------*/

FILLLIST
generate_fill(char *fillcellname, float scale, COREBBOX corearea, unsigned char Flags)
{
    GATE gate, newfillinst;
    ROW row;
    int isfill, orient;
    int corew = 1, coreh = 0, testh;
    int instx, insty, instw, insth, fnamelen;
    int x, y, dx, nx, fillmin;
    char posname[32];

    int corellx = 0;
    int corelly = 0;
    int coreurx = 0;
    int coreury = 0;

    FILLLIST fillcells = NULL;
    FILLLIST newfill, testfill;

    /* Parse library macros and find CORE SITE definition */

    for (gate = GateInfo; gate; gate = gate->next)
    {
	if (!strncmp(gate->gatename, "site_", 5)) {
	    if (gate->gateclass == MACRO_CLASS_CORE) {
		corew = (int)(roundf(gate->width * scale));
		testh = (int)(roundf(gate->height * scale));
		// Sometimes there are multiple-height core sites. . .
		if (coreh == 0 || (testh < coreh)) coreh = testh;
	    }
	}
    }
    if (corew == 0) {
	fprintf(stderr, "Warning: failed to find any core site.\n");
	/* Use route pitch for step size */
	corew = (int)(roundf(LefGetRoutePitch(0) * scale));
    }
    if (corew == 0) {
	fprintf(stderr, "Error: failed to find any core site or route pitch.\n");
	return NULL;
    }

    /* Parse library macros and find fill cells.  Use "fillcellname" as	a   */
    /* prefix, if given;  if not, find spacer cells by class.		    */

    fnamelen = (fillcellname) ? strlen(fillcellname) : 0;

    for (gate = GateInfo; gate; gate = gate->next)
    {
	isfill = FALSE;
	if (fillcellname) {
	    if (!strncmp(gate->gatename, fillcellname, fnamelen))
		isfill = TRUE;
	}
	else if (gate->gatesubclass == MACRO_SUBCLASS_SPACER)
	    isfill = TRUE;

	if (isfill == TRUE) {
	    newfill = (FILLLIST)malloc(sizeof(struct filllist_));
	    newfill->gate = gate;
	    newfill->width = (int)(roundf(gate->width * scale));

	    /* Insert in fill list in width order, high to low */
	    if (fillcells == NULL) {
		newfill->next = newfill->last = NULL;
		fillcells = newfill;
	    }
	    else {
		for (testfill = fillcells; testfill; testfill = testfill->next) {
		    if (testfill->width < newfill->width) {
			/* insert before */
			newfill->next = testfill;
			newfill->last = testfill->last;
			if (testfill->last == NULL)
			    fillcells = newfill;
			else
			    testfill->last->next = newfill;
			testfill->last = newfill;
			break;
		    }
		    else if (testfill->next == NULL) {
			/* put at end */
			newfill->next = NULL;
			testfill->next = newfill;
			newfill->last = testfill;
			break;
		    }
		}
	    }
	}
    }

    if (fillcells == NULL) {
	fprintf(stderr, "Error:  No fill cells have been specified or found.\n");
	return NULL;
    }

    if (fillcells) {
	testh = (int)(roundf(fillcells->gate->height * scale));
	if ((coreh == 0) || (coreh < testh)) {
	    /* Use fill cell height for core height */
	    coreh = testh;
	}
    }
    if (coreh == 0) {
	fprintf(stderr, "Error: failed to find any core site or standard cell height.\n");
	return NULL;
    }
    if (Flags & VERBOSE)
	fprintf(stdout, "Core site is %g x %g um\n",
		(float)corew / scale, (float)coreh / scale);


    /* Rehash all instances by position */
    /* Find minimum and maximum bounds, and record cell in lower left position */

    InitializeHashTable(&CellPosTable, LARGEHASHSIZE);

    for (gate = Nlgates; gate; gate = gate->next)
    {
	/* Do not evaluate pins, only core cells */
	if (gate->gatetype == NULL) continue;

	instx = (int)(roundf(gate->placedX * scale));
	insty = (int)(roundf(gate->placedY * scale));
	instw = (int)(roundf(gate->width * scale));
	insth = (int)(roundf(gate->height * scale));
	sprintf(posname, "%dx%d", instx, insty);
	HashPtrInstall(posname, gate, &CellPosTable);

	if (corellx == coreurx) {
	    corellx = instx;
	    coreurx = instx + instw;
	    corelly = insty;
	    coreury = insty + insth;
	}
	else {
	    if (instx < corellx) corellx = instx;
	    else if (instx + instw > coreurx) coreurx = instx + instw;

	    if (insty < corelly) corelly = insty;
	    else if (insty + insth > coreury) coreury = insty + insth;
	}
    }

    fprintf(stdout, "Initial core layout: (%d %d) to (%d %d) (scale um * %d)\n",
		corellx, corelly, coreurx, coreury, (int)scale);

    if (Flags & VERBOSE) fprintf(stdout, "Adding fill cells.\n");

    /* Find the orientation of the first row and record this	    */
    /* in corearea.  NOTE:  This is simpler if the DEF file records */
    /* ROWs, something that needs to be done in place2def.	    */

    row = DefFindRow(corelly);
    if (row) {
	corearea->orient = row->orient & (RN | RS);
    }
    else {
	corearea->orient = RN;
	y = corelly;
	x = corellx;
	while (x < coreurx) {
	    sprintf(posname, "%dx%d", x, y);
	    gate = (GATE)HashLookup(posname, &CellPosTable);
	    if (gate != NULL) {
		corearea->orient = gate->orient & (RN | RS);
		break;
	    }
	    x += testfill->width;
	}
    }
    orient = corearea->orient;

    /* Starting from the lower-left corner, find gate at each site  */
    /* position.  If there is no gate at the site position, then    */
    /* find the next gate forward, and add fill.		    */

    /* NOTE:  This routine does not account for obstruction areas   */
    /* used to define a non-rectangular core area (to be done)	    */

    for (y = corelly; y < coreury; y += coreh) {
	x = corellx;
	while (x < coreurx) {
	    sprintf(posname, "%dx%d", x, y);
	    gate = (GATE)HashLookup(posname, &CellPosTable);
	    if (gate == NULL) {
		for (nx = x + corew; nx < coreurx; nx += corew) {
		    sprintf(posname, "%dx%d", nx, y);
		    gate = (GATE)HashLookup(posname, &CellPosTable);
		    if (gate != NULL) break;
		}
		if (Flags & VERBOSE)
		    fprintf(stdout, "Add fill from (%d %d) to (%d %d)\n",
				x, y, nx, y);
		dx = nx - x;

		while (dx > 0) {
		    for (testfill = fillcells; testfill; testfill = testfill->next) {
			if (testfill->width <= dx) break;
		    }
		    if (testfill == NULL) {
			if (nx == coreurx) {
			    if (!(Flags & FILLWARNED)) {
				fprintf(stderr, "Notice: Right edge of layout"
					    " cannot be cleanly aligned due to\n");
				fprintf(stderr, "limited fill cell widths.\n");
			    }
			    Flags |= FILLWARNED;    // Do not repeat this message
			}
			else {
			    fprintf(stderr, "Error: Empty slot at (%g, %g) is smaller"
				    " than any available fill cell.\n",
				    (float)x / scale, (float)y / scale);
			}
			x = nx;
			dx = 0;
			break;
		    }

		    /* Create new fill instance */
		    newfillinst = (GATE)malloc(sizeof(struct gate_));
		    newfillinst->gatetype = testfill->gate;
		    sprintf(posname, "FILL%dx%d", x, y);
		    newfillinst->gatename = strdup(posname);
		    newfillinst->placedX = (double)x / (double)scale;
		    newfillinst->placedY = (double)y / (double)scale;
		    newfillinst->clientdata = (void *)NULL;
		    row = DefFindRow(y);
		    newfillinst->orient = (row) ? row->orient : orient;
		    DefAddGateInstance(newfillinst);

		    /* Hash the new instance position */
		    sprintf(posname, "%dx%d", x, y);
		    HashPtrInstall(posname, newfillinst, &CellPosTable);

		    dx -= testfill->width;
		    x += testfill->width;
		}
	    }
	    else {
		x += (int)(roundf(gate->width * scale));
	    }
	}
	/* Flip orientation each row (NOTE:  This is not needed if ROW	*/
	/* statements are in the DEF file!				*/
	orient = (orient == RN) ? RS : RN;
    }

    if (fillcells) {
	for (testfill = fillcells; testfill->next; testfill = testfill->next);
	fillmin = testfill->width;
    }
    else fillmin = 0;

    corearea->llx = corellx;
    corearea->lly = corelly;
    corearea->urx = coreurx;
    corearea->ury = coreury;
    corearea->sitew = corew;
    corearea->siteh = coreh;
    corearea->fillmin = fillmin;

    return fillcells;
}

/*--------------------------------------------------------------*/
/* Check if there are not enough metal layers to route power	*/
/* over the cell.  If not, then force the NOSTRETCH option.	*/
/*--------------------------------------------------------------*/

unsigned char check_overcell_capable(unsigned char Flags)
{
    int ltop;

    if (!(Flags & NOSTRETCH)) {
        ltop = LefGetMaxRouteLayer() - 1;
        if (LefGetRouteOrientation(ltop) == 1) ltop--;
        if (ltop < 3) {
	    fprintf(stderr, "Warning:  Stretching requested, but not applicable.\n");
	    return (unsigned char)NOSTRETCH;
        }
    }
    return (unsigned char)0;
}

/*--------------------------------------------------------------*/
/* Generate power stripes					*/
/*--------------------------------------------------------------*/

SINFO
generate_stripefill(char *VddNet, char *GndNet, char *stripepat,
	float stripewidth_t, float stripepitch_t,
	char *fillcellname, float scale, FILLLIST fillcells,
	COREBBOX corearea, unsigned char Flags)
{
    int numstripes;
    int minstripes;
    int corew, tw, tp, tr, dx, x, y, nx;
    int stripepitch_i, stripewidth_i, stripeoffset_i;
    int stripepitch_f, stripewidth_f, stripeoffset_f;
    int totalw;
    int orient;
    int nextx, totalfx;
    FILLLIST fillseries, testfill, newfill;
    SINFO stripevals;
    char posname[32];
    GATE gate, newfillinst;
    ROW row;

    stripevals = (SINFO)malloc(sizeof(stripeinfo));
    stripevals->pitch = 0;
    stripevals->width = 0;
    stripevals->offset = 0;

    corew = corearea->urx - corearea->llx;

    if (stripewidth_t <= 0.0 || stripepitch_t <= 0.0) {
	fprintf(stdout, "No stripe information provided;  no power stripes added.\n");
	return stripevals;
    }

    minstripes = strlen(stripepat);

    /* Scale stripe width and pitch from microns to DEF database units */
    stripewidth_i = (int)(roundf(stripewidth_t * scale));
    stripepitch_i = (int)(roundf(stripepitch_t * scale));

    /* Adjust stripewidth to the nearest core site unit width.	    */
    /* If stretching is specified and the minimum fill cell width   */
    /* is greater than the core site width, then adjust to the	    */
    /* nearest multiple of the minimum fill cell width.		    */

    if ((!(Flags & NOSTRETCH)) && (corearea->fillmin > corearea->sitew)) {
	tw = stripewidth_i / corearea->fillmin;
	tr = stripewidth_i % corearea->fillmin;
	stripewidth_f = (tw + ((tr == 0) ? 0 : 1)) * corearea->fillmin;
    }
    else {
	tw = stripewidth_i / corearea->sitew;
	tr = stripewidth_i % corearea->sitew;
	stripewidth_f = (tw + ((tr == 0) ? 0 : 1)) * corearea->sitew;
    }

    /* Adjust stripepitch to the nearest core site unit width */
    tp = (int)(0.5 + (float)stripepitch_i / (float)corearea->sitew);
    stripepitch_f = (tp * corearea->sitew);

    if (stripepitch_f < stripewidth_f * 2) {
	fprintf(stderr, "Error: Stripe pitch is too small (pitch = %g, width = %g)!\n",
		(float)stripepitch_f / (float)scale,
		(float)stripewidth_f / (float)scale);
	return stripevals;
    }
    if ((fillcells == NULL) && (!(Flags & NOSTRETCH))) {
	fprintf(stderr, "No fill cells defined.  Not stretching layout.\n");
	Flags |= NOSTRETCH;
    }

    if (stripepitch_f != stripepitch_i) {
	fprintf(stderr, "Stripe pitch requested = %g, stripe pitch used = %g\n",
		stripepitch_t, (float)stripepitch_f / (float)scale);
    }
    if (stripewidth_f != stripewidth_i) {
	fprintf(stderr, "Stripe width requested = %g, stripe width used = %g\n",
		stripewidth_t, (float)stripewidth_f / (float)scale);
    }

    if (!(Flags & NOSTRETCH))
    {
	/* Cell will be stretched, so compute the amount to be	*/
	/* streteched per stripe, which is the stripewidth	*/
	/* adjusted to the nearest unit site width.		*/

	numstripes = corew / (stripepitch_f - stripewidth_f);

	if (numstripes < minstripes) {
	    numstripes = minstripes;

	    /* Recompute stripe pitch */
	    stripepitch_f = corew / numstripes;
	    tp = (int)(0.5 + (float)stripepitch_f / (float)corearea->sitew);
	    stripepitch_f = (tp * corearea->sitew);

	    fprintf(stdout, "Stripe pitch reduced from %g to %g to fit in layout\n",
		    stripepitch_t, (float)stripepitch_f / (float)scale);
	}
	totalw = corew + numstripes * stripewidth_f;

	/* Find a series of fill cell macros to match the stripe width */
	fillseries = NULL;
	dx = stripewidth_f;
	while (dx > 0) {
	    FILLLIST sfill;
	    int diff;

	    for (testfill = fillcells; testfill; testfill = testfill->next) {
		if (testfill->width <= dx) break;
	    }
	    if (testfill == NULL) {
		/* This can happen if there is no fill cell that is the	*/
		/* same width as the minimum site pitch.  If so, find	*/
		/* the first non-minimum-size fill cell and change it	*/
		/* to minimum size, then continue.			*/
		for (testfill = fillcells; testfill && testfill->next;
			    testfill = testfill->next);
		for (sfill = fillseries; sfill; sfill = sfill->next)
		    if (sfill->gate != testfill->gate) break;
		if (sfill == NULL) {
		    fprintf(stderr, "Error: failed to find fill cell series matching the"
			    " stripe width.\n");
		    fprintf(stderr, "Try specifying a different stripe width.\n");
		    dx = 0;
		    break;
		}
		diff = sfill->width - testfill->width;
		sfill->gate = testfill->gate;
		sfill->width = testfill->width;
		dx += diff;
	    }
	    newfill = (FILLLIST)malloc(sizeof(struct filllist_));
	    newfill->gate = testfill->gate;
	    newfill->width = testfill->width;
	    newfill->next = fillseries;
	    fillseries = newfill;
	    dx -= newfill->width;
	}

	/* Find offset to center of 1st power stripe that results in	*/
	/* centering the stripes on the layout.				*/

	stripeoffset_i = (totalw - (numstripes - 1) * stripepitch_f) / 2;
	tp = (int)(0.5 + (float)stripeoffset_i / (float)corearea->sitew);
	stripeoffset_f = (tp * corearea->sitew);

	/* Add fill cells (approximately) under stripe positions.	*/
	/* Note that this is independent of drawing the stripes and	*/
	/* power posts.  There is no requirement that the stretch fill	*/
	/* must be directly under the stripe, only that the total cell	*/
	/* width is increased by the total width of all stripes, and	*/
	/* the extra fill is added as close to each stripe as possible.	*/

	orient = corearea->orient;
        for (y = corearea->lly; y < corearea->ury; y += corearea->siteh) {
	    nextx = corearea->llx + stripeoffset_f - stripewidth_f / 2;
	    totalfx = 0;

	    x = corearea->llx;

	    sprintf(posname, "%dx%d", x, y);
	    gate = (GATE)HashLookup(posname, &CellPosTable);

	    while (x < corearea->urx) {
		while (x < nextx) {
		    nx = x + (int)(roundf(gate->width * scale));

		    /* If next position is larger than nextx but is also    */
		    /* farther from the stripe centerline than the current  */
		    /* position, then break here instead.		    */
		    if ((nx > nextx) && ((nextx - x) < (nx - nextx))) break;

		    gate->placedX += (double)totalfx / (double)scale;

		    sprintf(posname, "%dx%d", nx, y);
		    gate = (GATE)HashLookup(posname, &CellPosTable);
		    x = nx;

		    if ((x >= corearea->urx) || (gate == NULL)) break;
		}
		if ((x >= corearea->urx) || (gate == NULL)) break;

		if (Flags & VERBOSE)
		    fprintf(stdout, "Add fill under stripe from (%d %d) to (%d %d)\n",
				x, y, x + stripewidth_f, y);

		for (testfill = fillseries; testfill; testfill = testfill->next) {
		    /* Create new fill instance */
		    newfillinst = (GATE)malloc(sizeof(struct gate_));
		    newfillinst->gatetype = testfill->gate;
		    sprintf(posname, "SFILL%dx%d", x + totalfx, y);
		    newfillinst->gatename = strdup(posname);
		    newfillinst->placedX = (double)(x + totalfx) / (double)scale;
		    newfillinst->placedY = (double)y / (double)scale;
		    newfillinst->clientdata = (void *)NULL;
		    row = DefFindRow(y);
		    newfillinst->orient = (row) ? row->orient : orient;
		    DefAddGateInstance(newfillinst);

		    /* Position will not be revisited, so no need to 	*/
		    /* add to the position hash.			*/

		    totalfx += testfill->width;
		}
		nextx += stripepitch_f;
	    }
	    orient = (orient == RN) ? RS : RN;
	}

	/* Adjust pins */

	for (gate = Nlgates; gate; gate = gate->next) {
	    if (gate->gatetype == NULL) {
		int px, po, pitches;

		px = (int)(roundf(gate->placedX * scale));
		po = px - stripeoffset_f - (stripewidth_f / 2);
		if (po > 0)
		    pitches = 1 + po / stripepitch_f;
		else
		    pitches = -1;
		if (pitches <= 0) continue;

		px += pitches * stripewidth_f;
		gate->placedX = (float)(px) / scale;
	    }
	}

	if (Flags & VERBOSE) fprintf(stdout, "Layout stretched by %g um\n",
		    (double)totalfx / (double)scale);
    }
    else
    {
	/* Stripes are overlaid on core without stretching */
	numstripes = corew / stripepitch_f;
	if (numstripes < minstripes) {
	    numstripes = minstripes;

	    /* Recompute stripe pitch */
	    stripepitch_f = corew / numstripes;
	    tp = (int)(0.5 + (float)stripepitch_i / (float)corearea->sitew);
	    stripepitch_f = (tp * corearea->sitew);

	    fprintf(stdout, "Stripe pitch reduced from %g to %g to fit in layout\n",
		    stripepitch_t, (float)stripepitch_f / (float)scale);
	}
	totalw = corew;

	/* Find offset to center of 1st power stripe that results in	*/
	/* centering the stripes on the layout.				*/

	stripeoffset_i = (totalw - (numstripes - 1) * stripewidth_f) / 2;
	tp = (int)(0.5 + (float)stripeoffset_i / (float)corearea->sitew);
	stripeoffset_f = (tp * corearea->sitew);
    }

    /* Record and return final calculated power stripe pitch and width */
    stripevals->pitch = stripepitch_f;
    stripevals->width = stripewidth_f;
    stripevals->offset = stripeoffset_f;
    stripevals->stretch = totalfx;
    stripevals->number = numstripes;
    return stripevals;
}


/*--------------------------------------------------------------*/
/* Adjust obstructions						*/
/* If OBSTRUCT flag is set, then obstructions are to be read	*/
/* from the file <rootname>.obs, modified, and written to file	*/
/* <rootname>.obsx.						*/
/*--------------------------------------------------------------*/

void
fix_obstructions(char *definname, SINFO stripevals, float scale,
	    unsigned char Flags)
{
    FILE *fobsin, *fobsout;
    char *filename, *pptr;
    char line[256];
    char layer[32];
    float fllx, flly, furx, fury;
    int   illx, iurx, pitches, po;

    /* If no layout stretching was done, then nothing needs to be modified  */
    if (Flags & NOSTRETCH) return;

    /* Only handle obstruction layer file if the -O switch was specified    */
    /* (to do:  Handle obstructions via the DEF file BLOCKAGES records)	    */

    if (!(Flags & OBSTRUCT)) return;

    filename = (char *)malloc(strlen(definname) + 6);
    strcpy(filename, definname);
    pptr = strrchr(filename, '.');
    if (pptr == NULL)
	strcat(filename, ".obs");
    else
	sprintf(pptr, ".obs");

    fobsin = fopen(filename, "r");
    if (fobsin == NULL) {
	fprintf(stderr, "Cannot open obstruction file %s for reading\n", filename);
	free(filename);
	return;
    }

    pptr = strrchr(filename, '.');
    sprintf(pptr, ".obsx");

    fobsout = fopen(filename, "w");
    if (fobsout == NULL) {
	fprintf(stderr, "Cannot open obstruction file %s for writing\n", filename);
	fclose(fobsin);
	free(filename);
	return;
    }

    if (Flags & VERBOSE) fprintf(stdout, "Modifying obstruction positions.\n");

    while (1) {
	if (fgets(line, 256, fobsin) == NULL) break;

	if (!strncmp(line, "obstruction", 11)) {
	    sscanf(line + 11, "%g %g %g %g %s", &fllx, &flly, &furx, &fury, layer);

	    if (Flags & VERBOSE)
		fprintf(stdout, "In: %g %g %g %g\n", fllx, flly, furx, fury);

	    illx = (int)(roundf(fllx * scale));
	    iurx = (int)(roundf(furx * scale));

	    po = illx - stripevals->offset - (stripevals->width / 2);
	    if (po > 0) {
		pitches = 1 + po / stripevals->pitch;
		illx += pitches * stripevals->width;
	    }
	    po = iurx - stripevals->offset - (stripevals->width / 2);
	    if (po > 0) {
		pitches = 1 + po / stripevals->pitch;
		iurx += pitches * stripevals->width;
	    }

	    fllx = (float)illx / scale;
	    furx = (float)iurx / scale;

	    fprintf(fobsout, "obstruction %g %g %g %g %s\n",
			fllx, flly, furx, fury, layer);

	    if (Flags & VERBOSE)
		fprintf(stdout, "Out: %g %g %g %g\n", fllx, flly, furx, fury);
	}
    }

    free(filename);
    fclose(fobsin);
    fclose(fobsout);
}

/*--------------------------------------------------------------*/
/* Create a new VIA record from a VIA or VIARULE record, with	*/
/* total width and height as given.				*/
/*--------------------------------------------------------------*/

void
via_make_generated(LefList viagen, LefList lefl, int lbot, int lcut,
		int width, int height, float scale)
{
    float cutsizex, cutsizey;
    float bboundx, bboundy;
    float tboundx, tboundy;

    int xcuts, ycuts;
    char vianame[128];
    DSEG newseg;
    LefList cutrec;

    float borderx, bordery, spacingx, spacingy;
    float fwidth, fheight;
    float x, y;
    int i, j;

    int ltop = lbot + 1;

    /* Convert width and height to microns */
    fwidth = (float)width / scale;
    fheight = (float)height / scale;

    sprintf(vianame, "%s_post", lefl->lefName);
    viagen->lefName = strdup(vianame);

    /* Determine number of cuts in X and Y */

    cutsizex = LefGetViaWidth(lefl, lcut, 0);
    cutsizey = LefGetViaWidth(lefl, lcut, 1);
    bboundx = LefGetViaWidth(lefl, lbot, 0);
    bboundy = LefGetViaWidth(lefl, lbot, 1);
    tboundx = LefGetViaWidth(lefl, ltop, 0);
    tboundy = LefGetViaWidth(lefl, ltop, 1);
    
    /* Calculate number of cuts to fit */

    borderx = (((tboundx > bboundx) ? tboundx : bboundx) - cutsizex) / 2;
    bordery = (((tboundy > bboundy) ? tboundy : bboundy) - cutsizey) / 2;
    
    /* If there is a SPACING record in the via, use it.  If not, see if	*/
    /* there is a SPACING record in the record for the via cut.  If	*/
    /* not, then assume spacing is twice the border width.		*/

    cutrec = lefl;
    if (cutrec->info.via.spacing == NULL) cutrec = LefFindLayerByNum(lcut);
    if (cutrec && cutrec->info.via.spacing) {
	spacingx = cutrec->info.via.spacing->spacing;
	if (cutrec->info.via.spacing->next)
	    spacingy = cutrec->info.via.spacing->next->spacing;
	else
	    spacingy = spacingx;
    }
    else {
	spacingx = 2 * borderx;
	spacingy = 2 * bordery;
    }

    xcuts = 1 + (int)((fwidth - 2 * borderx) - cutsizex) / (cutsizex + spacingx);
    ycuts = 1 + (int)((fheight - 2 * bordery) - cutsizey) / (cutsizey + spacingy);

    /* Ensure at least one cut! */
    if (xcuts < 1) xcuts = 1;
    if (ycuts < 1) ycuts = 1;

    /* Make sure that width and height are enough to pass DRC.  Height	*/
    /* in particular is taken from the width of the power bus and may	*/
    /* not be wide enough for the topmost contacts.			*/

    fwidth = (xcuts * cutsizex) + ((float)(xcuts - 1) * spacingx) + (2 * borderx);
    fheight = (ycuts * cutsizey) + ((float)(ycuts - 1) * spacingy) + (2 * bordery);

    viagen->info.via.area.layer = lbot;
    viagen->info.via.area.x1 = -fwidth / 2;
    viagen->info.via.area.x2 = fwidth / 2;
    viagen->info.via.area.y1 = -fheight / 2;
    viagen->info.via.area.y2 = fheight / 2;

    newseg = (DSEG)malloc(sizeof(struct dseg_));
    newseg->layer = ltop;
    newseg->x1 = -fwidth / 2;
    newseg->x2 = fwidth / 2;
    newseg->y1 = -fheight / 2;
    newseg->y2 = fheight / 2;
    newseg->next = NULL;
    viagen->info.via.lr = newseg;

    x = (-fwidth / 2) + borderx + (cutsizex / 2);
    for (i = 0; i < xcuts; i++) {
        y = (-fheight / 2) + bordery + (cutsizey / 2);
	for (j = 0; j < ycuts; j++) {
	    newseg = (DSEG)malloc(sizeof(struct dseg_));
	    newseg->layer = lcut;
	    newseg->x1 = x - (cutsizex / 2);
	    newseg->x2 = x + (cutsizex / 2);
	    newseg->y1 = y - (cutsizey / 2);
	    newseg->y2 = y + (cutsizey / 2);
	    newseg->next = viagen->info.via.lr;
	    viagen->info.via.lr = newseg;
	    y += (cutsizey + spacingy);
	}
	x += (cutsizex + spacingy);
    }
}

/*--------------------------------------------------------------*/
/* Routine to check if a LefList entry is a valid via record	*/
/* for a via between metal layers "l" (ell) and "l + 1".	*/
/* Since the LefList record drops metal and cut layers more or	*/
/* less randomly into the area, lr, and lr->next records, all	*/
/* combinations of layers need to be checked.			*/
/*								*/
/* Since the metal layers are known and the cut layer is not	*/
/* necessarily known, return the cut layer number if the via	*/
/* record is valid.  If not, return -1.				*/
/*--------------------------------------------------------------*/

int
check_valid_via(LefList lefl, int l)
{
    int cutlayer = -1;

    if (lefl->info.via.area.layer == l) {
	if (lefl->info.via.lr && lefl->info.via.lr->layer == l + 1) {
	    if (lefl->info.via.lr->next)
		cutlayer = lefl->info.via.lr->next->layer;
	}
	else if (lefl->info.via.lr && lefl->info.via.lr->next &&
		lefl->info.via.lr->next->layer == l + 1) {
	    cutlayer = lefl->info.via.lr->layer;
	}
    }
    else if (lefl->info.via.area.layer == l + 1) {
	if (lefl->info.via.lr && lefl->info.via.lr->layer == l) {
	    if (lefl->info.via.lr->next)
		cutlayer = lefl->info.via.lr->next->layer;
	}
	else if (lefl->info.via.lr && lefl->info.via.lr->next &&
		lefl->info.via.lr->next->layer == l) {
	    cutlayer = lefl->info.via.lr->layer;
	}
    }
    else if (lefl->info.via.lr && lefl->info.via.lr->layer == l) {
	if (lefl->info.via.lr && lefl->info.via.lr->next &&
		lefl->info.via.lr->next->layer == l + 1)
	    cutlayer = lefl->info.via.area.layer;
    }
    else if (lefl->info.via.lr && lefl->info.via.lr->layer == l + 1) {
	if (lefl->info.via.lr && lefl->info.via.lr->next &&
		lefl->info.via.lr->next->layer == l)
	    cutlayer = lefl->info.via.area.layer;
    }
    return cutlayer;
}

/*--------------------------------------------------------------*/
/* Generate stripe contact posts and metal			*/
/*--------------------------------------------------------------*/

PSTRIPE
generate_stripes(SINFO stripevals, FILLLIST fillcells,
	COREBBOX corearea, char *pattern,
	char *VddNet, char *GndNet, float scale, unsigned char Flags)
{
    int i, j, l, p, n, y, hh;
    int testuse;
    int ltop, lbot;
    float syt, syb;
    double vw, vh;
    int cutlayer, lcut;
    int gnd_ymin, gnd_ymax, vdd_ymin, vdd_ymax, gate_ymin;
    int gnd_xmin, vdd_xmin, tmp;
    char *powername, *groundname;
    int corew;

    PSTRIPE rails = NULL, prail;
    PPOST post;
    LefList lefl, *vialist, topvia, vvalid;
    DSEG lr;

    /* Pick the first fill cell and parse it for the POWER and	*/
    /* GROUND pins as marked by pin USE.  If no pins are marked	*/
    /* as use POWER or GROUND then look for pins with names	*/
    /* matching the power and ground nets.			*/

    GATE fillgate = fillcells->gate;
    DSEG r;
    ROW row;

    lbot = 0;
    for (i = 0; i < fillgate->nodes; i++) {
	testuse = fillgate->use[i];
	if (testuse == PORT_USE_POWER) {
	    powername = fillgate->node[i];
	    break;
	}
    }
    if (i == fillgate->nodes) {
	for (i = 0; i < fillgate->nodes; i++) {
	    if (!strcmp(fillgate->node[i], VddNet)) {
		powername = VddNet;
		lbot = fillgate->taps[i]->layer;
		break;
	    }
	}
	if (i == fillgate->nodes) {
	    fprintf(stderr, "Failed to find power net pin in cell macro.\n"); 
	    return NULL;
	}
    }
    /* NOTE:  Need to parse all taps;  find one that crosses the whole	*/
    /* cell.  If none, then find one that touches or overlaps the cell	*/
    /* bottom or top.							*/

    r = fillcells->gate->taps[i];
    vdd_ymin =  (int)(roundf(r->y1 * scale));
    vdd_ymax =  (int)(roundf(r->y2 * scale));
    vdd_xmin =  (int)(roundf(r->x1 * scale));

    for (j = 0; j < fillgate->nodes; j++) {
	testuse = fillgate->use[j];
	if (testuse == PORT_USE_GROUND) {
	    groundname = fillgate->node[j];
	    break;
	}
    }
    if (j == fillgate->nodes) {
	for (j = 0; j < fillgate->nodes; j++) {
	    if (!strcmp(fillgate->node[j], GndNet)) {
		groundname = GndNet;
		lbot = fillgate->taps[i]->layer;
		break;
	    }
	}
	if (j == fillgate->nodes) {
	    fprintf(stderr, "Failed to find ground net pin in cell macro.\n"); 
	    return NULL;
	}
    }
    r = fillcells->gate->taps[j];
    gnd_ymin =  (int)(roundf(r->y1 * scale));
    gnd_ymax =  (int)(roundf(r->y2 * scale));
    gnd_xmin =  (int)(roundf(r->x1 * scale));

    /* If the first row is inverted then the ymin/ymax values need to be    */
    /* adjusted.							    */

    row = DefLowestRow();   /* Try this first */
    if (row) {
	if (row->orient & RS) {
	    gnd_ymax = corearea->siteh - gnd_ymax;
	    gnd_ymin = corearea->siteh - gnd_ymin;
	    vdd_ymax = corearea->siteh - vdd_ymax;
	    vdd_ymin = corearea->siteh - vdd_ymin;

	    tmp = gnd_ymax;
	    gnd_ymax = gnd_ymin;
	    gnd_ymin = tmp; 

	    tmp = vdd_ymax;
	    vdd_ymax = vdd_ymin;
	    vdd_ymin = tmp; 
	}
    }
    else {
	if (corearea->orient & RS) {
	    gnd_ymax = corearea->siteh - gnd_ymax;
	    gnd_ymin = corearea->siteh - gnd_ymin;
	    vdd_ymax = corearea->siteh - vdd_ymax;
	    vdd_ymin = corearea->siteh - vdd_ymin;

	    tmp = gnd_ymax;
	    gnd_ymax = gnd_ymin;
	    gnd_ymin = tmp; 

	    tmp = vdd_ymax;
	    vdd_ymax = vdd_ymin;
	    vdd_ymin = tmp; 
	}
    }

    n = strlen(pattern);

    /* Find the highest metal layer that is oriented vertically */

    ltop = LefGetMaxRouteLayer() - 1;
    if (LefGetRouteOrientation(ltop) == 1) ltop--;
    if (ltop < 3) {
	int mspace;

	fprintf(stderr, "Will not generate over-cell power stripes due to lack "
		    "of route layers\n");
	fprintf(stderr, "Generating comb structures instead.\n");

	/* Generate comb structures in metal1 on either side to connect	*/
	/* power and ground.						*/

	/* Get wide spacing rule relative to stripe width */
	mspace = (int)(roundf(LefGetRouteWideSpacing(lbot, 
			(float)(stripevals->width) / scale) * scale));

	/* Account for ground or power bus extending beyond the cell	*/
	/* bounding box.  Assumes that the extension is symmetric on	*/
	/* left and right, and the same for power and ground.		*/

	if (gnd_xmin < 0) mspace -= gnd_xmin;
	else if (vdd_xmin < 0) mspace -= vdd_xmin;
	corew = corearea->sitew;

	/* Generate power comb on left */

        prail = (PSTRIPE)malloc(sizeof(struct _powerstripe));
	prail->next = rails;
	rails = prail;
	
	prail->offset = -mspace - stripevals->width / 2;
	prail->pitch = corearea->urx;
	prail->num = 1;
	if ((n < 1) || (pattern[0] == 'P')) {
	    prail->name = VddNet;
	    y = corearea->lly + (vdd_ymax + vdd_ymin) / 2;
	    hh = (vdd_ymax - vdd_ymin) / 2;
	}
	else {
	    prail->name = GndNet;
	    y = corearea->lly + (gnd_ymax + gnd_ymin) / 2;
	    hh = (gnd_ymax - gnd_ymin) / 2;
	}

	prail->stripe = (DSEG)malloc(sizeof(struct dseg_));
	prail->stripe->layer = lbot;
	prail->stripe->next = NULL;
	prail->stripe->x1 = -(float)(stripevals->width / 2) / scale;
	prail->stripe->x2 = (float)(stripevals->width / 2) / scale;
	prail->stripe->y1 = (float)(corearea->lly - hh) / scale;
	prail->stripe->y2 = (float)(corearea->ury + hh) / scale;
	prail->posts = NULL;

	/* Create all posts */

	for (; y <= corearea->ury; y += 2 * corearea->siteh) { 
	    PPOST newpost = (PPOST)malloc(sizeof(powerpost));
	    newpost->strut = (DSEG)malloc(sizeof(struct dseg_));
	    newpost->viagen = NULL;
	    newpost->strut->layer = lbot;
	    newpost->strut->next = NULL;
	    newpost->strut->x1 = 0.0;
	    newpost->strut->x2 = (float)(-prail->offset + corew) / scale;
	    newpost->strut->y1 = (float)(y - hh) / scale;
	    newpost->strut->y2 = (float)(y + hh) / scale;
	    newpost->next = prail->posts;
	    prail->posts = newpost;
	}
	prail->offset += corearea->llx;

	/* Generate ground comb on right */

        prail = (PSTRIPE)malloc(sizeof(struct _powerstripe));
	prail->next = rails;
	rails = prail;
	
	prail->offset = mspace + stripevals->width / 2;
	prail->pitch = corearea->urx;
	prail->num = 1;
	if ((n < 2) || (pattern[1] == 'G')) {
	    prail->name = GndNet;
	    y = corearea->lly + (gnd_ymax + gnd_ymin) / 2;
	    hh = (gnd_ymax - gnd_ymin) / 2;
	}
	else {
	    prail->name = VddNet;
	    y = corearea->lly + (vdd_ymax + vdd_ymin) / 2;
	    hh = (vdd_ymax - vdd_ymin) / 2;
	}

	prail->stripe = (DSEG)malloc(sizeof(struct dseg_));
	prail->stripe->layer = lbot;
	prail->stripe->next = NULL;
	prail->stripe->x1 = -(float)(stripevals->width / 2) / scale;
	prail->stripe->x2 = (float)(stripevals->width / 2) / scale;
	prail->stripe->y1 = (float)(corearea->lly - hh) / scale;
	prail->stripe->y2 = (float)(corearea->ury + hh) / scale;
	prail->posts = NULL;

	/* Create all posts */

	for (; y <= corearea->ury; y += 2 * corearea->siteh) { 
	    PPOST newpost = (PPOST)malloc(sizeof(powerpost));
	    newpost->strut = (DSEG)malloc(sizeof(struct dseg_));
	    newpost->viagen = NULL;
	    newpost->strut->layer = lbot;
	    newpost->strut->next = NULL;
	    newpost->strut->x1 = (float)(-prail->offset - corew) / scale;
	    newpost->strut->x2 = 0.0;
	    newpost->strut->y1 = (float)(y - hh) / scale;
	    newpost->strut->y2 = (float)(y + hh) / scale;
	    newpost->next = prail->posts;
	    prail->posts = newpost;
	}
	prail->offset += corearea->urx;
	return rails;
    }

    /* Generate vias for posts */
    /* NOTE:  This assumes that the power and ground rails are the same	    */
    /* height.  If not, need to have two vialist records, one for each rail */

    vialist = (LefList *)malloc((ltop - lbot) * sizeof(LefList));
    for (l = lbot; l < ltop; l++) vialist[l] = (LefList)NULL;

    /* First find any VIARULE GENERATE vias;  these are preferred, as they  */
    /* have all the right information for generating a new via for the	    */
    /* post.								    */

    for (l = lbot; l < ltop; l++) {
	for (lefl = LefInfo; lefl; lefl = lefl->next) {
	    if (lefl->lefClass == CLASS_VIA) {
		if (lefl->info.via.generated) {

		    /* Check for top and bottom layers matching (l) and (l + 1) */

		    if ((cutlayer = check_valid_via(lefl, l)) != -1) {
			vialist[l] = LefNewVia(NULL);
			via_make_generated(vialist[l], lefl, l, cutlayer,
				stripevals->width, gnd_ymax - gnd_ymin, scale);
			break;		// Continue to next layer
		    }
		}
	    }
	}
    }

    /* Next find any VIAs that have the right layers.  Use this information */
    /* to create a new VIA record with the correct size for the post.	    */

    for (l = lbot; l < ltop; l++) {
	if (vialist[l] == NULL) {
	    vvalid = NULL;
	    for (lefl = LefInfo; lefl; lefl = lefl->next) {
		if (lefl->lefClass == CLASS_VIA) {
		    if ((lcut = check_valid_via(lefl, l)) != -1) {
			/* Don't include vias that this routine has created, */
			if (strstr(lefl->lefName, "_post") != NULL) continue;
			if (vvalid == NULL) {
			    vvalid = lefl;
			    vw = LefGetViaWidth(lefl, lcut, 0);
			    vh = LefGetViaWidth(lefl, lcut, 1);
			    cutlayer = lcut;
			}
			/* Find the smallest valid via.  Note that it	*/
			/* is preferred to find a via designed for a	*/
			/* power post, if there is one (to be done).	*/
			else {
			    double tw, th;
			    tw = LefGetViaWidth(lefl, lcut, 0);
			    th = LefGetViaWidth(lefl, lcut, 1);
			    if ((th < vh) || ((th == vh) && (tw < vw))) {
				vvalid = lefl;
				vw = tw;
				vh = th;
				cutlayer = lcut;
			    }
			}
		    }
		}
	    }
	    if (vvalid) {
		vialist[l] = LefNewVia(NULL);
		via_make_generated(vialist[l], vvalid, l, cutlayer,
			stripevals->width, gnd_ymax - gnd_ymin, scale);
	    }
	}
    }

    for (l = lbot; l < ltop; l++) {
	if (vialist[l] == NULL) {
	    LefList ll0, ll1;
	    ll0 = LefFindLayerByNum(l);
	    ll1 = LefFindLayerByNum(l + 1);
	    fprintf(stderr, "Error:  Failed to find a valid via record between "
			"metal layers %s and %s\n",
			ll0->lefName, ll1->lefName);
	    return NULL;
	}
    }

    /* Construct power stripe records */

    for (p = 0; p < n; p++) {

        prail = (PSTRIPE)malloc(sizeof(struct _powerstripe));
	prail->next = rails;
	rails = prail;
 
	prail->offset = stripevals->offset + p * stripevals->pitch;
	prail->pitch = stripevals->pitch * n;
	prail->num = 1 + (corearea->urx - prail->offset) / prail->pitch;

	/* Note this is not strdup(), so can compare string pointers */
	prail->name = (pattern[p] == 'P') ? VddNet : GndNet;

	/* Find vertical dimensions of power rails on the standard cells */
	y = corearea->lly;
	if (pattern[p] == 'P') {
	    y += (vdd_ymax + vdd_ymin) / 2;
	    hh = (vdd_ymax - vdd_ymin) / 2;
	}
	else {
	    y += (gnd_ymax + gnd_ymin) / 2;
	    hh = (gnd_ymax - gnd_ymin) / 2;
	}

	/* Query the extent of the highest via layer and make sure the	*/
	/* power stripe covers it completely.				*/

	topvia = vialist[ltop - 1];
	syb = topvia->info.via.area.y1;
	syt = topvia->info.via.area.y2;
	for (lr = topvia->info.via.lr; lr; lr = lr->next) {
	    if (lr->y1 < syb) syb = lr->y1;
	    if (lr->y2 > syt) syt = lr->y2;
	}

	/* First two rails, extend down by one track pitch.  A pin will	*/
	/* be added here.						*/
	if (p < 2) {
	    syb = -LefGetRoutePitch(ltop - 1);
	    if (syb > topvia->info.via.area.y1)
		syb -= LefGetRoutePitch(ltop - 1);
	}

	prail->stripe = (DSEG)malloc(sizeof(struct dseg_));
	prail->stripe->layer = ltop;
	prail->stripe->next = NULL;
	prail->stripe->x1 = -(float)(stripevals->width / 2) / scale;
	prail->stripe->x2 = (float)(stripevals->width / 2) / scale;
	prail->stripe->y1 = syb + ((float)corearea->lly / scale);
	prail->stripe->y2 = syt + ((float)corearea->ury / scale);
	prail->posts = NULL;

	/* Create all posts (also horizontally centered at X = 0) */

	/* To be done:  Check if rows are alternating N and S	*/
	/* or not.  This code assumes that they always are.	*/

	for (; y <= corearea->ury; y += 2 * corearea->siteh) { 
	    for (l = lbot; l < ltop; l++) {
		PPOST newpost = (PPOST)malloc(sizeof(powerpost));
		newpost->strut = (DSEG)malloc(sizeof(struct dseg_));
		newpost->viagen = vialist[l];
		newpost->strut->layer = l;
		newpost->strut->next = NULL;
		newpost->strut->x1 = -(float)(stripevals->width / 2) / scale;
		newpost->strut->x2 = (float)(stripevals->width / 2) / scale;
		newpost->strut->y1 = (float)(y - hh) / scale;
		newpost->strut->y2 = (float)(y + hh) / scale;
		newpost->next = prail->posts;
		prail->posts = newpost;
	    }
	}
    }

    /* link via records and prepend to LefInfo */
    for (l = lbot; l < ltop - 1; l++)
	vialist[l]->next = vialist[l + 1];
    
    vialist[l]->next = LefInfo;
    LefInfo = vialist[0];

    free(vialist);
    return rails;
}

/*--------------------------------------------------------------*/
/* Convert orient bitfield from GATE structure to character	*/
/* string for a DEF file.					*/
/*--------------------------------------------------------------*/

char *gate_to_orient(int orient)
{
    int oidx;
    static char *orients[8] = {"N", "S", "E", "W", "FN", "FS", "FE", "FW"};

    switch (orient & (RN | RS | RE | RW)) {
	case RS:
	    oidx = 1;
	    break;
	case RE:
	    oidx = 2;
	    break;
	case RW:
	    oidx = 3;
	    break;
	default:
	    oidx = 0;
	    break;
    }
    if (orient & RF) oidx += 4;
    return orients[oidx];
}
	    
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void
output_rail(FILE *outfptr, PSTRIPE rail, int x, int first, float scale)
{
    PPOST post;
    LefList lefl;
    char *otyp;
    float fyd, fya, fxd, fxa;
    int iyd, iya, ixd, ixa;

    static char *otypes[] = {"+ FIXED", "  NEW"};

    otyp = (first) ? otypes[0] : otypes[1];

    for (post = rail->posts; post; post = post->next) {
	lefl = LefFindLayerByNum(post->strut->layer);
	fyd = post->strut->y2 - post->strut->y1;
	fya = (post->strut->y2 + post->strut->y1) / 2;
	iyd =  (int)(roundf(fyd * scale));
	iya =  (int)(roundf(fya * scale));
	if (post->viagen) {
	    fprintf(outfptr, "\n%s %s %d ( %d %d ) ( * * ) %s",
		    otyp, lefl->lefName, iyd, x, iya, post->viagen->lefName);
	}
	else {
	    fxd = post->strut->x1;
	    ixd = x + (int)(roundf(fxd * scale));
	    fxa = post->strut->x2;
	    ixa = x + (int)(roundf(fxa * scale));
	    fprintf(outfptr, "\n%s %s %d ( %d %d ) ( %d * )",
		    otyp, lefl->lefName, iyd, ixd, iya, ixa);
	}	
	otyp = otypes[1];
    }
    lefl = LefFindLayerByNum(rail->stripe->layer);
    fxd = rail->stripe->x2 - rail->stripe->x1;
    fya = rail->stripe->y1;
    fyd = rail->stripe->y2;
    ixd =  (int)(roundf(fxd * scale));
    iya =  (int)(roundf(fya * scale));
    iyd =  (int)(roundf(fyd * scale));
    fprintf(outfptr, "\n%s %s %d ( %d %d ) ( * %d )",
	    otyp, lefl->lefName, ixd, x, iya, iyd);
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void
output_rails(FILE *outfptr, PSTRIPE rail, COREBBOX corearea, float scale, int first)
{
    int i, x;

    x = rail->offset;
    for (i = 0; i < rail->num; i++) {
	output_rail(outfptr, rail, x, first, scale);
	first = FALSE;
	x += rail->pitch;
	if (x > corearea->urx) break;
    }
}

/*--------------------------------------------------------------*/
/* write_output ---						*/
/*								*/
/*  write the modified DEF file to the output.			*/
/*--------------------------------------------------------------*/

void
write_output(char *definname, char *defoutname, float scale,
	COREBBOX corearea, SINFO stripevals, PSTRIPE rails,
	char *VddNet, char *GndNet, unsigned char Flags)
{
    FILE *outfptr, *infptr;
    static char line[LEF_LINE_MAX + 2];
    char *sptr;
    int i, copyspecial = 0, numVias, foundrail[2], ltop;
    double lh, ly;

    GATE gate, endgate;
    NET net;
    NODE node;
    PPOST post;
    LefList lefl, lname, lrec;
    DSEG seg;
    PSTRIPE rail;

    if (defoutname == NULL)
	outfptr = stdout;
    else {
	outfptr = fopen(defoutname, "w");
	if (outfptr == NULL) {
	    fprintf(stderr, "Error:  Failed to open file %s for writing modified output\n",
		    defoutname);
	    return;
	}
    }

    if (Flags & VERBOSE) fprintf(stdout, "Writing DEF file output.\n");

    /* Find the number of (new) power rail SPECIALNETS to be written	*/
    /* There will normally be one record for power and one for ground	*/
    /* unless rails were not written.  Power and ground rails are	*/
    /* checked separately for consistency of the output DEF.		*/

    foundrail[0] = foundrail[1] = FALSE;
    for (rail = rails; rail; rail = rail->next) {
	if ((foundrail[0] == FALSE) && (rail->name == VddNet)) foundrail[0] = TRUE;
	if ((foundrail[1] == FALSE) && (rail->name == GndNet)) foundrail[1] = TRUE;
    }
    numSpecial = ((foundrail[0] == TRUE) ? 1 : 0) + ((foundrail[1] == TRUE) ? 1 : 0);

    /* Write DEF header (copy input DEF file verbatim up to COMPONENTS) */

    infptr = fopen(definname, "r");
    while (1) {
	if (fgets(line, LEF_LINE_MAX + 1, infptr) == NULL) {
	    fprintf(stderr, "Error:  End of file reached before COMPONENTS.\n");
	    return;
	}
	sptr = line;
	while (isspace(*sptr)) sptr++;
	
	/* Assuming a typical DEF file here. . . */
	if (!strncmp(sptr, "COMPONENTS", 10)) break;

	/* Rewrite DIEAREA, ROWS, and TRACKS */
	else if (!strncmp(sptr, "DIEAREA", 7)) {
	    char *dptr;
	    int dllx, dlly, durx, dury;

	    dptr = strchr(line, '(');
	    sscanf(dptr + 1, "%d %d", &dllx, &dlly);
	    dptr = strchr(dptr + 1, '(');
	    sscanf(dptr + 1, "%d %d", &durx, &dury);

	    durx += stripevals->stretch;

	    fprintf(outfptr, "DIEAREA ( %d %d ) ( %d %d ) ;\n",
			dllx, dlly, durx, dury);
	}

	else if (!strncmp(sptr, "ROW", 3)) {
	    char *dptr;
	    int radd, xnum, ridx, rowy;
	    ROW row;
	    char namepos[16];
	    radd = stripevals->stretch / corearea->sitew;
	    static char *orientations[] = {"N", "S", "E", "W", "FN", "FS", "FE", "FW"};

	    dptr = sptr;
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%d", &rowy);
	    row = DefFindRow(rowy);

	    xnum = row->xnum + radd;
	    switch (row->orient & (RN | RS | RE | RW)) {
		case RS:
		    ridx = 1;
		    break;
		case RE:
		    ridx = 2;
		    break;
		case RW:
		    ridx = 3;
		    break;
		default:
		    ridx = 0;
	    }
	    if (row->orient & RF) ridx += 4;

	    fprintf(outfptr, "ROW %s %s %d %d %s DO %d BY %d STEP %d %d ;\n",
		    row->rowname, row->sitename, row->x, row->y, 
		    orientations[ridx], xnum, row->ynum,
		    row->xstep, row->ystep);
	}
	else if (!strncmp(sptr, "TRACKS", 6)) {
	    char *dptr;
	    char o;
	    char layer[64];
	    int roffset, rnum, rpitch;

	    dptr = sptr + 6;
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%c", &o);
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%d", &roffset);
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%d", &rnum);
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%d", &rpitch);
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    while (!isspace(*dptr)) dptr++; 
	    while (isspace(*dptr)) dptr++; 
	    sscanf(dptr, "%s", layer);

	    if (o == 'X') {
		rnum += (int)(stripevals->stretch / rpitch);
		if (stripevals->stretch % rpitch != 0) rnum++;
	    }
	    fprintf(outfptr, "TRACKS %c %d DO %d STEP %d LAYER %s ;\n",
		    o, roffset, rnum, rpitch, layer);
	}
	else
	    fprintf(outfptr, "%s", line);
    }

    /* Write generated vias for posts */

    numVias = 0;
    for (lefl = LefInfo; lefl; lefl = lefl->next)
	if (strstr(lefl->lefName, "_post") != NULL)
	    numVias++;

    fprintf(outfptr, "VIAS %d ;\n", numVias);
    for (lefl = LefInfo; lefl; lefl = lefl->next) {
	int llx, lly, urx, ury;

	if (strstr(lefl->lefName, "_post") != NULL) {
	    fprintf(outfptr, "- %s\n", lefl->lefName);
	    lname = LefFindLayerByNum(lefl->info.via.area.layer);
	    llx =  (int)(roundf(lefl->info.via.area.x1 * scale));
	    lly =  (int)(roundf(lefl->info.via.area.y1 * scale));
	    urx =  (int)(roundf(lefl->info.via.area.x2 * scale));
	    ury =  (int)(roundf(lefl->info.via.area.y2 * scale));
	    fprintf(outfptr, "+ RECT %s ( %d %d ) ( %d %d )",
		    lname->lefName, llx, lly, urx, ury);
	    if (lefl->info.via.lr) fprintf(outfptr, "\n");
	    for (seg = lefl->info.via.lr; seg; seg = seg->next) {
		lname = LefFindLayerByNum(seg->layer);
		llx =  (int)(roundf(seg->x1 * scale));
		lly =  (int)(roundf(seg->y1 * scale));
		urx =  (int)(roundf(seg->x2 * scale));
		ury =  (int)(roundf(seg->y2 * scale));
		fprintf(outfptr, "+ RECT %s ( %d %d ) ( %d %d )",
			    lname->lefName, llx, lly, urx, ury);
		if (seg->next) fprintf(outfptr, "\n");
	    }
	    fprintf(outfptr, " ;\n");
	}
    }
    fprintf(outfptr, "END VIAS\n\n");

    for (endgate = Nlgates; endgate->next; endgate = endgate->next);

    if (Numgates > 0) {

	/* Write instances (COMPONENTS) in the order read */
	fprintf(outfptr, "COMPONENTS %d ;\n", Numgates);
	for (gate = endgate; gate ; gate = gate->last) {
	    int px, py;
	    if (gate->gatetype == NULL) continue;

	    px =  (int)(roundf(gate->placedX * scale));
	    py =  (int)(roundf(gate->placedY * scale));
	    fprintf(outfptr, "- %s %s + PLACED ( %d %d ) %s ;\n",
		gate->gatename, gate->gatetype->gatename,
		px, py, gate_to_orient(gate->orient));
	}
	fprintf(outfptr, "END COMPONENTS\n\n");
    }

    if (Numpins > 0) {
	int llx, lly, urx, ury, px, py;
        static char *pin_classes[] = {
	    "DEFAULT", "INPUT", "OUTPUT", "OUTPUT TRISTATE", "INOUT", "FEEDTHRU"
	};
	LefList lefl;

	/* Write instances (PINS) in the order read, plus power pins */

	fprintf(outfptr, "PINS %d ;\n", Numpins + numSpecial);

	if (foundrail[0]) {
	    for (rail = rails; rail; rail = rail->next)
		if (rail->name == VddNet) break;

	    ltop = rail->stripe->layer;
	    lrec = LefFindLayerByNum(ltop);
	    lh = LefGetRoutePitch(ltop - 1) / 4;
	    ly = rail->stripe->y1 + lh;

	    /* NOTE: The over-simplified "arrangepins" Tcl script expects   */
	    /* LAYER and PLACED records to be on separate lines.  This will */
	    /* eventually be replaced by a C coded executable with a more   */
	    /* rigorous parser, like addspacers uses.			    */

	    fprintf(outfptr, "- %s + NET %s + DIRECTION INOUT\n", VddNet, VddNet);
	    fprintf(outfptr, "  + LAYER %s ( %d %d ) ( %d %d )\n",
		    lrec->lefName,
		    (int)(roundf(rail->stripe->x1 * scale)),
		    (int)(roundf(-lh * scale)),
		    (int)(roundf(rail->stripe->x2 * scale)),
		    (int)(roundf(lh * scale)));
	    fprintf(outfptr, "  + PLACED ( %d %d ) N ;\n",
		    rail->offset, (int)(roundf(ly * scale)));
	}
	if (foundrail[1]) {
	    for (rail = rails; rail; rail = rail->next)
		if (rail->name == GndNet) break;

	    ltop = rail->stripe->layer;
	    lrec = LefFindLayerByNum(ltop);
	    lh = LefGetRoutePitch(ltop - 1) / 4;
	    ly = rail->stripe->y1 + lh;

	    fprintf(outfptr, "- %s + NET %s + DIRECTION INOUT\n", GndNet, GndNet);
	    fprintf(outfptr, "  + LAYER %s ( %d %d ) ( %d %d )\n",
		    lrec->lefName,
		    (int)(roundf(rail->stripe->x1 * scale)),
		    (int)(roundf(-lh * scale)),
		    (int)(roundf(rail->stripe->x2 * scale)),
		    (int)(roundf(lh * scale)));
	    fprintf(outfptr, "  + PLACED ( %d %d ) N ;\n",
		    rail->offset, (int)(roundf(ly * scale)));
	}

	for (gate = endgate; gate ; gate = gate->last) {
	    int dir;

	    if (gate->gatetype != NULL) continue;

	    fprintf(outfptr, "- %s + NET %s",
		    gate->gatename, gate->node[0]);

	    if (gate->direction[0] != 0)
		fprintf(outfptr, " + DIRECTION %s",
			pin_classes[gate->direction[0]]);

	    fprintf(outfptr, "\n");

	    lefl = LefFindLayerByNum(gate->taps[0]->layer);
	    urx = (int)(roundf((gate->taps[0]->x2 - gate->taps[0]->x1) * scale) / 2.0);
	    ury = (int)(roundf((gate->taps[0]->y2 - gate->taps[0]->y1) * scale) / 2.0);
	    llx = -urx;
	    lly = -ury;
	    px =  (int)(roundf(gate->placedX * scale));
	    py =  (int)(roundf(gate->placedY * scale));

	    fprintf(outfptr, "  + LAYER %s ( %d %d ) ( %d %d )\n",
			lefl->lefName, llx, lly, urx, ury);
	    fprintf(outfptr, "  + PLACED ( %d %d ) %s ;\n",
			px, py, gate_to_orient(gate->orient));
	}
	fprintf(outfptr, "END PINS\n\n");
    }

    while (1) {
	if (fgets(line, LEF_LINE_MAX + 1, infptr) == NULL) {
	    fprintf(stderr, "Error:  End of file reached before NETS.\n");
	    return;
	}
	sptr = line;
	while (isspace(*sptr)) sptr++;
	
	/* Assuming a typical DEF file here. . . */
	if (!strncmp(sptr, "NETS", 4)) break;

    }
    fprintf(outfptr, "%s", line);
    while (1) {
	if (fgets(line, LEF_LINE_MAX + 1, infptr) == NULL) {
	    fprintf(stderr, "Error:  End of file reached before END NETS.\n");
	    return;
	}
	sptr = line;
	while (isspace(*sptr)) sptr++;

	/* Assuming a typical DEF file here. . . */
	if (!strncmp(sptr, "SPECIALNETS", 11)) {
	    sscanf(sptr + 11, "%d", &copyspecial);
	    break;
	}
	else if (!strncmp(sptr, "END DESIGN", 10)) {
	    break;
	}
	fprintf(outfptr, "%s", line);
    }

    /* Rewrite SPECIALNETS line with updated number */

    if (copyspecial + numSpecial > 0)
	fprintf(outfptr, "SPECIALNETS %d ;\n", numSpecial + copyspecial);

    if (numSpecial > 0) {
	/* Write power bus stripes (SPECIALNETS) */
	int i, first = TRUE;
	char *railnames[2] = {GndNet, VddNet};

	for (i = 0; i < 2; i++) {
	    fprintf(outfptr, "- %s", railnames[i]);
	    for (rail = rails; rail; rail = rail->next) {
		if (rail->name == railnames[i]) {
		    output_rails(outfptr, rail, corearea, scale, first);
		    first = FALSE;
		}
	    }
	    fprintf(outfptr, " ;\n");
	    first = TRUE;
	}
    }

    /* If there were previously no SPECIALNETS then add the ending line */
    if (numSpecial > 0 && copyspecial == 0)
	fprintf(outfptr, "END SPECIALNETS\n\n");

    /* Copy the remainder of the file verbatim */

    while (1) {
	if (fgets(line, LEF_LINE_MAX + 1, infptr) == NULL) break;
	sptr = line;
	while (isspace(*sptr)) sptr++;
	
	if (!strncmp(sptr, "END DESIGN", 10)) {
	    break;
	}
	fprintf(outfptr, "%s", line);
    }
    fprintf(outfptr, "END DESIGN\n");
    fclose(infptr);

    if (defoutname != NULL) fclose(outfptr);
    fflush(stdout);
}

/*--------------------------------------------------------------*/
/* helpmessage - tell user how to use the program               */
/*                                                              */
/*--------------------------------------------------------------*/

void helpmessage(FILE *outf)
{
    fprintf(outf, "addspacers [-options] <netlist>\n");
    fprintf(outf, "\n");
    fprintf(outf, "addspacers adds fill cells and power buses to a layout.\n");
    fprintf(outf, "Output on stdout unless redirected with -o option.\n");
    fprintf(outf, "\n");
    fprintf(outf, "options:\n");
    fprintf(outf, "  -o <path>  Output file path and name\n");
    fprintf(outf, "  -l <path>  Path to standard cell LEF file (for macro list)\n");
    fprintf(outf, "  -p <name>  Name of power net\n");
    fprintf(outf, "  -g <name>  Name of ground net\n");
    fprintf(outf, "  -f <name>  Name of fill cell (or prefix)\n");
    fprintf(outf, "  -w <width> Power bus stripe width\n");
    fprintf(outf, "  -P <pitch> Power bus stripe pitch\n");
    fprintf(outf, "  -s <pattern> Power bus stripe pattern (default \"PG\") \n");
    fprintf(outf, "  -n		Do not stretch layout under power buses.\n");
    fprintf(outf, "  -O		Handle obstruction areas in separate .obs file\n");
    fprintf(outf, "\n");
    fprintf(outf, "  -v		Verbose output\n");
    fprintf(outf, "  -h         Print this message\n");

} /* helpmessage() */

