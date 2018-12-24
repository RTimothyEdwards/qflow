/*
 * def.h --
 *
 * This file includes the DEF I/O functions
 *
 */

#ifndef _DEFINT_H
#define _DEFINT_H

#include "lef.h"

extern int numSpecial;
extern int DefRead(char *inName, float *);

extern GATE DefFindGate(char *name);
extern NET DefFindNet(char *name);
extern char *DefDesign();

/* External access to hash tables for recursion functions */
extern struct hashtable InstanceTable;
extern struct hashtable NetTable;

#endif /* _DEFINT_H */
