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
#include "condor_syscalls.h"
#include "condor_debug.h"

/*
   Block all signals for Condor checkpoint and RSC critical sections.
*/
sigset_t
condor_block_signals()
{
	int sigscm;
	sigset_t mask, omask;

	sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	sigfillset( &mask );
	if( sigprocmask(SIG_BLOCK,&mask,&omask) < 0 ) {
		dprintf(D_ALWAYS, "sigprocmask failed: %s\n", strerror(errno));
		return omask;
	}
	SetSyscalls( sigscm );
	return omask;
}

void
condor_restore_sigmask(sigset_t omask)
{
	int sigscm;

	sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	/* Restore previous signal mask - generally unblocks TSTP and USR1 */
	if( sigprocmask(SIG_SETMASK,&omask,0) < 0 ) {
		dprintf(D_ALWAYS, "sigprocmask failed: %s\n", strerror(errno));
		return;
	}
	SetSyscalls( sigscm );
}

