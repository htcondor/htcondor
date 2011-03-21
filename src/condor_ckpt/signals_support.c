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

#if DOES_SAVE_SIGSTATE

#include "condor_debug.h"		/* for EXCEPT */
#include "condor_sys.h"
#include "errno.h"
#include "condor_debug.h"

extern	char	*strerror();
extern int SetSyscalls(int);
extern int MappingFileDescriptors(void);
extern int SIGACTION(int signum, const struct sigaction *act, struct sigaction *oldact);
extern int SIGSUSPEND(const sigset_t *mask);

#define MASK_TYPE int

#if defined(NSIG)
#define NUM_SIGS NSIG
#else
#error Must know NSIG -  see comment here in the code !
/* If you get the error message above, you need to devise a way so that
 * NSIG is set to be the maximum number of signals supported on this
 * platform.  Usually, NSIG is defined in <signal.h>, but you may need
 * to edit condor_includes/condor_fix_signal.h to define or undefine something
 * in order to fix this.  DO NOT JUST DEFINE NSIG SOMEPLACE YOURSELF.  Why?  
 * Say you define NSIG to be 25 for the Foo operating system.  A few months
 * later, Foo 2.0 comes out and has 5 new signals for a total of 30.  Your
 * own little define still says 25, and is now wrong.  Get it? By using a 
 * true value in the system header files you are ensured they will get updated
 * with new versions of the operating system.
 */
#endif

void display_sigstate( int line, const char * file );
void _condor_save_sigstates(void);
void _condor_restore_sigstates(void);

typedef int				SS_TYPE;

struct signal_states_struct {
	int nsigs;		/* the number of signals supported on this platform +1 */
	sigset_t user_mask;
	struct sigaction actions[NUM_SIGS];
	sigset_t pending;
	SS_TYPE sig_stack; 
} ;

static struct signal_states_struct signal_states;


/* Here we save the signal state of the user process.  This is called
 * when we are checkpointing.  */
void _condor_save_sigstates()
{
	sigset_t mask;
	int scm;
	int sig;

	/* First, save the user's signal mask.  This must happen before we modify it in any way. */
	sigprocmask(0,0,&signal_states.user_mask);

	/* Disable handing of user-mode signal handlers. This prevents any signal handlers from getting called in local unmapped mode. */
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK,&mask,0);

	/* Switch to local unmapped mode.  This msut happen _after_ blocking user signal handlers */
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	/* Now, disable signal handling _again_.  This prevents checkpointing signals from interrupting us while checkpointing */
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK,&mask,0);

	/* Save pending signal information */
	sigpending( &(signal_states.pending) );

	if ( signal_states.nsigs == 0 ) {
#if !SIGISMEMBER_IS_BROKEN
		/* This code only runs once; here we figure out how many signals
		 * this OS supports and verify our array (of size NUM_SIGS) is
		 * big enough to store all the state. */
		sigfillset(&mask);
		sig = 1;
		while ( sigismember(&mask,sig) > 0 ) sig++;
		signal_states.nsigs = sig;
		if ( sig > NUM_SIGS ) {
			dprintf( D_ALWAYS,
					 "must recompile to support %d signals; current max=%d\n",
					 sig, NUM_SIGS);
			Suicide();
		}
#else 
		signal_states.nsigs = NUM_SIGS;
#endif
	}

	/* Save handler information for each signal */
	for ( sig=1; sig < signal_states.nsigs; sig++ ) {
		sigaction(sig,NULL,&(signal_states.actions[sig]));
	}

	(void) SetSyscalls( scm );
}

/* Here we restore the signal state of the user process.  This is called
 * upon a restart of a checkpointed process.  The data space must be
 * restored _before_ calling this function (all signal state info is saved
 * in the static structure signal_states) */
void _condor_restore_sigstates()
{
	int scm;
	int sig;
	pid_t mypid;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	/* Restore signal actions */
	for ( sig=1; sig < signal_states.nsigs; sig++ ) {
		switch (sig) {
			/* Do not fiddle with the special Condor signals under
			 * any circumstance! */
			case SIGUSR2:
			case SIGTSTP:
			case SIGKILL:
			case SIGSTOP:
			case SIGCONT:
				continue;
		}
		sigaction(sig, &(signal_states.actions[sig]), NULL);
	}

	/* Restore pending signals, again ignoring special Condor sigs */
	/* This code assumes that all signals are blocked. */
	mypid = getpid();
	for ( sig=1; sig < signal_states.nsigs; sig++ ) {
		switch (sig) {
			/* Do not fiddle with the special Condor signals under
			 * any circumstance! */
			case SIGUSR2:
			case SIGTSTP:
			case SIGKILL:
			case SIGSTOP:
			case SIGCONT:
				continue;
		}
		if ( sigismember(&(signal_states.pending),sig) == 1 ) {
			kill(mypid,sig);
		}
	}

	(void) SetSyscalls( scm );

	/* Finally, put the user's signal mask back. This must happen _after_ SetSyscalls so that any pending unblocked signals are handled in remote, mapped mode. */

	sigprocmask(SIG_SETMASK,&signal_states.user_mask,0);
}


