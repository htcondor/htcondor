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

#ifndef CONDOR_UNIVERSE_H
#define CONDOR_UNIVERSE_H

#include "condor_header_features.h"

BEGIN_C_DECLS

/*
Warning: These symbols must stay in sync
with the strings in condor_universe.c
*/

#define CONDOR_UNIVERSE_MIN       0  /* A placeholder, not a universe */
#define CONDOR_UNIVERSE_STANDARD  1  /* Single process relinked jobs */
#define CONDOR_UNIVERSE_PIPE      2  /* A placeholder, no longer used */
#define CONDOR_UNIVERSE_LINDA     3  /* A placeholder, no longer used */
#define CONDOR_UNIVERSE_PVM       4  /* Parallel Virtual Machine apps */
#define CONDOR_UNIVERSE_VANILLA   5  /* Single process non-relinked jobs */
#define CONDOR_UNIVERSE_PVMD      6  /* PVM daemon process */
#define CONDOR_UNIVERSE_SCHEDULER 7  /* A job run under the schedd */
#define CONDOR_UNIVERSE_MPI       8  /* Message Passing Interface jobs */
#define CONDOR_UNIVERSE_GLOBUS    9  /* Jobs managed by condor_gmanager */
#define CONDOR_UNIVERSE_JAVA      10 /* Jobs for the Java Virtual Machine */
#define CONDOR_UNIVERSE_PARALLEL  11 /* Generalized parallel jobs */
#define CONDOR_UNIVERSE_MAX       12 /* A placeholder, not a universe. */

/* To get the name of a universe, call this function */

const char *CondorUniverseName( int universe );

END_C_DECLS

#endif


