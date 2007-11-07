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

   This method is used by a few of the tests to waste a second.  it
   used to be defined seperately in each test.  Aside from code
   duplication, b/c of the platform-specific nature of it, it was a
   total pain to maintain.  Additionally, it was incredibly
   inaccurate, since all the values used were last calibrated in about
   1995, and processors have gotten, um, a little faster since
   then. :) All these seperate versions were difficult to maintain
   each time we tried to port to a new architecture, or to clean up
   the platform-identifying symbols we use.

   Instead of relying on hard-coded limits for how many times we
   should increment a variable to waste a second, we compute a
   reasonable value using a seperate program, compute_bogokips.  To
   avoid overflowing an int, we use a nested for() loop, and to try to
   have a fairly accurate number, we use kilo-instructions-per-second
   ("KIPS") instead of the more common mega-instructions-per-second
   ("MIPS").  However, these aren't really instructions, they're
   bogus, hence "compute_bogokips".  This program outputs a header
   file we can include here with "BOGOKIPS" #define'ed to the right
   value.  See compute_bogokips.c for more details.

   Author: Derek Wright <wright@cs.wisc.edu> 2003-11-23

*/

#include "x_my_bogokips.h"

#ifdef __cplusplus
extern "C" {
#endif

void
x_waste_a_second()
{
	int	i, j;

	for( i=0; i<BOGOKIPS; i++ ) {
		for(j=0; j<1000; j++ );
	}
}

#ifdef __cplusplus
}
#endif

