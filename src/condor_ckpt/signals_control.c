#include "signals_control.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"

static int disable_count = 0;

void _condor_signals_disable()
{
	int sigscm;
	sigset_t mask;

	disable_count++;

       	/* Block signals requesting an exit or checkpoint for duration of
	   system call. */

	sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	sigemptyset( &mask );
	sigaddset( &mask, SIGTSTP );
	sigaddset( &mask, SIGUSR1 );
	sigaddset( &mask, SIGUSR2 );

	if( sigprocmask(SIG_BLOCK,&mask,0) < 0 ) {
		dprintf(D_ALWAYS, "_condor_signals_disable: sigprocmask failed: %s\n", strerror(errno));
	}

	SetSyscalls( sigscm );
}

void _condor_signals_enable()
{
	int sigscm;
	sigset_t mask;

	disable_count--;

	if(disable_count<=0) {
		disable_count=0;

		sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		sigemptyset( &mask );
		sigaddset( &mask, SIGTSTP );
		sigaddset( &mask, SIGUSR1 );
		sigaddset( &mask, SIGUSR2 );

		if( sigprocmask(SIG_UNBLOCK,&mask,0) < 0 )
			dprintf(D_ALWAYS, "_condor_signals_enable: sigprocmask failed: %s\n", strerror(errno));

		SetSyscalls( sigscm );
	}
}	


