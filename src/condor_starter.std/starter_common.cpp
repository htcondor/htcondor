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


/* 
** Functions that are shared between condor_starter and condor_starter.smp. 
** This is a temporary hack, until we more elegantly merge the PVM
** starter with the real starter.
** Written by Derek Wright <wright@cs.wisc.edu> on 10/8/98.
*/

#include "condor_common.h"
#include "starter_common.h"
#include "condor_version.h"
#include "basename.h"
#include "internet.h"
#include "subsystem_info.h"
#include "std_univ_io.h"

extern int		EventSigs[];
extern char*	InitiatingHost;
extern StdUnivSock	*SyscallStream;	// stream to shadow for remote system calls
extern char*	UidDomain;				// Machines we share UID space with
extern bool		TrustUidDomain;	// Should we trust what the submit side claims?

MyString SlotName;

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
initial_bookeeping( int /* argc */, char *argv[] )
{
	char*	logAppend = NULL;
	char*	submitHost = NULL;
	int		usageError = FALSE;
	char**	ptr;

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

		// we need to call this *before* we try to set_condor_priv(),
		// so that if CONDOR_IDS is defined in the config file, we'll
		// get the right value.
	config();

	set_condor_priv();

	init_shadow_connections();

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

		// Stash logAppend as the SlotName
		SlotName = logAppend;
	} else {
		// We were not told to append anything to the name of the
		// log file.  This means we are not on an SMP machine,
		// so set SlotName to be slot1 (i.e. cpu 1).
		SlotName = "slot1";
	}

	init_logging();
	dprintf( D_ALWAYS, "********** STARTER starting up ***********\n" );
	dprintf( D_ALWAYS, "** %s\n", CondorVersion() );
	dprintf( D_ALWAYS, "** %s\n", CondorPlatform() );
	dprintf( D_ALWAYS, "******************************************\n" );

	InitiatingHost = submitHost;
	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", InitiatingHost );

	init_params();

		/* Now if we have an error, we can print a message */
	if( usageError ) {
		usage( argv[0] );	/* usage() exits */
	}

	init_sig_mask();

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
	char	*pval;

	if( param_boolean_crufty( "STARTER_LOCAL_LOGGING", true ) ) {
			// Use regular, local logging.
		dprintf_config( get_mySubSystem()->getName() );// Log file on local machine 
	} else {
			// Set up to do logging through the shadow
		close( fileno(stderr) );
		dup2( CLIENT_LOG, fileno(stderr) );
		setvbuf( stderr, NULL, _IOLBF, 0 ); // line buffering

		pval = param( "STARTER_DEBUG" );
		if( pval ) {
			set_debug_flags( pval, 0 );
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
	int		syscall_number = CONDOR_new_connection;
	ReliSock* reli = new ReliSock();
	char 	addr[128];
	memset( addr, '\0', 128 );

	SyscallStream->encode();

		// Send the request
	if( !SyscallStream->code(syscall_number) ) {
		EXCEPT( "Can't send CONDOR_new_connection request" );
	}
	if( !SyscallStream->code(id) ) {
		EXCEPT( "Can't send process id for CONDOR_new_connection request" );
	}

		// Turn the stream around
	SyscallStream->end_of_message();
	SyscallStream->decode();

		// Read the port number
	if( !SyscallStream->code(portno) ) {
		EXCEPT( "Can't read port number for new connection" );
	}
	if( portno < 0 ) {
		SyscallStream->code( errno );
		EXCEPT( "Can't get port for new connection" );
	}
	SyscallStream->end_of_message();

		// Create the sinful string we want to connect back to.
	generate_sinful(addr, 128, SyscallStream->peer_ip_str(), portno );
	dprintf( D_FULLDEBUG, "New Socket address: %s\n", addr );	
	if( ! reli->connect(addr, 0) ) {
		EXCEPT( "Can't connect to shadow at %s", addr );	
	}
	return reli;
}

/*
	Support USER_JOB_WRAPPER parameter; this function will twiddle
	the a_out_name and args as appropiate.
*/
void
support_job_wrapper(MyString &a_out_name, ArgList *args)
{
	char *wrapper = NULL;

	ASSERT(args);
	ASSERT(args->Count() > 0);

	if ( (wrapper=param("USER_JOB_WRAPPER")) == NULL ) {
			// no wrapper desired, we have nothing to do.
		return;
	}

		// make certain this wrapper program exists and is executable
	if ( access(wrapper,X_OK) < 0 ) {
		EXCEPT("Cannot find/execute USER_JOB_WRAPPER file %s",wrapper);
	}

	// reset old argv[0] to the full path to the job executable
	args->RemoveArg(0);
	args->InsertArg(a_out_name.Value(),0);

		// now reset a_out_name and insert new argv[0]
	a_out_name = wrapper;
	args->InsertArg(condor_basename(wrapper),0);

		// and finally free wrapper, which was malloced by param()
	free(wrapper);

	return;
}

/*
  We've been requested to start the user job with the given UID, but
  we apply our own rules to determine whether we'll honor that request.
*/
void
determine_user_ids( uid_t &requested_uid, gid_t &requested_gid )
{

	struct passwd	*pwd_entry;

		// don't allow any root processes
	if( requested_uid == 0 || requested_gid == 0 ) {
		EXCEPT( "Attempt to start user process with root privileges" );
	}
		
		// if the submitting machine is in our shared UID domain, honor
		// the request
		// alternatively, if the site has "TRUST_UID_DOMAIN = True" in
		// their config file, we don't do the DNS check 
	if( TrustUidDomain || host_in_domain(InitiatingHost, UidDomain) ) {

		/* check to see if there is an entry in the passwd file for this uid */
		if( (pwd_entry=getpwuid(requested_uid)) == NULL ) {
			if ( param_boolean_crufty("SOFT_UID_DOMAIN", false) ) {
			  EXCEPT("Uid not found in passwd file & SOFT_UID_DOMAIN is False");
			}
		}
		(void)endpwent();

		return;
	}

		// otherwise, we run the process an "nobody"
	if( (pwd_entry = getpwnam("nobody")) == NULL ) {
#ifdef HPUX
		// the HPUX9 release does not have a default entry for nobody,
		// so we'll help condor admins out a bit here...
		requested_uid = 59999;
		requested_gid = 59999;
		return;
#else
		EXCEPT( "Can't find UID for \"nobody\" in passwd file" );
#endif
	}

	requested_uid = pwd_entry->pw_uid;
	requested_gid = pwd_entry->pw_gid;

#ifdef HPUX
	// HPUX9 has a bug in that getpwnam("nobody") always returns
	// a gid of 60001, no matter what the group file (or NIS) says!
	// on top of that, legal UID/GIDs must be -1<x<60000, so unless we
	// patch here, we will generate an EXCEPT later when we try a
	// setgid().  -Todd Tannenbaum, 3/95
	if ( (requested_uid > 59999) || (requested_uid < 0) )
		requested_uid = 59999;
	if ( (requested_gid > 59999) || (requested_gid < 0) )
		requested_gid = 59999;
#endif

}

void
setSlotEnv( Env* env_obj ) {
	// add name of SMP slot (from startd) into environment
	MyString slot_env;
	slot_env.formatstr("_CONDOR_SLOT=%s", SlotName.Value());
	env_obj->SetEnv(slot_env.Value());
	if (param_boolean("ALLOW_VM_CRUFT", false)) {
		slot_env.formatstr("CONDOR_VM=%s", SlotName.Value());
		env_obj->SetEnv(slot_env.Value());
	}
}
