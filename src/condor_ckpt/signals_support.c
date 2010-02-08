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
#include "condor_debug.h"		/* for EXCEPT */
#include "condor_sys.h"
#include "errno.h"
#include "condor_debug.h"

extern	char	*strerror();
extern int SetSyscalls(int);
extern int MappingFileDescriptors(void);

#if defined(HPUX)
extern void _sigreturn();
#endif

#if defined(HPUX)
#define MASK_TYPE long
#else
#define MASK_TYPE int
#endif

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

#if defined(LINUX)
typedef int				SS_TYPE;
#elif defined(Solaris)
typedef struct sigaltstack SS_TYPE;
#else
typedef struct sigstack SS_TYPE;
#endif

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
void
_condor_save_sigstates()
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

	/* Save pointer to signal stack (not POSIX, but widely supported) */	
#if defined(Solaris)
	sigaltstack( (SS_TYPE *) 0, &(signal_states.sig_stack) ); 
#elif !defined(LINUX)
	sigstack( (struct sigstack *) 0, &(signal_states.sig_stack) ); 
#endif

	(void) SetSyscalls( scm );
}

/* Here we restore the signal state of the user process.  This is called
 * upon a restart of a checkpointed process.  The data space must be
 * restored _before_ calling this function (all signal state info is saved
 * in the static structure signal_states) */
void
_condor_restore_sigstates()
{
	int scm;
	int sig;
	pid_t mypid;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

#if defined(HPUX)
     /* Tell the kernel to fill in our process's "trampoline" return
	  * code function with _sigreturn, so that signal handlers can
	  * return properly.  HPUX normally does this in _start().  But
	  * since we are restarting and thus not calling _start(), we must
	  * do it here.  (The constant is a magic cookie to prove we are
	  * post pa-risc 89 revision era) -Todd Tannenbaum, 12/94  
	  */
     _sigsetreturn(_sigreturn,0x06211988,sizeof(struct sigcontext));
#endif

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


	/* Restore signal stack pointer */
#if defined(Solaris)
	sigaltstack( &(signal_states.sig_stack), (SS_TYPE *) 0 );  
#elif !defined(LINUX)
	sigstack( &(signal_states.sig_stack), (struct sigstack *) 0 );  
#endif

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
#if defined(HPUX)
sigvector( sig, vec, ovec )
#else
sigvec( sig, vec, ovec )
#endif
int sig;
const struct sigvec *vec;
struct sigvec *ovec;
{
	int rval;
	struct	sigvec	nvec;

	if( ! MappingFileDescriptors() ) {
#if defined(HPUX)
			rval = syscall( SYS_sigvector, sig, vec, ovec );
			rval = syscall( SYS_sigvec, sig, vec, ovec );
#endif
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
#if defined(LINUX)
				nvec.sv_mask |= __sigmask( SIGTSTP );
				nvec.sv_mask |= __sigmask( SIGUSR2 );
#else
				nvec.sv_mask |= sigmask( SIGTSTP );
				nvec.sv_mask |= sigmask( SIGUSR2 );
#endif
			}

#if defined(HPUX)
			rval = syscall( SYS_sigvector, sig, vec ? &nvec : vec, ovec );
#else
			rval = syscall( SYS_sigvec, sig, vec ? &nvec : vec, ovec );
#endif
			break;
		}
	}

	return( rval );
}
#endif

#if defined (SYS_sigvec)
#if defined(HPUX)
_sigvector( int sig, const struct sigvec vec, struct sigvec ovec )
{
	sigvector( sig, vec, ovec );
}
#else
_sigvec( int sig, const struct sigvec vec, struct sigvec ovec )
{
	sigvec( sig, vec, ovec );
}
#endif
#endif


#if defined(SYS_sigblock)
MASK_TYPE 
sigblock( mask )
MASK_TYPE mask;
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( ! MappingFileDescriptors() ) {
		condor_sig_mask = ~0;
	} else {
		/* mask out the special Condor signals */
#if defined(LINUX)
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
#else
		condor_sig_mask = ~( sigmask( SIGUSR2 ) |
			 sigmask( SIGTSTP ) | sigmask(SIGCONT));
#endif
		mask &= condor_sig_mask;
	}
	rval =  syscall( SYS_sigblock, mask ) & condor_sig_mask;
	return rval;
}
#endif

#if defined(SYS_sigpause)
MASK_TYPE 
sigpause( mask )
MASK_TYPE mask;
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( MappingFileDescriptors() ) {
		/* mask out the special Condor signals */
#if defined(LINUX)
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
#else
		condor_sig_mask = ~( sigmask( SIGUSR2 ) |
			 sigmask( SIGTSTP ) | sigmask(SIGCONT));
#endif
		mask &= condor_sig_mask;
	}
	rval =  syscall( SYS_sigpause, mask );
	return rval;
}
#endif

