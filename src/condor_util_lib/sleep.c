/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

