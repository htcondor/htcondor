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
#include "condor_debug.h"
#include "condor_version.h"

#include "starter.h"
#include "jic_shadow.h"

#include "NTsenders.h"
#include "syscall_numbers.h"
#include "my_hostname.h"
#include "internet.h"
#include "basename.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "classad_command_util.h"
#include "directory.h"
#include "nullfile.h"
#include "stream_handler.h"
#include "condor_vm_universe_types.h"
#include "authentication.h"
#include "condor_mkstemp.h"

extern CStarter *Starter;
ReliSock *syscall_sock = NULL;
extern const char* JOB_AD_FILENAME;
extern const char* MACHINE_AD_FILENAME;

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_contains contains_anycase
#	define file_remove remove_anycase
#else
#	define file_contains contains
#	define file_remove remove
#endif

JICShadow::JICShadow( const char* shadow_name ) : JobInfoCommunicator()
{
	if( ! shadow_name ) {
		EXCEPT( "Trying to instantiate JICShadow with no shadow name!" );
	}
	m_shadow_name = strdup( shadow_name );

	shadow = NULL;
	shadow_version = NULL;
	filetrans = NULL;
	m_filetrans_sec_session = NULL;
	m_reconnect_sec_session = NULL;
	
	trust_uid_domain = false;
	uid_domain = NULL;
	fs_domain = NULL;

	transfer_at_vacate = false;
	wants_file_transfer = false;
	job_cleanup_disconnected = false;

		// now we need to try to inherit the syscall sock from the startd
	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks[0] == NULL ||
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit job ClassAd startd update socket.\n");
		Starter->StarterExit( 1 );
	}
	m_job_startd_update_sock = socks[0];
	socks++;

	if (socks[0] == NULL || 
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
		Starter->StarterExit( 1 );
	}
	syscall_sock = (ReliSock *)socks[0];
	socks++;

		/* Set a timeout on remote system calls.  This is needed in
		   case the user job exits in the middle of a remote system
		   call, leaving the shadow blocked.  -Jim B. */
	syscall_sock->timeout(param_integer( "STARTER_UPLOAD_TIMEOUT", 300));

	ASSERT( socks[0] == NULL );
}


JICShadow::~JICShadow()
{
	if( shadow ) {
		delete shadow;
	}
	if( shadow_version ) {
		delete shadow_version;
	}
	if( filetrans ) {
		delete filetrans;
	}
	if( m_shadow_name ) {
		free( m_shadow_name );
	}
	if( uid_domain ) {
		free( uid_domain );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
	free(m_reconnect_sec_session);
	free(m_filetrans_sec_session);
}


bool
JICShadow::init( void ) 
{ 
		// First, get a copy of the job classad by doing an RSC to the
		// shadow.  This is totally independent of the shadow version,
		// etc, and is the first step to everything else. 
	if( ! getJobAdFromShadow() ) {
		dprintf( D_ALWAYS|D_FAILURE,
				 "Failed to get job ad from shadow!\n" );
		return false;
	}

	if ( m_job_startd_update_sock )
	{
		receiveMachineAd(m_job_startd_update_sock);
	}

		// stash a copy of the unmodified job ad in case we decide
		// below that we want to write out an execution visa
	ClassAd orig_ad = *job_ad;	

		// now that we have the job ad, see if we should go into an
		// infinite loop, waiting for someone to attach w/ the
		// debugger.
	checkForStarterDebugging();

		// Next, instantiate our DCShadow object.  We want to do this
		// right away, since the rest of this stuff might depend on
		// what shadow version we're talking to...
	if( shadow ) {
		delete shadow;
	}
	shadow = new DCShadow( m_shadow_name );
	ASSERT( shadow );

		// Now, initalize our version information about the shadow
		// with whatever attributes we can find in the job ad.
	initShadowInfo( job_ad );

	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", shadow->name());

		// Now that we know what version of the shadow we're talking
		// to, we can register information about ourselves with the
		// shadow in a method that it understands.
	registerStarterInfo();

		// If we are supposed to specially create a security session
		// for file transfer and reconnect, do that now.
	initMatchSecuritySession();

		// Grab all the interesting stuff out of the ClassAd we need
		// to know about the job itself, like are we doing file
		// transfer, what should the std files be called, etc.
	if( ! initJobInfo() ) { 
		dprintf( D_ALWAYS|D_FAILURE,
				 "Failed to initialize job info from ClassAd!\n" );
		return false;
	}
	
		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! initUserPriv() ) {
		dprintf( D_ALWAYS, "ERROR: Failed to determine what user "
				 "to run this job as, aborting\n" );
		return false;
	}

		// Now that we have the user_priv, we can make the temp
		// execute dir
	if( ! Starter->createTempExecuteDir() ) { 
		return false;
	}

		// If the user wants it, initialize our io proxy
		// Must have user priv to drop the config info	
		// into the execute dir.
        priv_state priv = set_user_priv();
	initIOProxy();
	priv = set_priv(priv);

		// Now that the user priv is setup and the temp execute dir
		// exists, we can initialize the LocalUserLog.  if the job
		// defines StarterUserLog, we'll write the events.  if not,
		// all attemps to log events will just be no-ops.
	if( ! u_log->initFromJobAd(job_ad) ) {
		return false;
	}

		// Drop a job ad "visa" into the sandbox now if the job
		// requested it
	writeExecutionVisa(orig_ad);

	return true;
}


void
JICShadow::config( void ) 
{ 
	if( uid_domain ) {
		free( uid_domain );
	}
	uid_domain = param( "UID_DOMAIN" );  

	if( fs_domain ) {
		free( fs_domain );
	}
	fs_domain = param( "FILESYSTEM_DOMAIN" );  
	
	trust_uid_domain = false;
	char* tmp = param( "TRUST_UID_DOMAIN" );
	if( tmp ) {
		if( tmp[0] == 't' || tmp[0] == 'T' ) { 
			trust_uid_domain = true;
		}			
		free( tmp );
	}
}


void
JICShadow::setupJobEnvironment( void )
{ 
		// call our helper method to see if we want to do a file
		// transfer at all, and if so, initiate it.
	if( beginFileTransfer() ) {
			// We started a transfer, so we just want to return to
			// DaemonCore asap and wait for the file transfer callback
			// that was registered
		return;
	} 

		// Otherwise, there were no files to transfer, so we can
		// pretend the transfer just finished and try to spawn the job
		// now.
	transferCompleted( NULL );
}

bool
JICShadow::streamInput()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_INPUT,result);
	return result;
}

bool
JICShadow::streamOutput()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_OUTPUT,result);
	return result;
}

bool
JICShadow::streamError()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_ERROR,result);
	return result;
}

float
JICShadow::bytesSent( void )
{
	if( filetrans ) {
		return filetrans->TotalBytesSent();
	} 
	return 0.0;
}


float
JICShadow::bytesReceived( void )
{
	if( filetrans ) {
		return filetrans->TotalBytesReceived();
	}
	return 0.0;
}


void
JICShadow::Suspend( void )
{

		// If we have a file transfer object, we want to tell it to
		// suspend any work it might be doing...
	if( filetrans ) {
		filetrans->Suspend();
	}
	
		// Next, we need the update ad for our job.  We'll use this
		// for the LocalUserLog (if it's doing anything), updating the
		// shadow, etc, etc.
	ClassAd update_ad;
	publishUpdateAd( &update_ad );
	
		// See if the LocalUserLog wants it
	u_log->logSuspend( &update_ad );

		// Now that everything is suspended, we want to send another
		// update to the shadow to let it know the job state.  We want
		// to confirm the update gets there on this important state
		// change, to pass in "true" to updateShadow() for that.
	updateShadow( &update_ad, true );

}


void
JICShadow::Continue( void )
{
		// If we have a file transfer object, we want to tell it to
		// resume any work it might want to be doing...
	if( filetrans ) {
		filetrans->Continue();
	}

		// Next, we need the update ad for our job.  We'll use this
		// for the LocalUserLog (if it's doing anything), updating the
		// shadow, etc, etc.
	ClassAd update_ad;
	publishUpdateAd( &update_ad );

		// See if the LocalUserLog wants it
	u_log->logContinue( &update_ad );

		// Now that everything is running again, we want to send
		// another update to the shadow to let it know the job state.
		// We want to confirm the update gets there on this important
		// state change, to pass in "true" to updateShadow() for that.
	updateShadow( &update_ad, true );
}


