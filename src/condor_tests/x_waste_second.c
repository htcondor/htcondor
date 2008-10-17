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


/* 

   This method is used by a few of the tests to waste a second. It
   used to spin through the for loop a pre-defined number of times
   that should take about a second. This number was calculated by a
   separate program (x_compute_bogokips) that was executed just before
   this file was compiled. However, I've seen execution times vary by
   an order of magnitude. I believe this was caused by heavy load on
   the machine.

   Now, we call gettimeofday() periodically while spinning in the for
   loop until a full second has passed. This adds a new dependency on
   the syscall library's gettimeofday(), which goes back to the shadow
   on the first call. I think that's better than occassionally returning
   after one tenth of a second instead of one second, which causes
   tests to fail when the code isn't broken.

       Jaime Frey (June 16, 2008)

*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/time.h>

void
x_waste_a_second()
{
	int i;
	struct timeval old_tv;
	struct timeval new_tv;

	gettimeofday( &old_tv, NULL );

	while ( 1 ) {
		for ( i = 0; i < 10000; i++ ) {
		}
		gettimeofday( &new_tv, NULL );
		if ( ( new_tv.tv_sec > old_tv.tv_sec + 1 ) ||
			 ( new_tv.tv_sec == old_tv.tv_sec + 1 &&
			   new_tv.tv_usec > old_tv.tv_usec ) ) {
			break;
		}
	}
}

#ifdef __cplusplus
}
#endif