#if defined (SYS_sigvec)
sigvec( int sig, const struct sigvec * vec, struct sigvec * ovec )
{
	int rval;
	struct	sigvec	nvec;

	if( ! MappingFileDescriptors() ) {
	} else {
		switch (sig) {
				/* disallow the special Condor signals */
				/* this list could be shortened */
		case SIGUSR2:
		case SIGTSTP:
		case SIGCONT:
			rval = -1;
			errno = EINVAL;
			break;
		default:
			if (vec) {
				memcpy( (char *) &nvec,(char *) vec, sizeof nvec );
				/* while in the handler, block checkpointing */
				nvec.sv_mask |= __sigmask( SIGTSTP );
				nvec.sv_mask |= __sigmask( SIGUSR2 );
			}

			rval = syscall( SYS_sigvec, sig, vec ? &nvec : vec, ovec );
			break;
		}
	}

	return( rval );
}
#endif

#if defined (SYS_sigvec)
_sigvec( int sig, const struct sigvec vec, struct sigvec ovec )
{
	sigvec( sig, vec, ovec );
}
#endif


#if defined(SYS_sigblock)
MASK_TYPE sigblock( MASK_TYPE mask )
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( ! MappingFileDescriptors() ) {
		condor_sig_mask = ~0;
	} else {
		/* mask out the special Condor signals */
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
		mask &= condor_sig_mask;
	}
	rval =  syscall( SYS_sigblock, mask ) & condor_sig_mask;
	return rval;
}
#endif

#if defined(SYS_sigpause)
MASK_TYPE sigpause( MASK_TYPE mask )
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( MappingFileDescriptors() ) {
		/* mask out the special Condor signals */
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
		mask &= condor_sig_mask;
	}
	rval =  syscall( SYS_sigpause, mask );
	return rval;
}
#endif

#if defined(SYS_sigsetmask)
MASK_TYPE sigsetmask( MASK_TYPE mask )
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( ! MappingFileDescriptors() ) {
		condor_sig_mask = ~0;
	} else {
		/* mask out the special Condor signals */
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
	}
	rval =  syscall( SYS_sigsetmask, mask ) & condor_sig_mask;
	return rval;
}
#endif

#if defined(SYS_sigaction)
int sigaction( int sig, const struct sigaction *act, struct sigaction *oact )
{
	struct sigaction tmp, *my_act = &tmp;

	if( act ) {
		*my_act = *act;
	} else {
		my_act = NULL;
	}

	if ( MappingFileDescriptors() && act ) {		/* called by User code */
		switch (sig) {
			case SIGUSR2:
			case SIGTSTP:
			case SIGCONT: /* don't let user code mess with these signals */
				errno = EINVAL;
				return -1;
			default: /* block checkpointing inside users signal handler */
				sigaddset( &(my_act->sa_mask), SIGTSTP );
				sigaddset( &(my_act->sa_mask), SIGUSR2 );
		}
	}

	return SIGACTION( sig, my_act, oact);
}
#endif

#if defined(SYS_sigprocmask)
int sigprocmask( int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t tmp, *my_set;

	if( set ) {
		tmp = *set;
		my_set = &tmp;
	} else {
		my_set = NULL;
	}

	if ( MappingFileDescriptors() && set ) {
		if ( (how == SIG_BLOCK) || (how == SIG_SETMASK) ) {
			sigdelset(my_set,SIGUSR2);
			sigdelset(my_set,SIGTSTP);
			sigdelset(my_set,SIGCONT);
		}
	}
	return syscall(SYS_sigprocmask,how,my_set,oset);
}
#endif

#if defined (SYS_sigsuspend)
int sigsuspend(const sigset_t *set)
{
	sigset_t	my_set = *set;
	if ( MappingFileDescriptors() ) {
		sigdelset(&my_set,SIGUSR2);
		sigdelset(&my_set,SIGTSTP);
		sigdelset(&my_set,SIGCONT);
	}
	return SIGSUSPEND(&my_set);
}
#endif

#if defined(SYS_signal)
#if defined(VOID_SIGNAL_RETURN)
void (*
#else
int (*
#endif
signal(int sig,void (*func) (int) ))(int)
{
	if ( MappingFileDescriptors() ) {	
		switch (sig) {
			/* don't let user code mess with these signals */
			case SIGUSR2:
			case SIGTSTP:
			case SIGCONT:
				errno = EINVAL;
				return SIG_ERR; 
		}
	}

#if defined(VOID_SIGNAL_RETURN)
	return ( (void (*) (int)) syscall(SYS_signal,sig,func) );
#else
	return ( (int (*) (int)) syscall(SYS_signal,sig,func) );
#endif

}
#endif /* defined(SYS_signal) */


#endif
