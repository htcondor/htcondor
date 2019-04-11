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

static int g_initialized = FALSE;
static unsigned int g_mii= FALSE;

/* Get a monotonically increasing number which is modulo UINT_MAX and
	the seconds since epoch. As long as I don't call this function
	UINT_MAX times in one second, I shouldn't get the same
	pair twice. The monotonically increasing number will be initially
	initialized to a random number to lessen the chances of an ID
	collision when a daemon core program starts and dies multiple times
	in one second. The probability should be 1/UINT_MAX, but a collision
	will eventually happen. There is basically no way to prevent this
	collision since it would involve a system wide unique id, which we
	don't have access to.
 
	*t: Address of a time_t variable in which seconds since epoch
		are placed.

	*mii: Address of an unsigned int in which a monotonically
		increasing number held by daemon core that is initially 
		initialized to random is placed.
*/

void
create_id(time_t *t, unsigned int *mii)
{
	if (g_initialized == FALSE) {
		g_mii = get_random_uint_insecure();
		g_initialized = TRUE;
	}

	*t = time(NULL);
	*mii = g_mii;

	/* when G_ii is UINT_MAX, then this wraps to zero */
	g_mii++;
}


