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
		g_mii = get_random_uint();
		g_initialized = TRUE;
	}

	*t = time(NULL);
	*mii = g_mii;

	/* when G_ii is UINT_MAX, then this wraps to zero */
	g_mii++;
}


