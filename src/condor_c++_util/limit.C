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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"


extern "C" {

void
limit( int resource, rlim_t new_limit )
{
	int		scm;
	struct	rlimit lim;

	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );

		/* Find out current limits */
	if( getrlimit(resource,&lim) < 0 ) {
		EXCEPT( "getrlimit(%d,0x%x)", resource, &lim );
	}

		/* Don't try to exceed the max */
	if( new_limit > lim.rlim_max ) {
		new_limit = lim.rlim_max;
	}

		/* Set the new limit */
	lim.rlim_cur = new_limit;
	if( setrlimit(resource,&lim) < 0 ) {
		EXCEPT( "setrlimit(%d,0x%x), errno: %d", resource, &lim, errno );
	}

	(void)SetSyscalls( scm );
}

} /* extern "C" */
