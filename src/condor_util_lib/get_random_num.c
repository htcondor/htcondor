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

#include "condor_common.h" 

/* srand48, lrand48, and drand48 seem to be available on all Condor
   platforms except WIN32.  -Jim B. */

static char initialized = 0;

/* (re)sets the seed for the random number generator -- may be useful
   for generating less predictable random numbers */
int set_seed(int seed)
{
	if (seed == 0) {
		seed = time(0);
	}

#if defined(WIN32)
	srand(seed);
#else
	srand48(seed);
#endif
	initialized = 1;

	return seed;
}

/* returns a random positive integer, trying to use best random number
   generator available on each platform */
int get_random_int()
{
	if (!initialized) {
		set_seed(0);
	}

#if defined(WIN32)
	return rand();
#else
	return (int) (lrand48() & MAXINT);
#endif
}

/* returns a random floating point number between 0.0 and 1.0, trying
   to use best random number generator available on each platform */
float get_random_float()
{
	if (!initialized) {
		set_seed(0);
	}

#if defined(WIN32)
	return (float)rand()/(float)RAND_MAX;
#else
	return (float) drand48();
#endif
}
