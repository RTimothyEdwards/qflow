/*
 * readdef.c --      
 *
 * This module incorporates the LEF/DEF format for standard-cell place and
 * route.
 *
 * Version 0.1 (September 26, 2003):  DEF input of designs.
 *
 * Written by Tim Edwards, Open Circuit Design
 * Modified April 2013 for use with qrouter
 * Modified December 2018 for use with qflow (DEF2Verilog, back-annotate verilog
 * netlist from DEF output).
 *
 * It is assumed that the LEF files have been read in prior to this, and
 * layer information is already known.  The DEF file should have information
 * primarily on die are, track placement, pins, components, and nets.
 *
 * Routed nets have their routes dropped into track obstructions, and the
 * nets are ignored.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>		/* for roundf() function, if std=c99 */

#include "readlef.h"
#include "readdef.h"
#include "hash.h"

int numSpecial = 0;		/* Tracks number of specialnets */
int Numnets = 0;		/* Total nets, including special nets */
int Numgates = 0;		/* Number of components */
int Numpins = 0;		/* Number of pins */

/* These hash tables speed up DEF file reading */

struct hashtable InstanceTable;
struct hashtable NetTable;
struct hashtable RowTable;
char *DEFDesignName;

DSEG UserObs = NULL;
double Xlowerbound = 0, Xupperbound = 0, Ylowerbound = 0, Yupperbound = 0;
double PitchX = 1.0, PitchY = 1.0;
NET *Nlnets = NULL;
GATE Nlgates = NULL;
GATE PinMacro;

/*--------------------------------------------------------------*/
/* Cell macro lookup based on the hash table			*/
/*--------------------------------------------------------------*/

static void
DefHashInit(void)
{
   /* Initialize the macro hash table */

   InitializeHashTable(&InstanceTable, LARGEHASHSIZE);
   InitializeHashTable(&NetTable, LARGEHASHSIZE);
   InitializeHashTable(&RowTable, TINYHASHSIZE);
}

GATE
DefFindGate(char *name)
{
    GATE ginst;

    ginst = (GATE)HashLookup(name, &InstanceTable);
    return ginst;
}

ROW
DefFindRow(int yval)
{
    ROW row;
    char namepos[32];

    sprintf(namepos, "%d", yval);
    row = (ROW)HashLookup(namepos, &RowTable);
    return row;
}

/*--------------------------------------------------------------*/
/* Find the lowest row and return the row record.		*/
/*--------------------------------------------------------------*/

struct nlist *rowfindlowest(struct hashlist *p, void *clientdata)
{
    ROW row = (ROW)(p->ptr);
    ROW *lrow = (ROW *)clientdata;

    if ((*lrow == NULL) || (row->y < (*lrow)->y))
	*lrow = row;

    return NULL;
}

/*--------------------------------------------------------------*/

ROW
DefLowestRow()
{
    ROW row = NULL;

    RecurseHashTablePointer(&RowTable, rowfindlowest, (void *)(&row));

    return row;
}

/*--------------------------------------------------------------*/

NET
DefFindNet(char *name)
{
    NET net;

    // Guard against calls to find nets before DEF file is read
    if (Numnets == 0) return NULL;

    net = (NET)HashLookup(name, &NetTable);
    return net;
}

/*--------------------------------------------------------------*/
/* Cell macro hash table generation				*/
/* Given an instance record, create an entry in the hash table	*/
/* for the instance name, with the record entry pointing to the	*/
/* instance record.						*/
/*--------------------------------------------------------------*/

static void
DefHashInstance(GATE gateginfo)
{
    HashPtrInstall(gateginfo->gatename, gateginfo, &InstanceTable);
}

/*--------------------------------------------------------------*/

char *
DefDesign()
{
    return DEFDesignName;
}

/*--------------------------------------------------------------*/
/* Net hash table generation					*/
/* Given a net record, create an entry in the hash table for	*/
/* the net name, with the record entry pointing to the net	*/
/* record.							*/
/*--------------------------------------------------------------*/

static void
DefHashNet(NET net)
{
    HashPtrInstall(net->netname, net, &NetTable);
}

/*
 *------------------------------------------------------------
 *
 * DefAddRoutes --
 *
 *	Parse a network route statement from the DEF file.
 *	If "special" is 1, then, add the geometry to the
 *	list of obstructions.  If "special" is 0, then read
 *	the geometry into a route structure for the net.
 *
 * Results:
 *	Returns the last token encountered.
 *
 * Side Effects:
 *	Reads from input stream;
 *	Adds information to the layout database.
 *
 *------------------------------------------------------------
 */

