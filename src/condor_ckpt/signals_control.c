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
#include "signals_control.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"

/* Please read documentation in signals_control.h */

static int ckpt_disable_count = 0;
static int ckpt_deferred = 0;
static int ckpt_deferred_signal = 0;

void _condor_ckpt_disable()
{
	ckpt_disable_count++;
}

void _condor_ckpt_enable()
{
	int scm;

	ckpt_disable_count--;

	if(ckpt_disable_count<=0) {
		ckpt_disable_count=0;
		if(ckpt_deferred) {
			dprintf(D_ALWAYS,"_condor_ckpt_enable: forcing a deferred checkpoint on signal %d\n",ckpt_deferred_signal);
			ckpt_deferred=0;
			scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			kill( getpid(), ckpt_deferred_signal );
			SetSyscalls( scm );
		}
	}
}

int _condor_ckpt_is_disabled()
{
	return ckpt_disable_count;
}

int _condor_ckpt_is_deferred()
{
	return ckpt_deferred;
}

void _condor_ckpt_defer( int s )
{
	ckpt_deferred = 1;
	ckpt_deferred_signal = s;
	dprintf(D_ALWAYS,"received ckpt signal %d, but deferred it for later\n",s);
}

sigset_t _condor_signals_disable()
{
	sigset_t mask, omask;
	int scm;

	sigfillset( &mask );
	sigdelset( &mask, SIGABRT );
	sigdelset( &mask, SIGBUS );
	sigdelset( &mask, SIGFPE );
	sigdelset( &mask, SIGILL );
	sigdelset( &mask, SIGSEGV );
	sigdelset( &mask, SIGTRAP );

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	if( sigprocmask(SIG_BLOCK,&mask,&omask) < 0 ) {
		dprintf(D_ALWAYS, "_condor_signals_disable: sigprocmask failed: %s\n", strerror(errno));
	}
	SetSyscalls( scm );

	return omask;
}

void _condor_signals_enable( sigset_t mask )
{
	if( sigprocmask(SIG_SETMASK,&mask,0) < 0 ) {
		dprintf(D_ALWAYS, "_condor_signals_enable: sigprocmask failed: %s\n", strerror(errno));
	}
}	


