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