bool
JICShadow::transferOutput( void )
{
		// make sure we only do this step once.
	static bool did_transfer = false;
	if (did_transfer) {
		return true;
	}

		// transfer output files back if requested job really
		// finished.  may as well do this in the foreground,
		// since we do not want to be interrupted by anything
		// short of a hardkill. 
	if( filetrans && ((requested_exit == false) || transfer_at_vacate) ) {

			// add any dynamically-added output files to the FT
			// object's list
		m_added_output_files.rewind();
		char* filename;
		while ((filename = m_added_output_files.next()) != NULL) {
			filetrans->addOutputFile(filename);
		}

			// remove any dynamically-removed output files from
			// the ft's list (i.e. a renamed Windows script)
		m_removed_output_files.rewind();
		while ((filename = m_removed_output_files.next()) != NULL) {
			filetrans->addFileToExeptionList(filename);
		}

		// remove the job and machine classad files from the
		// ft list
		filetrans->addFileToExeptionList(JOB_AD_FILENAME);
		filetrans->addFileToExeptionList(MACHINE_AD_FILENAME);
	
			// true if job exited on its own
		bool final_transfer = (requested_exit == false);	

			// The shadow may block on disk I/O for long periods of
			// time, so set a big timeout on the starter's side of the
			// file transfer socket.

		int timeout = param_integer( "STARTER_UPLOAD_TIMEOUT", 200 );
		filetrans->setClientSocketTimeout(timeout);

			// The user job may have created files only readable
			// by the user, so set_user_priv here.
		priv_state saved_priv = set_user_priv();

			// this will block
		bool rval = filetrans->UploadFiles( true, final_transfer );
		set_priv(saved_priv);

		if( ! rval ) {
				// Failed to transfer.  See if there is a reason to put
				// the job on hold.
			FileTransfer::FileTransferInfo ft_info = filetrans->GetInfo();
			if(!ft_info.success && !ft_info.try_again) {
				ASSERT(ft_info.hold_code != 0);
				notifyStarterError(ft_info.error_desc.Value(), true,
				                   ft_info.hold_code,ft_info.hold_subcode);
				return false;
			}

				// Some other kind of error.  Would like to know
				// for sure whether this really means we are disconnected
				// from the shadow, but for now, force the socket to
				// disconnect by closing it.

				// Please forgive us these hacks
				// as we forgive those who hack against us
			static int timesCalled = 0;
			timesCalled++;
			if (timesCalled < 5) {
					dprintf(D_ALWAYS,"File transfer failed, forcing disconnect.\n");

					if (syscall_sock != NULL) {
							syscall_sock->close();
					}

					job_cleanup_disconnected = true;
					return false;
			}

				// We tried 5 times and kept failing
				// now just tell the user we're giving up
			notifyStarterError("Repeated attempts to transfer output failed for unknown reasons", true,0,0);
			return false;
		}
	}
		// Either the job doesn't need transfer, or we just succeeded.
		// In both cases, we should record that we were successful so
		// that if we ever come through here again to retry the whole
		// job cleanup process we don't attempt to transfer again.
	did_transfer = true;
	return true;
}


void
JICShadow::gotShutdownFast( void )
{
		// Despite what the user wants, do not transfer back any files 
		// on a ShutdownFast.
	transfer_at_vacate = false;   

		// let our parent class do the right thing, too.
	JobInfoCommunicator::gotShutdownFast();
}


int
JICShadow::reconnect( ReliSock* s, ClassAd* ad )
{
		// first, make sure the entity requesting this is authorized.
		/*
		  TODO!!  UGH, since the results of getFullyQualifiedUser()
		  are not serialized properly when we inherit the relisock, we
		  have no way to figure out the original entity that
		  authenticated to the startd to spawn this job.  :( We really
		  should call getFullyQualifiedUser() on the socket we
		  inherit, stash that in a variable, and compare that against
		  the result on this socket right here.

		  But, we can't do that. :( So, instead, we call getOwner()
		  and compare that to whatever ATTR_OWNER is in our job ad
		  (the original ad, not whatever they're requesting now).

		  WORSE YET, even if it did work, there's no user credential
		  management when you use strong authentication, so this
		  socket would be authenticated with the shadow's system
		  credentials, not the user.  So, *NONE* of this will work,
		  anyway.  

		  We could try to introduce some kind of real shared
		  secret/capability to make this more secure, but that's going
		  to have to wait for time to make some bigger changes.
		*/
#if 0
	const char* new_owner = s->getOwner();
	char* my_owner = NULL; 
	if( ! job_ad->LookupString(ATTR_OWNER, &my_owner) ) {
		EXCEPT( "impossible: ATTR_OWNER must be in job ad by now" );
	}

	if( strcmp(new_owner, my_owner) ) {
		MyString err_msg = "User '";
		err_msg += new_owner;
		err_msg += "' does not match the owner of this job";
		sendErrorReply( s, getCommandString(CA_RECONNECT_JOB), 
						CA_NOT_AUTHORIZED, err_msg.Value() ); 
		dprintf( D_COMMAND, "Denied request for %s by invalid user '%s'\n", 
				 getCommandString(CA_RECONNECT_JOB), new_owner );
		return FALSE;
	}
#endif

	ClassAd reply;
	publishStarterInfo( &reply );

	MyString line;
	line = ATTR_RESULT;
	line += "=\"";
	line += getCAResultString( CA_SUCCESS );
	line += '"';
	reply.Insert( line.Value() );

	if( ! sendCAReply(s, getCommandString(CA_RECONNECT_JOB), &reply) ) {
		dprintf( D_ALWAYS, "Failed to reply to request\n" );
		return FALSE;
	}

		// If we managed to send the reply, finally commit to the
		// switch.  Destroy all the info we're storing about the
		// previous shadow and switch over to the new info.

		// Destroy our old DCShadow object and make a new one with the
		// current info.
	dprintf( D_ALWAYS, "Accepted request to reconnect from <%s:%d>\n",
			 syscall_sock->peer_ip_str(), 
			 syscall_sock->peer_port() );
	dprintf( D_ALWAYS, "Ignoring old shadow %s\n", shadow->addr() );
	delete shadow;
	shadow = new DCShadow;
	initShadowInfo( ad );	// this dprintf's D_ALWAYS for us
	free( m_shadow_name );
		/*
		  normally, it'd be nice to have a hostname here for
		  dprintf(), etc.  unfortunately, because of how the
		  information flows, we don't know the real hostname at this
		  point.  the reconnect command classad only contains the
		  ip/port (sinful string), not the shadow's hostname.  so, if
		  we want a hostname here, we'd have to do a reverse DNS
		  lookup.  someday, we might do that, but admins have
		  (rightfully) complained about the load Condor puts on DNS
		  servers, and since we don't *really* need this to be a
		  hostname anymore (we've already decided what UID/FS domain
		  to spawn the starter as, so we don't need it for that),
		  we'll just use the sinful string as the shadow's name...
		*/
	m_shadow_name = strdup( shadow->addr() );

		// switch over to the new syscall_sock
	dprintf( D_FULLDEBUG, "Closing old syscall sock <%s:%d>\n",
			 syscall_sock->peer_ip_str(), 
			 syscall_sock->peer_port() );
	delete syscall_sock;
	syscall_sock = s;
	syscall_sock->timeout(param_integer( "STARTER_UPLOAD_TIMEOUT", 300));
	dprintf( D_FULLDEBUG, "Using new syscall sock <%s:%d>\n",
			 syscall_sock->peer_ip_str(), 
			 syscall_sock->peer_port() );

	initMatchSecuritySession();

		// tell our FileTransfer object to point to the new 
		// shadow using the transsock and key in the ad from the
		// reconnect request
	if ( filetrans ) {
		char *value1 = NULL;
		char *value2 = NULL;
		ad->LookupString(ATTR_TRANSFER_KEY,&value1);
		ad->LookupString(ATTR_TRANSFER_SOCKET,&value2);
		bool result = filetrans->changeServer(value1,value2);
		if ( value1 && value2 && result ) {
			dprintf(D_FULLDEBUG,
				"Switching to new filetrans server, "
				"sock=%s key=%s\n",value2,value1);
		} else {
			dprintf(D_ALWAYS,
				"ERROR Failed to switch to new filetrans server, "
				"sock=%s key=%s\n", value2 ? value2 : "(null)",
				value1 ? value1 : "(null)");
		}
		if ( value1 ) free(value1);
		if ( value2 ) free(value2);

		if ( shadow_version == NULL ) {
			dprintf( D_ALWAYS, "Can't determine shadow version for FileTransfer!\n" );
		} else {
			filetrans->setPeerVersion( *shadow_version );
		}
		filetrans->setSecuritySession(m_filetrans_sec_session);
	}

	StreamHandler::ReconnectAll();

	if( job_cleanup_disconnected ) {
			/*
			  if we were trying to cleanup our job and we noticed we
			  were disconnected from the shadow, this flag will be
			  set.  in this case, want to call out to the Starter
			  object to tell it to try to clean up the job again.
			*/
		if( Starter->allJobsDone() ) {
			dprintf(D_ALWAYS,"Job cleanup finished, now Starter is exiting\n");
			Starter->StarterExit(0);
		}
	}

		// Now that we're holding onto the ReliSock, we can't let
		// DaemonCore close it on us!
	return KEEP_STREAM;
}


