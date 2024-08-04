/***************************************************************
 *
 * Copyright (C) 1990-2020, Condor Team, Computer Sciences Department,
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


/* This file is the implementation of the RemoteResource class. */

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "dc_coroutines.h"
#include "remoteresource.h"
#include "exit.h"             // for JOB_BLAH_BLAH exit reasons
#include "condor_debug.h"     // for D_debuglevel #defines
#include "condor_attributes.h"
#include "internet.h"
#include "dc_starter.h"
#include "directory.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
#include "globus_utils.h"
#include "limit_directory_access.h"
#include <filesystem>
#include "manifest.h"

#include <fstream>
#include <algorithm>

#include "spooled_job_files.h"
#include "job_ad_instance_recording.h"

#include "file_transfer_constants.h"
#include "null_file_transfer.h"

#define SANDBOX_STARTER_LOG_FILENAME ".starter.log"
extern const char* public_schedd_addr;	// in shadow_v61_main.C

// for remote syscalls, this is currently in NTreceivers.C.
extern int do_REMOTE_syscall();

// for remote syscalls...
ReliSock *syscall_sock;
RemoteResource *thisRemoteResource;

static const char *Resource_State_String [] = {
	"PRE", 
	"EXECUTING", 
	"PENDING_DEATH", 
	"PENDING_TRANSFER",
	"FINISHED",
	"SUSPENDED",
	"STARTUP",
	"RECONNECT",
	"CHECKPOINTED",
};


RemoteResource::RemoteResource( BaseShadow *shad ) 
	: m_want_remote_updates(false),
	  m_want_delayed(true),
	  m_wait_on_kill_failure(false)
{
	shadow = shad;
	dc_startd = NULL;
	machineName = NULL;
	starterAddress = NULL;
	jobAd = NULL;
	claim_sock = NULL;
	last_job_lease_renewal = 0;
	exit_reason = -1;
	claim_is_closing = false;
	exited_by_signal = false;
	exit_value = -1;
	memset( &remote_rusage, 0, sizeof(struct rusage) );
	disk_usage = 0;
	scratch_dir_file_count = -1; // -1 for 'unspecified'
	image_size_kb = 0;
	memory_usage_mb = -1;
	proportional_set_size_kb = -1;
	state = RR_PRE;
	began_execution = false;
	supports_reconnect = false;
	next_reconnect_tid = -1;
	proxy_check_tid = -1;
	no_update_received_tid = -1;
	last_proxy_timestamp = time(0); // We haven't sent the proxy to the starter yet, so anything before "now" means it hasn't changed.
	m_remote_proxy_expiration = 0;
	m_remote_proxy_renew_time = 0;
	reconnect_attempts = 0;

	lease_duration = -1;
	already_killed_graceful = false;
	already_killed_fast = false;
	m_got_job_exit = false;
	m_want_chirp = false;
	m_want_streaming_io = false;
	m_attempt_shutdown_tid = -1;
	m_started_attempting_shutdown = 0;
	m_upload_xfer_status = XFER_STATUS_UNKNOWN;
	m_download_xfer_status = XFER_STATUS_UNKNOWN;

	std::string prefix;
	param(prefix, "CHIRP_DELAYED_UPDATE_PREFIX");
	m_delayed_update_prefix = split(prefix);

	param_and_insert_attrs("PROTECTED_JOB_ATTRS", m_unsettable_attrs);
	param_and_insert_attrs("SYSTEM_PROTECTED_JOB_ATTRS", m_unsettable_attrs);
	param_and_insert_attrs("IMMUTABLE_JOB_ATTRS", m_unsettable_attrs);
	param_and_insert_attrs("SYSTEM_IMMUTABLE_JOB_ATTRS", m_unsettable_attrs);
}


RemoteResource::~RemoteResource()
{
	if( syscall_sock == claim_sock ) {
		syscall_sock = NULL;
	}
	if( thisRemoteResource == this ) {
		thisRemoteResource = NULL;
	}
	if ( dc_startd     ) delete dc_startd;
	if ( machineName   ) free( machineName );
	if ( starterAddress) free( starterAddress );
	if ( starterAd ) { delete starterAd; starterAd = nullptr; }
	closeClaimSock();
	if ( jobAd && jobAd != shadow->getJobAd() ) {
		delete jobAd;
	}
	if( proxy_check_tid != -1) {
		daemonCore->Cancel_Timer(proxy_check_tid);
		proxy_check_tid = -1;
	}

	if (no_update_received_tid != -1) {
		daemonCore->Cancel_Timer(no_update_received_tid);
		no_update_received_tid = -1;
	}


	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) ) {
		if( m_claim_session.secSessionId()[0] != '\0' ) {
			daemonCore->getSecMan()->invalidateKey( m_claim_session.secSessionId() );
		}
		if( m_filetrans_session.secSessionId()[0] != '\0' ) {
			daemonCore->getSecMan()->invalidateKey( m_filetrans_session.secSessionId() );
		}
	}

	if( m_attempt_shutdown_tid != -1 ) {
		daemonCore->Cancel_Timer(m_attempt_shutdown_tid);
		m_attempt_shutdown_tid = -1;
	}
	if ( next_reconnect_tid != -1 ) {
		daemonCore->Cancel_Timer( next_reconnect_tid );
	}
}


