/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
   This file implements our own version of sleep that doesn't use
   SIGALRM.  It also has a Sleep() function that sleeps for the given
   number of miliseconds.  It is implemented using select().
   Author: Derek Wright <wright@cs.wisc.edu> 1/12/98
*/

#include "condor_common.h"

unsigned int
sleep( unsigned int seconds ) 
{
	struct timeval timer;
	timer.tv_sec = seconds;
	timer.tv_usec = 0;

	select( 0, NULL, NULL, NULL, &timer );
	return 0;
}


unsigned int
Sleep( unsigned int milliseconds ) 
{
	struct timeval timer;
	timer.tv_sec = 0;
	timer.tv_usec = milliseconds;

	select( 0, NULL, NULL, NULL, &timer );
	return 0;
}