static char *
DefAddRoutes(FILE *f, float oscale, NET net, char special)
{
    char *token;
    DSEG lr, drect;
    struct point_ refp;
    char valid = FALSE;		/* is there a valid reference point? */
    char noobstruct;
    char initial = TRUE;
    struct dseg_ locarea;
    double x, y, lx, ly, w, hw, s;
    int routeLayer = -1, paintLayer;
    LefList lefl;

    refp.x1 = 0;
    refp.y1 = 0;

    /* Don't create obstructions or routes on routed specialnets inputs	*/
    noobstruct = (special == (char)1) ? TRUE : FALSE;

    while (initial || (token = LefNextToken(f, TRUE)) != NULL)
    {
	/* Get next point, token "NEW", or via name */
	if (initial || !strcmp(token, "NEW") || !strcmp(token, "new"))
	{
	    /* initial pass is like a NEW record, but has no NEW keyword */
	    initial = FALSE;

	    /* invalidate reference point */
	    valid = FALSE;

	    token = LefNextToken(f, TRUE);
	    routeLayer = LefFindLayerNum(token);

	    if (routeLayer < 0)
	    {
		LefError(DEF_ERROR, "Unknown layer type \"%s\" for NEW route\n", token); 
		continue;
	    }
	    paintLayer = routeLayer;

	    if (special == (char)1)
	    {
		/* SPECIALNETS has the additional width */
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%lg", &w) != 1)
		{
		    LefError(DEF_ERROR, "Bad width in special net\n");
		    continue;
		}
		if (w != 0)
		    w /= oscale;
		else
		    w = LefGetRouteWidth(paintLayer); 
	    }
	    else
		w = LefGetRouteWidth(paintLayer); 

	}
	else if (*token != '(')	/* via name */
	{
	    /* A '+' or ';' record ends the route */
	    if (*token == ';' || *token == '+')
		break;

	    else if (valid == FALSE)
	    {
		LefError(DEF_ERROR, "Route has via name \"%s\" but no points!\n", token);
		continue;
	    }
	    lefl = LefFindLayer(token);
	    if (lefl != NULL)
	    {
		/* The area to paint is derived from the via definitions. */

		if (lefl != NULL)
		{
		    if (lefl->lefClass == CLASS_VIA) {

			// Note: layers may be defined in any order, metal or cut.
			// Check both via.area and via.lr layers, and reject those
			// that exceed the number of metal layers (those are cuts).

			paintLayer = 100;
			routeLayer = -1;
			routeLayer = lefl->info.via.area.layer;
			if (routeLayer < paintLayer) paintLayer = routeLayer;
			if ((routeLayer >= 0) && (special == (char)1) &&
					(valid == TRUE) && (noobstruct == FALSE)) {
			    s = LefGetRouteSpacing(routeLayer); 
			    drect = (DSEG)malloc(sizeof(struct dseg_));
			    drect->x1 = x + (lefl->info.via.area.x1 / 2.0) - s;
			    drect->x2 = x + (lefl->info.via.area.x2 / 2.0) + s;
			    drect->y1 = y + (lefl->info.via.area.y1 / 2.0) - s;
			    drect->y2 = y + (lefl->info.via.area.y2 / 2.0) + s;
			    drect->layer = routeLayer;
			    drect->next = UserObs;
			    UserObs = drect;
			}
			for (lr = lefl->info.via.lr; lr; lr = lr->next) {
			   routeLayer = lr->layer;
			   if (routeLayer < paintLayer) paintLayer = routeLayer;
			   if ((routeLayer >= 0) && (special == (char)1) &&
					(valid == TRUE) && (noobstruct == FALSE)) {
				s = LefGetRouteSpacing(routeLayer); 
				drect = (DSEG)malloc(sizeof(struct dseg_));
				drect->x1 = x + (lr->x1 / 2.0) - s;
				drect->x2 = x + (lr->x2 / 2.0) + s;
				drect->y1 = y + (lr->y1 / 2.0) - s;
				drect->y2 = y + (lr->y2 / 2.0) + s;
				drect->layer = routeLayer;
				drect->next = UserObs;
				UserObs = drect;
			   }
			}
			if (routeLayer == -1) paintLayer = lefl->type;
		    }
		    else {
		    	paintLayer = lefl->type;
			if (special == (char)1)
			    s = LefGetRouteSpacing(paintLayer); 
		    }
		}
		else
		{
		    LefError(DEF_ERROR, "Error: Via \"%s\" named but undefined.\n", token);
		    paintLayer = routeLayer;
		}
	    }
	    else
		LefError(DEF_ERROR, "Via name \"%s\" unknown in route.\n", token);
	}
	else
	{
	    /* Revert to the routing layer type, in case we painted a via */
	    paintLayer = routeLayer;

	    /* Record current reference point */
	    locarea.x1 = refp.x1;
	    locarea.y1 = refp.y1;
	    lx = x;
	    ly = y;

	    /* Read an (X Y) point */
	    token = LefNextToken(f, TRUE);	/* read X */
	    if (*token == '*')
	    {
		if (valid == FALSE)
		{
		    LefError(DEF_ERROR, "No reference point for \"*\" wildcard\n"); 
		    goto endCoord;
		}
	    }
	    else if (sscanf(token, "%lg", &x) == 1)
	    {
		x /= oscale;		// In microns
		/* Note: offsets and stubs are always less than half a pitch,	*/
		/* so round to the nearest integer grid point.			*/
		refp.x1 = (int)(0.5 + ((x - Xlowerbound) / PitchX));
	    }
	    else
	    {
		LefError(DEF_ERROR, "Cannot parse X coordinate.\n"); 
		goto endCoord;
	    }
	    token = LefNextToken(f, TRUE);	/* read Y */
	    if (*token == '*')
	    {
		if (valid == FALSE)
		{
		    LefError(DEF_ERROR, "No reference point for \"*\" wildcard\n"); 
		    goto endCoord;
		}
	    }
	    else if (sscanf(token, "%lg", &y) == 1)
	    {
		y /= oscale;		// In microns
		refp.y1 = (int)(0.5 + ((y - Ylowerbound) / PitchY));
	    }
	    else
	    {
		LefError(DEF_ERROR, "Cannot parse Y coordinate.\n"); 
		goto endCoord;
	    }

	    /* Indicate that we have a valid reference point */

	    if (valid == FALSE)
	    {
		valid = TRUE;
	    }
	    else if ((locarea.x1 != refp.x1) && (locarea.y1 != refp.y1))
	    {
		/* Skip over nonmanhattan segments, reset the reference	*/
		/* point, and output a warning.				*/

		LefError(DEF_ERROR, "Can't deal with nonmanhattan geometry in route.\n");
		locarea.x1 = refp.x1;
		locarea.y1 = refp.y1;
		lx = x;
		ly = y;
	    }
	    else
	    {
		locarea.x2 = refp.x1;
		locarea.y2 = refp.y1;

		if (special != (char)0) {
		   if ((valid == TRUE) && (noobstruct == FALSE)) {
		      s = LefGetRouteSpacing(routeLayer); 
		      hw = w / 2;
		      drect = (DSEG)malloc(sizeof(struct dseg_));
		      if (lx > x) {
		         drect->x1 = x - s;
		         drect->x2 = lx + s;
		      }
		      else if (lx < x) {
		         drect->x1 = lx - s;
		         drect->x2 = x + s;
		      }
		      else {
		         drect->x1 = x - hw - s;
		         drect->x2 = x + hw + s;
		      }
		      if (ly > y) {
		         drect->y1 = y - s;
		         drect->y2 = ly + s;
		      }
		      else if (ly < y) {
		         drect->y1 = ly - s;
		         drect->y2 = y + s;
		      }
		      else {
		         drect->y1 = y - hw - s;
		         drect->y2 = y + hw + s;
		      }
		      drect->layer = routeLayer;
		      drect->next = UserObs;
		      UserObs = drect;
		   }
		}
	    }

endCoord:
	    /* Find the closing parenthesis for the coordinate pair */
	    while (*token != ')')
		token = LefNextToken(f, TRUE);
	}
    }

    return token;	/* Pass back the last token found */
}

/*
 *------------------------------------------------------------
 *
 * DefReadGatePin ---
 *
 *	Given a gate name and a pin name in a net from the
 *	DEF file NETS section, find the position of the
 *	gate, then the position of the pin within the gate,
 *	and add pin and obstruction information to the grid
 *	network.
 *
 *------------------------------------------------------------
 */

static void
DefReadGatePin(NET net, NODE node, char *instname, char *pinname)
{
    int i;
    GATE gateginfo;
    DSEG drect;
    GATE g;
    double dx, dy;
    int gridx, gridy;
    DPOINT dp;

    g = DefFindGate(instname);
    if (g) {

	gateginfo = g->gatetype;

	if (!gateginfo) {
	    // Instances marked "<net>/pin have a NULL gatetype;  this is okay.
	    if (strcmp(pinname, "pin"))
		LefError(DEF_ERROR, "Endpoint %s/%s of net %s not found\n",
				instname, pinname, net->netname);
	    return;
	}
	for (i = 0; i < gateginfo->nodes; i++) {
	    if (!strcasecmp(gateginfo->node[i], pinname)) {
		node->taps = (DPOINT)NULL;
		node->extend = (DPOINT)NULL;

		for (drect = g->taps[i]; drect; drect = drect->next) {

		    // Add all routing gridpoints that fall inside
		    // the rectangle.  Much to do here:
		    // (1) routable area should extend 1/2 route width
		    // to each side, as spacing to obstructions allows.
		    // (2) terminals that are wide enough to route to
		    // but not centered on gridpoints should be marked
		    // in some way, and handled appropriately.

		    gridx = (int)((drect->x1 - Xlowerbound) / PitchX) - 1;

		    if (gridx < 0) gridx = 0;
		    while (1) {
			dx = (gridx * PitchX) + Xlowerbound;
			if (dx > drect->x2) break;
			if (dx < drect->x1) {
			    gridx++;
			    continue;
			}
			gridy = (int)((drect->y1 - Ylowerbound) / PitchY) - 1;

			if (gridy < 0) gridy = 0;
			while (1) {
			    dy = (gridy * PitchY) + Ylowerbound;
			    if (dy > drect->y2) break;
			    if (dy < drect->y1) {
				gridy++;
				continue;
			    }

			    // Routing grid point is an interior point
			    // of a gate port.  Record the position

			    dp = (DPOINT)malloc(sizeof(struct dpoint_));
			    dp->layer = drect->layer;
			    dp->x = dx;
			    dp->y = dy;
			    dp->gridx = gridx;
			    dp->gridy = gridy;

			    if ((dy >= drect->y1) &&
					(dx >= drect->x1) &&
					(dy <= drect->y2) &&
					(dx <= drect->x2)) {
				dp->next = node->taps;
				node->taps = dp;
			    }
			    else {
				dp->next = node->extend;
				node->extend = dp;
			    }
			    gridy++;
			}
			gridx++;
		    }
		}
		node->netnum = net->netnum;
		g->netnum[i] = net->netnum;
		g->noderec[i] = node;
		node->netname = net->netname;
		node->next = net->netnodes;
		net->netnodes = node;
		break;
	    }
	}
	if (i < gateginfo->nodes) return;
    }
}

