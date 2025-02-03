/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_random_num.h"

/*
   Note that we originally wanted to replace all of this as a thin shim
   over the CSRNG (less code the better...) variants.  However, libcondorapi.so
   utilizes some of these function calls and we don't want to have an OpenSSL
   dependency in libcondorapi.so.
 */

/* srand48, lrand48, and drand48 seem to be available on all Condor
   platforms except WIN32.  -Jim B. */

static char initialized = 0;

/* (re)sets the seed for the random number generator -- may be useful
   for generating less predictable random numbers */
int set_seed(int seed)
{
	if (seed == 0) {
		seed = int(time(nullptr) & 0xffffffff);
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
int get_random_int_insecure( void )
{
	if (!initialized) {
		set_seed(getpid());
	}

#if defined(WIN32)
	return rand();
#else
	return (int) (lrand48() & INT_MAX);
#endif
}


/* returns a random floating point number in this range: [0.0, 1.0), trying
   to use best random number generator available on each platform */
float get_random_float_insecure( void )
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

static
double get_random_double( void )
{
    if (!initialized) {
        set_seed(getpid());
    }

#if defined(WIN32)
    return (((double)rand())*(((unsigned int)1)<<15) + (double)rand()) /
	       (((double)RAND_MAX)*(((unsigned int)1)<<15) + (double)RAND_MAX + 1);
#else
    return drand48();
#endif
}

/* returns a random unsigned integer, trying to use best random number
   generator available on each platform */
unsigned int
get_random_uint_insecure( void )
{
	if (!initialized) {
		set_seed(getpid());
	}

	/*  get_random_float() doesn't have enough precision to use here.
	    Since get_random_double returns [0.0, 1.0), add one to UINT_MAX
		to ensure the probability fencepost error doesn't
		happen and I actually can get ALL the numbers from 0 up to and
		including UINT_MAX */
	return (unsigned) (get_random_double() * (((double)UINT_MAX)+1) );
}

/* returns a fuzz factor to be added to a timer period to decrease
   chance of many daemons being synchronized */
int
timer_fuzz(int period)
{
	int fuzz = period/10;
	if( fuzz <= 0 ) {
		if( period <= 0 ) {
			return 0;
		}
		fuzz = period - 1;
	}
	fuzz = (int)( get_random_float_insecure() * ((float)fuzz+1) ) - fuzz/2;

	if( period + fuzz <= 0 ) { // sanity check
		fuzz = 0;
	}
	return fuzz;
}
