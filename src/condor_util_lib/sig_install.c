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