void
JICShadow::notifyJobPreSpawn( void )
{
			// Notify the shadow we're about to exec.
	REMOTE_CONDOR_begin_execution();

		// let the LocalUserLog know so it can log if necessary.  it
		// doesn't use the ClassAd for this event at all, so it's not
		// worth the trouble of creating one and publishing anything
		// into it.
	u_log->logExecute( NULL );
}


bool
JICShadow::notifyJobExit( int exit_status, int reason, UserProc*
						  user_proc )
{
	static bool wrote_local_log_event = false;
	bool job_exit_wants_ad = true;

		// protocol changed w/ v6.3.0 so the Update Ad is sent
		// with the final REMOTE_CONDOR_job_exit system call.
		// to keep things backwards compatible, do not send the 
		// ad with this system call if the shadow is older.

		// However, b/c the shadow didn't start sending it's version
		// to the starter until 6.3.2, we confuse 6.3.0 and 6.3.1
		// shadows with 6.2.X shadows that don't support the new
		// protocol.  Luckily, we never released 6.3.0 or 6.3.1 for
		// windoze, and we never released any part of the new
		// shadow/starter for Unix until 6.3.0.  So, we only have to
		// do this compatibility check on windoze, and we don't have
		// to worry about it not being able to tell the difference
		// between 6.2.X, 6.3.0, and 6.3.1, since we never released
		// 6.3.0 or 6.3.1. :) Derek <wright@cs.wisc.edu> 1/25/02

#ifdef WIN32		
	job_exit_wants_ad = false;


	if( shadow_version && shadow_version->built_since_version(6,3,0) ) {

		job_exit_wants_ad = true;	// new shadow; send ad

	}
#endif		

	ClassAd ad;
	ClassAd *ad_to_send;

		// We want the update ad anyway, in case we want it for the
		// LocalUserLog
	user_proc->PublishUpdateAd( &ad );

		// depending on the exit reason, we want a different event. 
		// however, don't write multiple events if we've already been
		// here, which might happen if we were disconnected when we
		// first tried and we're trying again...
	if( ! wrote_local_log_event ) {
		if( u_log->logJobExit(&ad, reason) ) {
			wrote_local_log_event = true;
		}
	}

	updateStartd(&ad, true);

	if ( job_exit_wants_ad ) {
		ad_to_send = &ad;
	} else {
		dprintf( D_FULLDEBUG,
				 "Shadow is pre-v6.3.0 - not sending final update ad\n" ); 
		ad_to_send = NULL;
	}
			
	if( !had_hold ) {
		if( REMOTE_CONDOR_job_exit(exit_status, reason, ad_to_send) < 0 ) {    
			dprintf( D_ALWAYS, "Failed to send job exit status to shadow\n" );
			job_cleanup_disconnected = true;
			return false;
		}
	}

	return true;
}

void
JICShadow::updateStartd( ClassAd *ad, bool final_update )
{
	ASSERT( ad );

	// update the startd's copy of the job ClassAd
	if( !m_job_startd_update_sock ) {
		return;
	}

	m_job_startd_update_sock->encode();
	if( !m_job_startd_update_sock->put((int)final_update) ||
		!ad->put(*m_job_startd_update_sock) ||
		!m_job_startd_update_sock->end_of_message() )
	{
		dprintf(D_FULLDEBUG,"Failed to send job ClassAd update to startd.\n");
	}
	else {
		dprintf(D_FULLDEBUG,"Sent job ClassAd update to startd.\n");
	}
	if( DebugFlags & D_FULLDEBUG ) {
		ad->dPrint(D_JOB);
	}

	if( final_update ) {
		delete m_job_startd_update_sock;
		m_job_startd_update_sock = NULL;
	}
}

bool
JICShadow::notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode )
{
	u_log->logStarterError( err_msg, critical );

	if( REMOTE_CONDOR_ulog_error(hold_reason_code, hold_reason_subcode, err_msg) < 0 ) {
		dprintf( D_ALWAYS, 
				 "Failed to send starter error string to Shadow.\n" );
		return false;
	}
	return true;
}

bool
JICShadow::holdJob( const char* hold_reason, int hold_reason_code, int hold_reason_subcode )
{
	gotHold();
	return notifyStarterError( hold_reason, true, hold_reason_code, hold_reason_subcode );
}

bool
JICShadow::registerStarterInfo( void )
{
	int rval;

	if( ! shadow ) {
		EXCEPT( "registerStarterInfo called with NULL DCShadow object" );
	}

		// CRUFT!
		// If the shadow is older than 6.3.3, we need to use the
		// CONDOR_register_machine_info method, which sends a bunch of
		// strings over the wire.  If we're 6.3.3 or later, we can use
		// CONDOR_register_starter_info, which just sends a ClassAd
		// with all the relevent info.
	if( shadow_version && shadow_version->built_since_version(6,3,3) ) {
		ClassAd starter_info;
		publishStarterInfo( &starter_info );
		rval = REMOTE_CONDOR_register_starter_info( &starter_info );

	} else {
			// We've got to use the old method.
		char *mfhn = strnewp ( my_full_hostname() );
		rval = REMOTE_CONDOR_register_machine_info( uid_domain,
			     fs_domain, daemonCore->InfoCommandSinfulString(), 
				 mfhn, 0 );
		delete [] mfhn;
	}
	if( rval < 0 ) {
		return false;
	}
	return true;
}