#if defined(SYS_sigsetmask)
MASK_TYPE 
sigsetmask( mask )
MASK_TYPE mask;
{
	MASK_TYPE condor_sig_mask;
	MASK_TYPE rval;

	if ( ! MappingFileDescriptors() ) {
		condor_sig_mask = ~0;
	} else {
		/* mask out the special Condor signals */
#if defined(LINUX)
		condor_sig_mask = ~( __sigmask( SIGUSR2 ) |
			 __sigmask( SIGTSTP ) | __sigmask(SIGCONT));
#else
		condor_sig_mask = ~( sigmask( SIGUSR2 ) |
			 sigmask( SIGTSTP ) | sigmask(SIGCONT));
#endif
		mask &= condor_sig_mask;
	}
	rval =  syscall( SYS_sigsetmask, mask ) & condor_sig_mask;
	return rval;
}
#endif

/* fork() and sigaction() are not in fork.o or sigaction.o on Solaris 2.5
   but instead are only in the threads libraries.  We access the old
   versions through their new names. */

#if defined(Solaris)
#include <signal.h>

int 
SIGACTION(int sig, const struct sigaction *act, struct sigaction *oact)
{
	return _libc_SIGACTION(sig, act, oact);
}
#endif

#if defined(SYS_sigaction)
int
sigaction( int sig, const struct sigaction *act, struct sigaction *oact )
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

#if defined(OSF1) || defined(Solaris) || defined(LINUX)
	return SIGACTION( sig, my_act, oact);
#else
	return syscall(SYS_sigaction, sig, my_act, oact);
#endif
}
#endif

/* On OSF1 there is a sigprocmask.o and a _sigprocmsk.o.  We extract the
   former and upcase SIGPROCMASK, but we can no longer extract the
   latter, because it results in a redefinition of _user_signal_mask.
   sigprocmask (in sigprocmask.o) calls _sigprocmask (in _sigprocmsk.o).
   In the upcase versions, SIGPROCMASK wants to call _SIGPROCMASK.  Since
   we do not trap _sigprocmask in remote syscalls, we just define
   _SIGPROCMASK in terms of _sigprocmask, and the _user_signal_mask
   data structure is kept in synch whether SIGPROCMASK or sigprocmask is
   called.  -Jim B. */
#if defined(OSF1)
int
_SIGPROCMASK( int how, const sigset_t *set, sigset_t *oset)
{
	return _sigprocmask( how, set, oset );
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
#if defined(OSF1)
	SIGPROCMASK(how,my_set,oset);
#else
	return syscall(SYS_sigprocmask,how,my_set,oset);
#endif
}
#endif

#if defined (SYS_sigsuspend)
int
sigsuspend(set)
const sigset_t *set;
{
	sigset_t	my_set = *set;
	if ( MappingFileDescriptors() ) {
		sigdelset(&my_set,SIGUSR2);
		sigdelset(&my_set,SIGTSTP);
		sigdelset(&my_set,SIGCONT);
	}
#if defined(OSF1) || defined(LINUX)
	return SIGSUSPEND(&my_set);
#else
	return syscall(SYS_sigsuspend,&my_set);
#endif
}
#endif

#if defined(SYS_signal)
#if defined(VOID_SIGNAL_RETURN)
void (*
#else
int (*
#endif
signal(sig,func))(int)
int sig;
void (*func)(int);
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

#if defined(OSF1)
	SIGNAL(sig, func);
#elif defined(VOID_SIGNAL_RETURN)
	return ( (void (*) (int)) syscall(SYS_signal,sig,func) );
#else
	return ( (int (*) (int)) syscall(SYS_signal,sig,func) );
#endif

}
#endif /* defined(SYS_signal) */

#if 0
void
display_sigstate( int line, const char * file )
{
	int			scm;
	int			i;
	sigset_t	pending_sigs;
	sigset_t	blocked_sigs;
	struct sigaction	act;
	int			pending, blocked;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
	sigpending( &pending_sigs );
	sigprocmask(SIG_SETMASK,NULL,&blocked_sigs);

	dprintf( D_ALWAYS, "At line %d in file %s ==============\n", line, file );
	for( i=1; i<NSIG; i++ ) {
		sigaction( i, NULL, &act );

		pending = sigismember( &pending_sigs, i );
		blocked = sigismember( &blocked_sigs, i );
		dprintf( D_ALWAYS, "%d %s pending %s blocked handler = 0x%p\n",
			i,
			pending ? "IS " : "NOT",
			blocked ? "IS " : "NOT",
			act.sa_handler
		);
	}
	dprintf( D_ALWAYS, "====================================\n" );
	SetSyscalls( scm );
}
#endif
