//*****************************************************************************
//
// A couple of useful procedures.
//
//*****************************************************************************

#ifndef _CLASSAD_UTIL_H
#define _CLASSAD_UTIL_H
#include "classad.h"

//
// Given a ClassAd and a ClassAdList, a pointer to the first match in the 
// list is returned. 
// If match found, 1 returned. Otherwise, 0 returned.
//
int FirstMatch(ClassAd *, ClassAdList *, ClassAd *&);

//
// Given a ClassAd and a ClassAdList, a list of pointers to the matches in the
// list is returned. 
// If match(es) found, 1 returned. Otherwise, 0 returned.
//
int AllMatch(ClassAd *, ClassAdList *, ClassAdList *&);

//
// Given a constraint, determine whether the first ClassAd is > the second 
// one, in the sense whether the constraint is evaluated to be TRUE. 
// The constraint should look just like a "Requirement". Every variable has 
// a prefix "MY." or "TARGET.". A variable with prefix "MY." should be 
// evaluated against the first ClassAd and a variable with prefix "TARGET."
// should be evaluated against the second ClassAd.
// If >, return 1;
// else, return 0. (Note it can be undefined.)  
//
int IsGT(ClassAd *, ClassAd *, char *);

//
// Sort a list according to a given constraint in ascending order.
//
void SortClassAdList(ClassAdList *, ClassAdList *&, char *);

#endif