/*
 *------------------------------------------------------------
 *
 * DefReadNets --
 *
 *	Read a NETS or SPECIALNETS section from a DEF file.
 *
 * Results:
 *	Return the total number of fixed or cover nets,
 *	excluding power and ground nets.  This gives the
 *	base number of nets to be copied verbatim from
 *	input to output (used only for SPECIALNETS, as
 *	regular nets are tracked with the NET_IGNORED flag).
 *
 * Side Effects:
 *	Many.  Networks are created, and geometry may be
 *	painted into the database top-level cell.
 *
 *------------------------------------------------------------
 */

enum def_net_keys {DEF_NET_START = 0, DEF_NET_END};
enum def_netprop_keys {
	DEF_NETPROP_USE = 0, DEF_NETPROP_ROUTED, DEF_NETPROP_FIXED,
	DEF_NETPROP_COVER, DEF_NETPROP_SHAPE, DEF_NETPROP_SOURCE,
	DEF_NETPROP_WEIGHT, DEF_NETPROP_PROPERTY};

static int
DefReadNets(FILE *f, char *sname, float oscale, char special, int total)
{
    char *token;
    int keyword, subkey;
    int i, processed = 0;
    int nodeidx;
    int fixed = 0;
    char instname[MAX_NAME_LEN], pinname[MAX_NAME_LEN];
    u_char is_new;

    NET net;
    int netidx;
    NODE node;

    static char *net_keys[] = {
	"-",
	"END",
	NULL
    };

    static char *net_property_keys[] = {
	"USE",
	"ROUTED",
	"FIXED",
	"COVER",
	"SHAPE",
	"SOURCE",
	"WEIGHT",
	"PROPERTY",
	NULL
    };

    if (Numnets == 0)
    {
	// Initialize net and node records
	netidx = 0;
	Nlnets = (NET *)malloc(total * sizeof(NET));
	for (i = 0; i < total; i++) Nlnets[i] = NULL;
    }
    else {
	netidx = Numnets;
	Nlnets = (NET *)realloc(Nlnets, (Numnets + total) * sizeof(NET));
	for (i = Numnets; i < (Numnets + total); i++) Nlnets[i] = NULL;
    }

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, net_keys);
	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in NET "
			"definition; ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}

	switch (keyword)
	{
	    case DEF_NET_START:

		/* Get net name */
		token = LefNextToken(f, TRUE);
		net = DefFindNet(token);

		if (net == NULL) {
		    net = (NET)malloc(sizeof(struct net_));
		    Nlnets[Numnets++] = net;
		    net->numnodes = 0;
		    net->netname = strdup(token);
		    net->netnodes = (NODE)NULL;
		    net->Flags = (special) ? NET_SPECIAL : 0;

		    /* Check for backslash-escape names modified by other tools */
		    /* (e.g., vlog2Cel) which replace the trailing space with a */
		    /* backslash, making the name verilog-incompatible.         */

		    if (*net->netname == '\\') {
			char *sptr, *bptr;
		        sptr = strchr(net->netname, ' ');
		        if (sptr == NULL) {
		            bptr = strrchr(net->netname + 1, '\\');
		            if (bptr != NULL) *bptr = ' ';
		        }
		    }

		    net->netnum = netidx++;
		    DefHashNet(net);

		    nodeidx = 0;
		    is_new = TRUE;
		}
		else {
		    nodeidx = net->numnodes;
		    is_new = FALSE;
		}

		/* Update the record of the number of nets processed	*/
		/* and spit out a message for every 5% finished.	*/

		processed++;

		/* Get next token;  will be '(' if this is a netlist	*/
		token = LefNextToken(f, TRUE);

		/* Process all properties */
		while (token && (*token != ';'))
		{
		    /* Find connections for the net */
		    if (*token == '(')
		    {
			token = LefNextToken(f, TRUE);  /* get pin or gate */
			strcpy(instname, token);

			/* Check for instname with sanitized backslash syntax */
			if (*instname == '\\') {
			    char *sptr, *bptr;
			    sptr = strchr(instname, ' ');
			    if (sptr == NULL) {
			        bptr = strrchr(instname + 1, '\\');
			        if (bptr != NULL) *bptr = ' ';
			    }
			}

			token = LefNextToken(f, TRUE);	/* get node name */

			if (!strcasecmp(instname, "pin")) {
			    strcpy(instname, token);
			    strcpy(pinname, "pin");
			}
			else
			    strcpy(pinname, token);

			node = (NODE)calloc(1, sizeof(struct node_));
			node->nodenum = nodeidx++;
			DefReadGatePin(net, node, instname, pinname);

			token = LefNextToken(f, TRUE);	/* should be ')' */

			continue;
		    }
		    else if (*token != '+')
		    {
			token = LefNextToken(f, TRUE);	/* Not a property */
			continue;	/* Ignore it, whatever it is */
		    }
		    else
			token = LefNextToken(f, TRUE);

		    subkey = Lookup(token, net_property_keys);
		    if (subkey < 0)
		    {
			LefError(DEF_WARNING, "Unknown net property \"%s\" in "
				"NET definition; ignoring.\n", token);
			continue;
		    }
		    switch (subkey)
		    {
			case DEF_NETPROP_USE:
			    /* Presently, we ignore this */
			    break;
			case DEF_NETPROP_SHAPE:
			    /* Ignore this too, along with the next keyword */
			    token = LefNextToken(f, TRUE);
			    break;
			case DEF_NETPROP_FIXED:
			case DEF_NETPROP_COVER:
			    /* Read in fixed nets like regular nets but mark
			     * them as NET_IGNORED.  HOWEVER, if the net
			     * already exists and is not marked NET_IGNORED,
			     * then don't force it to be ignored.  That is
			     * particularly an issue for a net like power or
			     * ground, which may need to be routed like a
			     * regular net but also has fixed portions. */
			    if (is_new) {
				fixed++;
			    }
			    // fall through
			case DEF_NETPROP_ROUTED:
			    // Read in the route;  qrouter now takes
			    // responsibility for this route.
			    while (token && (*token != ';'))
			        token = DefAddRoutes(f, oscale, net, special);
			    // Treat power and ground nets in specialnets as fixed 
			    if (subkey == DEF_NETPROP_ROUTED && special == (char)1)
				fixed++;
			    break;
		    }
		}
		break;

	    case DEF_NET_END:
		if (!LefParseEndStatement(f, sname))
		{
		    LefError(DEF_ERROR, "Net END statement missing.\n");
		    keyword = -1;
		}
		break;
	}
	if (keyword == DEF_NET_END) break;
    }

    // Set the number of nodes per net for each node on the net

    if (special == FALSE) {

	// Fill in the netnodes list for each net, needed for checking
	// for isolated routed groups within a net.

	for (i = 0; i < Numnets; i++) {
	    net = Nlnets[i];
	    for (node = net->netnodes; node; node = node->next)
		net->numnodes++;
	    for (node = net->netnodes; node; node = node->next)
		node->numnodes = net->numnodes;
	}
    }

    if (processed == total) {
	if (Verbose > 0)
	    fprintf(stdout, "  Processed %d%s nets total.\n", processed,
			(special) ? " special" : "");
    }
    else
	LefError(DEF_WARNING, "Warning:  Number of nets read (%d) does not match "
		"the number declared (%d).\n", processed, total);
    return fixed;
}

