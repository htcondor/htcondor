/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
//*****************************************************************************
//
// A couple of useful procedures.
//
//*****************************************************************************

#ifndef _CLASSAD_UTIL_H
#define _CLASSAD_UTIL_H
#include "condor_classad.h"

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
#if 0
void SortClassAdList(ClassAdList *, ClassAdList *&, char *);
#endif

#endif