void
JICShadow::publishStarterInfo( ClassAd* ad )
{
	char *tmp = NULL;
	char* tmp_val = NULL;
	int size;

	size = strlen(uid_domain) + strlen(ATTR_UID_DOMAIN) + 5;
	tmp = (char*) malloc( size * sizeof(char) );
	sprintf( tmp, "%s=\"%s\"", ATTR_UID_DOMAIN, uid_domain );
	ad->Insert( tmp );
	free( tmp );

	size = strlen(fs_domain) + strlen(ATTR_FILE_SYSTEM_DOMAIN) + 5;
	tmp = (char*) malloc( size * sizeof(char) );
	sprintf( tmp, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, fs_domain ); 
	ad->Insert( tmp );
	free( tmp );

	int slot = Starter->getMySlotNumber();
	MyString line = ATTR_NAME;
	line += "=\"";
	if( slot ) { 
		line += "slot";
		line += slot;
		line += '@';
	}
	line += my_full_hostname();
	line += '"';
	ad->Insert( line.Value() );

	ad->Assign(ATTR_STARTER_IP_ADDR, daemonCore->InfoCommandSinfulString() );

	tmp_val = param( "ARCH" );
	size = strlen(tmp_val) + strlen(ATTR_ARCH) + 5;
	tmp = (char*) malloc( size * sizeof(char) );
	sprintf( tmp, "%s=\"%s\"", ATTR_ARCH, tmp_val );
	ad->Insert( tmp );
	free( tmp );
	free( tmp_val );

	tmp_val = param( "OPSYS" );
	size = strlen(tmp_val) + strlen(ATTR_OPSYS) + 5;
	tmp = (char*) malloc( size * sizeof(char) );
	sprintf( tmp, "%s=\"%s\"", ATTR_OPSYS, tmp_val );
	ad->Insert( tmp );
	free( tmp );
	free( tmp_val );

	tmp_val = param( "CKPT_SERVER_HOST" );
	if( tmp_val ) {
		size = strlen(tmp_val) + strlen(ATTR_CKPT_SERVER) + 5; 
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_CKPT_SERVER, tmp_val ); 
		ad->Insert( tmp );
		free( tmp );
		free( tmp_val );
	}

	size = strlen(ATTR_HAS_RECONNECT) + 6;
	tmp = (char*) malloc( size * sizeof(char) );
	sprintf( tmp, "%s=TRUE", ATTR_HAS_RECONNECT );
	ad->Insert( tmp );
	free( tmp );

		// Finally, publish all the DC-managed attributes.
	daemonCore->publish(ad);
}

void
JICShadow::addToOutputFiles( const char* filename )
{
	if (!m_removed_output_files.file_contains(filename)) {
		m_added_output_files.append(filename);
	}
}

void
JICShadow::removeFromOutputFiles( const char* filename )
{
	if (m_added_output_files.file_contains(filename)) {
		m_added_output_files.file_remove(filename);
	}
	m_removed_output_files.append(filename);
}

bool
JICShadow::uploadWorkingFiles(void)
{
	if( ! filetrans ) {
		return false;
	}

	// The shadow may block on disk I/O for long periods of
	// time, so set a big timeout on the starter's side of the
	// file transfer socket.

	int timeout = param_integer( "STARTER_UPLOAD_TIMEOUT", 200 );
	filetrans->setClientSocketTimeout(timeout);

	// The user job may have created files only readable
	// by the user, so set_user_priv here.
	priv_state saved_priv = set_user_priv();

	// this will block
	bool rval = filetrans->UploadFiles( true, false );
	set_priv(saved_priv);

	if( !rval ) {
		// Failed to transfer.
		dprintf(D_ALWAYS,"JICShadow::uploadWorkingFiles() failed.\n");
		return false;
	}
	dprintf(D_FULLDEBUG,"JICShadow::uploadWorkingFiles() succeeds.\n");
	return true;
}

void
JICShadow::updateCkptInfo(void)
{
	// We need the update ad for our job. 
	ClassAd update_ad;
	publishUpdateAd( &update_ad );

	// See if the LocalUserLog wants it
	u_log->logCheckpoint( &update_ad );

	// Now we want to send another update to the shadow.
	// To confirm this update, we pass in "true" to updateShadow() for that.
	updateShadow( &update_ad, true );
}

bool
JICShadow::initUserPriv( void )
{

#ifdef WIN32
		// Windoze

	return initUserPrivWindows();

#else
		// Unix

		// by default, the uid we choose is assumed not to be
		// dedicated to this job
	setExecuteAccountIsDedicated( NULL );

		// Before we go through any trouble, see if we even need
		// ATTR_OWNER to initialize user_priv.  If not, go ahead and
		// initialize it as appropriate.  
	if( initUserPrivNoOwner() ) {
		return true;
	}

	bool run_as_owner = allowRunAsOwner( true, true );

	MyString owner;
	if( run_as_owner ) {
		if( job_ad->LookupString( ATTR_OWNER, owner ) != 1 ) {
			dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
			         ATTR_OWNER );
			return false;
		}
	}

		// First, we decide if we're in the same UID_DOMAIN as the
		// submitting machine.  If so, we'll try to initialize
		// user_priv via ATTR_OWNER.  If there's no such user in the
		// passwd file, SOFT_UID_DOMAIN is True, and we're talking to
		// at least a 6.3.3 version of the shadow, we'll do a remote 
		// system call to ask the shadow what uid and gid we should
		// use.  If SOFT_UID_DOMAIN is False and there's no such user
		// in the password file, but the UID_DOMAIN's match, it's a
		// fatal error.  If the UID_DOMAIN's just don't match, we
		// initialize as "nobody".

	if( run_as_owner && !sameUidDomain() ) {
		run_as_owner = false;
		dprintf( D_FULLDEBUG, "Submit host is in different UidDomain\n" ); 
	}

	MyString vm_univ_type;
	if( job_universe == CONDOR_UNIVERSE_VM ) {
		job_ad->LookupString(ATTR_JOB_VM_TYPE, vm_univ_type);
	}

	if( run_as_owner && (job_universe == CONDOR_UNIVERSE_VM) ) {
		bool use_vm_nobody_user = param_boolean("ALWAYS_VM_UNIV_USE_NOBODY", false);
		if( use_vm_nobody_user ) {
			run_as_owner = false;
			dprintf( D_ALWAYS, "Using VM_UNIV_NOBODY_USER instead of %s\n", 
					owner.Value());
		}
	}

	CondorPrivSepHelper* privsep_helper = Starter->condorPrivSepHelper();

	if( run_as_owner ) {
			// Cool, we can try to use ATTR_OWNER directly.
			// NOTE: we want to use the "quiet" version of
			// init_user_ids, since if we're dealing with a
			// "SOFT_UID_DOMAIN = True" scenario, it's entirely
			// possible this call will fail.  We don't want to fill up
			// the logs with scary and misleading error messages.
		if( init_user_ids_quiet(owner.Value()) ) {
			if (privsep_helper != NULL) {
				privsep_helper->initialize_user(owner.Value());
			}
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n", 
			         owner.Value() );
			if( checkDedicatedExecuteAccounts( owner.Value() ) ) {
				setExecuteAccountIsDedicated( owner.Value() );
			}
		}
		else if( stricmp(vm_univ_type.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
			// For VMware vm universe, we can't use SOFT_UID_DOMAIN
			run_as_owner = false;
		}
		else {
				// There's a problem, maybe SOFT_UID_DOMAIN can help.
			bool shadow_is_old = true;
			bool try_soft_uid = param_boolean( "SOFT_UID_DOMAIN", false );

			if( try_soft_uid ) {
					// first, see if the shadow is new enough to
					// support the RSC we need to do...
				if( shadow_version && 
					shadow_version->built_since_version(6,3,3) ) {
						shadow_is_old = false;
				}
			} else {
					// No soft_uid_domain or it's set to False.  No
					// need to do the RSC, we can just fail.
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file and SOFT_UID_DOMAIN is False\n",
						 owner.Value() ); 
				return false;
            }

				// if the shadow is old, we have to just print an error
				// message and fail, since we can't do the RSC we need
				// to find out the right uid/gid.
			if( shadow_is_old ) {
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file, SOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting host is too old "
						 "to support SOFT_UID_DOMAIN.  You must upgrade "
						 "Condor on the submitting host to at least "
						 "version 6.3.3.\n", owner.Value() ); 
				return false;
			}

				// if we're here, it means that 1) the owner we want
				// isn't in the passwd file, 2) SOFT_UID_DOMAIN is
				// True, and 3) the shadow we're talking to can
				// support the CONDOR_REMOTE_get_user_info RSC.  So,
				// we'll do that call to get the uid/gid pair we need
				// and initialize user priv with that. 

			ClassAd user_info;
			if( REMOTE_CONDOR_get_user_info( &user_info ) < 0 ) {
				dprintf( D_ALWAYS, "ERROR: "
						 "REMOTE_CONDOR_get_user_info() failed\n" );
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file, SOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting host cannot "
						 "support SOFT_UID_DOMAIN.  You must upgrade "
						 "Condor on the submitting host to at least "
						 "version 6.3.3.\n", owner.Value() );
				return false;
			}

			int user_uid, user_gid;
			if( user_info.LookupInteger( ATTR_UID, user_uid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_UID );
				return false;
			}
			if( user_info.LookupInteger( ATTR_GID, user_gid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_GID );
				return false;
			}

				// now, we should have the uid and gid of the user.	
			dprintf( D_FULLDEBUG, "Got UserInfo from the Shadow: "
					 "uid: %d, gid: %d\n", user_uid, user_gid );
			if( ! set_user_ids((uid_t)user_uid, (gid_t)user_gid) ) {
					// This should never really fail, unless the
					// shadow told us it wants us to use 0 for either
					// the uid or gid...
				dprintf( D_ALWAYS, "ERROR: Could not initialize user "
						 "priv with uid %d and gid %d\n", user_uid,
						 user_gid );
				return false;
			}

			if (privsep_helper != NULL) {
				privsep_helper->initialize_user((uid_t)user_uid);
			}
		}
	} 

	if( !run_as_owner) {

       // first see if we define SLOTx_USER in the config file
        char *nobody_user = NULL;
			// 20 is the longest param: len(VM_UNIV_NOBODY_USER) + 1
        char nobody_param[20];
		int slot = Starter->getMySlotNumber();
		if( ! slot ) {
			slot = 1;
		}

		if( job_universe == CONDOR_UNIVERSE_VM ) {
			// If "VM_UNIV_NOBODY_USER" is defined in Condor configuration file, 
			// we will use it. 
			snprintf( nobody_param, 20, "VM_UNIV_NOBODY_USER" );
			nobody_user = param(nobody_param);
			if( nobody_user == NULL ) {
				// "VM_UNIV_NOBODY_USER" is NOT defined.
				// Next, we will try to use SLOTx_VMUSER
				snprintf( nobody_param, 20, "SLOT%d_VMUSER", slot );
				nobody_user = param(nobody_param);
			}
		}
		if( nobody_user == NULL ) {
			snprintf( nobody_param, 20, "SLOT%d_USER", slot );
			nobody_user = param(nobody_param);
			if (!nobody_user && param_boolean("ALLOW_VM_CRUFT", true)) {
				snprintf( nobody_param, 20, "VM%d_USER", slot );
				nobody_user = param(nobody_param);
			}
		}

        if ( nobody_user != NULL ) {
            if ( strcmp(nobody_user, "root") == MATCH ) {
                dprintf(D_ALWAYS, "WARNING: %s set to root, which is not "
                       "allowed. Ignoring.\n", nobody_param);
                free(nobody_user);
                nobody_user = strdup("nobody");
            } else {
                dprintf(D_ALWAYS, "%s set, so running job as %s\n",
                        nobody_param, nobody_user);
            }
        } else {
            nobody_user = strdup("nobody");
        }


		if( strcasecmp(vm_univ_type.Value(), CONDOR_VM_UNIVERSE_VMWARE ) == MATCH ) {
			// VMware doesn't support that "nobody" creates a VM.
			// So for VMware vm universe, we will "condor" instead of "nobody".
			if( strcmp(nobody_user, "nobody") == MATCH ) {
				free(nobody_user);
				nobody_user = strdup(get_condor_username());
			}
		}

			// passing NULL for the domain is ok here since this is
			// UNIX code
		if( ! init_user_ids(nobody_user, NULL) ) { 
			dprintf( D_ALWAYS, "ERROR: Could not initialize user_priv "
					 "as \"%s\"\n", nobody_user );
			free( nobody_user );
			return false;
		} else {
			if (privsep_helper != NULL) {
				privsep_helper->initialize_user(nobody_user);
			}
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n",
				  nobody_user );
			if( checkDedicatedExecuteAccounts( nobody_user ) ) {
				setExecuteAccountIsDedicated( nobody_user );
			}
			free( nobody_user );
		}			
	}

	user_priv_is_initialized = true;
	return true;