/*
 *------------------------------------------------------------
 *
 * DefReadUseLocation --
 *
 *	Read location and orientation of a cell use
 *	Syntax: ( X Y ) O
 *
 * Results:
 *	0 on success, -1 on failure
 *
 * Side Effects:
 *	GATE definition for the use has the placedX, placedY,
 *	and orient values filled.
 *------------------------------------------------------------
 */
enum def_orient {DEF_NORTH, DEF_SOUTH, DEF_EAST, DEF_WEST,
	DEF_FLIPPED_NORTH, DEF_FLIPPED_SOUTH, DEF_FLIPPED_EAST,
	DEF_FLIPPED_WEST};

static int
DefReadLocation(gate, f, oscale)
    GATE gate;
    FILE *f;
    float oscale;
{
    int keyword;
    char *token;
    float x, y;
    char mxflag, myflag;

    static char *orientations[] = {
	"N", "S", "E", "W", "FN", "FS", "FE", "FW"
    };
    static int oflags[] = {
	RN, RS, RE, RW, RN | RF, RS | RF, RE | RF, RW | RF
    };

    token = LefNextToken(f, TRUE);
    if (*token != '(') goto parse_error;
    token = LefNextToken(f, TRUE);
    if (sscanf(token, "%f", &x) != 1) goto parse_error;
    token = LefNextToken(f, TRUE);
    if (sscanf(token, "%f", &y) != 1) goto parse_error;
    token = LefNextToken(f, TRUE);
    if (*token != ')') goto parse_error;
    token = LefNextToken(f, TRUE);

    keyword = Lookup(token, orientations);
    if (keyword < 0)
    {
	LefError(DEF_ERROR, "Unknown macro orientation \"%s\".\n", token);
	return -1;
    }

    mxflag = myflag = (char)0;

    switch (keyword)
    {
	case DEF_NORTH:
	    break;
	case DEF_SOUTH:
	    mxflag = 1;
	    myflag = 1;
	    break;
	case DEF_FLIPPED_NORTH:
	    mxflag = 1;
	    break;
	case DEF_FLIPPED_SOUTH:
	    myflag = 1;
	    break;
    }

    if (gate) {
	gate->placedX = x / oscale;
	gate->placedY = y / oscale;
	gate->orient = MNONE;
	if (mxflag) gate->orient |= MX;
	if (myflag) gate->orient |= MY;
	gate->orient |= oflags[keyword];
    }
    return 0;

parse_error:
    LefError(DEF_ERROR, "Cannot parse location: must be ( X Y ) orient\n");
    return -1;
}

/*
 *------------------------------------------------------------
 *
 * DefReadPins --
 *
 *	Read a PINS section from a DEF file.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Generates paint and labels in the layout.
 *
 *------------------------------------------------------------
 */

enum def_pins_keys {DEF_PINS_START = 0, DEF_PINS_END};
enum def_pins_prop_keys {
	DEF_PINS_PROP_NET = 0, DEF_PINS_PROP_DIR,
	DEF_PINS_PROP_LAYER, DEF_PINS_PROP_PLACED,
	DEF_PINS_PROP_USE, DEF_PINS_PROP_FIXED,
	DEF_PINS_PROP_COVER};

static void
DefReadPins(FILE *f, char *sname, float oscale, int total)
{
    char *token;
    char pinname[MAX_NAME_LEN];
    int keyword, subkey;
    int processed = 0;
    DSEG currect, drect;
    GATE gate;
    int curlayer;
    double hwidth;
    u_char pin_use;

    static char *pin_keys[] = {
	"-",
	"END",
	NULL
    };

    static char *pin_property_keys[] = {
	"NET",
	"DIRECTION",
	"LAYER",
	"PLACED",
	"USE",
	"FIXED",
	"COVER",
	NULL
    };

    static char *pin_classes[] = {
	"DEFAULT",
	"INPUT",
	"OUTPUT TRISTATE",
	"OUTPUT",
	"INOUT",
	"FEEDTHRU",
	NULL
    };

    static char *pin_uses[] = {
	"DEFAULT",
	"SIGNAL",
	"ANALOG",
	"POWER",
	"GROUND",
	"CLOCK",
	"TIEOFF",
	"SCAN",
	"RESET",
	NULL
    };

    pin_use = PORT_USE_DEFAULT;

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, pin_keys);

	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in PINS "
			"definition; ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}
	switch (keyword)
	{
	    case DEF_PINS_START:		/* "-" keyword */

		/* Update the record of the number of pins		*/
		/* processed and spit out a message for every 5% done.	*/
 
		processed++;

		/* Get pin name */
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%2047s", pinname) != 1)
		{
		    LefError(DEF_ERROR, "Bad pin statement:  Need pin name\n");
		    LefEndStatement(f);
		    break;
		}

		/* Create the pin record */
		gate = (GATE)malloc(sizeof(struct gate_));
		gate->gatetype = PinMacro;
		gate->gatename = NULL;	/* Use NET, but if none, use	*/
					/* the pin name, set at end.	*/
		gate->width = gate->height = 0;
		curlayer = -1;

		/* Pin record has one node;  allocate memory for it */
		gate->taps = (DSEG *)malloc(sizeof(DSEG));
		gate->noderec = (NODE *)malloc(sizeof(NODE));
		gate->direction = (u_char *)malloc(sizeof(u_char));
		gate->area = (float *)malloc(sizeof(float));
		gate->netnum = (int *)malloc(sizeof(int));
		gate->node = (char **)malloc(sizeof(char *));
		gate->taps[0] = NULL;
		gate->noderec[0] = NULL;
		gate->netnum[0] = -1;
		gate->node[0] = NULL;
		gate->direction[0] = PORT_CLASS_DEFAULT;
		gate->area[0] = 0.0;
		gate->clientdata = (void *)NULL;

		/* Now do a search through the line for "+" entries	*/
		/* And process each.					*/

		while ((token = LefNextToken(f, TRUE)) != NULL)
		{
		    if (*token == ';') break;
		    if (*token != '+') continue;

		    token = LefNextToken(f, TRUE);
		    subkey = Lookup(token, pin_property_keys);
		    if (subkey < 0)
		    {
			LefError(DEF_WARNING, "Unknown pin property \"%s\" in "
				"PINS definition; ignoring.\n", token);
			continue;
		    }
		    switch (subkey)
		    {
			case DEF_PINS_PROP_NET:
			    /* Get the net name */
			    token = LefNextToken(f, TRUE);
			    gate->gatename = strdup(token);
			    gate->node[0] = strdup(token);
			    break;
			case DEF_PINS_PROP_DIR:
			    token = LefNextToken(f, TRUE);
			    subkey = Lookup(token, pin_classes);
			    if (subkey < 0)
				LefError(DEF_ERROR, "Unknown pin class %s\n", token);
			    else
				gate->direction[0] = subkey;
			    break;
			case DEF_PINS_PROP_LAYER:
			    curlayer = LefReadLayer(f, FALSE);
			    currect = LefReadRect(f, curlayer, oscale);
			    /* Warn if pin is on layer above routing layer limit? */
			    if (currect) {
				gate->width = currect->x2 - currect->x1;
				gate->height = currect->y2 - currect->y1;
			    }
			    break;
			case DEF_PINS_PROP_USE:
			    token = LefNextToken(f, TRUE);
			    subkey = Lookup(token, pin_uses);
			    if (subkey < 0)
				LefError(DEF_ERROR, "Unknown pin use %s\n", token);
			    else
				pin_use = subkey;
			    break;
			case DEF_PINS_PROP_PLACED:
			case DEF_PINS_PROP_FIXED:
			case DEF_PINS_PROP_COVER:
			    DefReadLocation(gate, f, oscale);
			    break;
		    }
		}

		/* If no NET was declared for pin, use pinname */
		if (gate->gatename == NULL)
		    gate->gatename = strdup(pinname);

		/* Make sure pin is at least the size of the route layer */
		drect = (DSEG)malloc(sizeof(struct dseg_));
		gate->taps[0] = drect;
		drect->next = (DSEG)NULL;

		hwidth = LefGetRouteWidth(curlayer);
		if (gate->width < hwidth) gate->width = hwidth;
		if (gate->height < hwidth) gate->height = hwidth;
		hwidth /= 2.0;
		drect->x1 = gate->placedX - hwidth;
		drect->y1 = gate->placedY - hwidth;
		drect->x2 = gate->placedX + hwidth;
		drect->y2 = gate->placedY + hwidth;
		drect->layer = curlayer;
		gate->obs = (DSEG)NULL;
		gate->nodes = 1;
		gate->next = Nlgates;
		gate->last = (GATE)NULL;
		if (Nlgates) Nlgates->last = gate;
		Nlgates = gate;
		Numpins++;

		// Used by Tcl version of qrouter
		DefHashInstance(gate);

		break;

	    case DEF_PINS_END:
		if (!LefParseEndStatement(f, sname))
		{
		    LefError(DEF_ERROR, "Pins END statement missing.\n");
		    keyword = -1;
		}
		if (pin_use != PORT_USE_DEFAULT && gate->direction[0] ==
			PORT_CLASS_DEFAULT)
		{
		    /* Derive pin use from pin class, if needed */
		    switch (pin_use) {
			case PORT_USE_SIGNAL:
			case PORT_USE_RESET:
			case PORT_USE_CLOCK:
			case PORT_USE_SCAN:
			    gate->direction[0] = PORT_CLASS_INPUT;
			    break;

			case PORT_USE_POWER:
			case PORT_USE_GROUND:
			case PORT_USE_TIEOFF:
			case PORT_USE_ANALOG:
			    gate->direction[0] = PORT_CLASS_BIDIRECTIONAL;
			    break;
		    }
		}
		break;
	}
	if (keyword == DEF_PINS_END) break;
    }

    if (processed == total) {
	if (Verbose > 0)
	    fprintf(stdout, "  Processed %d pins total.\n", processed);
    }
    else
	LefError(DEF_WARNING, "Warning:  Number of pins read (%d) does not match "
		"the number declared (%d).\n", processed, total);
}
 
