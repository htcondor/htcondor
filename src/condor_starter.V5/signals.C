/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "signals.h"
#include "proto.h"
#include "name_tab.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */


 static NAME_VALUE SigNameArray[] = {
	{ SIGABRT, "SIGABRT" },
	{ SIGALRM, "SIGALRM" },
	{ SIGFPE, "SIGFPE" },
	{ SIGHUP, "SIGHUP" },
	{ SIGILL, "SIGILL" },
	{ SIGINT, "SIGINT" },
	{ SIGKILL, "SIGKILL" },
	{ SIGPIPE, "SIGPIPE" },
	{ SIGQUIT, "SIGQUIT" },
	{ SIGSEGV, "SIGSEGV" },
	{ SIGTERM, "SIGTERM" },
	{ SIGUSR1, "SIGUSR1" },
	{ SIGUSR2, "SIGUSR2" },
	{ SIGCHLD, "SIGCHLD" },
	{ SIGCONT, "SIGCONT" },
	{ SIGSTOP, "SIGSTOP" },
	{ SIGTSTP, "SIGTSTP" },
	{ SIGTTIN, "SIGTTIN" },
	{ SIGTTOU, "SIGTTOU" },
	{ -1, 0 }
};
NameTable SigNames( SigNameArray );


EventHandler::EventHandler( void (*f)(int), sigset_t m )
{
	func = f;
	mask = m;
	is_installed = FALSE;
}

void
EventHandler::display()
{
	dprintf( D_ALWAYS, "EventHandler {\n" );

	dprintf( D_ALWAYS, "\tfunc = 0x%x\n", func );
	display_sigset( "\tmask = ", &mask );

	dprintf( D_ALWAYS, "}\n" );
}

void
display_sigset( char *msg, sigset_t *mask )
{
	int					signo;
	NameTableIterator	next_sig( SigNames );

	if( msg ) {
		dprintf( D_ALWAYS, msg );
	}
	while( (signo = next_sig()) != -1 ) {
		if( sigismember(mask,signo) ) {
			dprintf( D_ALWAYS | D_NOHEADER, "%s ", SigNames.get_name(signo) );
		}
	}
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
}


void
EventHandler::install()
{
	NameTableIterator next_sig( SigNames );
	struct sigaction action;
	int		i;
	int		signo;

	dprintf( D_FULLDEBUG, "EventHandler::install() {\n" );

	if( is_installed ) {
		EXCEPT( "ERROR EventHandler::install(), already installed" );
	}


	for( i=0; i<N_POSIX_SIGS; i++ ) {
		signo = next_sig();
		if( sigismember(&mask,signo) ) {
			action.sa_handler = func;
			action.sa_mask = mask;
			action.sa_flags = SA_NOCLDSTOP;
			if( sigaction(signo,&action,&o_action[i]) < 0 ) {
				perror( "sigaction" );
				exit( 1 );
			}
			dprintf( D_FULLDEBUG,
				"\t*FSM* Installed handler 0x%x for signal %s, flags = 0x%x\n",
				action.sa_handler, SigNames.get_name(signo), action.sa_flags
			);
		}
	}
	is_installed = TRUE;

	dprintf( D_FULLDEBUG, "}\n" );
}

void
EventHandler::de_install()
{
	NameTableIterator next_sig( SigNames );
	struct sigaction action;
	int		signo;
	int		i;

	dprintf( D_FULLDEBUG, "EventHandler::de_install() {\n" );

	if( !is_installed ) {
		EXCEPT( "ERROR EventHandler::de_install(), not installed" );
	}

	for( i=0; i<N_POSIX_SIGS; i++ ) {
		signo = next_sig();
		if( sigismember(&mask,signo) ) {
			if( sigaction(signo,&o_action[i],0) < 0 ) {
				perror( "sigaction" );
				exit( 1 );
			}
			dprintf( D_FULLDEBUG,
				"\t*FSM* Installed handler 0x%x for signal %s\n",
				o_action[i].sa_handler, SigNames.get_name(signo)
			);
		}
	}

	is_installed = FALSE;

	dprintf( D_FULLDEBUG, "}\n" );
}

void
EventHandler::allow_events( sigset_t &sigset )
{
	if( !is_installed ) {
		EXCEPT( "ERROR EventHandler::allow_events(), not installed");
	} 
	sigprocmask( SIG_UNBLOCK, &sigset, 0 );
	// display_sigset( "\t*FSM* Unblocked: ", &sigset );
}

void
EventHandler::block_events( sigset_t &sigset )
{
	if( !is_installed ) {
		EXCEPT( "ERROR EventHandler::block_events(), not installed");
	}
	sigprocmask( SIG_BLOCK, &sigset, 0 );
	// display_sigset( "\t*FSM* Blocked: ", &sigset );
}

#if 0
CriticalSection::CriticalSection()
{
	is_critical = FALSE;
}

/*
  Block all signals, saving the current mask.
*/
void
CriticalSection::begin()
{
	sigset_t	all_mask;

	if( is_critical ) {
		EXCEPT( "CriticalSection.begin() - already in critical section");
	}
	sigfillset( &all_mask );
	sigprocmask( SIG_BLOCK, &all_mask, &save_mask );
	is_critical = TRUE;
}

/*
  Restore the previously saved mask.
*/
void
CriticalSection::end();
{
	if( !is_critical ) {
		EXCEPT( "CriticalSection.end() - not in critical section" );
	}
	is_critical = FALSE;
	sigprocmask( SIG_SETMASK, &save_mask );
}

/*
  Allow delivery of exactly one signal which is included in the saved mask.
*/
void
CriticalSection::deliver_one()
{
	if( !is_critical ) {
		EXCEPT( "CricicalSection.deliver_one() - not it critical section" );
	}
	sigsuspend( &save_mask );
}
#endif
