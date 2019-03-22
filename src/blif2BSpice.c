//--------------------------------------------------------------
// blif2BSpice
//
// Revision 0, 2006-11-11: First release by R. Timothy Edwards.
// Revision 1, 2009-07-13: Minor cleanups by Philipp Klaus Krause.
// Revision 2, 2013-05-10: Modified to take a library of subcell
//		definitions to use for determining port order.
// Revision 3, 2013-10-09: Changed from BDnet2BSpice to
//		blif2BSpice
//
//--------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

#define	EXIT_SUCCESS	0
#define	EXIT_FAILURE	1
#define	EXIT_HELP	2
#define TRUE 		1
#define FALSE		0
#define NMOS		1
#define PMOS		0

#define LengthOfLine    	16384
#define LengthOfNodeName  	512

/* getopt stuff */
extern	int	optind, getopt();
extern	char	*optarg;

void ReadNetlistAndConvert(FILE *, FILE *, char *, FILE *,
		char *, char *, char *, int);
void CleanupString(char text[]);
float getnumber(char *strpntbegin);
int loc_getline(char s[], int lim, FILE *fp);
void helpmessage();

//--------------------------------------------------------
// Structures for maintaining port order for subcircuits
// read from a SPICE library
//--------------------------------------------------------

typedef struct _portrec *portrecp;

typedef struct _portrec {
   portrecp next;
   char *name;
   char signal[LengthOfNodeName];	// Instance can write signal name here
} portrec;

typedef struct _subcircuit *subcircuitp;

typedef struct _subcircuit {
   subcircuitp next; 
   char *name;
   portrecp ports;
   int gatecount;
} subcircuit;

//--------------------------------------------------------

int main (int argc, char *argv[])
{
    FILE *NET1 = NULL;
    FILE *NET2 = NULL;
    FILE *outfile;
    int i;
    int doinclude = 0;

    char *Net1name = NULL;
    char *Net2name = NULL;

    char *vddnet = NULL;
    char *gndnet = NULL;
    char *subnet = NULL;

    // Use implicit power if power and ground nodes are global in SPICE
    // Otherwise, use "-p".

    while ((i = getopt( argc, argv, "hHil:p:g:s:" )) != EOF) {
	switch (i) {
	   case 'p':
	       vddnet = strdup(optarg);
	       break;
	   case 'g':
	       gndnet = strdup(optarg);
	       break;
	   case 's':
	       subnet = strdup(optarg);
	       break;
	   case 'l':
	       Net2name = strdup(optarg);
	       break;
	   case 'i':
	       doinclude = 1;
	       break;
	   case 'h':
	   case 'H':
	       helpmessage();
	       break;
	   default:
	       fprintf(stderr, "\nbad switch %d\n", i);
	       helpmessage();
	       break;
	}
    }

    if (optind < argc) {
	Net1name = strdup(argv[optind]);
	optind++;
    }
    else {
	fprintf(stderr, "Couldn't find a filename as input\n");
	exit(EXIT_FAILURE);
    }
    optind++;

    if (Net1name)
	NET1 = fopen(Net1name,"r");
    if (NET1 == NULL) {
	fprintf(stderr, "Couldn't open %s for reading\n", Net1name);
	exit(EXIT_FAILURE);
    }

    if (Net2name) {
	NET2 = fopen(Net2name, "r");
	if (NET2 == NULL)
	    fprintf(stderr, "Couldn't open %s for reading\n", Net2name);
    }

    outfile = stdout;
    ReadNetlistAndConvert(NET1, NET2, Net2name, outfile,
		vddnet, gndnet, subnet, doinclude);
    return 0;
}

/*--------------------------------------------------------------*/
/*C *Alloc - Allocates memory for linked list elements		*/
/*								*/
/*         ARGS: 
        RETURNS: 1 to OS
   SIDE EFFECTS: 
\*--------------------------------------------------------------*/