#endif
}


bool
JICShadow::initJobInfo( void ) 
{
		// Give our base class a chance.
	JobInfoCommunicator::initJobInfo();

	char *orig_job_iwd;

	if( ! job_ad ) {
		EXCEPT( "JICShadow::initJobInfo() called with NULL job ad!" );
	}

		// stash the executable name in orig_job_name
	if( ! job_ad->LookupString(ATTR_JOB_CMD, &orig_job_name) ) {
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_CMD );
		return false;
	} else {
			// put the orig job name in class ad
		dprintf(D_ALWAYS, "setting the orig job name in starter\n");
		job_ad->Assign(ATTR_ORIG_JOB_CMD,orig_job_name);
	}

		// stash the iwd name in orig_job_iwd
	if( ! job_ad->LookupString(ATTR_JOB_IWD, &orig_job_iwd) ) {
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_IWD );
		return false;
	} else {
			// put the orig job iwd in class ad
		dprintf(D_ALWAYS, "setting the orig job iwd in starter\n");
		job_ad->Assign(ATTR_ORIG_JOB_IWD, orig_job_iwd);
		free(orig_job_iwd);
	}

	if( ! job_ad->LookupInteger(ATTR_JOB_UNIVERSE, job_universe) ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
		job_universe = CONDOR_UNIVERSE_VANILLA;
	}

	if( ! job_ad->LookupInteger(ATTR_CLUSTER_ID, job_cluster) ) { 
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_CLUSTER_ID );
		return false;
	}

	if( ! job_ad->LookupInteger(ATTR_PROC_ID, job_proc) ) { 
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_PROC_ID );
		return false;
	}

	job_ad->LookupString( ATTR_JOB_REMOTE_IWD, &job_remote_iwd );

		// everything else is related to if we're transfering files or
		// not. that can get a little complicated, so it all lives in
		// its own set of functions.
	return initFileTransfer();
}