bool
RemoteResource::activateClaim( int starterVersion )
{
	int reply;
	const int max_retries = 20;
	const int retry_delay = 1;
	int num_retries = 0;

	if ( ! dc_startd ) {
		dprintf( D_ALWAYS, "Shadow doesn't have startd contact "
		         "information in RemoteResource::activateClaim()\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

	if ( !jobAd ) {
		dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

		// we'll eventually return out of this loop...
	while( 1 ) {
		reply = dc_startd->activateClaim( jobAd, starterVersion,
										  &claim_sock );
		switch( reply ) {
		case OK:
			dprintf( D_ALWAYS,
			         "Request to run on %s %s was ACCEPTED\n",
			         machineName ? machineName:"", dc_startd->addr() );
			// Record the activation start time (HTCONDOR-861).
			activation.StartTime = time(NULL);
				// first, set a timeout on the socket
			claim_sock->timeout( 300 );
				// Now, register it for remote system calls.
				// It's a bit funky, but works.
			daemonCore->Register_Socket( claim_sock, "RSC Socket",
				   (SocketHandlercpp)&RemoteResource::handleSysCalls,
				   "HandleSyscalls", this );
			setResourceState( RR_STARTUP );

			// clear out reconnect attempt
			jobAd->AssignExpr(ATTR_JOB_CURRENT_RECONNECT_ATTEMPT, "undefined");
			hadContact();

				// This expiration time we calculate here may be
				// sooner than the proxy, because the proxy may be
				// delegated some time in the future when file
				// transfer happens.  However, that just means we may
				// renew the proxy a few seconds earlier than we would
				// if we were more accurate, so it is ok.
			setRemoteProxyRenewTime();

			return true;
			break;
		case CONDOR_TRY_AGAIN:
			dprintf( D_ALWAYS,
			         "Request to run on %s %s was DELAYED (previous job still being vacated)\n",
			         machineName ? machineName:"", dc_startd->addr() );
			num_retries++;
			if( num_retries > max_retries ) {
				dprintf( D_ALWAYS, "activateClaim(): Too many retries, "
						 "giving up.\n" );
				return false;
			}			  
			dprintf( D_FULLDEBUG, 
					 "activateClaim(): will try again in %d seconds\n",
					 retry_delay ); 
			sleep( retry_delay );
			break;

		case CONDOR_ERROR:
			dprintf( D_ALWAYS, "%s: %s\n", machineName ? machineName:"", dc_startd->error() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;

		case NOT_OK:
			dprintf( D_ALWAYS,
			         "Request to run on %s %s was REFUSED\n",
			         machineName ? machineName:"", dc_startd->addr() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;
		default:
			dprintf( D_ALWAYS, "Got unknown reply(%d) from "
			         "request to run on %s %s\n", reply,
			         machineName ? machineName:"", dc_startd->addr() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;
		}
	}
		// should never get here, but keep gcc happy and return
		// something. 
	return false;
}


bool
RemoteResource::killStarter( bool graceful )
{
	if( (graceful && already_killed_graceful) ||
		(!graceful && already_killed_fast) ) {
			// we've already sent this command to the startd.  we can
			// just return true right away
		return true;
	}
	if ( ! dc_startd ) {
		dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
		         "DCStartd object NULL!\n");
		return false;
	}

	if( !graceful ) {
			// stop any lingering file transfers, if any
		abortFileTransfer();
	}

	// If we got the job_exit syscall, then we should expect
	// deactivateClaim() to fail, because the starter exits right
	// after that. But old starters (prior to 8.7.8) wait around
	// indefinitely, expecting our deactivateClaim() to trigger their
	// demise. So don't do our retry-and-wait-on-failure song and dance
	// if we saw a job_exit.
	// TODO If we add a version check or decide we don't care about 8.6.X
	//   and earlier, we can just return true if m_got_job_exit==true.
	bool wait_on_failure = m_wait_on_kill_failure && !m_got_job_exit;
	int num_tries = wait_on_failure ? 3 : 1;
	while (num_tries > 0) {
		if (dc_startd->deactivateClaim(graceful, &claim_is_closing)) {
			break;
		}
		num_tries--;
		if (num_tries) {
			const int delay = 5;
			dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
			         "Could not send command to startd, will retry in %d seconds\n", delay );
			sleep(delay);
		} else {
			dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
			         "Could not send command to startd\n" );
		}
	}

	if (num_tries == 0) {
		if (wait_on_failure) {
			disconnectClaimSock("Failed to contact startd, forcing disconnect from starter");
			int remaining = remainingLeaseDuration();
			if (remaining > 0) {
				dprintf(D_ALWAYS, "Failed to kill starter, sleeping for remaining lease duration of %d seconds\n", remaining);
				sleep(remaining);
			}
		}
		return false;
	}

	if( state != RR_FINISHED ) {
		setResourceState( RR_PENDING_DEATH );
	}

	if( graceful ) {
		already_killed_graceful = true;
	} else {
		already_killed_fast = true;
	}

	const char* addr = dc_startd->addr();
	if( addr ) {
		dprintf( D_FULLDEBUG, "Killed starter (%s) at %s\n", 
				 graceful ? "graceful" : "fast", addr );
	}

	bool wantReleaseClaim = false;
	jobAd->LookupBool(ATTR_RELEASE_CLAIM, wantReleaseClaim);
	if (wantReleaseClaim) {
		classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg(RELEASE_CLAIM, dc_startd->getClaimId());
		msg->setSecSessionId(m_claim_session.secSessionId());
		msg->setSuccessDebugLevel(D_ALWAYS);
		msg->setTimeout( 20);
		msg->setStreamType(Stream::reli_sock);
		dc_startd->sendBlockingMsg(msg.get());
	}
	return true;
}

bool RemoteResource::suspend()
{
	bool bRet = false;
	
	if ( state!= RR_RECONNECT )
	{
		if ( dc_startd )
		{

			if ( dc_startd->_suspendClaim() )
				bRet = true;
			else
				dprintf( D_ALWAYS, "RemoteResource::suspend(): dc_startd->suspendClaim FAILED!\n");

		}
		else
			dprintf( D_ALWAYS, "RemoteResource::suspend(): DCStartd object NULL!\n");

	}
	else
		dprintf( D_ALWAYS, "RemoteResource::suspend(): Not connected to resource!\n");

	return (bRet);
}

bool RemoteResource::resume()
{
	bool bRet = false;
	
	if (state != RR_RECONNECT )
	{
		if ( dc_startd->_continueClaim() )
			bRet = true;
		else
			dprintf( D_ALWAYS, "RemoteResource::resume(): dc_startd->resume FAILED!\n");
	}
	else
		dprintf( D_ALWAYS, "RemoteResource::resume(): Not connected to resource!\n");
		
	return (bRet);
}


void
RemoteResource::dprintfSelf( int debugLevel )
{
	dprintf ( debugLevel, "RemoteResource::dprintSelf printing "
					  "host info:\n");
	if( dc_startd ) {
		const char* addr = dc_startd->addr();
		const char* id = dc_startd->getClaimId();
		ClaimIdParser cid(id ? id : "");
		dprintf( debugLevel, "\tstartdAddr: %s\n",
		         addr ? addr : "Unknown" );
		dprintf( debugLevel, "\tClaimId: %s\n",
		         cid.publicClaimId() );
	}
	if( machineName ) {
		dprintf( debugLevel, "\tmachineName: %s\n", machineName );
	}
	if( starterAddress ) {
		dprintf( debugLevel, "\tstarterAddr: %s\n", starterAddress );
	}
	dprintf( debugLevel, "\texit_reason: %d\n", exit_reason );
	dprintf( debugLevel, "\texited_by_signal: %s\n",
	         exited_by_signal ? "True" : "False" );
	if( exited_by_signal ) {
		dprintf( debugLevel, "\texit_signal: %lld\n",
		         (long long)exit_value );
	} else {
		dprintf( debugLevel, "\texit_code: %lld\n",
		         (long long)exit_value );
	}
}

void
RemoteResource::attemptShutdownTimeout( int /* timerID */ )
{
	m_attempt_shutdown_tid = -1;
	attemptShutdown();
}

void
RemoteResource::abortFileTransfer()
{
	filetrans.stopServer();
	if( state == RR_PENDING_TRANSFER ) {
		setResourceState(RR_FINISHED);
	}
}

void
RemoteResource::attemptShutdown()
{
	if( filetrans.transferIsInProgress() ) {
			// This can happen if we process the job exit message
			// before the file transfer reaper processes the exit of
			// the file transfer process

		if( m_attempt_shutdown_tid != -1 ) {
			return;
		}
		if( m_started_attempting_shutdown == 0 ) {
			m_started_attempting_shutdown = time(NULL);
		}
		int total_delay = time(NULL) - m_started_attempting_shutdown;
		if( abs(total_delay) > 300 ) {
				// Something is wrong.  We should not have had to wait this long
				// for the file transfer reaper to finish.
			dprintf(D_ALWAYS,"WARNING: giving up waiting for file transfer "
					"to finish after %ds.  Shutting down shadow.\n",total_delay);
		}
		else {
			m_attempt_shutdown_tid = daemonCore->Register_Timer(1, 0,
				(TimerHandlercpp)&RemoteResource::attemptShutdownTimeout,
				"attempt shutdown", this);
			if( m_attempt_shutdown_tid != -1 ) {
				dprintf(D_FULLDEBUG,"Delaying shutdown of shadow, because file transfer is still active.\n");
				return;
			}
		}
	}

	abortFileTransfer();

		// we call our shadow's shutdown method:
	shadow->shutDown( exit_reason, "" );
}

int
RemoteResource::handleSysCalls( Stream * /* sock */ )
{

		// change value of the syscall_sock to correspond with that of
		// this claim sock right before do_REMOTE_syscall().

	syscall_sock = claim_sock;
	thisRemoteResource = this;

	if (do_REMOTE_syscall() < 0) {
		dprintf(D_SYSCALLS,"Shadow: do_REMOTE_syscall returned < 0\n");
		attemptShutdown();
		return KEEP_STREAM;
	}
	hadContact();
	return KEEP_STREAM;
}


void
RemoteResource::getMachineName( char *& mName )
{

	if ( !mName ) {
		mName = strdup( machineName );
	} else {
		if ( machineName ) {
			free( mName );
			mName = strdup( machineName );
		} else {
			mName[0] = '\0';
		}
	}
}


void
RemoteResource::getStartdAddress( char *& sinful )
{
	if( sinful ) {
		sinful[0] = '\0';
	}
	if( ! dc_startd ) {
		return;
	}
	const char* addr = dc_startd->addr();
	if( ! addr ) {
		return;
	}
	if( sinful ) {
		free( sinful );	}
	sinful = strdup( addr );
}

void
RemoteResource::getStartdName( char *& remoteName )
{
	if( remoteName ) {
		remoteName[0] = '\0';
	}
	if( ! dc_startd ) {
		return;
	}
	const char* localName = dc_startd->name();
	if( ! localName ) {
		return;
	}
	if( remoteName ) {
		free( remoteName );
	}
	remoteName = strdup( localName );
}



void
RemoteResource::getClaimId( char *& id )
{
	if( id ) {
		id[0] = '\0';
	}
	if( ! dc_startd ) {
		return;
	}
	const char* my_id = dc_startd->getClaimId();
	if( ! my_id ) {
		return;
	}
	if( id ) {
		free( id );
	}
	id = strdup( my_id );
}

void
RemoteResource::getStarterAddress( char *& starterAddr )
{

	if (!starterAddr) {
		starterAddr = starterAddress ? strdup( starterAddress ) : NULL;
	} else {
		if ( starterAddress ) {
			free( starterAddr );
			starterAddr = strdup( starterAddress );
		} else {
			starterAddr[0] = '\0';
		}
	}
}

ReliSock*
RemoteResource::getClaimSock()
{
	return claim_sock;
}


void
RemoteResource::closeClaimSock( void )
{
	if( claim_sock ) {
		daemonCore->Cancel_Socket( claim_sock );
		delete claim_sock;
		claim_sock = NULL;
	}
}

void
RemoteResource::disconnectClaimSock(const char *err_msg)
{
	abortFileTransfer();

	if (!claim_sock) {
		return;
	}

	std::string my_err_msg;
	formatstr(my_err_msg, "%s %s",
	          (err_msg ? err_msg : "Disconnecting from starter"),
	          claim_sock->get_sinful_peer());

	if (!thisRemoteResource) return;

	thisRemoteResource->closeClaimSock();

	if( Shadow->supportsReconnect() ) {
			// instead of having to EXCEPT, we can now try to
			// reconnect.  happy day! :)
		dprintf( D_ALWAYS, "%s\n", my_err_msg.c_str() );

		Shadow->resourceDisconnected(thisRemoteResource);

		if (!Shadow->shouldAttemptReconnect(thisRemoteResource)) {
			dprintf(D_ALWAYS, "This job cannot reconnect to starter, so job exiting\n");
			Shadow->gracefulShutDown();
			EXCEPT( "%s", my_err_msg.c_str() );
		}
			// tell the shadow to start trying to reconnect
		Shadow->reconnect();
	} else {
			// The remote starter doesn't support it, so give up
			// like we always used to.
		EXCEPT( "%s", my_err_msg.c_str() );
	}
}

int
RemoteResource::getExitReason() const
{
	return exit_reason;
}

bool
RemoteResource::claimIsClosing() const
{
	return claim_is_closing;
}


int64_t
RemoteResource::exitSignal( void ) const
{
	if( exited_by_signal ) {
		return exit_value;
	}
	return -1;
}


int64_t
RemoteResource::exitCode( void ) const
{
	if( ! exited_by_signal ) {
		return exit_value;
	}
	return -1;
}


void
RemoteResource::setStartdInfo( ClassAd* ad )
{
	char* name = NULL;
	ad->LookupString( ATTR_REMOTE_HOST, &name );
	if( ! name ) {
		ad->LookupString( ATTR_NAME, &name );
		if( ! name ) {
			dPrintAd(D_ALWAYS, *ad);
			EXCEPT( "ad includes neither %s nor %s!", ATTR_NAME,
					ATTR_REMOTE_HOST );
		}
	}

	char* pool = NULL;
	ad->LookupString( ATTR_REMOTE_POOL, &pool );
		// we don't care if there's no pool specified...

	char* claim_id = NULL;
	ad->LookupString( ATTR_CLAIM_ID, &claim_id );
	if( ! claim_id ) {
		EXCEPT( "ad does not include %s!", ATTR_CLAIM_ID );
	}

	char* addr = NULL;
	ad->LookupString( ATTR_STARTD_IP_ADDR, &addr );
	if( ! addr ) {
		EXCEPT( "missing %s in ad", ATTR_STARTD_IP_ADDR);
	}

	initStartdInfo( name, pool, addr, claim_id );
	free(name);
	if(pool) free(pool);
	free(addr);
	free(claim_id);
}


void
RemoteResource::setStartdInfo( const char *sinful, 
							   const char* claim_id ) 
{
	initStartdInfo( sinful, NULL, NULL, claim_id );
}


void
RemoteResource::initStartdInfo( const char *name, const char *pool,
								const char *addr, const char* claim_id )
{
	dprintf( D_FULLDEBUG, "in RemoteResource::initStartdInfo()\n" );  

	if( dc_startd ) {
		delete dc_startd;
	}
	dc_startd = new DCStartd( name, pool, addr, claim_id );

	if( name ) {
		setMachineName( name );
	} else if( addr ) {
		setMachineName( addr );
	} else {
		EXCEPT( "in RemoteResource::setStartdInfo() without name or addr" );
	}

	m_claim_session = ClaimIdParser(claim_id);
	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) ) {
		if( m_claim_session.secSessionId()[0] == '\0' ) {
			dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: warning - failed to create security session from claim id %s because claim has no session information, likely because the matched startd has SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION set to False\n",m_claim_session.publicClaimId());
		}
		else {
			bool rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				DAEMON,
				m_claim_session.secSessionId(),
				m_claim_session.secSessionKey(),
				m_claim_session.secSessionInfo(),
				AUTH_METHOD_MATCH,
				EXECUTE_SIDE_MATCHSESSION_FQU,
				dc_startd->addr(),
				0 /*don't expire*/,
				nullptr, false );

			if( !rc ) {
				dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create security session for %s, so will fall back on security negotiation\n",m_claim_session.publicClaimId());
			}

				// For the file transfer session, we do not want to use the
				// same claim session used for other DAEMON traffic to the
				// execute node, because file transfer is normally done at
				// WRITE level, giving at least some level of indepenent
				// control for file transfers for settings such as encryption
				// and integrity checking.  Also, the session attributes
				// (e.g. encryption, integrity) for the claim session were set
				// by the startd, but for file transfer, it makes more sense
				// to use the shadow's policy.

			std::string filetrans_claimid;
				// prepend something to the claim id so that the session id
				// is different for file transfer than for the claim session
			formatstr(filetrans_claimid,"filetrans.%s",claim_id);
			m_filetrans_session = ClaimIdParser(filetrans_claimid.c_str());

				// Get rid of session parameters set by startd.
				// We will set our own based on the shadow WRITE policy.
			m_filetrans_session.setSecSessionInfo(NULL);

				// Since we just removed the session info, we must
				// set ignore_session_info=true in the following call or
				// we will get NULL for the session id.
			std::string filetrans_session_id =
				m_filetrans_session.secSessionId(/*ignore_session_info=*/ true);

			rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				WRITE,
				filetrans_session_id.c_str(),
				m_filetrans_session.secSessionKey(),
				NULL,
				AUTH_METHOD_MATCH,
				EXECUTE_SIDE_MATCHSESSION_FQU,
				NULL,
				0 /*don't expire*/,
				nullptr, true );

			if( !rc ) {
				dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create security session for %s, so will fall back on security negotiation\n",m_filetrans_session.publicClaimId());
			}
			else {
					// fill in session_info so that starter will have
					// enough info to create a security session
					// compatible with the one we just created.
				std::string session_info;
				rc = daemonCore->getSecMan()->ExportSecSessionInfo(
					filetrans_session_id.c_str(),
					session_info );

				if( !rc ) {
					dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to get session info for claim id %s\n",m_filetrans_session.publicClaimId());
				}
				else {
					m_filetrans_session.setSecSessionInfo( session_info.c_str() );
				}
			}
		}
	}
}


