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


#include "condor_common.h" 
#include "condor_random_num.h"

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
int get_random_int( void )
{
	if (!initialized) {
		set_seed(getpid());
	}

#if defined(WIN32)
	return rand();
#else
	return (int) (lrand48() & MAXINT);
#endif
}

/* returns a random floating point number in this range: [0.0, 1.0), trying
   to use best random number generator available on each platform */
float get_random_float( void )
{
	if (!initialized) {
		set_seed(getpid());
	}

#if defined(WIN32)
	return (float)rand()/((float)RAND_MAX + 1);
#else
	return (float) drand48();
#endif
}

/* returns a random unsigned integer, trying to use best random number
   generator available on each platform */
unsigned int get_random_uint( void )
{
	if (!initialized) {
		set_seed(getpid());
	}

	/* since get_random_float returns [0.0, 1.0), add one to UINT_MAX
		to ensure the probability fencepost error doesn't
		happen and I actually can get ALL the numbers from 0 up to and
		including UINT_MAX */
	return get_random_float() * (((double)UINT_MAX)+1);
}