bool
JICShadow::initFileTransfer( void )
{
	bool rval = false;

		// first, figure out if the job supports the new file transfer
		// attributes.  if so, we can see if it wants optional file
		// transfer, and if we need to use it or not...

	ShouldTransferFiles_t should_transfer;
	char* should_transfer_str = NULL;

	job_ad->LookupString( ATTR_SHOULD_TRANSFER_FILES, &should_transfer_str );
	if( should_transfer_str ) {
		should_transfer = getShouldTransferFilesNum( should_transfer_str );
		if( should_transfer < 0 ) {
			dprintf( D_ALWAYS, "ERROR: Invalid %s (%s) in job ad, "
					 "aborting\n", ATTR_SHOULD_TRANSFER_FILES, 
					 should_transfer_str );
			free( should_transfer_str );
			return false;
		}
	} else { 
			// new attribute is not defined, use the old method.  we
			// just want to call the initWithFileTransfer() method,
			// since it will check the ATTR_TRANSFER_FILES if it can't
			// find ATTR_WHEN_TO_TRANSFER_OUTPUT...
		dprintf( D_FULLDEBUG, "No %s specified, looking for deprecated %s "
				 "attribute\n", ATTR_SHOULD_TRANSFER_FILES, 
				 ATTR_TRANSFER_FILES );
		return initWithFileTransfer();
	}

	switch( should_transfer ) {
	case STF_IF_NEEDED:
		if( sameFSDomain() ) {
			dprintf( D_FULLDEBUG, "%s is \"%s\" and job's FileSystemDomain "
					 "matches local value, NOT transfering files\n",
					 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
			rval = initNoFileTransfer();
		} else {
			dprintf( D_FULLDEBUG, "%s is \"%s\" but job's FileSystemDomain "
					 "does NOT match local value, transfering files\n",
					 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
			rval = initWithFileTransfer();
		}
		break;
	case STF_YES:
		dprintf( D_FULLDEBUG, "%s is \"%s\", transfering files\n", 
				 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
		rval = initWithFileTransfer();
		break;
	case STF_NO:
		dprintf( D_FULLDEBUG, "%s is \"%s\", NOT transfering files\n", 
				 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
		rval = initNoFileTransfer();
		break;
	}
	free( should_transfer_str );
	return rval;
}


bool
JICShadow::initNoFileTransfer( void )
{
	wants_file_transfer = false;
	change_iwd = false;

		/* We assume that transfer_files == Never means that we want
		   to live in the submit directory, so we DON'T change the
		   ATTR_JOB_CMD or the ATTR_JOB_IWD.  This is important to
		   MPI!  -MEY 12-8-1999 */
	job_ad->LookupString( ATTR_JOB_IWD, &job_iwd );
	if( ! job_iwd ) {
		dprintf( D_ALWAYS, "Can't find job's IWD, aborting\n" );
		return false;
	}

		// now that we've got the iwd we're using and all our
		// transfer-related flags set, we can finally initialize the
		// job's standard files.  this is shared code if we're
		// transfering or not, so it's in its own function.
	return initStdFiles();
}


bool
JICShadow::initWithFileTransfer()
{
		// First, see if the new attribute to decide when to transfer
		// the output back is defined, and if so, use it. 
	char* when_str = NULL;
	FileTransferOutput_t when;
	job_ad->LookupString( ATTR_WHEN_TO_TRANSFER_OUTPUT, &when_str );
	if( when_str ) {
		when = getFileTransferOutputNum( when_str );
		if( when < 0 ) {
			dprintf( D_ALWAYS, "ERROR: Invalid %s (%s) in job ad, "
					 "aborting\n", ATTR_WHEN_TO_TRANSFER_OUTPUT, when_str );
			free( when_str );
			return false;
		}
		free( when_str );
		if( when == FTO_ON_EXIT_OR_EVICT ) {
			transfer_at_vacate = true;
		}
	} else { 
			// no new attribute, try the old...
		char* tmp = NULL;
		job_ad->LookupString( ATTR_TRANSFER_FILES, &tmp );
		if ( tmp == NULL ) {
			dprintf( D_ALWAYS, "ERROR: No file transfer attributes in job "
					 "ad, aborting\n" );
			return false;
		}
			// if set to "ALWAYS", then set transfer_at_vacate to true
		switch ( tmp[0] ) {
		case 'a':
		case 'A':
			transfer_at_vacate = true;
			break;
		case 'n':  /* for "Never" */
		case 'N':
			return initNoFileTransfer();
			break;
		}
		free( tmp );
	}

		// if we're here, it means we're transfering files, so we need
		// to reset the job's iwd to the starter directory

	wants_file_transfer = true;
	change_iwd = true;
	job_iwd = strdup( Starter->GetWorkingDir() );
	MyString line = ATTR_JOB_IWD;
	line += " = \"";
	line += job_iwd;
	line+= '"';
	job_ad->InsertOrUpdate( line.Value() );

		// now that we've got the iwd we're using and all our
		// transfer-related flags set, we can finally initialize the
		// job's standard files.  this is shared code if we're
		// transfering or not, so it's in its own function.
	return initStdFiles();
}


bool
JICShadow::initStdFiles( void )
{
		// now that we know about file transfer and the real iwd we'll
		// be using, we can initialize the std files... 
	if( ! job_input_name ) {
		job_input_name = getJobStdFile( ATTR_JOB_INPUT, NULL );
	}

	// NOTE: We only need to look at _ORIG values for backwards
	// compatibility with old submitters (pre 6.7.14).  Modern
	// submitters do not mess with the filename when streaming
	// is being used, so there will be no _ORIG attribute.

	if( ! job_output_name ) {
		job_output_name = getJobStdFile( ATTR_JOB_OUTPUT,
										 ATTR_JOB_OUTPUT_ORIG );
	}
	if( ! job_error_name ) {
		job_error_name = getJobStdFile( ATTR_JOB_ERROR,
										ATTR_JOB_ERROR_ORIG );
	}

		// so long as all of the above are initialized, we were
		// successful, regardless of if any of them are NULL...
	return true;
}


char* 
JICShadow::getJobStdFile( const char* attr_name, const char* alt_name )
{
	char* tmp = NULL;
	const char* base = NULL;
	MyString filename;

	if(streamStdFile(attr_name)) {
		if(!tmp && alt_name) job_ad->LookupString(alt_name,&tmp);
		if(!tmp && attr_name) job_ad->LookupString(attr_name,&tmp);
		return tmp;
	}

	if( ! wants_file_transfer && alt_name ) {
		job_ad->LookupString( alt_name, &tmp );
	}

	if( !tmp ) {
		job_ad->LookupString( attr_name, &tmp );
	}
	if( !tmp ) {
		return NULL;
	}
	if ( !nullFile(tmp) ) {
		if( wants_file_transfer ) {
			base = condor_basename( tmp );
		} else {
			base = tmp;
		}
		if( ! fullpath(base) ) {	// prepend full path
			filename.sprintf( "%s%c", job_iwd, DIR_DELIM_CHAR );
		}
		filename += base;
	}
	free( tmp );
	if( filename[0] ) { 
		return strdup( filename.Value() );
	}
	return NULL;
}


bool
JICShadow::sameUidDomain( void ) 
{
	char* job_uid_domain = NULL;
	bool same_domain = false;

	ASSERT( uid_domain );
	ASSERT( shadow->name() );

	if( job_ad->LookupString( ATTR_UID_DOMAIN, &job_uid_domain ) != 1 ) {
			// No UidDomain in the job ad, what should we do?
			// For now, we'll just have to assume that we're not in
			// the same UidDomain...
		dprintf( D_FULLDEBUG, "SameUidDomain(): Job ClassAd does not "
				 "contain %s, returning false\n", ATTR_UID_DOMAIN );
		return false;
	}

	dprintf( D_FULLDEBUG, "Submit UidDomain: \"%s\"\n",
			 job_uid_domain );
	dprintf( D_FULLDEBUG, " Local UidDomain: \"%s\"\n",
			 uid_domain );

	if( stricmp(job_uid_domain, uid_domain) == MATCH ) {
		same_domain = true;
	}

	free( job_uid_domain );

	if( ! same_domain ) {
		return false;
	}

		// finally, for "security", make sure that the submitting host
		// contains our UidDomain as a substring of its hostname.
		// this way, we know someone's not just lying to us about what
		// UidDomain they're in.
		// However, if the site defines "TRUST_UID_DOMAIN = True" in
		// their config file, don't perform this check, so sites can
		// use UID domains that aren't substrings of DNS names if they
		// have to.
	if( trust_uid_domain ) {
		dprintf( D_FULLDEBUG, "TRUST_UID_DOMAIN is 'True' in the config "
				 "file, not comparing shadow's UidDomain (%s) against its "
				 "hostname (%s)\n", uid_domain, shadow->name() );
		return true;
	}
	if( host_in_domain(shadow->name(), uid_domain) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: the submitting host claims to be in our "
			 "UidDomain (%s), yet its hostname (%s) does not match.  "
			 "If the above hostname is actually an IP address, Condor "
			 "could not perform a reverse DNS lookup to convert the IP "
			 "back into a name.  To solve this problem, you can either "
			 "correctly configure DNS to allow the reverse lookup, or you "
			 "can enable TRUST_UID_DOMAIN in your condor configuration.\n",
			 uid_domain, shadow->name() );

		// TODO: maybe we should be more harsh in this case than just
		// running their job as nobody... perhaps we should EXCEPT()
		// in this case and force the user/admins to get it right
		// before we agree to run *any* jobs from a shadow like this? 

	return false;
}


bool
JICShadow::sameFSDomain( void ) 
{
	char* job_fs_domain = NULL;
	bool same_domain = false;

	ASSERT( fs_domain );

	if( ! job_ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &job_fs_domain) ) {
			// No FileSystemDoamin in the job ad, what should we do?
			// For now, we'll just have to assume that we're not in
			// the same FSDomain...
		dprintf( D_FULLDEBUG, "SameFSDomain(): Job ClassAd does not "
				 "contain %s, returning false\n", ATTR_FILE_SYSTEM_DOMAIN );
		return false;
	}

	dprintf( D_FULLDEBUG, "Submit FsDomain: \"%s\"\n",
			 job_fs_domain );
	dprintf( D_FULLDEBUG, " Local FsDomain: \"%s\"\n",
			 fs_domain );

	if( stricmp(job_fs_domain, fs_domain) == MATCH ) {
		same_domain = true;
	}

		// do we want to do any "security" checking on the FS domain
		// here?  i don't think so...  it should just be an arbitrary
		// string, not necessarily a domain name or substring.

	free( job_fs_domain );
	return same_domain;
}

bool
JICShadow::usingFileTransfer( void )
{
	return wants_file_transfer;
}


static void
refuse(ReliSock * s)
{
	ASSERT(s);
	s->encode();
	int i = 0; // == failure;
	s->code(i); // == failure
	s->eom();
}

// Based on Scheduler::updateGSICred
static bool
updateX509Proxy(int cmd, ReliSock * rsock, const char * path)
{
	ASSERT(rsock);
	ASSERT(path);

	rsock->timeout(10);
	rsock->decode();

	dprintf(D_FULLDEBUG,
	        "Remote side requests to update X509 proxy at %s\n",
	        path);

	MyString tmp_path;
#if defined(LINUX)
	GLExecPrivSepHelper* gpsh = Starter->glexecPrivSepHelper();
#else
	// dummy for non-linux platforms.
	int* gpsh = NULL;
#endif
	if (gpsh != NULL) {
		// in glexec mode, we may not have permission to write the
		// new proxy directly into the sandbox, so we stage it into
		// /tmp first, then use a GLExec helper script
		//
		char tmp[] = "/tmp/condor_proxy_XXXXXX";
		int fd = condor_mkstemp(tmp);
		if (fd == -1) {
			dprintf(D_ALWAYS,
			        "updateX509Proxy: error creating temp file "
			            "for proxy: %s\n",
			        strerror(errno));
			return 0;
		}
		close(fd);
		tmp_path = tmp;
	}
	else {
		tmp_path = path;
		tmp_path += ".tmp";
	}

	priv_state old_priv = set_priv(PRIV_USER);

	int reply;
	filesize_t size = 0;
	int rc;
	if ( cmd == UPDATE_GSI_CRED ) {
		rc = rsock->get_file(&size,tmp_path.Value());
	} else if ( cmd == DELEGATE_GSI_CRED_STARTER ) {
		rc = rsock->get_x509_delegation(&size,tmp_path.Value());
	} else {
		dprintf( D_ALWAYS,
		         "unknown CEDAR command %d in updateX509Proxy\n",
		         cmd );
		rc = -1;
	}
	if ( rc < 0 ) {
			// transfer failed
		reply = 0; // == failure
	} else {
		if (gpsh != NULL) {
#if defined(LINUX)
			// use our glexec helper object, which will
			// call out to GLExec
			//
			if (gpsh->update_proxy(tmp_path.Value())) {
				reply = 1;
			}
			else {
				reply = 0;
			}
#else
			EXCEPT("not on a linux platform and encounterd GLEXEC code!");
#endif
		}
		else {
				// transfer worked, now rename the file to
				// final_proxy_path
			if ( rotate_file(tmp_path.Value(), path) < 0 ) 
			{
					// the rename failed!!?!?!
				dprintf( D_ALWAYS,
				         "updateX509Proxy failed, "
				             "could not rename file\n");
				reply = 0; // == failure
			} else {
				reply = 1; // == success
			}
		}
	}
	set_priv(old_priv);

		// Send our reply back to the client
	rsock->encode();
	rsock->code(reply);
	rsock->eom();

	if(reply) {
		dprintf(D_FULLDEBUG,
		        "Attempt to refresh X509 proxy succeeded.\n");
	} else {
		dprintf(D_ALWAYS,
		        "Attempt to refresh X509 proxy FAILED.\n");
	}
	
	return reply;
}

bool
JICShadow::updateX509Proxy(int cmd, ReliSock * s)
{
	if( ! usingFileTransfer() ) {
		s->encode();
		int i = 2; // == success, but please don't call any more.
		s->code(i); // == success, but please don't call any more.
		s->eom();
		refuse(s);
		return false;
	}
	MyString path;
	if( ! job_ad->LookupString(ATTR_X509_USER_PROXY, path) ) {
		dprintf(D_ALWAYS, "Refusing shadow's request to update proxy as this job has no proxy\n");
		return false;
	}
	const char * proxyfilename = condor_basename(path.Value());
	return ::updateX509Proxy(cmd, s, proxyfilename);
}


bool
JICShadow::publishUpdateAd( ClassAd* ad )
{
	filesize_t execsz = 0;
	char buf[200];

	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		Directory starter_dir( Starter->GetWorkingDir(), PRIV_USER );
		execsz = starter_dir.GetDirectorySize();
		sprintf( buf, "%s=%lu", ATTR_DISK_USAGE, (long unsigned)((execsz+1023)/1024) ); 
		ad->InsertOrUpdate( buf );
	}

	// Insert the starter's address into the update ad, because all
	// parties who subscribe to updates (shadow & startd) also should
	// be informed of any changes in the starter's contact info
	// (important for CCB and shadow-starter reconnect, because startd
	// needs to relay starter's full contact info to the shadow when
	// queried).  It's a bit of a hack to do it through this channel,
	// but better than nothing.
	ad->Assign( ATTR_STARTER_IP_ADDR, daemonCore->publicNetworkIpAddr() );

		// Now, get our Starter object to publish, as well.  This will
		// walk through all the UserProcs and have those publish, as
		// well.  It returns true if there was anything published,
		// false if not.
	return Starter->publishUpdateAd( ad );
}


bool
JICShadow::periodicJobUpdate( ClassAd* update_ad, bool insure_update )
{
	bool r1, r2;
	r1 = JobInfoCommunicator::periodicJobUpdate(update_ad, insure_update);
	r2 = updateShadow(update_ad, insure_update);
	return (r1 && r2);
}


bool
JICShadow::updateShadow( ClassAd* update_ad, bool insure_update )
{
	dprintf( D_FULLDEBUG, "Entering JICShadow::updateShadow()\n" );
	static bool first_time = true;

	ClassAd local_ad;
	ClassAd* ad;
	if( update_ad ) {
			// we already have the update info, so don't bother trying
			// to publish another one.
		ad = update_ad;
	} else {
		ad = &local_ad;
		if( ! publishUpdateAd(ad) ) {  
			dprintf( D_FULLDEBUG, "JICShadow::updateShadow(): "
					 "Didn't find any info to update!\n" );
			return false;
		}
	}

	bool rval;

		// Try to send it to the shadow
	if (shadow_version && shadow_version->built_since_version(6,9,5)) {
			// Newer shadows understand the CONDOR_register_job_info
			// RSC, so we should just always use that, regardless of
			// insure_update, since we already have the socket open,
			// and we want to use it (e.g. to prevent firewalls from
			// closing it due to non-activity).
		rval = (REMOTE_CONDOR_register_job_info(ad) == 0);
	}
	else {
			// If it's an older shadow, the RSC would cause it to
			// EXCEPT(), so we just use the out-of-band DC message to
			// its command port, handled via the DCShadow object.
		if (first_time) {
			dprintf(D_FULLDEBUG, "Communicating with a shadow older than "
					"version 6.9.5, using command port to send job info "
					"instead of CONDOR_register_job_info RSC\n");
		}
		rval = shadow->updateJobInfo(ad, insure_update);
	}

	updateStartd(ad, false);

	first_time = false;
	if (rval) {
		dprintf(D_FULLDEBUG, "Leaving JICShadow::updateShadow(): success\n");
		return true;
	}
	dprintf(D_FULLDEBUG, "JICShadow::updateShadow(): failed to send update\n");
	return false;
}


bool
JICShadow::beginFileTransfer( void )
{

		// if requested in the jobad, transfer files over.  
	if( wants_file_transfer ) {
		// Only rename the executable if it is transferred.
		int xferExec = 1;
		job_ad->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec);

		if( xferExec ) {
			dprintf( D_FULLDEBUG, "Changing the executable name\n" );
			job_ad->Assign(ATTR_JOB_CMD,CONDOR_EXEC );
		}

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(job_ad, false, PRIV_USER) );
		filetrans->setSecuritySession(m_filetrans_sec_session);
		filetrans->RegisterCallback(
				  (FileTransferHandler)&JICShadow::transferCompleted,this );

		if ( shadow_version == NULL ) {
			dprintf( D_ALWAYS, "Can't determine shadow version for FileTransfer!\n" );
		} else {
			filetrans->setPeerVersion( *shadow_version );
		}

		if( ! filetrans->DownloadFiles(false) ) { // do not block
				// Error starting the non-blocking file transfer.  For
				// now, consider this a fatal error
			EXCEPT( "Could not initiate file transfer" );
		}
		return true;
	}
		// no transfer wanted or started, so return false
	return false;
}


