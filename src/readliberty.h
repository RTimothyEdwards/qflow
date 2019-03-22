/*--------------------------------------------------------------*/
/* readliberty.h ---						*/
/*								*/
/* Header file for readliberty.c				*/
/*--------------------------------------------------------------*/

#define LIB_LINE_MAX  65535

#define INIT		0
#define LIBBLOCK	1
#define CELLDEF		2
#define PINDEF		3
#define TIMING		4

// Pin types (a clock is also an input)
#define	PIN_UNKNOWN	-1
#define PIN_INPUT	0
#define PIN_CLOCK	1
#define PIN_OUTPUT	2

// Function translation
#define GROUPBEGIN	1
#define GROUPEND	2
#define SIGNAL		3
#define OPERATOR	4
#define XOPERATOR	5
#define SEPARATOR	6

/*--------------------------------------------------------------*/
/* Database							*/
/*--------------------------------------------------------------*/

typedef struct _lutable *lutableptr;

typedef struct _lutable {
    char *name;
    char invert;	// 0 if times x caps, 1 if caps x times
    char *var1;		// Name of array in index1
    char *var2;		// Name of array in index2
    int  tsize;		// Number of entries in time array
    int  csize;		// Number of entries in cap array
    double *times;	// Time array (units fF)
    double *caps;	// Cap array (units ps)
    lutableptr next;
} LUTable;

/*--------------------------------------------------------------*/

typedef struct _bustype *bustypeptr;

typedef struct _bustype {
    char *name;
    int  from;		// Bus array first index
    int  to;		// Bus array last index
    bustypeptr next;
} BUStype;

/*--------------------------------------------------------------*/

typedef struct _pin *pinptr;

typedef struct _pin {
    char *name;
    int	type;
    double cap;
    double maxtrans;
    double maxcap;
    pinptr next;
} Pin;

/*--------------------------------------------------------------*/

typedef struct _cell *cellptr;

typedef struct _cell {
    char *name;
    char *function;
    Pin	 *pins;
    double area;
    double slope;
    double mintrans;
    LUTable *reftable;
    double *times;	// Local values for time indexes, if given
    double *caps;	// Local values for cap indexes, if given
    double *values;	// Matrix of all values
    cellptr next;
} Cell;

/*--------------------------------------------------------------*/

extern int get_pintype(Cell *curcell, char *pinname);
extern int get_pincap(Cell *curcell, char *pinname, double *retcap);
extern int get_values(Cell *curcell, double *retdelay, double *retcap);
extern Cell *read_liberty(char *libfile, char *pattern);
extern Cell *get_cell_by_name(Cell *cell, char *name);
extern Pin *get_pin_by_name(Cell *curcell, char *pinname);
extern void delete_cell_list(Cell *cell);

/*--------------------------------------------------------------*/