/*
 *------------------------------------------------------------
 *
 * DefReadVias --
 *
 *	Read a VIAS section from a DEF file.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Technically, this routine should be creating a cell for
 *	each defined via.  For now, it just computes the bounding
 *	rectangle and layer.
 *
 *------------------------------------------------------------
 */

enum def_vias_keys {DEF_VIAS_START = 0, DEF_VIAS_END};
enum def_vias_prop_keys {
	DEF_VIAS_PROP_RECT = 0};

static void
DefReadVias(f, sname, oscale, total)
    FILE *f;
    char *sname;
    float oscale;
    int total;
{
    char *token;
    char vianame[LEF_LINE_MAX];
    int keyword, subkey;
    int processed = 0;
    int curlayer;
    LefList lefl;

    static char *via_keys[] = {
	"-",
	"END",
	NULL
    };

    static char *via_property_keys[] = {
	"RECT",
	NULL
    };

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, via_keys);

	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in VIAS "
			"definition; ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}
	switch (keyword)
	{
	    case DEF_VIAS_START:		/* "-" keyword */

		/* Update the record of the number of vias		*/
		/* processed and spit out a message for every 5% done.	*/
 
		processed++;

		/* Get via name */
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%2047s", vianame) != 1)
		{
		    LefError(DEF_ERROR, "Bad via statement:  Need via name\n");
		    LefEndStatement(f);
		    break;
		}
		lefl = LefFindLayer(token);
                if (lefl == NULL)
                {
                    lefl = (LefList)calloc(1, sizeof(lefLayer));
                    lefl->type = -1;
                    lefl->obsType = -1;
                    lefl->lefClass = CLASS_VIA;
                    lefl->info.via.area.x1 = 0.0;
                    lefl->info.via.area.y1 = 0.0;
                    lefl->info.via.area.x2 = 0.0;
                    lefl->info.via.area.y2 = 0.0;
                    lefl->info.via.area.layer = -1;
                    lefl->info.via.cell = (GATE)NULL;
                    lefl->info.via.lr = (DSEG)NULL;
		    /* Note:  "generated" flag only refers to vias that	*/
		    /* are internally generated by qrouter.  All others	*/
		    /* in the DEF file are read/written verbatim.	*/
		    lefl->info.via.generated = FALSE;
                    lefl->lefName = strdup(token);

                    lefl->next = LefInfo;
                    LefInfo = lefl;
		}
		else
		{
		    LefError(DEF_WARNING, "Warning:  Composite via \"%s\" "
				"redefined.\n", vianame);
		    lefl = LefRedefined(lefl, vianame);
		}

		/* Now do a search through the line for "+" entries	*/
		/* And process each.					*/

		while ((token = LefNextToken(f, TRUE)) != NULL)
		{
		    if (*token == ';') break;
		    if (*token != '+') continue;

		    token = LefNextToken(f, TRUE);
		    subkey = Lookup(token, via_property_keys);
		    if (subkey < 0)
		    {
			LefError(DEF_WARNING, "Unknown via property \"%s\" in "
				"VIAS definition; ignoring.\n", token);
			continue;
		    }
		    switch (subkey)
		    {
			case DEF_VIAS_PROP_RECT:
			    curlayer = LefReadLayer(f, FALSE);
			    LefAddViaGeometry(f, lefl, curlayer, oscale);
			    break;
		    }
		}
		break;

	    case DEF_VIAS_END:
		if (!LefParseEndStatement(f, sname))
		{
		    LefError(DEF_ERROR, "Vias END statement missing.\n");
		    keyword = -1;
		}
		break;
	}
	if (keyword == DEF_VIAS_END) break;
    }

    if (processed == total) {
	if (Verbose > 0)
	    fprintf(stdout, "  Processed %d vias total.\n", processed);
    }
    else
	LefError(DEF_WARNING, "Warning:  Number of vias read (%d) does not match "
		"the number declared (%d).\n", processed, total);
}
 
/*
 *------------------------------------------------------------
 *
 * DefReadBlockages --
 *
 *	Read a BLOCKAGES section from a DEF file.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	UserObs list is updated with the additional
 *	obstructions.
 *
 *------------------------------------------------------------
 */

enum def_block_keys {DEF_BLOCK_START = 0, DEF_BLOCK_END};

static void
DefReadBlockages(FILE *f, char *sname, float oscale, int total)
{
    char *token;
    int keyword;
    int processed = 0;
    DSEG drect, rsrch;
    LefList lefl;

    static char *blockage_keys[] = {
	"-",
	"END",
	NULL
    };

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, blockage_keys);

	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in BLOCKAGE "
			"definition; ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}
	switch (keyword)
	{
	    case DEF_BLOCK_START:		/* "-" keyword */

		/* Update the record of the number of components	*/
		/* processed and spit out a message for every 5% done.	*/
 
		processed++;

		/* Get layer name */
		token = LefNextToken(f, TRUE);
		lefl = LefFindLayer(token);
		if (lefl != NULL)
		{
		    drect = LefReadGeometry(NULL, f, oscale);
		    if (UserObs == NULL)
			UserObs = drect;
		    else {
			for (rsrch = UserObs; rsrch->next; rsrch = rsrch->next);
			rsrch->next = drect;
		    }
		}
		else
		{
		    LefError(DEF_ERROR, "Bad blockage statement:  Need layer name\n");
		    LefEndStatement(f);
		    break;
		}
		break;

	    case DEF_BLOCK_END:
		if (!LefParseEndStatement(f, sname))
		{
		    LefError(DEF_ERROR, "Blockage END statement missing.\n");
		    keyword = -1;
		}
		break;
	}
	if (keyword == DEF_BLOCK_END) break;
    }

    if (processed == total) {
	if (Verbose > 0)
	    fprintf(stdout, "  Processed %d blockages total.\n", processed);
    }
    else
	LefError(DEF_WARNING, "Warning:  Number of blockages read (%d) does not match "
		"the number declared (%d).\n", processed, total);
}

