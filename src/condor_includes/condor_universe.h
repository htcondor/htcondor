/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef CONDOR_UNIVERSE_H
#define CONDOR_UNIVERSE_H

#include "condor_header_features.h"

BEGIN_C_DECLS

/*
Warning: These symbols must stay in sync
with the strings in condor_universe.c

Please also sync condor__UniverseType in 
src/condor_schedd.V6/gsoap_schedd_types.h
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
#define CONDOR_UNIVERSE_GRID      9  /* Jobs managed by condor_gmanager */
#define CONDOR_UNIVERSE_JAVA      10 /* Jobs for the Java Virtual Machine */
#define CONDOR_UNIVERSE_PARALLEL  11 /* Generalized parallel jobs */
#define CONDOR_UNIVERSE_LOCAL     12 /* Job run locally by the schedd */
#define CONDOR_UNIVERSE_VM        13 /* Job run under virtual machine (xen,vmware) */
#define CONDOR_UNIVERSE_MAX       14 /* A placeholder, not a universe. */

/* To get the name of a universe, call this function */
const char *CondorUniverseName( int universe );
const char *CondorUniverseNameUcFirst( int universe );

/* To get the number of a universe from a string, call this.  Returns
   0 if the given string doesn't correspond to a known universe */
int CondorUniverseNumber( const char* univ );

BOOLEAN universeCanReconnect( int universe );

END_C_DECLS

#endif