void
RemoteResource::setStarterInfo( ClassAd* ad )
{

	std::string buf;
	dprintf(D_MACHINE, "StarterInfo ad:\n%s", formatAd(buf, *ad, "\t")); // formatAt guarantees a newline.

	// save (most of) the incoming starter ad for later
	if (starterAd) { starterAd->Clear(); }
	else { starterAd = new ClassAd(); }
	starterAd->Update(*ad);

	if( ad->LookupString(ATTR_STARTER_IP_ADDR, buf) ) {
		setStarterAddress( buf.c_str() );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_STARTER_IP_ADDR, buf.c_str() );
		starterAd->Delete(ATTR_STARTER_IP_ADDR);
	}

	if( ad->LookupString(ATTR_UID_DOMAIN, buf) ) {
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_UID_DOMAIN, buf.c_str() );
		starterAd->Delete(ATTR_UID_DOMAIN);
	}

	if( ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, buf) ) {
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_FILE_SYSTEM_DOMAIN, buf.c_str() );
		starterAd->Delete(ATTR_FILE_SYSTEM_DOMAIN);
	}

	if( ad->LookupString(ATTR_NAME, buf) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				free(machineName);
				machineName = strdup( buf.c_str() );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, buf.c_str() );
	} else if( ad->LookupString(ATTR_MACHINE, buf) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				free(machineName);
				machineName = strdup( buf.c_str() );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, buf.c_str() );
	}

	char* starter_version=NULL;
	if( ad->LookupString(ATTR_VERSION, &starter_version) ) {
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_VERSION, starter_version ); 
	}

	if ( starter_version == NULL ) {
		dprintf( D_ALWAYS, "Can't determine starter version for FileTransfer!\n" );
	} else {
		filetrans.setPeerVersion( starter_version );
		free(starter_version);
	}

	filetrans.setTransferQueueContactInfo( shadow->getTransferQueueContactInfo() );

	if( ad->LookupBool(ATTR_HAS_RECONNECT, supports_reconnect) ) {
			// Whatever the starter defines in its own classad
			// overrides whatever we might think...
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_HAS_RECONNECT, 
				 supports_reconnect ? "TRUE" : "FALSE" );
	} else {
		dprintf( D_SYSCALLS, "  %s = FALSE (not specified)\n",
				 ATTR_HAS_RECONNECT );
	}
}

void
RemoteResource::populateExecuteEvent(std::string & slotName, ClassAd & props)
{
	const ClassAd * starterAd = getStarterAd();
	if (starterAd) {
		starterAd->LookupString(ATTR_NAME, slotName);

		std::string scratch;
		if (starterAd->LookupString(ATTR_CONDOR_SCRATCH_DIR, scratch)) {
			props.Assign(ATTR_CONDOR_SCRATCH_DIR, scratch);
		}

		CopyMachineResources(props, *starterAd, false);
		if (jobAd) {
			std::string requestAttrs;
			if (jobAd->LookupString(ATTR_ULOG_EXECUTE_EVENT_ATTRS, requestAttrs)) {
				CopySelectAttrs(props, *starterAd, requestAttrs, false); //Don't allow overwrites
			}
		}
	} else if (machineName) {
		slotName = machineName;
	} else if (dc_startd) {
		const char* localName = dc_startd->name();
		if (localName ) {
			slotName = localName;
		}
	}
}

void
RemoteResource::setMachineName( const char * mName )
{

	if ( machineName )
		free(machineName);
	
	machineName = strdup ( mName );
}

void
RemoteResource::setStarterAddress( const char * starterAddr )
{
	if( starterAddress ) {
		free( starterAddress );
	}	
	starterAddress = strdup( starterAddr );
}


void
RemoteResource::setExitReason( int reason )
{
	// Set the exit_reason, but not if the reason is JOB_KILLED.
	// This prevents exit_reason being reset from JOB_KILLED to
	// JOB_SHOULD_REQUEUE or some such when the starter gets killed
	// and the syscall sock goes away.

	if( exit_reason != JOB_KILLED && -1 == exit_reason) {
		dprintf( D_FULLDEBUG, "setting exit reason on %s to %d\n",
		         machineName ? machineName : "???", reason );
			
		exit_reason = reason;
	}

	// record that this resource is really finished
	if( filetrans.transferIsInProgress() ) {
		setResourceState( RR_PENDING_TRANSFER );
	}
	else {
		setResourceState( RR_FINISHED );
	}
}


float
RemoteResource::bytesSent() const
{
	float bytes = 0.0;

	// add in bytes sent by transferring files
	bytes += filetrans.TotalBytesSent();

	// add in bytes sent via remote system calls

	/*** until the day we support syscalls in the new shadow 
	if (syscall_sock) {
		bytes += syscall_sock->get_bytes_sent();
	}
	****/
	
	return bytes;
}


float
RemoteResource::bytesReceived() const
{
	float bytes = 0.0;

	// add in bytes sent by transferring files
	bytes += filetrans.TotalBytesReceived();

	// add in bytes sent via remote system calls

	/*** until the day we support syscalls in the new shadow 
	if (syscall_sock) {
		bytes += syscall_sock->get_bytes_recvd();
	}
	****/
	
	return bytes;
}


void
RemoteResource::setJobAd( ClassAd *jA )
{
	this->jobAd = jA;

		// now, see if anything we care about is defined in the ad.
		// this prevents us (for example) from logging another
		// ImageSizeEvent everytime we start running, even if the
		// image size hasn't changed at all...

	int64_t long_value;
	double real_value;

	// REMOTE_SYS_CPU and REMOTE_USER_CPU reflect usage for this execution only.
	// reset to 0 on start or restart.
	real_value = 0.0;
	remote_rusage.ru_stime.tv_sec = (time_t) real_value;
	remote_rusage.ru_utime.tv_sec = (time_t) real_value;
	jA->Assign(ATTR_JOB_REMOTE_USER_CPU, real_value);
	jA->Assign(ATTR_JOB_REMOTE_SYS_CPU, real_value);
			
	if( jA->LookupInteger(ATTR_IMAGE_SIZE, long_value) ) {
		image_size_kb = long_value;
	}

	if( jA->LookupInteger(ATTR_MEMORY_USAGE, long_value) ) {
		memory_usage_mb = long_value;
	}

	if( jA->LookupInteger(ATTR_RESIDENT_SET_SIZE, long_value) ) {
		remote_rusage.ru_maxrss = long_value;
	}

	if( jA->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE, long_value) ) {
		proportional_set_size_kb = long_value;
	}

	// DiskUsage and ScratchDirFileCount should reflect the usage for this single execution
	// so we *don't* propagate the values from the initial job ad
#if 1
	disk_usage = 0;
	scratch_dir_file_count = -1; // -1 because the starter may never send us a value
#else

	if( jA->LookupInteger(ATTR_DISK_USAGE, long_value) ) {
		disk_usage = long_value;
	}

	if( jA->LookupInteger(ATTR_SCRATCH_DIR_FILE_COUNT, long_value) ) {
		scratch_dir_file_count = long_value;
	}
#endif

	if( jA->LookupInteger(ATTR_LAST_JOB_LEASE_RENEWAL, long_value) ) {
		last_job_lease_renewal = (time_t)long_value;
	}

	jA->LookupBool( ATTR_WANT_IO_PROXY, m_want_chirp );
	jA->LookupBool( ATTR_WANT_REMOTE_UPDATES, m_want_remote_updates );
	jA->LookupBool( ATTR_WANT_DELAYED_UPDATES, m_want_delayed );

	bool stream_input=false, stream_output=false, stream_error=false;
	jA->LookupBool(ATTR_STREAM_INPUT,stream_input);
	jA->LookupBool(ATTR_STREAM_OUTPUT,stream_output);
	jA->LookupBool(ATTR_STREAM_ERROR,stream_error);

	m_want_streaming_io = stream_input || stream_output || stream_error;
	if( m_want_chirp || m_want_streaming_io ) {
		dprintf(D_FULLDEBUG,
			"Enabling remote IO syscalls (want chirp=%s,want streaming=%s).\n",
			m_want_chirp ? "true" : "false",
			m_want_streaming_io ? "true" : "false");
	}
	if( m_want_chirp || m_want_remote_updates )
	{
		dprintf(D_FULLDEBUG, "Enabling remote updates.\n");
	}

	jA->LookupString(ATTR_X509_USER_PROXY, proxy_path);
}

void
RemoteResource::updateFromStarterTimeout( int /* timerID */ )
{
	// If we landed here, then we expected to receive an update from the starter,
	// but it didn't arrive yet.  Even if the remote syscall sock is still connected,
	// failing to receive an update could mean that the starter is wedged or dead
	// (and some goofed up NAT box is artifically keeping the remote syscall sock connected).

	// So instead of the shadow hanging out forever waiting to hear from a starter that
	// might be dead, close our remote syscall sock now and try to reconnect.

	// NOTE: only close the sock if we are executing the job, not shutting down or
	// transferring files or whatever.  Why?  Well, in these situations, the starter
	// may legitimately be fine and yet not sending updates anymore
	// because it is about to shutdown, or because a long file transfer is
	// happening in the foreground.

	if (getResourceState() == RR_EXECUTING && !filetrans.transferIsInProgress()) {
		disconnectClaimSock("Failed to receive an update from the starter when expected, forcing disconnect");
	}

	// Timer is not periodic, so since it has now fired, the tid is invalid. Clear it.
	no_update_received_tid = -1;
}