/*
 *------------------------------------------------------------
 *
 * DefAddGateInstance --
 *
 *	Add a gate instance to the list of instances and
 *	instance hash table.  The instance is assumed to
 *	have records gatename, gatetype, placedX, and
 *	placedY already set.  The gate macro is found from
 *	the gatetype record, and all information about the
 *	cell macro is copied to the instance record, with
 *	positions adjusted for the instance.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Many.  Cell instances are created and added to
 *	the database.
 *
 *------------------------------------------------------------
 */

void
DefAddGateInstance(GATE gate)
{
    GATE gateginfo;
    int i;
    DSEG drect, newrect;
    double tmp;

    if (gate == NULL) return;
    gateginfo = gate->gatetype;
    if (gateginfo == NULL) return;

    /* Process the gate */
    gate->width = gateginfo->width;   
    gate->height = gateginfo->height;   
    gate->nodes = gateginfo->nodes;   
    gate->obs = (DSEG)NULL;

    gate->taps = (DSEG *)malloc(gate->nodes * sizeof(DSEG));
    gate->noderec = (NODE *)malloc(gate->nodes * sizeof(NODE));
    gate->direction = (u_char *)malloc(gate->nodes * sizeof(u_char));
    gate->area = (float *)malloc(gate->nodes * sizeof(float));
    gate->netnum = (int *)malloc(gate->nodes * sizeof(int));
    gate->node = (char **)malloc(gate->nodes * sizeof(char *));

    /* Let the node names point to the master cell; */
    /* this is just diagnostic;  allows us, for	    */
    /* instance, to identify vdd and gnd nodes, so  */
    /* we don't complain about them being	    */
    /* disconnected.				    */

    for (i = 0; i < gate->nodes; i++) {
	gate->node[i] = gateginfo->node[i];  /* copy pointer */
	gate->direction[i] = gateginfo->direction[i];  /* copy */
	gate->area[i] = gateginfo->area[i];
	gate->taps[i] = (DSEG)NULL;
	gate->netnum[i] = 0;		/* Until we read NETS */
	gate->noderec[i] = NULL;

	/* Make a copy of the gate nodes and adjust for	*/
	/* instance position and number of layers	*/

	for (drect = gateginfo->taps[i]; drect; drect = drect->next) {
	    newrect = (DSEG)malloc(sizeof(struct dseg_));
	    *newrect = *drect;
	    newrect->next = gate->taps[i];
	    gate->taps[i] = newrect;
	}

	for (drect = gate->taps[i]; drect; drect = drect->next) {
	    // handle offset from gate origin
	    drect->x1 -= gateginfo->placedX;
	    drect->x2 -= gateginfo->placedX;
	    drect->y1 -= gateginfo->placedY;
	    drect->y2 -= gateginfo->placedY;

	    // handle rotations and orientations here
	    if (gate->orient & MX) {
		tmp = drect->x1;
		drect->x1 = -drect->x2;
		drect->x1 += gate->placedX + gateginfo->width;
		drect->x2 = -tmp;
		drect->x2 += gate->placedX + gateginfo->width;
	    }
	    else {
		drect->x1 += gate->placedX;
		drect->x2 += gate->placedX;
	    }
	    if (gate->orient & MY) {
		tmp = drect->y1;
		drect->y1 = -drect->y2;
		drect->y1 += gate->placedY + gateginfo->height;
		drect->y2 = -tmp;
		drect->y2 += gate->placedY + gateginfo->height;
	    }
	    else {
		drect->y1 += gate->placedY;
		drect->y2 += gate->placedY;
	    }
	}
    }

    /* Make a copy of the gate obstructions and adjust	*/
    /* for instance position				*/
    for (drect = gateginfo->obs; drect; drect = drect->next) {
	newrect = (DSEG)malloc(sizeof(struct dseg_));
	*newrect = *drect;
	newrect->next = gate->obs;
	gate->obs = newrect;
    }

    for (drect = gate->obs; drect; drect = drect->next) {
	drect->x1 -= gateginfo->placedX;
	drect->x2 -= gateginfo->placedX;
	drect->y1 -= gateginfo->placedY;
	drect->y2 -= gateginfo->placedY;

	// handle rotations and orientations here
	if (gate->orient & MX) {
	    tmp = drect->x1;
	    drect->x1 = -drect->x2;
	    drect->x1 += gate->placedX + gateginfo->width;
	    drect->x2 = -tmp;
	    drect->x2 += gate->placedX + gateginfo->width;
	}
	else {
	    drect->x1 += gate->placedX;
	    drect->x2 += gate->placedX;
	}
	if (gate->orient & MY) {
	    tmp = drect->y1;
	    drect->y1 = -drect->y2;
	    drect->y1 += gate->placedY + gateginfo->height;
	    drect->y2 = -tmp;
	    drect->y2 += gate->placedY + gateginfo->height;
	}
	else {
	    drect->y1 += gate->placedY;
	    drect->y2 += gate->placedY;
	}
    }
    gate->next = Nlgates;
    gate->last = (GATE)NULL;
    if (Nlgates) Nlgates->last = gate;
    Nlgates = gate;
    Numgates++;

    // Used by Tcl version of qrouter
    DefHashInstance(gate);
}

/*
 *------------------------------------------------------------
 *
 * DefReadComponents --
 *
 *	Read a COMPONENTS section from a DEF file.
 *
 * Results:
 *	0 on success, 1 on fatal error.
 *
 * Side Effects:
 *	Many.  Cell instances are created and added to
 *	the database.
 *
 *------------------------------------------------------------
 */

enum def_comp_keys {DEF_COMP_START = 0, DEF_COMP_END};
enum def_prop_keys {
	DEF_PROP_FIXED = 0, DEF_PROP_COVER,
	DEF_PROP_PLACED, DEF_PROP_UNPLACED,
	DEF_PROP_SOURCE, DEF_PROP_WEIGHT, DEF_PROP_FOREIGN,
	DEF_PROP_REGION, DEF_PROP_GENERATE, DEF_PROP_PROPERTY,
	DEF_PROP_EEQMASTER};