void ReadNetlistAndConvert(FILE *netfile, FILE *libfile, char *libname,
		FILE *outfile, char *vddnet, char *gndnet, char *subnet,
		int doinclude)
{
	int i, NumberOfInputs, NumberOfOutputs;

	char *lptr;
        char line[LengthOfLine];

	char InputName[LengthOfNodeName];
	char OutputName[LengthOfNodeName];
        char MainSubcktName[LengthOfNodeName];
	char InstanceName[LengthOfNodeName];
	char InstancePortName[LengthOfNodeName];
	char InstancePortWire[LengthOfNodeName];

	subcircuitp subcktlib = NULL, tsub;
	portrecp tport;

	int uniquenode = 1000;

	// Read a SPICE library of subcircuits

	if (libfile != NULL) {
	    char *sp, *sp2;
	    subcircuitp newsubckt;
	    portrecp newport, lastport;

	    /* If we specify a library, then we need to make sure that	*/
	    /* "vddnet" and "gndnet" are non-NULL, so that they will be	*/
	    /* filled in correctly.  If not specified on the command	*/
	    /* line, they default to "vdd" and "vss".			*/

	    if (vddnet == NULL) vddnet = strdup("vdd");
	    if (gndnet == NULL) gndnet = strdup("gnd");

	    /* Read SPICE library of subcircuits, if one is specified.	*/
	    /* Retain the name and order of ports passed to each	*/
	    /* subcircuit.						*/
	    while (loc_getline(line, sizeof(line), libfile) > 0) {
		if (!strncasecmp(line, ".subckt", 7)) {
		   /* Read cellname */
		   sp = line + 7;
		   while (isspace(*sp) && (*sp != '\n')) sp++;
		   sp2 = sp;
		   while (!isspace(*sp2) && (*sp2 != '\n')) sp2++;
		   *sp2 = '\0';

		   newsubckt = (subcircuitp)malloc(sizeof(subcircuit));
		   newsubckt->name = strdup(sp);
		   newsubckt->next = subcktlib;
		   subcktlib = newsubckt;
		   newsubckt->ports = NULL;
		   newsubckt->gatecount = 0;

		   sp = sp2 + 1;
		   while (isspace(*sp) && (*sp != '\n') && (*sp != '\0')) sp++;
		   while (1) {

		      /* Move string pointer to next port name */

		      if (*sp == '\n' || *sp == '\0') {
			 loc_getline(line, sizeof(line), libfile);
			 if (*line == '+') {
			    sp = line + 1;
			    while (isspace(*sp) && (*sp != '\n')) sp++;
			 }
			 else
			    break;
		      }

		      /* Terminate port name and advance pointer */
		      sp2 = sp;
		      while (!isspace(*sp2) && (*sp2 != '\n')) sp2++;
		      *sp2 = '\0';

		      /* Get next port */

		      newport = (portrecp)malloc(sizeof(portrec));
		      newport->next = NULL;
		      newport->name = strdup(sp);
	
		      /* This is a bit of a hack.  It's difficult to	*/
		      /* tell what the standard cell set is going to	*/
		      /* use for power bus names.  It's okay to fill in	*/
		      /* signals with similar names here, as they will	*/
		      /* (should!) match up to port names in the BLIF	*/
		      /* file, and will be overwritten later.  Well	*/
		      /* connections are not considered here, but maybe	*/
		      /* they should be?				*/

		      if (!strncasecmp(sp, "vdd", 3))
			 strcpy(newport->signal, vddnet);
		      else if (!strncasecmp(sp, "vss", 3))
			 strcpy(newport->signal, gndnet);
		      else if (!strncasecmp(sp, "gnd", 3))
			 strcpy(newport->signal, gndnet);
		      else if (!strncasecmp(sp, "sub", 3)) {
			 if (subnet != NULL)
			    strcpy(newport->signal, subnet);
		      }
		      else 
		         newport->signal[0] = '\0';

		      if (newsubckt->ports == NULL)
			 newsubckt->ports = newport;
		      else
			 lastport->next = newport;

		      lastport = newport;

		      sp = sp2 + 1;
		   }

		   /* Read input to end of subcircuit */

		   if (strncasecmp(line, ".ends", 4)) {
		      while (loc_getline(line, sizeof(line), libfile) > 0)
		         if (!strncasecmp(line, ".ends", 4))
			    break;
		   }
		}
	    }
	}

	/* Read in line by line */

        while (loc_getline(line, sizeof(line), netfile) > 0 ) {
	   if (strstr(line, ".model") != NULL ) {
              if (sscanf(line, ".model %s", MainSubcktName) == 1) {
	         CleanupString(MainSubcktName);
		 fprintf(outfile, "*SPICE netlist created from BLIF module "
			"%s by blif2BSpice\n", MainSubcktName);
		 fprintf(outfile, "");

		 /* If doinclude == 1 then dump the contents of the	*/
		 /* libraray.  If 0, then just write a .include line.	*/

		 if (doinclude == 0) {
		    if (subcktlib != NULL) {
		       /* Write out the subcircuit library file verbatim */
		       rewind(libfile);
		       while (loc_getline(line, sizeof(line), libfile) > 0)
		          fputs(line, outfile);
		       fclose(libfile);
		       fprintf(outfile, "");
		    }
		 }
		 else {
		     fprintf(outfile, ".include %s\n", libname);
		 }

	         fprintf(outfile, ".subckt %s ", MainSubcktName);
	         if (vddnet == NULL)
		    fprintf(outfile, "vdd ");
		 else
		    fprintf(outfile, "%s ", vddnet);

	         if (gndnet == NULL)
		    fprintf(outfile, "vss ");
		 else
		    fprintf(outfile, "%s ", gndnet);

		 if ((subnet != NULL) && strcasecmp(subnet, gndnet))
		    fprintf(outfile, "%s ", subnet);
	      }
	   }
	   else if (strstr(line, ".end") != NULL) {
              fprintf(outfile, ".ends %s\n ", MainSubcktName);
           }
	   if (strstr(line, ".inputs") != NULL) {
	      NumberOfInputs = 0;
	      lptr = line;
	      while (!isspace(*lptr)) lptr++;
	      while (isspace(*lptr)) lptr++;
	      while (1) {
		 if (strstr(lptr, ".outputs")) break;
                 while (sscanf(lptr, "%s", InputName) == 1) {
	            CleanupString(InputName);
	            fprintf(outfile, "%s ", InputName);
	            NumberOfInputs++;
	            while (!isspace(*lptr)) lptr++;
	            while (isspace(*lptr)) lptr++;
		    if (*lptr == '\\') break;
	         }
		 if (loc_getline(line, sizeof(line), netfile) <= 1 )
		    break;
		 else
		    lptr = line;
	      } 
	   }
	   if (strstr(line, ".outputs") != NULL) {
	      NumberOfOutputs = 0;
	      lptr = line;
	      while (!isspace(*lptr)) lptr++;
	      while (isspace(*lptr)) lptr++;
	      while (1) {
		 if (strstr(lptr, ".gate") || strstr(lptr, ".subckt")) break;
                 while (sscanf(lptr, "%s", OutputName) == 1) {
	            CleanupString(OutputName);
	            fprintf(outfile,"%s ", OutputName);
	            NumberOfOutputs++;
	            while (!isspace(*lptr)) lptr++;
	            while (isspace(*lptr)) lptr++;
		    if (*lptr == '\\') break;
	         }
	         if (loc_getline(line, sizeof(line), netfile) <= 1)
		    break;
		 else
		    lptr = line;
	      }
	      fprintf(outfile, "\n");
	   }
	   if (strstr(line, ".gate") != NULL || strstr(line, ".subckt") != NULL) {
	
	      lptr = line;
	      while (isspace(*lptr)) lptr++;
	      if (sscanf(lptr, ".%*s %s", InstanceName) == 1) {
	         CleanupString(InstanceName);

		 /* Search library records for subcircuit */
		 for (tsub = subcktlib; tsub; tsub = tsub->next) {
		    if (!strcasecmp(InstanceName, tsub->name))
			break;
		 }
		 if (tsub == NULL) {
		    // Create an entry so we can track instances
		    tsub = (subcircuitp)malloc(sizeof(subcircuit));
		    tsub->next = subcktlib;
		    subcktlib = tsub;
		    tsub->name = strdup(InstanceName);
		    tsub->gatecount = 1;
		    tsub->ports = NULL;
		 }
		 else
		    tsub->gatecount++;

	         fprintf(outfile, "X%s_%d ", tsub->name, tsub->gatecount);

	         if (tsub == NULL) {
	            if (vddnet == NULL) fprintf(outfile,"vdd ");
	            if (gndnet == NULL) fprintf(outfile,"vss ");
	         }

		 while (!isspace(*lptr)) lptr++;
		 while (isspace(*lptr)) lptr++;
		 while (!isspace(*lptr)) lptr++;
		 while (isspace(*lptr)) lptr++;
		 while (1) {
		    char *eptr;
		    eptr = strchr(lptr, '=');
		    if (eptr == NULL) break;
		    *eptr = '\0';
		    if (sscanf(lptr, "%s", InstancePortName) != 1) break;
		    lptr = eptr + 1;
		    if (sscanf(lptr, "%s", InstancePortWire) != 1) break;
	            CleanupString(InstancePortWire);

		    if (tsub == NULL)
	               fprintf(outfile,"%s ", InstancePortWire);
		    else {
		       // Find port name in list
	               CleanupString(InstancePortName);
		       for (tport = tsub->ports; tport; tport = tport->next) {
			  if (!strcmp(tport->name, InstancePortName)) {
			     sprintf(tport->signal, "%s", InstancePortWire);
			     break;
			  }
		       }
		       if (tport == NULL)
			  /* This will likely screw everything up. . . */
	                  fprintf(outfile,"%s ", InstancePortWire);
		    }
		    while (!isspace(*lptr)) lptr++;
		    while (isspace(*lptr)) lptr++;
		    if (*lptr == '\\') {
	               if (loc_getline(line, sizeof(line), netfile) <= 1) break;
		       lptr = line;
		    }
		    else if (*lptr == '\n') break;
	         }
	      }

	      /* Done with I/O section, add instance name to subckt line */

	      if (tsub != NULL) {
		 /* Write out all ports in proper order */
		 for (tport = tsub->ports; tport; tport = tport->next) {
		    if (tport->signal[0] == '\0')
		       fprintf(outfile,"%d ", uniquenode++);
		    else
		       fprintf(outfile,"%s ", tport->signal);
		 }
	      }
              fprintf(outfile,"%s\n",InstanceName);
	   } 
	}
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void CleanupString(char text[LengthOfNodeName])
{
	int i;
	char *CitationPnt;
	
	CitationPnt=strchr(text,'"');
	if( CitationPnt != NULL) {
	   i=0;
	   while( CitationPnt[i+1] != '"' ) {
	      CitationPnt[i]=CitationPnt[i+1];
	      i++;
	   }
	   CitationPnt[i]='\0';
           CitationPnt=strchr(text,'[');
	   if(CitationPnt != NULL) {
              i=0;
              while( CitationPnt[i+1] != ']' ) {
                 CitationPnt[i]=CitationPnt[i+1];
                 i++;
              }
              CitationPnt[i]='\0';
	   }
	}
}

/*--------------------------------------------------------------*/
/*C getnumber - gets number pointed by strpntbegin		*/
/*								*/
/*         ARGS: strpntbegin - number expected after '='        */ 
/*        RETURNS: 1 to OS
   SIDE EFFECTS: 
\*--------------------------------------------------------------*/
float getnumber(char *strpntbegin)
{       int i;
        char *strpnt,magn1,magn2;
        float number;

        strpnt=strpntbegin;
        strpnt=strchr(strpntbegin,'=');
        if(strpnt == NULL) {
           fprintf(stderr,"Error: getnumber: Didn't find '=' in string "
                               "%s\n",strpntbegin);
           return DBL_MAX;
        }
        strpnt=strpnt+1;
        
	if(sscanf(strpnt,"%f%c%c",&number,&magn1, &magn2)!=3) {
            if(sscanf(strpnt,"%f%c",&number,&magn1)!=2) {
               fprintf(stderr,"Error: getnumber : Couldn't read number in "
                      "%s %s\n",strpntbegin,strpnt);
               return DBL_MAX;
            }
        }

        switch( magn1 ) {
        case 'f':
           number *= 1e-15;
           break;          
        case 'p':
           number *= 1e-12;
           break;          
        case 'n':
           number *= 1e-9;
           break;          
        case 'u':
           number *= 1e-6;
           break;          
        case 'm':
           if (magn2 == 'e') number *= 1e6;
           else number *= 1e-3;
           break;          
        case 'k':
           number *= 1e3;
           break;          
        case 'g':
           number *= 1e9;
           break;
        case ' ':
           default:
           return number;
        }
        return number;
}          

/*--------------------------------------------------------------*/
/*C loc_getline: read a line, return length		*/
/*								*/
/*         ARGS: 
        RETURNS: 1 to OS
   SIDE EFFECTS: 
\*--------------------------------------------------------------*/
int loc_getline( char s[], int lim, FILE *fp)
{
	int c, i;
	
	i=0;
	while(--lim > 0 && (c=getc(fp)) != EOF && c != '\n')
		s[i++] = c;
	if (c == '\n')
		s[i++] = c;
	s[i] = '\0';
	if ( c == EOF ) i=0; 
	return i;
}

/*--------------------------------------------------------------*/
/*C helpmessage - tell user how to use the program		*/
/*								*/
/*         ARGS: 
        RETURNS: 1 to OS
   SIDE EFFECTS: 
\*--------------------------------------------------------------*/

void helpmessage()
{

    fprintf(stderr,"blif2BSpice [-options] netlist \n");
    fprintf(stderr,"\n");
    fprintf(stderr,"blif2BSpice converts a netlist in blif format \n");
    fprintf(stderr,"to BSpice subcircuit format. Output on stdout\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"option, -h this message\n");    
    fprintf(stderr,"option, -p means: don't add power nodes to instances\n");
    fprintf(stderr,"        only nodes present in the .gate statement used\n");

    exit( EXIT_HELP );	
} /* helpmessage() */

