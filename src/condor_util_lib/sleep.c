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

