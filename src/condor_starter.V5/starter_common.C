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

/* 
** Functions that are shared between condor_starter and condor_starter.smp. 
** This is a temporary hack, until we more elegantly merge the PVM
** starter with the real starter.
** Written by Derek Wright <wright@cs.wisc.edu> on 10/8/98.
*/

#include "condor_common.h"
#include "starter_common.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

extern int		EventSigs[];
extern char*	InitiatingHost;
extern char*	mySubSystem;
extern ReliSock	*SyscallStream;	// stream to shadow for remote system calls

/*
  Unblock all signals except those which will encode asynchronous
  events.  Those will be unblocked at the proper moment by the finite
  state machine driver.
*/
void
init_sig_mask()
{
	sigset_t	sigmask;
	int			i;

	sigemptyset( &sigmask );
	for( i=0; EventSigs[i]; i++ ) {
		sigaddset( &sigmask, EventSigs[i] );
	}
	(void)sigprocmask( SIG_SETMASK, &sigmask, 0 );
}


void
initial_bookeeping( int argc, char *argv[] )
{
	char*	logAppend = NULL;
	char*	submitHost = NULL;
	int		usageError = FALSE;
	char**	ptr;
	char*	tmp;

	(void) SetSyscalls( SYS_LOCAL | SYS_MAPPED );

           // Process command line args.
	submitHost = argv[1];
	for(ptr = argv + 2; *ptr && !usageError; ptr++) {
		if(ptr[0][0] == '<') {
				// We were passed the startd sinful string, which for
				// now, we're just ignoring.
			continue;
		}
		if(ptr[0][0] == '-') {
			switch( ptr[0][1] ) {
			case 'a':
				ptr++;
				if( ptr && *ptr ) {
					logAppend = *ptr;
				} else {
					usageError = TRUE;
				}
				break;
			default:
				fprintf( stderr, "Error:  Unknown option %s\n", *ptr );
				usageError = TRUE;
			}
		}
	}

	set_condor_priv();

	init_shadow_connections();

	read_config_files();

		// If we're told on the command-line to append something to
		// the name of our log file (b/c of the SMP startd), we do
		// that here, so that when we setup logging, we get the right
		// filename.  -Derek Wright 9/2/98
	if( logAppend ) {
		char *tmp1, *tmp2;
		if( !(tmp1 = param("STARTER_LOG")) ) { 
			EXCEPT( "STARTER_LOG not defined!" );
		}
		tmp2 = (char*)malloc( (strlen(tmp1) + strlen(logAppend) + 2) *
							  sizeof(char) );
		if( !tmp2 ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp2, "%s.%s", tmp1, logAppend );
		config_insert( "STARTER_LOG", tmp2 );
		free( tmp1 );
		free( tmp2 );
	}

	init_logging();


		/* Now if we have an error, we can print a message */
	if( usageError ) {
		usage( argv[0] );	/* usage() exits */
	}

	init_sig_mask();

	InitiatingHost = submitHost;

	dprintf( D_ALWAYS, "********** STARTER starting up ***********\n" );
	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", InitiatingHost );

	_EXCEPT_Cleanup = exception_cleanup;
}


/*
  Set up file descriptor for log file.  If LOCAL_LOGGING is TRUE, we
  use regular dprintf() logging to the file specified in STARTER_LOG. 

  If LOCAL_LOGGING is FALSE, we will send all our logging information
  to the shadow, where it will show up in the shadow's log file.
*/
void
init_logging()
{
	int		is_local = TRUE;
	char	*pval;
	
	pval = param( "STARTER_LOCAL_LOGGING" );
	if( pval ) {
		if( pval[0] == 'f' || pval[0] == 'F' ) {
			is_local=FALSE;
		}
		free( pval );
	}

	if( is_local ) {
			// Use regular, local logging.
		dprintf_config( mySubSystem, -1 );	// Log file on local machine 
	} else {
			// Set up to do logging through the shadow
		close( fileno(stderr) );
		dup2( CLIENT_LOG, fileno(stderr) );
		setvbuf( stderr, NULL, _IOLBF, 0 ); // line buffering

		pval = param( "STARTER_DEBUG" );
		if( pval ) {
			set_debug_flags( pval );
			free( pval );
		}
	}
	return;
}


/*
  Create a new connection to the shadow using the existing remote
  system call stream.
*/
ReliSock*
NewConnection( int id )
{
	int 	portno;
	int		syscall = CONDOR_new_connection;
	ReliSock* reli = new ReliSock();
	char 	addr[128], *tmp;
	memset( addr, '\0', 128 );

	SyscallStream->encode();

		// Send the request
	if( !SyscallStream->code(syscall) ) {
		EXCEPT( "Can't send CONDOR_new_connection request" );
	}
	if( !SyscallStream->code(id) ) {
		EXCEPT( "Can't send process id for CONDOR_new_connection request" );
	}

		// Turn the stream around
	SyscallStream->eom();
	SyscallStream->decode();

		// Read the port number
	if( !SyscallStream->code(portno) ) {
		EXCEPT( "Can't read port number for new connection" );
	}
	if( portno < 0 ) {
		SyscallStream->code( errno );
		EXCEPT( "Can't get port for new connection" );
	}
	SyscallStream->eom();

		// Create the sinful string we want to connect back to.
	sprintf( addr, "<%s:%d>", SyscallStream->endpoint_ip_str(), portno );
	dprintf( D_FULLDEBUG, "New Socket address: %s\n", addr );	
	if( ! reli->connect(addr, 0) ) {
		EXCEPT( "Can't connect to shadow at %s", addr );	
	}
	return reli;
}


