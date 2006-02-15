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

