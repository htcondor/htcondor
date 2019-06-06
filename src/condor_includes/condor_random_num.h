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

#ifndef _CONDOR_RANDOM_NUM
#define _CONDOR_RANDOM_NUM

#if defined(__cplusplus)
extern "C" {
#endif

// First, the "insecure" variants: these generate numbers that are
// well-distributed across the presecribed intervals but may be easy
// to guess the next in the sequence given a small number of values.
//
// As the name suggests, these should only be used in cases where
// security is entirely irrelevant (example: condor_mkstemp, where
// there will be an additional layer of checking if the attacker can
// guess the random number.
int set_seed_insecure(int seed);
int get_random_int_insecure(void);
unsigned int get_random_uint_insecure(void);
float get_random_float_insecure(void);
int timer_fuzz(int period);

// The cryptographically secure RNG variants.  These are higher quality,
// mostly-non-blocking random number generators suitable for things like
// session keys.  An attacker would need to observe an enormous number of
// outputs of these to guess the PRNG state.
// get_csrng_int() returns a non-negative value.
int get_csrng_int(void);
unsigned int get_csrng_uint(void);

// We do not currently have a true RNG (i.e., /dev/random on linux); those
// are more suitable for long-term key material, which HTCondor does not
// currently use.

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_RANDOM_NUM */