static int
DefReadComponents(FILE *f, char *sname, float oscale, int total)
{
    GATE gateginfo;
    GATE gate = NULL;
    char *token;
    char usename[512];
    int keyword, subkey, i;
    int processed = 0;
    char OK;
    int err_fatal = 0;

    static char *component_keys[] = {
	"-",
	"END",
	NULL
    };

    static char *property_keys[] = {
	"FIXED",
	"COVER",
	"PLACED",
	"UNPLACED",
	"SOURCE",
	"WEIGHT",
	"FOREIGN",
	"REGION",
	"GENERATE",
	"PROPERTY",
	"EEQMASTER",
	NULL
    };

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, component_keys);

	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in COMPONENT "
			"definition; ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}
	switch (keyword)
	{
	    case DEF_COMP_START:		/* "-" keyword */

		/* Update the record of the number of components	*/
		/* processed and spit out a message for every 5% done.	*/
 
		processed++;

		/* Get use and macro names */
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%511s", usename) != 1)
		{
		    LefError(DEF_ERROR, "Bad component statement:  Need use "
				"and macro names\n");
		    LefEndStatement(f);
		    err_fatal++;
		    break;
		}
		token = LefNextToken(f, TRUE);

		// Watch for backslash-escaped instance names that have
		// been sanitized for use in SPICE netlists with an
		// extra backslash replacing the space, which needs to
		// be converted back.

		if (*usename == '\\') {
		    char *sptr, *bptr;
		    sptr = strchr(usename, ' ');
		    if (sptr == NULL) {
		        bptr = strrchr(usename + 1, '\\');
		        if (bptr != NULL) *bptr = ' ';
		    }
		}

		/* Find the corresponding macro */
		OK = 0;
		for (gateginfo = GateInfo; gateginfo; gateginfo = gateginfo->next) {
		    if (!strcasecmp(gateginfo->gatename, token)) {
			OK = 1;
			break;
		    }
		}
		if (!OK) {
		    LefError(DEF_ERROR, "Could not find a macro definition for \"%s\"\n",
				token);
		    gate = NULL;
		    err_fatal++;
		}
		else {
		    gate = (GATE)malloc(sizeof(struct gate_));
		    gate->gatename = strdup(usename);
		    gate->gatetype = gateginfo;
		    gate->clientdata = (void *)NULL;
		}
		
		/* Now do a search through the line for "+" entries	*/
		/* And process each.					*/

		while ((token = LefNextToken(f, TRUE)) != NULL)
		{
		    if (*token == ';') break;
		    if (*token != '+') continue;

		    token = LefNextToken(f, TRUE);
		    subkey = Lookup(token, property_keys);
		    if (subkey < 0)
		    {
			LefError(DEF_WARNING, "Unknown component property \"%s\" in "
				"COMPONENT definition; ignoring.\n", token);
			continue;
		    }
		    switch (subkey)
		    {
			case DEF_PROP_PLACED:
			case DEF_PROP_UNPLACED:
			case DEF_PROP_FIXED:
			case DEF_PROP_COVER:
			    DefReadLocation(gate, f, oscale);
			    break;
			case DEF_PROP_SOURCE:
			case DEF_PROP_WEIGHT:
			case DEF_PROP_FOREIGN:
			case DEF_PROP_REGION:
			case DEF_PROP_GENERATE:
			case DEF_PROP_PROPERTY:
			case DEF_PROP_EEQMASTER:
			    token = LefNextToken(f, TRUE);
			    break;
		    }
		}
		DefAddGateInstance(gate);
		break;

	    case DEF_COMP_END:
		if (!LefParseEndStatement(f, sname))
		{
		    LefError(DEF_ERROR, "Component END statement missing.\n");
		    keyword = -1;
		    err_fatal++;
		}

		/* Finish final call by placing the cell use */
		if ((total > 0) && (gate != NULL))
		{
		    // Nothing to do. . . gate has already been placed in list.
		    gate = NULL;
		}
		break;
	}
	if (keyword == DEF_COMP_END) break;
    }

    if (processed == total) {
	if (Verbose > 0)
	    fprintf(stdout, "  Processed %d subcell instances total.\n", processed);
    }
    else
	LefError(DEF_WARNING, "Warning:  Number of subcells read (%d) does not match "
		"the number declared (%d).\n", processed, total);
    return err_fatal;
}

/*
 *------------------------------------------------------------
 *
 * DefRead --
 *
 *	Read a .def file and parse die area, track positions,
 *	components, pins, and nets.
 *
 * Results:
 *	Returns the units scale, so the routed output can be
 *	scaled to match the DEF file header.
 *
 * Side Effects:
 *	Many.
 *
 *------------------------------------------------------------
 */

/* Enumeration of sections defined in DEF files */

enum def_sections {DEF_VERSION = 0, DEF_NAMESCASESENSITIVE,
	DEF_UNITS, DEF_DESIGN, DEF_REGIONS, DEF_ROW, DEF_TRACKS,
	DEF_GCELLGRID, DEF_DIVIDERCHAR, DEF_BUSBITCHARS,
	DEF_PROPERTYDEFINITIONS, DEF_DEFAULTCAP, DEF_TECHNOLOGY,
	DEF_HISTORY, DEF_DIEAREA, DEF_COMPONENTS, DEF_VIAS,
	DEF_PINS, DEF_PINPROPERTIES, DEF_SPECIALNETS,
	DEF_NETS, DEF_IOTIMINGS, DEF_SCANCHAINS, DEF_BLOCKAGES,
	DEF_CONSTRAINTS, DEF_GROUPS, DEF_EXTENSION,
	DEF_END};

