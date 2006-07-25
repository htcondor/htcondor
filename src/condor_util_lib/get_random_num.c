/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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

/* returns a random floating point number in this range: [0.0, 1.0), trying
   to use best random number generator available on each platform */
float get_random_float()
{
	if (!initialized) {
		set_seed(0);
	}

#if defined(WIN32)
	return (float)rand()/((float)RAND_MAX + 1);
#else
	return (float) drand48();
#endif
}

/* returns a random unsigned integer, trying to use best random number
   generator available on each platform */
unsigned int get_random_uint()
{
	if (!initialized) {
		set_seed(0);
	}

	/* since get_random_float returns [0.0, 1.0), add one to UINT_MAX
		to ensure the probability fencepost error doesn't
		happen and I actually can get ALL the numbers from 0 up to and
		including UINT_MAX */
	return get_random_float() * (((double)UINT_MAX)+1);
}





