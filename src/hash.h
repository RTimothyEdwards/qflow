#ifndef _HASH_H
#define _HASH_H

// #define OBJHASHSIZE 997
#define OBJHASHSIZE 99997

struct hashlist {
  char *name;
  void *ptr;
  struct hashlist *next;
};

extern void InitializeHashTable(struct hashlist **tab);
extern int RecurseHashTable(struct hashlist **hashtab,
	int (*func)(struct hashlist *elem));
extern int RecurseHashTableValue(struct hashlist **hashtab,
	int (*func)(struct hashlist *elem, int), int);
extern struct nlist *RecurseHashTablePointer(struct hashlist **hashtab,
	struct nlist *(*func)(struct hashlist *elem,
	void *), void *pointer);


extern int CountHashTableEntries(struct hashlist *p);
extern int CountHashTableBinsUsed(struct hashlist *p);
extern void HashDelete(char *name, struct hashlist **hashtab);
extern void HashIntDelete(char *name, int value, struct hashlist **hashtab);
extern void HashKill(struct hashlist **hashtab);

/* these functions return a pointer to a hash list element */
extern struct hashlist *HashInstall(char *name, struct hashlist **hashtab);
extern struct hashlist *HashPtrInstall(char *name, void *ptr, 
		struct hashlist **hashtab);
extern struct hashlist *HashIntPtrInstall(char *name, int value, void *ptr, 
		struct hashlist **hashtab);

/* these functions return the ->ptr field of a struct hashlist */
extern void *HashLookup(char *s, struct hashlist **hashtab);
extern void *HashIntLookup(char *s, int i, struct hashlist **hashtab);
extern void *HashFirst(struct hashlist **hashtab);
extern void *HashNext(struct hashlist **hashtab);

extern unsigned long hashnocase(char *s);
extern unsigned long hash(char *s);

extern int (*matchfunc)(char *, char *);
/* matchintfunc() compares based on the name and the first	*/
/* entry of the pointer value, which is cast as an integer	*/
extern int (*matchintfunc)(char *, char *, int, int);
extern unsigned long (*hashfunc)(char *);

/* the matching functions themselves */
extern int match(char *s1, char *s2);
extern int matchnocase(char *s1, char *s2);

#endif /* _HASH_H */