int
DefRead(char *inName, float *retscale)
{
    FILE *f;
    char filename[256];
    char namepos[32];
    char *token;
    int keyword, dscale, total;
    int curlayer = -1, channels;
    int i;
    int err_fatal = 0;
    float oscale;
    double start, step;
    double llx, lly, urx, ury, locpitch;
    double dXlowerbound, dYlowerbound, dXupperbound, dYupperbound;
    char corient = '.';
    DSEG diearea;
    ROW newrow;

    static char *orientations[] = {
	"N", "S", "E", "W", "FN", "FS", "FE", "FW"
    };
    static int oflags[] = {
	RN, RS, RE, RW, RN | RF, RS | RF, RE | RF, RW | RF
    };

    static char *sections[] = {
	"VERSION",
	"NAMESCASESENSITIVE",
	"UNITS",
	"DESIGN",
	"REGIONS",
	"ROW",
	"TRACKS",
	"GCELLGRID",
	"DIVIDERCHAR",
	"BUSBITCHARS",
	"PROPERTYDEFINITIONS",
	"DEFAULTCAP",
	"TECHNOLOGY",
	"HISTORY",
	"DIEAREA",
	"COMPONENTS",
	"VIAS",
	"PINS",
	"PINPROPERTIES",
	"SPECIALNETS",
	"NETS",
	"IOTIMINGS",
	"SCANCHAINS",
	"BLOCKAGES",
	"CONSTRAINTS",
	"GROUPS",
	"BEGINEXT",
	"END",
	NULL
    };

    if (!strrchr(inName, '.'))
	sprintf(filename, "%s.def", inName);
    else
	strcpy(filename, inName);
   
    f = fopen(filename, "r");

    if (f == NULL)
    {
	fprintf(stderr, "Cannot open input file: ");
	perror(filename);
	*retscale = (float)0.0;
	return 1;
    }

    /* Initialize */

    if (Verbose > 0) {
	fprintf(stdout, "Reading DEF data from file %s.\n", filename);
	fflush(stdout);
    }

    oscale = 1;
    lefCurrentLine = 0;

    DefHashInit();

    /* Read file contents */

    while ((token = LefNextToken(f, TRUE)) != NULL)
    {
	keyword = Lookup(token, sections);
	if (keyword < 0)
	{
	    LefError(DEF_WARNING, "Unknown keyword \"%s\" in DEF file; "
			"ignoring.\n", token);
	    LefEndStatement(f);
	    continue;
	}
	if (keyword != DEF_TRACKS) corient = '.';

	switch (keyword)
	{
	    case DEF_VERSION:
		LefEndStatement(f);
		break;
	    case DEF_NAMESCASESENSITIVE:
		LefEndStatement(f);
		break;
	    case DEF_TECHNOLOGY:
		token = LefNextToken(f, TRUE);
		if (Verbose > 0)
		    fprintf(stdout, "Diagnostic: DEF file technology: \"%s\"\n",
				token);
		LefEndStatement(f);
	 	break;
	    case DEF_REGIONS:
		LefSkipSection(f, sections[DEF_REGIONS]);
		break;
	    case DEF_DESIGN:
		token = LefNextToken(f, TRUE);
		if (Verbose > 0)
		    fprintf(stdout, "Diagnostic: Design name: \"%s\"\n", token);
		DEFDesignName = strdup(token);
		LefEndStatement(f);
		break;
	    case DEF_UNITS:
		token = LefNextToken(f, TRUE);
		token = LefNextToken(f, TRUE);
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &dscale) != 1)
		{
		    LefError(DEF_ERROR, "Invalid syntax for UNITS statement.\n");
		    LefError(DEF_WARNING, "Assuming default value of 100\n");
		    dscale = 100;
		}
		/* We don't care if the scale is 100, 200, 1000, or 2000. */
		/* Do we need to deal with numeric roundoff issues?	  */
		oscale *= (float)dscale;
		LefEndStatement(f);
		break;
	    case DEF_ROW:
		newrow = (ROW)malloc(sizeof(struct row_));
		token = LefNextToken(f, TRUE);
		newrow->rowname = strdup(token);
		token = LefNextToken(f, TRUE);
		newrow->sitename = strdup(token);
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->x);
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->y);
		token = LefNextToken(f, TRUE);
		keyword = Lookup(token, orientations);
		if (keyword < 0)
		    newrow->orient = 0;
		else
		    newrow->orient = oflags[keyword];
		token = LefNextToken(f, TRUE);	    /* skip "DO" */
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->xnum);
		token = LefNextToken(f, TRUE);	    /* skip "BY" */
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->ynum);
		token = LefNextToken(f, TRUE);	    /* skip "STEP" */
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->xstep);
		token = LefNextToken(f, TRUE);
		sscanf(token, "%d", &newrow->ystep);
		sprintf(namepos, "%d", newrow->y);
		HashPtrInstall(namepos, newrow, &RowTable);
		LefEndStatement(f);
		break;
	    case DEF_TRACKS:
		token = LefNextToken(f, TRUE);
		if (strlen(token) != 1) {
		    LefError(DEF_ERROR, "Problem parsing track orientation (X or Y).\n");
		}
		corient = tolower(token[0]);	// X or Y
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%lg", &start) != 1) {
		    LefError(DEF_ERROR, "Problem parsing track start position.\n");
		    err_fatal++;
		}
		token = LefNextToken(f, TRUE);
		if (strcmp(token, "DO")) {
		    LefError(DEF_ERROR, "TRACKS missing DO loop.\n");
		    err_fatal++;
		}
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &channels) != 1) {
		    LefError(DEF_ERROR, "Problem parsing number of track channels.\n");
		    err_fatal++;
		}
		token = LefNextToken(f, TRUE);
		if (strcmp(token, "STEP")) {
		    LefError(DEF_ERROR, "TRACKS missing STEP size.\n");
		    err_fatal++;
		}
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%lg", &step) != 1) {
		    LefError(DEF_ERROR, "Problem parsing track step size.\n");
		    err_fatal++;
		}
		token = LefNextToken(f, TRUE);
		if (!strcmp(token, "LAYER")) {
		    curlayer = LefReadLayer(f, FALSE);
		}
		if (corient == 'x') {
		    locpitch = step / oscale;
		    if ((PitchX == 0.0) || ((locpitch < PitchX) && (locpitch != 0)))
			PitchX = locpitch;
		    llx = start;
		    urx = start + step * channels;
		    if ((llx / oscale) < Xlowerbound)
			Xlowerbound = llx / oscale;
		    if ((urx / oscale) > Xupperbound)
			Xupperbound = urx / oscale;
		}
		else {
		    locpitch = step / oscale;
		    if ((PitchY == 0.0) || ((locpitch < PitchY) && (locpitch != 0)))
			PitchY = locpitch;
		    lly = start;
		    ury = start + step * channels;
		    if ((lly / oscale) < Ylowerbound)
			Ylowerbound = lly / oscale;
		    if ((ury / oscale) > Yupperbound)
			Yupperbound = ury / oscale;
		}
		LefEndStatement(f);
		break;
	    case DEF_GCELLGRID:
		LefEndStatement(f);
		break;
	    case DEF_DIVIDERCHAR:
		LefEndStatement(f);
		break;
	    case DEF_BUSBITCHARS:
		LefEndStatement(f);
		break;
	    case DEF_HISTORY:
		LefEndStatement(f);
		break;
	    case DEF_DIEAREA:
		diearea = LefReadRect(f, 0, oscale); // no current layer, use 0
		dXlowerbound = diearea->x1;
		dYlowerbound = diearea->y1;
		dXupperbound = diearea->x2;
		dYupperbound = diearea->y2;
		/* Seed actual lower/upper bounds with the midpoint */
		Xlowerbound = (diearea->x1 + diearea->x2) / 2;
		Ylowerbound = (diearea->y1 + diearea->y2) / 2;
		Xupperbound = Xlowerbound;
		Yupperbound = Ylowerbound;
		LefEndStatement(f);
		break;
	    case DEF_PROPERTYDEFINITIONS:
		LefSkipSection(f, sections[DEF_PROPERTYDEFINITIONS]);
		break;
	    case DEF_DEFAULTCAP:
		LefSkipSection(f, sections[DEF_DEFAULTCAP]);
		break;
	    case DEF_COMPONENTS:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		err_fatal += DefReadComponents(f, sections[DEF_COMPONENTS], oscale, total);
		break;
	    case DEF_BLOCKAGES:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		DefReadBlockages(f, sections[DEF_BLOCKAGES], oscale, total);
		break;
	    case DEF_VIAS:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		DefReadVias(f, sections[DEF_VIAS], oscale, total);
		break;
	    case DEF_PINS:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		DefReadPins(f, sections[DEF_PINS], oscale, total);
		break;
	    case DEF_PINPROPERTIES:
		LefSkipSection(f, sections[DEF_PINPROPERTIES]);
		break;
	    case DEF_SPECIALNETS:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		numSpecial = DefReadNets(f, sections[DEF_SPECIALNETS], oscale, TRUE,
				total);
		break;
	    case DEF_NETS:
		token = LefNextToken(f, TRUE);
		if (sscanf(token, "%d", &total) != 1) total = 0;
		LefEndStatement(f);
		DefReadNets(f, sections[DEF_NETS], oscale, FALSE, total);
		break;
	    case DEF_IOTIMINGS:
		LefSkipSection(f, sections[DEF_IOTIMINGS]);
		break;
	    case DEF_SCANCHAINS:
		LefSkipSection(f, sections[DEF_SCANCHAINS]);
		break;
	    case DEF_CONSTRAINTS:
		LefSkipSection(f, sections[DEF_CONSTRAINTS]);
		break;
	    case DEF_GROUPS:
		LefSkipSection(f, sections[DEF_GROUPS]);
		break;
	    case DEF_EXTENSION:
		LefSkipSection(f, sections[DEF_EXTENSION]);
		break;
	    case DEF_END:
		if (!LefParseEndStatement(f, "DESIGN"))
		{
		    LefError(DEF_ERROR, "END statement out of context.\n");
		    keyword = -1;
		}
		break;
	}
	if (keyword == DEF_END) break;
    }
    if (Verbose > 0)
	fprintf(stdout, "DEF read: Processed %d lines.\n", lefCurrentLine);
    LefError(DEF_ERROR, NULL);	/* print statement of errors, if any, and reset */

    /* If there were no TRACKS statements, then use the DIEAREA */
    if (Xlowerbound == Xupperbound) {
	Xlowerbound = dXlowerbound;
	Xupperbound = dXupperbound;
    }
    if (Ylowerbound == Yupperbound) {
	Ylowerbound = dYlowerbound;
	Yupperbound = dYupperbound;
    }

    /* Cleanup */

    if (f != NULL) fclose(f);
    *retscale = oscale;
    return err_fatal;
}