void
RemoteResource::updateFromStarter( ClassAd* update_ad )
{
	int64_t long_value;
	std::string string_value;
	bool bool_value;

	int maxinterval = 0;
	update_ad->LookupInteger(ATTR_JOBINFO_MAXINTERVAL, maxinterval);
	if (no_update_received_tid != -1) {
		daemonCore->Cancel_Timer(no_update_received_tid);
		no_update_received_tid = -1;
	}
	if (maxinterval > 0) {
		no_update_received_tid = daemonCore->Register_Timer(
			maxinterval * 3,  // should receive another update in maxinterval secs, x3 to be sure
			(TimerHandlercpp)&RemoteResource::updateFromStarterTimeout,
			"RemoteResource::updateFromStarterTimeout()", this
		);
	}

	dprintf( D_FULLDEBUG, "Inside RemoteResource::updateFromStarter() maxinterval=%d\n",
		maxinterval);
	hadContact();

	if( IsDebugLevel(D_MACHINE) ) {
		dprintf( D_MACHINE, "Update ad:\n" );
		dPrintAd( D_MACHINE, *update_ad );
		dprintf( D_MACHINE, "--- End of ClassAd ---\n" );
	}

	double real_value;
	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, real_value) ) {
		double prevUsage;
		if (!jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, prevUsage)) {
			prevUsage = 0.0;
		}

		// Remote cpu usage should be strictly increasing
		if (real_value > prevUsage) {
			double prevTotalUsage = 0.0;
			jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU, prevTotalUsage);
			jobAd->Assign(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU, prevTotalUsage + (real_value - prevUsage));

			// Also, do not reset remote cpu unless there was an increase, to guard
			// against the case starter sending a zero value (perhaps right when the
			// job terminates).
			remote_rusage.ru_stime.tv_sec = (time_t)real_value;
			jobAd->Assign(ATTR_JOB_REMOTE_SYS_CPU, real_value);
		}
	}

	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, real_value) ) {
		double prevUsage;
		if (!jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, prevUsage)) {
			prevUsage = 0.0;
		}

		// Remote cpu usage should be strictly increasing
		if (real_value > prevUsage) {
			double prevTotalUsage = 0.0;
			jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU, prevTotalUsage);
			jobAd->Assign(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU, prevTotalUsage + (real_value - prevUsage));

			// Also, do not reset remote cpu unless there was an increase, to guard
			// against the case starter sending a zero value (perhaps right when the
			// job terminates).
			remote_rusage.ru_utime.tv_sec = (time_t)real_value;
			jobAd->Assign(ATTR_JOB_REMOTE_USER_CPU, real_value);

		}
	}

	if( update_ad->LookupInteger(ATTR_IMAGE_SIZE, long_value) ) {
		if( long_value > image_size_kb ) {
			image_size_kb = long_value;
			jobAd->Assign(ATTR_IMAGE_SIZE, image_size_kb);
		}
	}

	// Update memory_usage_mb, which should be the maximum value seen
	// in the update ad for ATTR_MEMORY_USAGE
	if( EvalInteger(ATTR_MEMORY_USAGE, update_ad, NULL, long_value) ) {
		if( long_value > memory_usage_mb ) {
			memory_usage_mb = long_value;
		}
	}
	// Now update MemoryUsage in the job ad.  If the update ad sent us 
	// a literal value for MemoryUsage, then insert in the job ad the MAX 
	// value we have seen.  But if the update ad sent us MemoryUsage as an
	// expression, then just copy the expression into the job ad.
	classad::ExprTree * tree = update_ad->Lookup(ATTR_MEMORY_USAGE);
	if (tree) {
		if (dynamic_cast<classad::Literal *>(tree) == nullptr) {
				// Copy the exression over
			tree = tree->Copy();
			jobAd->Insert(ATTR_MEMORY_USAGE, tree);
		} else {
				// It is a literal, so insert the MAX of the literals seen
			jobAd->Assign(ATTR_MEMORY_USAGE, memory_usage_mb);
		}
	}

	if( update_ad->LookupFloat(ATTR_JOB_VM_CPU_UTILIZATION, real_value) ) {
		  jobAd->Assign(ATTR_JOB_VM_CPU_UTILIZATION, real_value);
	}

	if( update_ad->LookupInteger(ATTR_RESIDENT_SET_SIZE, long_value) ) {
		int64_t rss = remote_rusage.ru_maxrss;
		if( !jobAd->LookupInteger(ATTR_RESIDENT_SET_SIZE,rss) || rss < long_value ) {
			remote_rusage.ru_maxrss = long_value;
			jobAd->Assign(ATTR_RESIDENT_SET_SIZE, long_value);
		}
	}

	if( update_ad->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE, long_value) ) {
		int64_t pss = proportional_set_size_kb;
		if( !jobAd->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE,pss) || pss < long_value ) {
			proportional_set_size_kb = long_value;
			jobAd->Assign(ATTR_PROPORTIONAL_SET_SIZE, long_value);
		}
	}

    CopyAttribute(ATTR_NETWORK_IN, *jobAd, *update_ad);
    CopyAttribute(ATTR_NETWORK_OUT, *jobAd, *update_ad);

    CopyAttribute(ATTR_BLOCK_READ_KBYTES, *jobAd, *update_ad);
    CopyAttribute(ATTR_BLOCK_WRITE_KBYTES, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_READ_KBYTES, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_WRITE_KBYTES, *jobAd, *update_ad);

    CopyAttribute(ATTR_BLOCK_READ_BYTES, *jobAd, *update_ad);
    CopyAttribute(ATTR_BLOCK_WRITE_BYTES, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_READ_BYTES, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_WRITE_BYTES, *jobAd, *update_ad);

    CopyAttribute(ATTR_BLOCK_READS, *jobAd, *update_ad);
    CopyAttribute(ATTR_BLOCK_WRITES, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_READS, *jobAd, *update_ad);
    CopyAttribute("Recent" ATTR_BLOCK_WRITES, *jobAd, *update_ad);

    CopyAttribute(ATTR_IO_WAIT, *jobAd, *update_ad);
    CopyAttribute(ATTR_JOB_CPU_INSTRUCTIONS, *jobAd, *update_ad);

	// FIXME: If we're convinced that we want a whitelist here (chirp
	// would seem to make a mockery of that), we should at least rewrite
	// all of the copies to be based on a table.
	CopyAttribute( "PreExitCode", *jobAd, *update_ad );
	CopyAttribute( "PreExitSignal", *jobAd, *update_ad );
	CopyAttribute( "PreExitBySignal", *jobAd, *update_ad );

	CopyAttribute( "PostExitCode", *jobAd, *update_ad );
	CopyAttribute( "PostExitSignal", *jobAd, *update_ad );
	CopyAttribute( "PostExitBySignal", *jobAd, *update_ad );

	classad::ClassAd * toeTag = dynamic_cast<classad::ClassAd *>(update_ad->Lookup(ATTR_JOB_TOE));
	if( toeTag ) {
		CopyAttribute(ATTR_JOB_TOE, *jobAd, *update_ad );

		// Required to actually update the schedd's copy.  (sigh)
		shadow->watchJobAttr(ATTR_JOB_TOE);
	}

    // You MUST NOT use CopyAttribute() here, because the starter doesn't
    // send this on every update: CopyAttribute() deletes the target's
    // attribute if it doesn't exist in the source, which means the schedd
    // may never see this update (because update the schedd on a timer
    // instead of after receiving an update).
    int checkpointNumber = -1;
    if( update_ad->LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber )) {
        jobAd->Assign( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
    }

    // Likewise, most starter updates don't include the newly committed time.
    int newlyCommittedTime = 0;
    if( update_ad->LookupInteger( ATTR_JOB_NEWLY_COMMITTED_TIME, newlyCommittedTime ) ) {
        int committedTime = 0;
        jobAd->LookupInteger( ATTR_JOB_COMMITTED_TIME, committedTime );
        committedTime += newlyCommittedTime;
        jobAd->Assign( ATTR_JOB_COMMITTED_TIME, committedTime );
    }

    // ... or the time of the time of the last checkpoint.  At some point,
    // we might decide that it's safe to trigger all of the left-over old
    // standard universe code by using its attribute names, but let's not
    // for now.
    int lastCheckpointTime = -1;
    if( update_ad->LookupInteger( ATTR_JOB_LAST_CHECKPOINT_TIME, lastCheckpointTime ) ) {
        jobAd->Assign( ATTR_JOB_LAST_CHECKPOINT_TIME, lastCheckpointTime );
    }

    // these are headed for job ads in the scheduler, so rename them
    // to prevent these from colliding with similar attributes from schedd statistics
    CopyAttribute("StatsLastUpdateTimeStarter", *jobAd, "StatsLastUpdateTime", *update_ad);
    CopyAttribute("StatsLifetimeStarter", *jobAd, "StatsLifetime", *update_ad);
    CopyAttribute("RecentStatsLifetimeStarter", *jobAd, "RecentStatsLifetime", *update_ad);
    CopyAttribute("RecentWindowMaxStarter", *jobAd, "RecentWindowMax", *update_ad);
    CopyAttribute("RecentStatsTickTimeStarter", *jobAd, "RecentStatsTickTime", *update_ad);

	if( update_ad->LookupInteger(ATTR_DISK_USAGE, long_value) ) {
		if( long_value > disk_usage ) {
			disk_usage = long_value;
			jobAd->Assign(ATTR_DISK_USAGE, disk_usage);
		}
	}

	if( update_ad->LookupInteger(ATTR_SCRATCH_DIR_FILE_COUNT, long_value) ) {
		if( long_value > scratch_dir_file_count ) {
			scratch_dir_file_count = long_value;
			jobAd->Assign(ATTR_SCRATCH_DIR_FILE_COUNT, scratch_dir_file_count);
		}
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_HIERARCHY,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_HIERARCHY, string_value);
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_NAME,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_NAME, string_value);
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_TYPE,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_TYPE, string_value);
	}

	if( update_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		jobAd->Assign(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal);
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, long_value) ) {
		jobAd->Assign(ATTR_ON_EXIT_SIGNAL, long_value);
		exit_value = long_value;
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_CODE, long_value) ) {
		jobAd->Assign(ATTR_ON_EXIT_CODE, long_value);
		exit_value = long_value;
	}

	if( update_ad->LookupString(ATTR_EXIT_REASON,string_value) ) {
		jobAd->Assign(ATTR_EXIT_REASON, string_value);
	}

	if( update_ad->LookupBool(ATTR_JOB_CORE_DUMPED, bool_value) ) {
		jobAd->Assign(ATTR_JOB_CORE_DUMPED, bool_value);
	}


	std::string PluginResultList = "PluginResultList";
	std::array< std::string, 3 > prefixes( { "Input", "Checkpoint", "Output" } );
	for( const auto & prefix : prefixes ) {
		std::string attributeName = prefix + PluginResultList;
		ExprTree * resultList = update_ad->LookupExpr( attributeName );
		if( resultList != NULL ) {
			classad::ClassAd c;
			// Arguably, the epoch log would be easier to parse if the
			// attribute name were always PluginResultList.
			c.Insert( attributeName, resultList );
			writeJobEpochFile( jobAd, & c, as_upper_case(prefix).c_str() );
			c.Remove( attributeName );
			writeJobEpochFile( jobAd, starterAd, "STARTER" );
		}
	}


		// The starter sends this attribute whether or not we are spooling
		// output (because it doesn't know if we are).  Technically, we
		// only need to write this attribute into the job ClassAd if we
		// are spooling output.  However, it doesn't hurt to have it there
		// otherwise.
	if( update_ad->LookupString(ATTR_SPOOLED_OUTPUT_FILES,string_value) ) {
		jobAd->Assign(ATTR_SPOOLED_OUTPUT_FILES,string_value);
	}
	else if( jobAd->LookupString(ATTR_SPOOLED_OUTPUT_FILES,string_value) ) {
		jobAd->AssignExpr(ATTR_SPOOLED_OUTPUT_FILES,"UNDEFINED");
	}

		// Process all chirp-based updates from the starter.
	for (classad::ClassAd::const_iterator it = update_ad->begin(); it != update_ad->end(); it++) {
		size_t offset = -1;
		if (allowRemoteWriteAttributeAccess(it->first)) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		} else if( (offset = it->first.rfind( "AverageUsage" )) != std::string::npos
			&& offset == it->first.length() - 12 ) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		} else if( (offset = it->first.rfind( "Usage" )) != std::string::npos
			&& it->first != ATTR_MEMORY_USAGE  // ignore MemoryUsage, we handle it above
			&& it->first != ATTR_DISK_USAGE    // ditto
			// the ATTR_JOB_*_CPU attributes don't end in "Usage"
			&& offset == it->first.length() - 5 ) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		} else if( (offset = it->first.rfind( "Provisioned" )) != std::string::npos
			&& offset == it->first.length() - 11 ) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		} else if( it->first.find( "Assigned" ) == 0 ) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		// Arguably, this should actually check the list of container services
		// and only forward the matching attributes through, but I'm not
		// actually worried.
		} else if( ends_with( it->first, "_HostPort" ) ) {
			classad::ExprTree *expr_copy = it->second->Copy();
			jobAd->Insert(it->first, expr_copy);
			shadow->watchJobAttr(it->first);
		}
	}

	char* job_state = NULL;
	ResourceState new_state = state;
	update_ad->LookupString( ATTR_JOB_STATE, &job_state );
	if( job_state ) { 
			// The starter told us the job state, see what it is and
			// if we need to log anything to the UserLog
		if( strcasecmp(job_state, "Suspended") == MATCH ) {
			new_state = RR_SUSPENDED;
		} else if ( strcasecmp(job_state, "Running") == MATCH ) {
			new_state = RR_EXECUTING;
		} else if ( strcasecmp(job_state, "Checkpointed") == MATCH ) {
			new_state = RR_CHECKPOINTED;
		} else { 
				// For our purposes in here, we don't care about any
				// other possible states at the moment.  If the job
				// has state "Exited", we'll see all that in a second,
				// and we don't want to log any events here. 
		}
		free( job_state );
		if( state == RR_PENDING_DEATH || state == RR_PENDING_TRANSFER ) {
				// we're trying to shutdown, so don't bother recording
				// what we just heard from the starter.  we're done
				// dealing with this update.
			return;
		}
		if( new_state != state ) {
				// State change!  Let's log the appropriate event to
				// the UserLog and keep track of suspend/resume
				// statistics.
			switch( new_state ) {
			case RR_SUSPENDED:
				recordSuspendEvent( update_ad );
				break;
			case RR_EXECUTING:
				if( state == RR_SUSPENDED ) {
					recordResumeEvent( update_ad );
				}
				break;
			case RR_CHECKPOINTED:
				recordCheckpointEvent( update_ad );
				break;
			default:
				EXCEPT( "Trying to log state change for invalid state %s",
						rrStateToString(new_state) );
			}
				// record the new state
			setResourceState( new_state );
		}
	}

	std::string starter_addr;
	update_ad->LookupString( ATTR_STARTER_IP_ADDR, starter_addr );
	if( !starter_addr.empty() ) {
		// The starter sends updated contact info along with the job
		// update (useful if CCB info changes).  It's a bit of a hack
		// to do it through this channel, but better than nothing.
		setStarterAddress( starter_addr.c_str() );
	}

	if( IsDebugLevel(D_MACHINE) ) {
		dprintf( D_MACHINE, "shadow's job ad after update:\n" );
		dPrintAd( D_MACHINE, * jobAd );
		dprintf( D_MACHINE, "--- End of ClassAd ---\n" );
	}

		// now that we've gotten an update, we should evaluate our
		// periodic user policy again, since we have new information
		// and something might now evaluate to true which we should
		// act on.
	shadow->evalPeriodicUserPolicy();
}