int
JICShadow::transferCompleted( FileTransfer *ftrans )
{
	if ( ftrans ) {
			// Make certain the file transfer succeeded.
			// Until "multi-starter" has meaning, it's ok to
			// EXCEPT here, since there's nothing else for us
			// to do.
		FileTransfer::FileTransferInfo ft_info = ftrans->GetInfo();
		if ( !ft_info.success ) {
			if(!ft_info.try_again) {
					// Put the job on hold.
				ASSERT(ft_info.hold_code != 0);
				notifyStarterError(ft_info.error_desc.Value(), true,
				                   ft_info.hold_code,ft_info.hold_subcode);
			}

			EXCEPT( "Failed to transfer files" );
		}
			// If we transferred the executable, make sure it
			// has its execute bit set.
		MyString cmd;
		if (job_ad->LookupString(ATTR_JOB_CMD, cmd) &&
		    (cmd == CONDOR_EXEC))
		{
				// if we are running as root, the files were downloaded
				// as PRIV_USER, so switch to that priv level to do chmod
			priv_state saved_priv = set_priv( PRIV_USER );

			if (chmod(CONDOR_EXEC, 0755) == -1) {
				dprintf(D_ALWAYS,
				        "warning: unable to chmod %s to "
				            "ensure execute bit is set: %s\n",
				        CONDOR_EXEC,
				        strerror(errno));
			}

			set_priv( saved_priv );
		}
	}

		// Now that we're done, let our parent class do its thing.
	JobInfoCommunicator::setupJobEnvironment();

	return TRUE;
}


