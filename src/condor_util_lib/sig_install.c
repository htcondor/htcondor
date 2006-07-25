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
#include "sig_install.h"
#include "condor_debug.h"


#ifdef __cplusplus 
extern "C" {
#endif

void
install_sig_handler( int sig, SIG_HANDLER handler )
{
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;

	if( sigaction(sig,&act,0) < 0 ) {
		EXCEPT( "sigaction" );
	}
}

void
install_sig_handler_with_mask( int sig, sigset_t* set, SIG_HANDLER handler )
{
	struct sigaction act;

	act.sa_handler = handler;
	act.sa_mask = *set;
	act.sa_flags = 0;

	if( sigaction(sig,&act,0) < 0 ) {
		EXCEPT( "sigaction" );
	}
}

void
unblock_signal( int sig)
{
    sigset_t    set;
    if ( sigprocmask(SIG_SETMASK,0,&set)  == -1 ) {
        EXCEPT("Error in reading procmask, errno = %d\n", errno);
    }
    sigdelset(&set, sig);
    if ( sigprocmask(SIG_SETMASK,&set, 0)  == -1 ) {
        EXCEPT("Error in setting procmask, errno = %d\n", errno);
    }
}	

void
block_signal( int sig)
{
    sigset_t    set;
    if ( sigprocmask(SIG_SETMASK,0,&set)  == -1 ) {
        EXCEPT("block_signal:Error in reading procmask, errno = %d\n", errno);
    }
    sigaddset(&set, sig);
    if ( sigprocmask(SIG_SETMASK,&set, 0)  == -1 ) {
        EXCEPT("block_signal:Error in setting procmask, errno = %d\n", errno);
    }
}	

#ifdef __cplusplus 
}
#endif