bool
RemoteResource::recordSuspendEvent( ClassAd* update_ad )
{
	bool rval = true;
		// First, grab the number of pids that were suspended out of
		// the update ad.
	int num_pids;

	if( ! update_ad->LookupInteger(ATTR_NUM_PIDS, num_pids) ) {
		dprintf( D_FULLDEBUG, "update ad from starter does not define %s\n",
				 ATTR_NUM_PIDS );  
		num_pids = 0;
	}
	
		// Now, we can log this event to the UserLog
	JobSuspendedEvent event;
	event.num_pids = num_pids;
	if( !writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n" );
		rval = false;
	}

		// Finally, we need to update some attributes in our in-memory
		// copy of the job ClassAd
	time_t now = time(nullptr);
	int total_suspensions = 0;

	jobAd->LookupInteger( ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	total_suspensions++;
	jobAd->Assign( ATTR_TOTAL_SUSPENSIONS, total_suspensions );

	jobAd->Assign( ATTR_LAST_SUSPENSION_TIME, now );

	// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );
	
	// TSTCLAIR: In promotion to 1st class status we *must* 
	// update the job in the queue
	jobAd->Assign( ATTR_JOB_STATUS, SUSPENDED );
	shadow->updateJobInQueue(U_STATUS);
	
	return rval;
}


bool
RemoteResource::recordResumeEvent( ClassAd* /* update_ad */ )
{
	bool rval = true;

		// First, log this to the UserLog
	JobUnsuspendedEvent event;
	if( !writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n" );
		rval = false;
	}

		// Now, update our in-memory copy of the job ClassAd
	time_t now = time(nullptr);
	int cumulative_suspension_time = 0;
	time_t last_suspension_time = 0;

		// add in the time I spent suspended to a running total
	jobAd->LookupInteger( ATTR_CUMULATIVE_SUSPENSION_TIME,
						  cumulative_suspension_time );
	jobAd->LookupInteger( ATTR_LAST_SUSPENSION_TIME,
						  last_suspension_time );
	if( last_suspension_time > 0 ) {
		// There was a real job suspension.
		cumulative_suspension_time += now - last_suspension_time;

		int uncommitted_suspension_time = 0;
		jobAd->LookupInteger( ATTR_UNCOMMITTED_SUSPENSION_TIME,
							  uncommitted_suspension_time );
		uncommitted_suspension_time += now - last_suspension_time;
		jobAd->Assign(ATTR_UNCOMMITTED_SUSPENSION_TIME, uncommitted_suspension_time);
	}

	jobAd->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, cumulative_suspension_time );

		// set the current suspension time to zero, meaning not suspended
	jobAd->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );

		// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );

	// TSTCLAIR: In promotion to 1st class status we *must* 
	// update the job in the queue
	jobAd->Assign( ATTR_JOB_STATUS, RUNNING );
	shadow->updateJobInQueue(U_STATUS);

	return rval;
}


bool
RemoteResource::recordCheckpointEvent( ClassAd* update_ad )
{
	bool rval = true;
	std::string string_value;
	time_t int_value = 0;
	static float last_recv_bytes = 0.0;

		// First, log this to the UserLog
	CheckpointedEvent event;

	event.run_remote_rusage = getRUsage();

	float recv_bytes = bytesReceived();

	// Received Bytes for checkpoint
	event.sent_bytes = recv_bytes - last_recv_bytes;
	last_recv_bytes = recv_bytes;

	if( !shadow->uLog.writeEventNoFsync(&event, jobAd) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_CHECKPOINTED event\n" );
		rval = false;
	}

	// Now, update our in-memory copy of the job ClassAd
	time_t now = time(nullptr);

	// Increase the total count of checkpoint
	// by default, we round ATTR_NUM_CKPTS, so fetch the raw value
	// here (if available) for us to increment later.
	int ckpt_count = 0;
	if( !jobAd->LookupInteger(ATTR_NUM_CKPTS_RAW, ckpt_count) || !ckpt_count ) {
		jobAd->LookupInteger(ATTR_NUM_CKPTS, ckpt_count );
	}
	ckpt_count++;
	jobAd->Assign(ATTR_NUM_CKPTS, ckpt_count);

	time_t last_ckpt_time = 0;
	jobAd->LookupInteger(ATTR_LAST_CKPT_TIME, last_ckpt_time);

	time_t current_start_time = 0;
	jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, current_start_time);

	int_value = (last_ckpt_time > current_start_time) ? 
						last_ckpt_time : current_start_time;

	// Update Job committed time
	if( int_value > 0 ) {
		int job_committed_time = 0;
		jobAd->LookupInteger(ATTR_JOB_COMMITTED_TIME, job_committed_time);
		job_committed_time += now - int_value;
		jobAd->Assign(ATTR_JOB_COMMITTED_TIME, job_committed_time);

		double slot_weight = 1;
		jobAd->LookupFloat(ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0, slot_weight);
		double slot_time = 0;
		jobAd->LookupFloat(ATTR_COMMITTED_SLOT_TIME, slot_time);
		slot_time += slot_weight * (now - int_value);
		jobAd->Assign(ATTR_COMMITTED_SLOT_TIME, slot_time);
	}

	// Update timestamp of the last checkpoint
	jobAd->Assign(ATTR_LAST_CKPT_TIME, now);

	// Update Ckpt MAC and IP address of VM
	if( update_ad->LookupString(ATTR_VM_CKPT_MAC,string_value) ) {
		jobAd->Assign(ATTR_VM_CKPT_MAC, string_value);
	}
	if( update_ad->LookupString(ATTR_VM_CKPT_IP,string_value) ) {
		jobAd->Assign(ATTR_VM_CKPT_IP, string_value);
	}

	shadow->CommitSuspensionTime(jobAd);

		// Log stuff so we can check our sanity
	printCheckpointStats( D_FULLDEBUG );

	// We update the schedd right now
	shadow->updateJobInQueue(U_CHECKPOINT);

	return rval;
}


bool
RemoteResource::writeULogEvent( ULogEvent* event )
{
	if( !shadow->uLog.writeEvent(event, jobAd) ) {
		return false;
	}
	return true;
}


void
RemoteResource::printSuspendStats( int debug_level )
{
	int total_suspensions = 0;
	int last_suspension_time = 0;
	int cumulative_suspension_time = 0;

	dprintf( debug_level, "Statistics about job suspension:\n" );
	jobAd->LookupInteger( ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	dprintf( debug_level, "%s = %d\n", ATTR_TOTAL_SUSPENSIONS, 
			 total_suspensions );

	jobAd->LookupInteger( ATTR_LAST_SUSPENSION_TIME,
						  last_suspension_time );
	dprintf( debug_level, "%s = %d\n", ATTR_LAST_SUSPENSION_TIME,
			 last_suspension_time );

	jobAd->LookupInteger( ATTR_CUMULATIVE_SUSPENSION_TIME, 
						  cumulative_suspension_time );
	dprintf( debug_level, "%s = %d\n",
			 ATTR_CUMULATIVE_SUSPENSION_TIME,
			 cumulative_suspension_time );
}


void
RemoteResource::printCheckpointStats( int debug_level )
{
	int int_value = 0;
	std::string string_attr;

	dprintf( debug_level, "Statistics about job checkpointing:\n" );

	// total count of the number of checkpoint
	int_value = 0;
	jobAd->LookupInteger( ATTR_NUM_CKPTS, int_value );
	dprintf( debug_level, "%s = %d\n", ATTR_NUM_CKPTS, int_value );

	// Total Job committed time
	int_value = 0;
	jobAd->LookupInteger(ATTR_JOB_COMMITTED_TIME, int_value);
	dprintf( debug_level, "%s = %d\n", ATTR_JOB_COMMITTED_TIME, int_value);

	int committed_suspension_time = 0;
	jobAd->LookupInteger( ATTR_COMMITTED_SUSPENSION_TIME, 
						  committed_suspension_time );
	dprintf( debug_level, "%s = %d\n",
			 ATTR_COMMITTED_SUSPENSION_TIME,
			 committed_suspension_time );

	// timestamp of the last checkpoint
	int_value = 0;
	jobAd->LookupInteger( ATTR_LAST_CKPT_TIME, int_value);
	dprintf( debug_level, "%s = %d\n", ATTR_LAST_CKPT_TIME, int_value);

	// MAC and IP address of VM
	string_attr = "";
	jobAd->LookupString(ATTR_VM_CKPT_MAC, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_VM_CKPT_MAC, string_attr.c_str());

	string_attr = "";
	jobAd->LookupString(ATTR_VM_CKPT_IP, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_VM_CKPT_IP, string_attr.c_str());
}


void
RemoteResource::resourceExit( int reason_for_exit, int exit_status )
{
	dprintf( D_FULLDEBUG, "Inside RemoteResource::resourceExit()\n" );
	setExitReason( reason_for_exit );

	m_got_job_exit = true;

	// Record the activation stop time (HTCONDOR-861) and set the
	// corresponding duration attributes.
	if (began_execution) {
		// but only if we have begun execution, which is where we set activation.StartTime
		activation.TerminationTime = time(nullptr);
		time_t ActivationDuration = activation.TerminationTime - activation.StartTime;
		time_t ActivationTeardownDuration = activation.TerminationTime - activation.ExitExecutionTime;

		// Where would these attributes get rotated?  Here?
		jobAd->InsertAttr( ATTR_JOB_ACTIVATION_DURATION, ActivationDuration );
		jobAd->InsertAttr( ATTR_JOB_ACTIVATION_TEARDOWN_DURATION, ActivationTeardownDuration );
		shadow->updateJobInQueue( U_STATUS );
	}

	// record the start time of transfer output into the job ad.
	time_t tStart = -1, tEnd = -1;
	if (filetrans.GetDownloadTimestamps(&tStart, &tEnd)) {
		jobAd->Assign(ATTR_JOB_CURRENT_START_TRANSFER_OUTPUT_DATE, (int)tStart);
		jobAd->Assign(ATTR_JOB_CURRENT_FINISH_TRANSFER_OUTPUT_DATE, (int)tEnd);
	}
	if (filetrans.GetUploadTimestamps(&tStart, &tEnd)) {
		jobAd->Assign(ATTR_JOB_CURRENT_START_TRANSFER_INPUT_DATE, (int)tStart);
		jobAd->Assign(ATTR_JOB_CURRENT_FINISH_TRANSFER_INPUT_DATE, (int)tEnd);
	}

	if( exit_value == -1 ) {
			/* 
			   Backwards compatibility code...  If we don't have a
			   real value for exit_value yet, it means the starter
			   we're talking to doesn't parse the status integer
			   itself and set the various ClassAd attributes
			   appropriately.  To prevent any trouble in this case, we
			   do the parsing here so everything in the shadow can
			   still work.  Doing this ourselves is potentially
			   inaccurate, since the starter might be a different
			   platform and we might get different results, but it's
			   better than nothing.  However, this is what we've
			   always done in the past, so it's no less accurate than
			   an old shadow talking to the same starter...
			*/
		if( WIFSIGNALED(exit_status) ) {
			exited_by_signal = true;
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, true );

			exit_value = WTERMSIG( exit_status );
			jobAd->Assign( ATTR_ON_EXIT_SIGNAL, exit_value );

		} else {
			exited_by_signal = false;
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );

			exit_value = WEXITSTATUS( exit_status );
			jobAd->Assign( ATTR_ON_EXIT_CODE, exit_value );
		}
	}
}

void
RemoteResource::incrementJobCompletionCount() {
	int numJobCompletions = 0;
	jobAd->LookupInteger( ATTR_NUM_JOB_COMPLETIONS, numJobCompletions );
	++numJobCompletions;
	jobAd->Assign( ATTR_NUM_JOB_COMPLETIONS, numJobCompletions );
}

void 
RemoteResource::setResourceState( ResourceState s )
{
	if ( state != s )
	{
		dprintf( D_FULLDEBUG,
		         "Resource %s changing state from %s to %s\n",
		         machineName ? machineName : "???",
		         rrStateToString(state),
		         rrStateToString(s) );
		state = s;
	}
}


const char*
rrStateToString( ResourceState s )
{
	if( s >= _RR_STATE_THRESHOLD ) {
		return "Unknown State";
	}
	return Resource_State_String[s];
}

void
RemoteResource::startCheckingProxy()
{
	if( proxy_check_tid != -1 ) {
		return; // already running
	}
	if( !proxy_path.empty() ) {
		// This job has a proxy.  We need to check it regularlly to
		// potentially upload a renewed one.
		int PROXY_CHECK_INTERVAL = param_integer("SHADOW_CHECKPROXY_INTERVAL",60*10,1);
		proxy_check_tid = daemonCore->Register_Timer( 0, PROXY_CHECK_INTERVAL,
							(TimerHandlercpp)&RemoteResource::checkX509Proxy,
							"RemoteResource::checkX509Proxy()", this );
	}
}

