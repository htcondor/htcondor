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
