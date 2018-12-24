/*----------------------------------------------------------------------*/
/* readverilog.h -- Input for Verilog format (structural verilog only)	*/
/*----------------------------------------------------------------------*/

/*------------------------------------------------------*/
/* Definitions and structures 				*/
/*------------------------------------------------------*/

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

/* 'X' is used here to separate the single-character delimiters	*/
/* from the two-character delimiters.				*/

#define VLOG_DELIMITERS "///**/(**)#(X,;:(){}[]="
#define VLOG_PIN_NAME_DELIMITERS "///**/(**)X()"

#define VERILOG_EXTENSION ".v"

/*------------------------------------------------------*/
/* Ports and instances are hashed for quick lookup but	*/
/* also placed in a linked list so that they can be	*/
/* output in the same order as the original file.	*/
/*------------------------------------------------------*/

struct portrec {
    char *name;
    char *net;
    int   direction;
    struct portrec *next;
};

struct instance {		/* Hashed by instance name */
    char *instname;
    char *cellname;
    struct portrec *portlist;
    struct hashtable propdict;	/* Instance properties */
    struct instance *next;
};

/*------------------------------------------------------*/
/* Basic cell definition (hashed by name)		*/
/*------------------------------------------------------*/

struct cellrec {
    char *name;			/* Cellname */

    struct hashtable nets;		/* Internal nets */
    struct hashtable propdict;		/* Properties */

    struct portrec *portlist;
    struct instance *instlist;
};

/*------------------------------------------------------*/

#define PORT_NONE	 0	
#define PORT_INPUT	 1
#define PORT_OUTPUT	 2
#define PORT_INOUT	 3

#define BUS_NONE	-1

/*------------------------------------------------------*/
/* Net structure (hashed by root name, if a bus)	*/
/*------------------------------------------------------*/

struct netrec {
    int start;		/* start index, if a bus */
    int end;		/* end index, if a bus */
};

/*------------------------------------------------------*/
/* Structure for stacking nested module definitions	*/
/*------------------------------------------------------*/

struct cellstack {
   struct cellrec *cell;
   struct cellstack *next;
};

/*------------------------------------------------------*/
/* Structure for nested "include" files			*/
/*------------------------------------------------------*/

struct filestack {
    FILE *file;
    struct filestack *next;
};

/*------------------------------------------------------*/
/* External function declarations 			*/
/*------------------------------------------------------*/

extern void IncludeVerilog(char *, struct cellstack **, int);
extern struct cellrec *ReadVerilog(char *);
extern struct instance *AppendInstance(struct cellrec *cell, char *cellname);
extern struct instance *PrependInstance(struct cellrec *cell, char *cellname);
extern struct portrec *InstPort(struct instance *inst, char *portname, char *netname);
extern void *BusHashLookup(char *s, struct hashtable *table);

// readverilog.h