void
RemoteResource::beginExecution( void )
{
	//
	// Self-checkpointing jobs may begin execution more than once per
	// activation.  Record that information for use by the activation
	// metrics (HTCONDOR-861).
	//
	time_t now = time(NULL);
	activation.StartExecutionTime = now;

	if( began_execution ) {
		return;
	}

	began_execution = true;
	setResourceState( RR_EXECUTING );

	// add the execution start time into the job ad.
	jobAd->Assign( ATTR_JOB_CURRENT_START_EXECUTING_DATE, now);

	// add ActivationSetupDuration into the job ad.
	// note: we do this just once after the the first exectution, we do not
	// want to update after every exectuion (if the job checkpoints).
	time_t ActivationSetUpDuration = activation.StartExecutionTime - activation.StartTime;
    // Where would this attribute get rotated?  Here?
	jobAd->InsertAttr( ATTR_JOB_ACTIVATION_SETUP_DURATION, ActivationSetUpDuration );
	shadow->updateJobInQueue(U_STATUS);

	startCheckingProxy();

	// Let our shadow know so it can make global decisions (for
	// example, should it log a JOB_EXECUTE event)
	shadow->resourceBeganExecution( this );
}

void
RemoteResource::hadContact( void )
{
	last_job_lease_renewal = time(0);
	jobAd->Assign( ATTR_LAST_JOB_LEASE_RENEWAL, last_job_lease_renewal );
}


bool
RemoteResource::supportsReconnect( void )
{
		// even if the starter we're talking to supports reconnect, we
		// only want to return true if the job we're running supports
		// it too (i.e. has a GlobalJobId and a JobLeaseDuration).
	const char* gjid = shadow->getGlobalJobId();
	if( ! gjid ) {
		return false;
	}
	int tmp;
	if( ! jobAd->LookupInteger(ATTR_JOB_LEASE_DURATION, tmp) || tmp <= 0 ) {
		return false;
	}

		// if we got this far, the job supports it, so we can just
		// return whether the remote resource does or not.
	return supports_reconnect;
}


void
RemoteResource::reconnect( void )
{
	const char* gjid = shadow->getGlobalJobId();
	if( ! gjid ) {
		EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
				ATTR_GLOBAL_JOB_ID );
	}
	if( lease_duration < 0 ) { 
			// if it's our first time, figure out what we've got to
			// work with...
		dprintf( D_FULLDEBUG, "Trying to reconnect job %s\n", gjid );
		if( ! jobAd->LookupInteger(ATTR_JOB_LEASE_DURATION,
								   lease_duration) ) {
			EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
					ATTR_JOB_LEASE_DURATION );
		}
		if( ! last_job_lease_renewal ) {
				// if we were spawned in reconnect mode, this should
				// be set.  if we're just trying a reconnect because
				// the syscall socket went away, we'll already have
				// initialized last_job_lease_renewal when we started
				// the job
			EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
					ATTR_LAST_JOB_LEASE_RENEWAL );
		}
		dprintf( D_ALWAYS, "Trying to reconnect to disconnected job\n" );
		dprintf( D_ALWAYS, "%s: %lld %s", ATTR_LAST_JOB_LEASE_RENEWAL,
				 (long long)last_job_lease_renewal,
				 ctime(&last_job_lease_renewal) );
		dprintf( D_ALWAYS, "%s: %d seconds\n",
				 ATTR_JOB_LEASE_DURATION, lease_duration );
	}

		// If we got here, we're trying to reconnect.  keep track of
		// that since we need to know in certain situations...
	if( state != RR_RECONNECT ) {
		setResourceState( RR_RECONNECT );
	}

		// each time we get here, see how much time remains...
	int remaining = remainingLeaseDuration();
	if( !remaining ) {
		dprintf( D_ALWAYS, "%s remaining: EXPIRED!\n",
			 ATTR_JOB_LEASE_DURATION );
		std::string reason;
		formatstr( reason, "Job disconnected too long: %s (%d seconds) expired",
		           ATTR_JOB_LEASE_DURATION, lease_duration );
		shadow->reconnectFailed( reason.c_str() );
	}
	dprintf( D_ALWAYS, "%s remaining: %d\n", ATTR_JOB_LEASE_DURATION,
			 remaining );

	if( next_reconnect_tid >= 0 ) {
		EXCEPT( "in reconnect() and timer for next attempt already set" );
	}

    int delay = shadow->nextReconnectDelay( reconnect_attempts );
	if( delay > remaining ) {
		delay = remaining;
	}
	if( delay ) {
			// only need to dprintf if we're not doing it right away
		dprintf( D_ALWAYS, "Scheduling another attempt to reconnect "
				 "in %d seconds\n", delay );
	}
	next_reconnect_tid = daemonCore->
		Register_Timer( delay,
						(TimerHandlercpp)&RemoteResource::attemptReconnect,
						"RemoteResource::attemptReconnect()", this );

	if( next_reconnect_tid < 0 ) {
		EXCEPT( "Failed to register timer!" );
	}
}


void
RemoteResource::attemptReconnect( int /* timerID */ )
{
		// now that the timer went off, clear out this variable so we
		// don't get confused later.
	next_reconnect_tid = -1;

		// if if this attempt fails, we need to remember we tried
	reconnect_attempts++;

	jobAd->Assign(ATTR_JOB_CURRENT_RECONNECT_ATTEMPT, reconnect_attempts);
	int total_reconnects = 0;
	jobAd->LookupInteger(ATTR_TOTAL_JOB_RECONNECT_ATTEMPTS, total_reconnects);
	total_reconnects++;
	jobAd->Assign(ATTR_TOTAL_JOB_RECONNECT_ATTEMPTS, total_reconnects);
	shadow->updateJobInQueue(U_STATUS);
	
		// ask the startd if the starter is still alive, and if so,
		// where it is located.  note we must ask the startd, even
		// if we think we already know the address of the starter, since
		// the startd would know if the starter process went away or not.
	if( ! locateReconnectStarter() ) {
		return;
	}
	
		// if we got here, we already know the starter's address, so
		// we can go on to directly request a reconnect... 
	requestReconnect(); 
}


int
RemoteResource::remainingLeaseDuration( void )
{
	if (lease_duration < 0) {
			// No lease, nothing remains.
		return 0;
	}
	time_t now = (int)time(0);
	int remaining = lease_duration - (now - last_job_lease_renewal);
	return ((remaining < 0) ? 0 : remaining);
}


bool
RemoteResource::locateReconnectStarter( void )
{
	dprintf( D_ALWAYS, "Attempting to locate disconnected starter\n" );
	const char* gjid = shadow->getGlobalJobId();
	char *claimid = NULL;
	getClaimId(claimid);

	ClaimIdParser idp( claimid );
	dprintf( D_FULLDEBUG, "gjid is %s claimid is %s\n", gjid, idp.publicClaimId());
	ClassAd reply;
	if( dc_startd->locateStarter(gjid, claimid, public_schedd_addr, &reply, 20) ) {
			// it worked, save the results and return success.
		char* tmp = NULL;
		if( reply.LookupString(ATTR_STARTER_IP_ADDR, &tmp) ) {
			setStarterAddress( tmp );
			dprintf( D_ALWAYS, "Found starter: %s\n", tmp );
			free( tmp );
			free( claimid );
			return true;
		} else {
			reconnect();
			free( claimid );
			return false;
		}
	}
	
		// if we made it here figure out what kind of error we got and
		// act accordingly.  in all cases we want to either exit
		// completely or return false so that attemptReconnect() just
		// returns instead of calling requestReconnect().

	dprintf( D_ALWAYS, "locateStarter(): %s\n", dc_startd->error() );

	switch( dc_startd->errorCode() ) {

	case CA_FAILURE:

			// communication successful but GlobalJobId or starter not
			// found.  either way, we know the job is gone, and can
			// safely give up and restart.
		shadow->reconnectFailed( "Job not found at execution machine" );
		break;

	case CA_NOT_AUTHENTICATED:
			// some condor daemon is listening on the port, but it
			// doesn't believe us anymore, so it can't still be our
			// old startd. :( if our job was still there, the old
			// startd would be willing to talk to us.  Just to be
			// safe, try one last time to see if we can kill the old
			// starter.  We don't want the schedd to try this, since
			// it'd block, but we don't have anything better to do,
			// and it helps ensure run-only-once semantics for jobs.
		shadow->cleanUp();
		shadow->reconnectFailed( "Startd is no longer at old port, "
								 "job must have been killed" );
		break;

	case CA_CONNECT_FAILED:
	case CA_COMMUNICATION_ERROR:
			// for both of these, we need to keep trying until the
			// lease_duration expires, since the startd might still be alive
			// and only the network is dead...
		reconnect();
		break;

			// All the errors that can only be programmer mistakes:
			// starter should never return any of these...
	case CA_NOT_AUTHORIZED:
	case CA_INVALID_STATE:
	case CA_INVALID_REQUEST:
	case CA_INVALID_REPLY:
		EXCEPT( "impossible: startd returned %s for locateStarter",
				getCAResultString(dc_startd->errorCode()) );
		break;
	case CA_LOCATE_FAILED:
			// remember, this means we couldn't even find the address
			// of the startd, not the starter.  we already know the
			// startd's addr from the ClaimId...
		EXCEPT( "impossible: startd address already known" );
		break;
	case CA_SUCCESS:
		EXCEPT( "impossible: success already handled" );
		break;
	}
	free( claimid );
	return false;
}

void
RemoteResource::getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status) const
{
	upload_status = m_upload_xfer_status;
	download_status = m_download_xfer_status;
}

int
RemoteResource::transferStatusUpdateCallback(FileTransfer *transobject)
{
	ASSERT(jobAd);

	const FileTransfer::FileTransferInfo& info = transobject->GetInfo();
	dprintf(D_FULLDEBUG,"RemoteResource::transferStatusUpdateCallback(in_progress=%d)\n",info.in_progress);

	if( info.type == FileTransfer::DownloadFilesType ) {
		m_download_xfer_status = info.xfer_status;
		if( ! info.in_progress ) {
			m_download_file_stats = info.stats;
		}
	}
	else {
		m_upload_xfer_status = info.xfer_status;
		if( ! info.in_progress ) {
			m_upload_file_stats = info.stats;
		}
	}
	shadow->updateJobInQueue(U_PERIODIC);
	return 0;
}


void
RemoteResource::initFileTransfer()
{
	bool useNullFileTransfer = param_boolean(
		"SHADOW_INPUT_SANDBOX_USE_NULL_FILE_TRANSFER", false
	);

	if( useNullFileTransfer ) {
		initNullFileTransfer();
	} else {
		initRealFileTransfer();
	}
}