bool
JICShadow::getJobAdFromShadow( void )
{
		// Instantiate a new ClassAd for the job we'll be starting,
		// and get a copy of it from the shadow.
	if( job_ad ) {
		delete job_ad;
	}
    job_ad = new ClassAd;

	if( REMOTE_CONDOR_get_job_info(job_ad) < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, 
				 "Failed to get job info from Shadow!\n" );
		return false;
	}
	return true;
}


void
JICShadow::initShadowInfo( ClassAd* ad )
{
	ASSERT( shadow );
	if( ! shadow->initFromClassAd(ad) ) { 
		dprintf( D_ALWAYS, 
				 "Failed to initialize shadow info from ClassAd!\n" );
		return;
	}
	dprintf( D_ALWAYS, "Communicating with shadow %s\n",
			 shadow->addr() );

	if( shadow_version ) {
		delete shadow_version;
		shadow_version = NULL;
	}
	char* tmp = shadow->version();
	if( tmp ) {
		dprintf( D_FULLDEBUG, "Shadow version: %s\n", tmp );
		shadow_version = new CondorVersionInfo( tmp, "SHADOW" );
	} else {
		dprintf( D_FULLDEBUG, "Shadow version: unknown (pre v6.3.3)\n" ); 
	}
}



bool
JICShadow::initIOProxy( void )
{
	int want_io_proxy = 0;
	MyString io_proxy_config_file;

	if( job_ad->LookupBool( ATTR_WANT_IO_PROXY, want_io_proxy ) < 1 ) {
		dprintf( D_FULLDEBUG, "JICShadow::initIOProxy(): "
				 "Job does not define %s\n", ATTR_WANT_IO_PROXY );
		want_io_proxy = 0;
	} else {
		dprintf( D_ALWAYS, "Job has %s=%s\n", ATTR_WANT_IO_PROXY,
				 want_io_proxy ? "true" : "false" );
	}

	if( want_io_proxy || job_universe==CONDOR_UNIVERSE_JAVA ) {
		io_proxy_config_file.sprintf( "%s%cchirp.config",
				 Starter->GetWorkingDir(), DIR_DELIM_CHAR );
		if( !io_proxy.init(io_proxy_config_file.Value()) ) {
			dprintf( D_FAILURE|D_ALWAYS, 
					 "Couldn't initialize IO Proxy.\n" );
			return false;
		}
		dprintf( D_ALWAYS, "Initialized IO Proxy.\n" );
		return true;
	}
	return false;
}

void
JICShadow::initMatchSecuritySession()
{
	if( !param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION",false) ) {
		return;
	}

	if( shadow_version && !shadow_version->built_since_version(7,1,3) ) {
		return;
	}

	MyString reconnect_session_id;
	MyString reconnect_session_info;
	MyString reconnect_session_key;

	MyString filetrans_session_id;
	MyString filetrans_session_info;
	MyString filetrans_session_key;

		// For possible future use.  We could set security session
		// options here if we wanted to get an effect similar to
		// security negotiation, but currently we just take the
		// defaults from the shadow.
	char *starter_reconnect_session_info = "";
	char *starter_filetrans_session_info = "";

	int rc = REMOTE_CONDOR_get_sec_session_info(
		starter_reconnect_session_info,
		reconnect_session_id,
		reconnect_session_info,
		reconnect_session_key,
		starter_filetrans_session_info,
		filetrans_session_id,
		filetrans_session_info,
		filetrans_session_key);

	if( rc < 0 ) {
		dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to get security "
				"session info from shadow.  Skipping creation of match "
				"security session.\n");
		return;
	}

	if( m_reconnect_sec_session ) {
			// Already have one (must be reconnecting with shadow).
			// Continue using the one we have.  We cannot destroy it
			// and create a new one, because the syscall socket is
			// already using the session key from it.
	}
	else if( reconnect_session_id.Length() ) {
		rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			WRITE,
			reconnect_session_id.Value(),
			reconnect_session_key.Value(),
			reconnect_session_info.Value(),
			SUBMIT_SIDE_MATCHSESSION_FQU,
			NULL,
			0 /*don't expire*/ );

		if( !rc ) {
			dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create "
					"reconnect security session.  "
					"Will fall back on auto-negotiated session instead.\n");
		}
		else {
			m_reconnect_sec_session = strdup(reconnect_session_id.Value());
		}
	}

	if( m_filetrans_sec_session ) {
			// Already have one (must be reconnecting with shadow).
			// Destroy it and create a new one in its place, since
			// the shadow may have changed how it wants this to be
			// set up.
		daemonCore->getSecMan()->invalidateKey( m_filetrans_sec_session );
		free( m_filetrans_sec_session );
		m_filetrans_sec_session = NULL;
	}

	if( filetrans_session_id.Length() ) {
		rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			WRITE,
			filetrans_session_id.Value(),
			filetrans_session_key.Value(),
			filetrans_session_info.Value(),
			SUBMIT_SIDE_MATCHSESSION_FQU,
			shadow->addr(),
			0 /*don't expire*/ );

		if( !rc ) {
			dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create file "
					"transfer security session.  "
					"Will fall back on auto-negotiated session instead.\n");
		}
		else {
			m_filetrans_sec_session = strdup(filetrans_session_id.Value());
		}
	}
}

bool
JICShadow::receiveMachineAd( Stream *stream )
{
        bool ret_val = true;

	dprintf(D_FULLDEBUG, "Entering JICShadow::receiveMachineAd\n");
	mach_ad = new ClassAd();

	stream->decode();
	if (!mach_ad->initFromStream(*stream))
	{
		dprintf(D_ALWAYS, "Received invalid machine ad.  Discarding\n");
		delete mach_ad;
		mach_ad = NULL;
		ret_val = false;
	}
	else
	{
		mach_ad->dPrint(D_JOB);
	}

	return ret_val;
}
