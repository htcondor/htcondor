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


/*#include "condor_common.h"*/

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define SENT 1
#define RECEIVED 2

#ifdef IRIX
#define NSIG 65
#endif

#include "x_fake_ckpt.h"

extern int SetSyscalls(int);
int sig_array[ NSIG + 1];


void mark( int sig, int val );
void catch_sig( int sig, void (*handler)(int) );
int check_sigs_received();
void display_state();

void handler(int sig)
{
	printf("User signal handler: got signal %d\n", sig );
	mark( sig, RECEIVED );
	printf("Leaving user signal handler\n");
}

void block_sig( int sig )
{
	sigset_t mask,omask;
	sigemptyset( &mask );
	sigaddset( &mask, sig );
	sigprocmask( SIG_BLOCK, &mask, &omask );
}

void unblock_sig( int sig )
{
	sigset_t mask,omask;
	sigemptyset( &mask );
	sigaddset( &mask, sig );
	sigprocmask( SIG_UNBLOCK, &mask, &omask );
}

int
main()
{
	int	sig;
	pid_t pid;

	pid = getpid();

		/* First we send ourselves every reasonable signal,
		** but we keep them all blocked. */
	for( sig=1; sig < NSIG; sig++ ) {
		switch( sig ) {
				
				/* Special Condor Signals */
			case SIGQUIT:
			case SIGUSR1:
			case SIGUSR2:
			case SIGTSTP: 
				continue;

				/* Can't be caught or ignored */
			case SIGKILL:
			case SIGSTOP:
				continue;

				/* Wierd ones ... */
			case SIGCONT:
			case SIGTRAP:
				continue;
#ifdef HPUX
			case _SIGRESERVE:
			case _SIGDIL:
			case _SIGXCPU:
			case _SIGXFSZ:
				continue;
#endif
		}

		/* This is interesting.  On Linux, signals>=32 are considered real-time signals and cannot be blocked.  (try it!)  So, we will not test blocking of them here. */

		#ifdef LINUX
			if( sig>=32 ) continue;
		#endif

		catch_sig( sig, handler );
		block_sig( sig );
		kill( pid, sig ); 
		mark( sig, SENT );
	}

		/* Checkpoint and restart */
	ckpt_and_exit();

	display_state();

		/* Ok, now unblock them all */
	printf( "About to unblock all signals\n" );
	fflush( stdout );

	for( sig=1; sig < NSIG; sig++) {
		unblock_sig( sig );
	}

		/* Make sure we got evey one */
	if( check_sigs_received() ) {
		exit( 0 );
	} else {
		exit( 1 );
	}
}


void mark( int sig, int val )
{
	/*
	printf( "mark(%d,%d)\n", sig, val );
	*/
	switch( val ) {
		case SENT:
			if( sig_array[ sig ] != 0 ) {
				fprintf( stderr, "Signal %d sent twice\n", sig );
				abort();
			}
			break;
		case RECEIVED:
			if( sig_array[ sig ] == 0 ) {
				fprintf( stderr, "Signal %d received, but never sent\n", sig );
				abort();
			}
			if( sig_array[ sig ] == RECEIVED ) {
				fprintf( stderr, "Signal %d received twice\n", sig );
				abort();
			}
			break;
	}
	sig_array[ sig ] = val;
}

int check_sigs_received()
{
	int		i;
	int		ok = 1;

	printf("About to check to see if all signals < 33 received.\n");
	printf("Any signal > 32 is usually not important...\n");
	for( i=1; i<NSIG; i++ ) {
		if( sig_array[i] == SENT ) {
				printf( "Signal %d sent, but never received\n", i );
			if (i < 33) {
				ok = 0;
			}
		}
	}
	if( ok ) {
		printf( "All signals < 32 received\n" );
	}
	return ok;
}

void catch_sig( int sig, void (*handler)(int) )
{
	struct sigaction vec;
	memset(&vec, 0, sizeof(struct sigaction));

#ifndef IRIX65
	vec.sa_handler = handler;
#else
#if defined(__cplusplus) && defined(__GNUC__)
	vec.sa_handler = (void (*)())handler;
#else
	vec.sa_handler = handler;
#endif
#endif

	sigfillset(&vec.sa_mask); 
	if ( sigaction(sig,&vec,NULL) < 0 ) {
		perror("sigaction");
		fprintf(stderr, "sig=%d\n", sig);
		exit(1);
	}
}

#if 0
catch_sig( int sig, void (*handler)(int) )
{
	struct sigvec	vec;

	vec.sv_handler = handler;
	vec.sv_mask = -1; 
	vec.sv_flags = 0;
#ifdef HPUX
	if( sigvector(sig,&vec,0) < 0 ) {
#else
	if( sigvec(sig,&vec,0) < 0 ) {
#endif
		perror( "sigvec" );
		exit( 1 );
	}
}
#endif

void display_state()
{
	sigset_t	pending;
	sigset_t	mask;
	struct sigaction oact;
		int		i;

	printf("signal\tpending\tblocked\thandler\n");

	sigpending( &pending );
	sigprocmask( SIG_BLOCK, NULL, &mask );

       	for( i=1; i<NSIG; i++ ) {
		printf("%d\t",i);
		if( sigismember(&pending,i) ) {
			printf("P\t");
		} else {
			printf("\t");
		}
		if( sigismember(&mask,i) ) {
			printf("B\t");
		} else {
			printf("\t");
		}
		sigaction( i, NULL, &oact );
		printf("%p\n",oact.sa_handler);
	}
}