void
RemoteResource::initRealFileTransfer()
{
		// FileTransfer now makes sure we only do Init() once.
		//
		// Tell the FileTransfer object to create a file catalog if
		// the job's files are spooled. This prevents FileTransfer
		// from listing unmodified input files as intermediate files
		// that need to be transferred back from the starter.
	ASSERT(jobAd);
	int spool_time = 0;
	jobAd->LookupInteger(ATTR_STAGE_IN_FINISH,spool_time);
	int r = filetrans.Init( jobAd, false, PRIV_USER, spool_time != 0 );
	if (r == 0) {
		// filetransfer Init failed
		EXCEPT( "RemoteResource::initFileTransfer  Init failed");
	}

	filetrans.RegisterCallback(
		(FileTransferHandlerCpp)&RemoteResource::transferStatusUpdateCallback,
		this,
		true);

	if( !daemonCore->DoFakeCreateThread() ) {
		filetrans.SetServerShouldBlock(false);
	}

	int max_upload_mb = -1;
	int max_download_mb = -1;
	param_integer("MAX_TRANSFER_INPUT_MB",max_upload_mb,true,-1,false,INT_MIN,INT_MAX,jobAd);
	param_integer("MAX_TRANSFER_OUTPUT_MB",max_download_mb,true,-1,false,INT_MIN,INT_MAX,jobAd);

		// The job may override the system defaults for max transfer I/O
	int ad_max_upload_mb = -1;
	int ad_max_download_mb = -1;
	if( jobAd->LookupInteger(ATTR_MAX_TRANSFER_INPUT_MB,ad_max_upload_mb) ) {
		max_upload_mb = ad_max_upload_mb;
	}
	if( jobAd->LookupInteger(ATTR_MAX_TRANSFER_OUTPUT_MB,ad_max_download_mb) ) {
		max_download_mb = ad_max_download_mb;
	}

	filetrans.setMaxUploadBytes(max_upload_mb < 0 ? -1 : ((filesize_t)max_upload_mb)*1024*1024);
	filetrans.setMaxDownloadBytes(max_download_mb < 0 ? -1 : ((filesize_t)max_download_mb)*1024*1024);

	// Add extra remaps for the canonical stdout/err filenames.
	// If using the FileTransfer object, the starter will rename the
	// stdout/err files, and we need to remap them back here.
	std::string file;
	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, file ) &&
		 strcmp( file.c_str(), StdoutRemapName ) ) {

		filetrans.AddDownloadFilenameRemap( StdoutRemapName, file.c_str() );
	}
	if ( jobAd->LookupString( ATTR_JOB_ERROR, file ) &&
		 strcmp( file.c_str(), StderrRemapName ) ) {

		filetrans.AddDownloadFilenameRemap( StderrRemapName, file.c_str() );
	}
	if ( jobAd->LookupString( ATTR_JOB_STARTER_LOG, file ) && 
		 strcmp(file.c_str(), SANDBOX_STARTER_LOG_FILENAME )) {

		filetrans.AddDownloadFilenameRemap(SANDBOX_STARTER_LOG_FILENAME, file.c_str());
	}

	//
	// Check this job's SPOOL directory for MANIFEST files and pick the
	// highest-numbered valid one.  That MANIFEST file must be transferred
	// to the starter (so it can validate the checkpoint) and the files
	// therein converted to URLs and added to the transfer list for the
	// starter to fetch.
	//

	std::string checkpointDestination;
	if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination )) {
		return;
	}

	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath(jobAd, spoolPath);

	// Find the largest manifest number.
	int largestManifestNumber = -1;
	const char * currentFile = NULL;
	Directory spoolDirectory( spoolPath.c_str() );
	while( (currentFile = spoolDirectory.Next()) ) {
		// getNumberFromFileName() ignores FAILURE files, which is
		// exactly the behavior we want here.
		int manifestNumber = manifest::getNumberFromFileName( currentFile );
		if( manifestNumber > largestManifestNumber ) {
			largestManifestNumber = manifestNumber;
		}
	}
	if( largestManifestNumber == -1 ) {
		return;
	}

	// Validate the candidate manifests in reverse ordinal order.
	int manifestNumber = -1;
	std::string manifestFileName;
	if( largestManifestNumber != -1 ) {
		for( int i = largestManifestNumber; i >= 0; --i ) {
			formatstr( manifestFileName, "%s/_condor_checkpoint_MANIFEST.%.4d", spoolPath.c_str(), i );
			if( manifest::validateManifestFile( manifestFileName ) ) {
				manifestNumber = i;
				break;
			} else {
				dprintf( D_VERBOSE, "Manifest file '%s' failed validation.\n", manifestFileName.c_str() );
			}
		}
	}
	if( manifestNumber == -1 ) {
		// This should alarm the administrator, but is not job-fatal.
		dprintf( D_ALWAYS, "No manifest file validated.\n" );
		return;
	}

	std::set< std::string > pathsAlreadyPreserved;

	// Transfer the MANIFEST file.  We can't use AddDownloadFilenameRemap()
	// because those are applied on the starter side and aren't transferred
	// from the shadow (except as part of the job ad, which we've already
	// sent).
	filetrans.addInputFile( manifestFileName, ".MANIFEST", pathsAlreadyPreserved );

	//
	// Transfer every file listed in the MANIFEST file.  It could be quite
	// large, so process it line-by-line, ignoring the last one (which is
	// always itself).
	//

	std::ifstream ifs( manifestFileName.c_str() );
	if(! ifs.good()) {
		dprintf( D_ALWAYS, "Failed to open MANIFEST file (%s), aborting.\n", manifestFileName.c_str() );
		return; // FIMXE
	}

	std::string globalJobID;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, globalJobID );
	ASSERT(! globalJobID.empty());
	std::replace( globalJobID.begin(), globalJobID.end(), '#', '_' );

	std::string manifestLine;
	std::string nextManifestLine;
	std::getline( ifs, manifestLine );
	std::getline( ifs, nextManifestLine );
	for( ; ifs.good(); ) {
		std::string checkpointURL;
		std::string checkpointFile = manifest::FileFromLine( manifestLine );
		formatstr( checkpointURL, "%s/%s/%.4d/%s", checkpointDestination.c_str(),
		  globalJobID.c_str(), manifestNumber, checkpointFile.c_str() );
		filetrans.addCheckpointFile( checkpointURL, checkpointFile, pathsAlreadyPreserved );

		manifestLine = nextManifestLine;
		std::getline( ifs, nextManifestLine );
	}

}

void
RemoteResource::initNullFileTransfer()
{
	//
	// The shadow waits for a command from the starter (before it
	// then makes all the decisions about what to transfer).
	//
	daemonCore->Register_Command(
		FILETRANS_UPLOAD, "FILETRANS_UPLOAD",
		(CommandHandlercpp) & RemoteResource::handleNullFileTransfer,
		"RemoteResource::handleNullFileTransfer",
		this, WRITE
	);

	//
	// This is now a security appendix, but generate the transfer key.
	//
	std::string transferKey;
	NullFileTransfer::generateTransferKey( transferKey );
	jobAd->Assign( ATTR_TRANSFER_KEY, transferKey );

	//
	// The starter will learn the socket's address, and the transfer key,
	// from the job ad.
	//
	jobAd->Assign(ATTR_TRANSFER_SOCKET, global_dc_sinful());
}


void
RemoteResource::requestReconnect( void )
{
	DCStarter starter( starterAddress );

	dprintf( D_ALWAYS, "Attempting to reconnect to starter %s\n", 
			 starterAddress );
		// We want this on the heap, since if this works, we're going
		// to hold onto it and don't want it on the stack...
	ReliSock* rsock = new ReliSock;
	ClassAd req;
	ClassAd reply;
	std::string msg;

		// First, fill up the request with all the data we need
	shadow->publishShadowAttrs( &req );

		// And also put in the request the ATTR_TRANS_SOCK and
		// the ATTR_TRANS_KEY --- the starter will need these two
		// attribute value in order to re-establish the FileTransfer
		// objects.  To get the values for these, just instantiate a 
		// FileTransfer server object right here.  No worries if
		// one already exists, the FileTransfer object will just
		// quickly and quietly return success in that case.
		//
		// Tell the FileTransfer object to create a file catalog if
		// the job's files are spooled. This prevents FileTransfer
		// from listing unmodified input files as intermediate files
		// that need to be transferred back from the starter.
	ASSERT(jobAd);

	initFileTransfer();

	char* value = NULL;
	jobAd->LookupString(ATTR_TRANSFER_KEY,&value);
	if (value) {
		req.Assign(ATTR_TRANSFER_KEY, value);
		free(value);
		value = NULL;
	} else {
		dprintf( D_ALWAYS,"requestReconnect(): failed to determine %s\n",
			ATTR_TRANSFER_KEY );
	}
	jobAd->LookupString(ATTR_TRANSFER_SOCKET,&value);
	if (value) {
		req.Assign(ATTR_TRANSFER_SOCKET, value);
		free(value);
		value = NULL;
	} else {
		dprintf( D_ALWAYS,"requestReconnect(): failed to determine %s\n",
			ATTR_TRANSFER_SOCKET );
	}

		// Use 30s timeout, because we don't want to block forever
		// trying to connect and reestablish contact.  We'll retry if
		// we have to.

		// try the command itself...
	if( ! starter.reconnect(&req, &reply, rsock, 30, m_claim_session.secSessionId()) ) {
		dprintf( D_ALWAYS, "Attempt to reconnect failed: %s\n", 
				 starter.error() );
		delete rsock;
		switch( starter.errorCode() ) {
		case CA_CONNECT_FAILED:
		case CA_COMMUNICATION_ERROR:
			// for both of these, we need to keep trying until the
			// lease_duration expires, since the starter might still be alive
			// and only the network is dead...
			reconnect();
			return;  // reconnect will return right away, and we want
					 // to hand control back to DaemonCore ASAP
			break;

		case CA_NOT_AUTHORIZED:
		case CA_NOT_AUTHENTICATED:
				/*
				  Somehow we authenticated improperly and the starter
				  doesn't think we own our job anymore. :( Trying
				  again won't help us at this point...  Normally this
				  would never happen.  However, if it does, all we can
				  do is try to kill the job (maybe the startd will
				  trust us *grin*), and return failure.
				*/
			shadow->cleanUp();
			msg = "Starter refused request (";
			msg += starter.error();
			msg += ')';
			shadow->reconnectFailed( msg.c_str() );
			break;

				// All the errors that can only be programmer
				// mistakes: the starter should never return them...  
		default:
		case CA_FAILURE:
		case CA_INVALID_STATE:
		case CA_INVALID_REQUEST:
		case CA_INVALID_REPLY:
			EXCEPT( "impossible: starter returned %s for %s",
					getCAResultString(dc_startd->errorCode()),
					getCommandString(CA_RECONNECT_JOB) );
			break;
		case CA_LOCATE_FAILED:
				// we couldn't even find the address of the starter, but
				// we already know it or we wouldn't be trying this
				// method...
			EXCEPT( "impossible: starter address already known" );
			break;
		case CA_SUCCESS:
			EXCEPT( "impossible: success already handled" );
			break;
		}
	}

	dprintf( D_ALWAYS, "Reconnect SUCCESS: connection re-established\n" );

	dprintf( D_FULLDEBUG, "Registering socket for future syscalls\n" );
	if( claim_sock ) {
		dprintf( D_FULLDEBUG, "About to cancel old claim_sock\n" );
		daemonCore->Cancel_Socket( claim_sock );
		delete claim_sock;
	}
	claim_sock = rsock;
	claim_sock->timeout( 300 );
	daemonCore->Register_Socket( claim_sock, "RSC Socket", 
				   (SocketHandlercpp)&RemoteResource::handleSysCalls, 
				   "HandleSyscalls", this );

		// Read all the info out of the reply ad and stash it in our
		// private data members, just like we do when we get the
		// pseudo syscall on job startup for the starter to register
		// this stuff about itself. 
	setStarterInfo( &reply );

	int proxy_expiration = 0;
	if( jobAd->LookupInteger(ATTR_DELEGATED_PROXY_EXPIRATION,proxy_expiration) ) {
		setRemoteProxyRenewTime(proxy_expiration);
	}

	// If the shadow started in reconnect mode, check the job ad to see
	// if we previously heard about the job starting execution, and set
	// up our state accordingly.
	if ( shadow->attemptingReconnectAtStartup ) {
		time_t job_execute_date = 0;
		time_t claim_start_date = 0;
		jobAd->LookupInteger(ATTR_JOB_CURRENT_START_EXECUTING_DATE, job_execute_date);
		jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, claim_start_date);
		if ( job_execute_date >= claim_start_date ) {
			began_execution = true;
			setResourceState( RR_EXECUTING );
			startCheckingProxy();
		} else {
			setResourceState( RR_STARTUP );
		}
	}

	jobAd->AssignExpr(ATTR_JOB_CURRENT_RECONNECT_ATTEMPT, "undefined");
	shadow->updateJobInQueue(U_STATUS);

	reconnect_attempts = 0;
	hadContact();

		// Tell the Shadow object so it can take care of the rest.
	shadow->resourceReconnected( this );

		// that's it, we're done!  we can now return to DaemonCore and
		// wait to service requests on the syscall or command socket
	return;
}

void
RemoteResource::setRemoteProxyRenewTime(time_t expiration_time)
{
	m_remote_proxy_expiration = expiration_time;
	m_remote_proxy_renew_time = GetDelegatedProxyRenewalTime(expiration_time);
	jobAd->Assign(ATTR_DELEGATED_PROXY_EXPIRATION, expiration_time);
}

