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
#ifdef HAVE_EXT_OPENSSL
#include <openssl/rand.h>
#endif

//
// Note the insecure variants are actually implemented using the CSRNG
// cousins; this is because we couldn't identify any downside in doing
// so (nothing here is time-critical; there's no need to make things
// reproducible).
//
// That said, we leave the *interfaces* such that developers must declare
// what quality of random numbers they need, buying flexibility in the future.
//


/* returns a random non-negative integer, trying to use best random number
   generator available on each platform */
int get_random_int_insecure( void )
{
	return get_csrng_int();
}


/* returns a random floating point number in this range: [0.0, 1.0), trying
   to use best random number generator available on each platform */
float get_random_float_insecure( void )
{
	return static_cast<float>(get_csrng_uint() % RAND_MAX) /
              (static_cast<float>(RAND_MAX) + 1);
}

/* returns a random unsigned integer, trying to use best random number
   generator available on each platform */
unsigned int
get_random_uint_insecure( void )
{
	return get_csrng_uint();
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
