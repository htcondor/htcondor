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

 

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

/*
  Tell the operating system that we want to operate in "POSIX
  conforming mode".  Isn't it ironic that such a request is itself
  non-POSIX conforming?
*/
#if defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/sysinfo.h>
#define A_POSIX 2		// from exec.h, which I prefer not to include here...
extern "C" {
	int setsysinfo( unsigned, char *, unsigned, unsigned, unsigned );
}
void
set_posix_environment()
{
	int		name_value[2];

	name_value[0] = SSIN_PROG_ENV;
	name_value[1] = A_POSIX;

	if( setsysinfo(SSI_NVPAIRS,(char *)name_value,1,0,0) < 0 ) {
		EXCEPT( "setsysinfo" );
	}
}
#else
void
set_posix_environment() {}
#endif
