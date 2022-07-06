/***************************************************************
 *
 * Copyright (C) 2019, Condor Team, Computer Sciences Department,
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

/*
 * This utilizes OpenSSL cryptography routines in order to provide high-quality
 * PRNG data.
 *
 * If there's no high-quality PRNG available, these routines will fail to compile
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_random_num.h"
#include <openssl/rand.h>

#include <chrono>

static bool prng_initialized = false;

static void add_seed_prng() {
	if (prng_initialized) return;

	constexpr int size = 128;
	unsigned char * buf = (unsigned char *) malloc(size);
	ASSERT(buf);
	// RAND_seed mixes in a seed to the existing entropy pool; this
	// is also pre-seeded by OpenSSL using /dev/urandom.  This is
	// simply fallback code for platforms without /dev/urandom.
	//
	// We assume a determined attacker can signficantly influence the output
	// of the insecure variant; this is OK in this context.
	for (int i = 0; i < size; i++) {
                auto epoch_time = std::chrono::high_resolution_clock::now().time_since_epoch();
                auto epoch_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch_time).count();
                auto seed = static_cast<unsigned char>(epoch_ns);
		buf[i] = seed;
	}
	RAND_seed(buf, size);
	free(buf);
	prng_initialized = true;
}


/* returns a random non-negative integer that is effectively impossible
   for an attacker to guess.
 */
int get_csrng_int( void )
{
	add_seed_prng();

	int retval = 0;
	RAND_bytes(reinterpret_cast<unsigned char *>(&retval), sizeof(retval));
	return retval & INT_MAX;
}


/* returns a random unsigned int from the PRNG.
 */
unsigned int get_csrng_uint( void )
{
	add_seed_prng();

        unsigned retval = 0;
        RAND_bytes(reinterpret_cast<unsigned char *>(&retval), sizeof(retval));
	return retval;
}