void
RemoteResource::setRemoteProxyRenewTime()
{
		// When the proxy was (initially) delegated via the file
		// transfer object, we have to do our best here to compute the
		// expiration time of the delegated proxy.  To know what it
		// actually is, we would need to have a better interface for
		// obtaining that information from the file transfer object.

	if( proxy_path.empty() ) {
		return;
	}

	time_t desired_expiration_time = GetDesiredDelegatedJobCredentialExpiration(jobAd);
	time_t proxy_expiration_time = x509_proxy_expiration_time(proxy_path.c_str());
	time_t expiration_time = desired_expiration_time;

	if( proxy_expiration_time == (time_t)-1 ) {
		char const *errmsg = x509_error_string();
		dprintf(D_ALWAYS,"setRemoteProxyRenewTime: failed to get proxy expiration time for %s: %s\n",
				proxy_path.c_str(),
				errmsg ? errmsg : "");
	}
	else {
		if( proxy_expiration_time < desired_expiration_time || desired_expiration_time == 0 ) {
			expiration_time = proxy_expiration_time;
		}
	}

	setRemoteProxyRenewTime(expiration_time);
}

bool
RemoteResource::updateX509Proxy(const char * filename)
{
	ASSERT(filename);
	ASSERT(starterAddress);

	DCStarter starter( starterAddress );

	dprintf( D_FULLDEBUG, "Attempting to connect to starter %s to update X509 proxy\n", 
			 starterAddress );

	DCStarter::X509UpdateStatus ret = DCStarter::XUS_Error;
	if ( param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) == true ) {
		time_t expiration_time = GetDesiredDelegatedJobCredentialExpiration(jobAd);
		time_t result_expiration_time = 0;
		ret = starter.delegateX509Proxy(filename, expiration_time, m_claim_session.secSessionId(),&result_expiration_time);
		if( ret == DCStarter::XUS_Okay ) {
			setRemoteProxyRenewTime(result_expiration_time);
		}
	}
	if ( ret != DCStarter::XUS_Okay ) {
		ret = starter.updateX509Proxy(filename, m_claim_session.secSessionId());
	}
	switch(ret) {
		case DCStarter::XUS_Error:
			dprintf( D_FULLDEBUG, "Failed to send updated X509 proxy to starter.\n");
			return false;
		case DCStarter::XUS_Okay:
			dprintf( D_FULLDEBUG, "Successfully sent updated X509 proxy to starter.\n");
			return true;
		case DCStarter::XUS_Declined:
			dprintf( D_FULLDEBUG, "Starter doesn't want updated X509 proxies.\n");
			daemonCore->Cancel_Timer(proxy_check_tid);
			return true;
	}
	dprintf( D_FULLDEBUG, "Unexpected response %d from starter when "
		"updating X509 proxy.  Treating as failure.\n", ret);
	return false;
}

void 
RemoteResource::checkX509Proxy( int /* timerID */ )
{
	if( state != RR_EXECUTING ) {
		dprintf(D_FULLDEBUG,"checkX509Proxy() doing nothing, because resource is not in EXECUTING state.\n");
		return;
	}
	if(proxy_path.empty()) {
		/* Harmless, but suspicious. */
		return;
	}
	
	StatInfo si(proxy_path.c_str());
	time_t lastmod = si.GetModifyTime();
	dprintf(D_FULLDEBUG, "Proxy timestamps: remote estimated %ld, local %ld (%ld difference)\n",
		(long)last_proxy_timestamp, (long)lastmod,lastmod - last_proxy_timestamp);

	if( m_remote_proxy_renew_time ) {
		dprintf(D_FULLDEBUG, "Remote short-lived proxy remaining lifetime %lds, to be redelegated in %lds\n",
				(long)(m_remote_proxy_expiration-time(NULL)),
				(long)(m_remote_proxy_renew_time-time(NULL)));
	}

	if( m_remote_proxy_renew_time && m_remote_proxy_renew_time <= time(NULL) ) {
		dprintf(D_ALWAYS,"Time to redelegate short-lived proxy to starter.\n");
	}
	else if(lastmod <= last_proxy_timestamp) {
		// No change.
		return;
	}


	// if the proxy has been modified, attempt to reload the expiration time
	// and update it in the jobAd.  we do this on the submit side regardless
	// of whether or not the remote side succesfully receives the proxy, since
	// this allows us to use the attributes in job policy (periodic_hold, etc.)

	time_t proxy_expiration_time = x509_proxy_expiration_time(proxy_path.c_str());
	jobAd->Assign(ATTR_X509_USER_PROXY_EXPIRATION, proxy_expiration_time);
	shadow->updateJobInQueue(U_X509);

	// Proxy file updated.  Time to upload
	last_proxy_timestamp = lastmod;
	updateX509Proxy(proxy_path.c_str());
}

bool
RemoteResource::getSecSessionInfo(
	char const *,
	std::string &reconnect_session_id,
	std::string &reconnect_session_info,
	std::string &reconnect_session_key,
	char const *,
	std::string &filetrans_session_id,
	std::string &filetrans_session_info,
	std::string &filetrans_session_key)
{
	if( !param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) ) {
		dprintf(D_ALWAYS,"Request for security session info from starter, but SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION is not True, so ignoring.\n");
		return false;
	}
	if (m_claim_session.secSessionId()[0] == '\0' || m_filetrans_session.secSessionId()[0] == '\0') {
		dprintf(D_ALWAYS,"Request for security session info from starter, but claim id has no security session, so ignoring.\n");
		return false;
	}

		// For the reconnect session, we have to use something stable that
		// is guaranteed to be the same when the shadow restarts later
		// and tries to reconnect to the starter.  Therefore, we use the
		// main session created from the claim id that is already being
		// used to talk to the startd.

	reconnect_session_id = m_claim_session.secSessionId();
	reconnect_session_info = m_claim_session.secSessionInfo();
	reconnect_session_key = m_claim_session.secSessionKey();

	filetrans_session_id = m_filetrans_session.secSessionId();
	filetrans_session_info = m_filetrans_session.secSessionInfo();
	filetrans_session_key = m_filetrans_session.secSessionKey();

	return true;
}

bool
RemoteResource::allowRemoteReadFileAccess( char const * filename )
{
	bool response = (m_want_chirp || m_want_streaming_io) && allow_shadow_access(filename);
	logRemoteAccessCheck(response,"read access to file",filename);
	return response;
}

bool
RemoteResource::allowRemoteWriteFileAccess( char const * filename )
{
	bool response = (m_want_chirp || m_want_streaming_io) && allow_shadow_access(filename);
	logRemoteAccessCheck(response,"write access to file",filename);
	return response;
}

bool
RemoteResource::allowRemoteReadAttributeAccess( char const * name )
{
	bool response = m_want_chirp || m_want_remote_updates;
	logRemoteAccessCheck(response,"read access to attribute",name);
	return response;
}

bool
RemoteResource::allowRemoteWriteAttributeAccess( const std::string &name )
{
	bool response = m_want_chirp || m_want_remote_updates;
	if (!response && m_want_delayed)
	{
		response = contains_anycase_withwildcard(m_delayed_update_prefix, name);
	}

	// Since this function is called to see if a user job is allowed to update
	// the given attribute (via mechanisms like chirp), make certain we disallow 
	// protected attributes. We do this here because the schedd may allow it to happen 
	// since the shadow will likely be connected as a queue super user with access
	// to modify protected attributes.
	if (response && m_unsettable_attrs.find(name) != m_unsettable_attrs.end()) {
		response = false;
	}

	// Do NOT log failures -- unfortunately, this routine doesn't know about the other
	// whitelisted attributes (for example, ExitCode)
	if (response) logRemoteAccessCheck(response,"write access to attribute",name.c_str());
	return response;
}

void
RemoteResource::logRemoteAccessCheck(bool allow,char const *op,char const *name)
{
	int debug_level = allow ? D_FULLDEBUG : D_ALWAYS;
	dprintf(debug_level,"%s remote request for %s %s\n",
			allow ? "ALLOWING" : "DENYING",
			op,
			name);
}

void
RemoteResource::recordActivationExitExecutionTime(time_t when) {
    activation.ExitExecutionTime = when;
    time_t ActivationExecutionDuration = activation.ExitExecutionTime - activation.StartExecutionTime;

    // Where would this attribute get rotated?  Here?
    jobAd->InsertAttr( ATTR_JOB_ACTIVATION_EXECUTION_DURATION, ActivationExecutionDuration );
    shadow->updateJobInQueue( U_STATUS );
}


int
RemoteResource::handleNullFileTransfer( int command, Stream * s ) {
    dprintf( D_FULLDEBUG, "RemoteResource::handleNullFileTransfer(%d): begins\n", command );

    if( s->type() != Stream::reli_sock ) {
        dprintf( D_ALWAYS, "RemoteResource::handleNullFileTransfer(%d): ignoring non-TCP connection.\n", command );
        return 0;
    }
    ReliSock * sock = static_cast<ReliSock *>(s);


    //
    // Read the transfer key.
    //
    std::string peerTransferKey;
    NullFileTransfer::receiveTransferKey( sock, peerTransferKey );
    dprintf( D_FULLDEBUG, "RemoteResource::handleNullFileTransfer(%d): read transfer key '%s'.\n", command, peerTransferKey.c_str() );

    //
    // Verify the transfer key.
    //
    std::string myTransferKey;
    jobAd->LookupString( ATTR_TRANSFER_KEY, myTransferKey );
    if( myTransferKey != peerTransferKey ) {
        dprintf( D_FULLDEBUG, "RemoteResource::handleNullFileTransfer(%d): invalid transfer key '%s'.\n", command, peerTransferKey.c_str() );

        // The original code says "send back 0 for failure", but what
        // this actually does is send 0 as the final-transfer flag and
        // then cause the starter to fail to read the transfer info ClassAd.
        sock->put(0);
        sock->end_of_message();

        // Prevent brute-force attack.
        sleep(5);

        return 0;
    }

    switch( command ) {
        case FILETRANS_UPLOAD:
            sendFilesToStarter( sock );
            return KEEP_STREAM;
            break;

        default:
            dprintf( D_ALWAYS, "RemoteResource::handleNullFileTransfer(%d): ignoring unknown command.\n", command );
            return 0;
            break;
    }
}

condor::dc::void_coroutine
RemoteResource::sendFilesToStarter( ReliSock * sock ) {
    //
    // Which files are we going to send?  (We can't decide this on the
    // fly unless we're willing to lie to the receiver about  the
    // size of the input sandbox.  I'm not sure how that works with
    // the ability to send URLs, but we can experiment later.)
    //
    int sandbox_size = 0;

    //
    // Send transfer info.
    //
    dprintf( D_FULLDEBUG, "RemoteResource::sendFilesToStarter(): sending transfer info.\n" );
    ClassAd transferInfoAd;
    transferInfoAd.Assign( ATTR_SANDBOX_SIZE, sandbox_size );
    NullFileTransfer::sendTransferInfo( sock,
        0 /* definitely not the final transfer */,
        transferInfoAd
    );
    dprintf( D_FULLDEBUG, "RemoteResource::sendFilesToStarter(): sent transfer info.\n" );

    // The idea of this hack is that the reaper for PID can't ever be
    // called, so co_await() just goes in and out of the event loop
    // via a 0-second timer every time we call it.
    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // Then we send the starter one command at a time until we've
    // transferred everything.
    //
    

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // After sending the last file, send the finish command.
    //
    dprintf( D_FULLDEBUG, "RemoteResource::sendFilesToStarter(): sending finished command.\n" );
    NullFileTransfer::sendFinishedCommand( sock );
    dprintf( D_FULLDEBUG, "RemoteResource::sendFilesToStarter(): sent finished command.\n" );

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // After the finish command, send our final report.
    //
    ClassAd myFinalReport;
    myFinalReport.Assign( ATTR_RESULT, 0 /* success */ );
    NullFileTransfer::sendFinalReport( sock, myFinalReport );

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // After sending our final report, receive our peer's final report.
    //
    ClassAd peerFinalReport;
    NullFileTransfer::receiveFinalReport( sock, peerFinalReport );

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // In the command handler, we returned KEEP_STREAM, so I think
    // nobody else will do this for us.
    //
    delete sock;

    //
    // So that we can spin up the other FTO, if necessary.
    //
    daemonCore->Cancel_Command( FILETRANS_UPLOAD );
}
