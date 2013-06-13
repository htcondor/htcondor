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


/* This file is the implementation of the RemoteResource class. */

#include "condor_common.h"
#include "remoteresource.h"
#include "exit.h"             // for JOB_BLAH_BLAH exit reasons
#include "condor_debug.h"     // for D_debuglevel #defines
#include "condor_string.h"    // for strnewp()
#include "condor_attributes.h"
#include "internet.h"
#include "condor_daemon_core.h"
#include "dc_starter.h"
#include "directory.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
#include "globus_utils.h"

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
	  m_want_volatile(true)
{
	shadow = shad;
	dc_startd = NULL;
	machineName = NULL;
	starterAddress = NULL;
	starterArch = NULL;
	starterOpsys = NULL;
	jobAd = NULL;
	claim_sock = NULL;
	last_job_lease_renewal = 0;
	exit_reason = -1;
	claim_is_closing = false;
	exited_by_signal = false;
	exit_value = -1;
	memset( &remote_rusage, 0, sizeof(struct rusage) );
	disk_usage = 0;
	image_size_kb = 0;
	memory_usage_mb = -1;
	proportional_set_size_kb = -1;
	state = RR_PRE;
	began_execution = false;
	supports_reconnect = false;
	next_reconnect_tid = -1;
	proxy_check_tid = -1;
	last_proxy_timestamp = time(0); // We haven't sent the proxy to the starter yet, so anything before "now" means it hasn't changed.
	m_remote_proxy_expiration = 0;
	m_remote_proxy_renew_time = 0;
	reconnect_attempts = 0;

	lease_duration = -1;
	already_killed_graceful = false;
	already_killed_fast = false;
	m_want_chirp = false;
	m_want_streaming_io = false;
	m_attempt_shutdown_tid = -1;
	m_started_attempting_shutdown = 0;
	m_upload_xfer_status = XFER_STATUS_UNKNOWN;
	m_download_xfer_status = XFER_STATUS_UNKNOWN;

	param(m_remote_update_prefix, "REMOTE_UPDATE_PREFIX", "CHIRP");
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
	if ( machineName   ) delete [] machineName;
	if ( starterAddress) delete [] starterAddress;
	if ( starterArch   ) delete [] starterArch;
	if ( starterOpsys  ) delete [] starterOpsys;
	closeClaimSock();
	if ( jobAd && jobAd != shadow->getJobAd() ) {
		delete jobAd;
	}
	if( proxy_check_tid != -1) {
		daemonCore->Cancel_Timer(proxy_check_tid);
		proxy_check_tid = -1;
	}

	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION",false) ) {
		if( m_claim_session.secSessionId() ) {
			daemonCore->getSecMan()->invalidateKey( m_claim_session.secSessionId() );
		}
		if( m_filetrans_session.secSessionId() ) {
			daemonCore->getSecMan()->invalidateKey( m_filetrans_session.secSessionId() );
		}
	}

	if( m_attempt_shutdown_tid != -1 ) {
		daemonCore->Cancel_Timer(m_attempt_shutdown_tid);
		m_attempt_shutdown_tid = -1;
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
		shadow->dprintf( D_ALWAYS, "Shadow doesn't have startd contact "
						 "information in RemoteResource::activateClaim()\n" ); 
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

	if ( !jobAd ) {
		shadow->dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

	// as part of the initial hack at glexec integration, we need
	// to provide the startd with our user proxy before it
	// executes the starter (since glexec takes the proxy as input
	// in order to determine the UID and GID to use).
	char* proxy;
	if( param_boolean( "GLEXEC_STARTER", false ) &&
	    (jobAd->LookupString( ATTR_X509_USER_PROXY, &proxy ) == 1) ) {
		dprintf( D_FULLDEBUG,
	                 "trying early delegation (for glexec) of proxy: %s\n", proxy );
		time_t expiration_time = GetDesiredDelegatedJobCredentialExpiration(jobAd);
		int dReply = dc_startd->delegateX509Proxy( proxy, expiration_time, NULL );
		if( dReply == OK ) {
			// early delegation was successful. this means the startd
			// is going to launch the starter using glexec, so we need
			// to add the user to our ALLOW_DAEMON list. for now, we'll
			// just pull the user name from the job ad. if we wanted to be
			// airtight here, we'd run the proxy subject through our mapfile,
			// since its possible it could result in a different user

			// TODO: we don't actually need to do the above, since we
			// already open up ALLOW_DAEMON to */<execute_host> (!?!?)
			// in initStartdInfo(). that needs to be fixed, in which case
			// the previous comment will apply and we'll need to open
			// ALLOW_DAEMON here

			dprintf( D_FULLDEBUG,
			         "successfully delegated user proxy to the startd\n" );
		}
		else if( dReply == NOT_OK ) {
			dprintf( D_FULLDEBUG,
			         "proxy delegation waived by startd\n" );
		}
		else {
			// delegation did not work. we log a message and just keep
			// going since it may just be that the startd is old and
			// doesn't know the DELETGATE_GSI_CRED_STARTD command
			dprintf( D_FULLDEBUG,
			         "error delegating credential to startd: %s\n    ",
			         dc_startd->error() );
		}
		free(proxy);
	}

		// we'll eventually return out of this loop...
	while( 1 ) {
		reply = dc_startd->activateClaim( jobAd, starterVersion,
										  &claim_sock );
		switch( reply ) {
		case OK:
			shadow->dprintf( D_ALWAYS, 
							 "Request to run on %s %s was ACCEPTED\n",
							 machineName ? machineName:"", dc_startd->addr() );
				// first, set a timeout on the socket 
			claim_sock->timeout( 300 );
				// Now, register it for remote system calls.
				// It's a bit funky, but works.
			daemonCore->Register_Socket( claim_sock, "RSC Socket", 
				   (SocketHandlercpp)&RemoteResource::handleSysCalls, 
				   "HandleSyscalls", this );
			setResourceState( RR_STARTUP );		
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
			shadow->dprintf( D_ALWAYS, 
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
			shadow->dprintf( D_ALWAYS, "%s: %s\n", machineName ? machineName:"", dc_startd->error() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;

		case NOT_OK:
			shadow->dprintf( D_ALWAYS, 
							 "Request to run on %s %s was REFUSED\n",
							 machineName ? machineName:"", dc_startd->addr() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;
		default:
			shadow->dprintf( D_ALWAYS, "Got unknown reply(%d) from "
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
		shadow->dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
						 "DCStartd object NULL!\n");
		return false;
	}

	if( !graceful ) {
			// stop any lingering file transfers, if any
		abortFileTransfer();
	}

	if( ! dc_startd->deactivateClaim(graceful,&claim_is_closing) ) {
		shadow->dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
						 "Could not send command to startd\n" );
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

	char* addr = dc_startd->addr();
	if( addr ) {
		dprintf( D_FULLDEBUG, "Killed starter (%s) at %s\n", 
				 graceful ? "graceful" : "fast", addr );
	}

	int wantReleaseClaim = 0;
	jobAd->LookupBool(ATTR_RELEASE_CLAIM, wantReleaseClaim);
	if (wantReleaseClaim) {
		ClassAd replyAd;
		dc_startd->releaseClaim(VACATE_FAST, &replyAd);
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
				shadow->dprintf( D_ALWAYS, "RemoteResource::suspend(): dc_startd->suspendClaim FAILED!\n");

		}
		else
			shadow->dprintf( D_ALWAYS, "RemoteResource::suspend(): DCStartd object NULL!\n");

	}
	else
		shadow->dprintf( D_ALWAYS, "RemoteResource::suspend(): Not connected to resource!\n");

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
			shadow->dprintf( D_ALWAYS, "RemoteResource::resume(): dc_startd->resume FAILED!\n");
	}
	else
		shadow->dprintf( D_ALWAYS, "RemoteResource::resume(): Not connected to resource!\n");
		
	return (bRet);
}


void
RemoteResource::dprintfSelf( int debugLevel )
{
	shadow->dprintf ( debugLevel, "RemoteResource::dprintSelf printing "
					  "host info:\n");
	if( dc_startd ) {
		char* addr = dc_startd->addr();
		char* id = dc_startd->getClaimId();
		shadow->dprintf( debugLevel, "\tstartdAddr: %s\n", 
						 addr ? addr : "Unknown" );
		shadow->dprintf( debugLevel, "\tClaimId: %s\n", 
						 id ? id : "Unknown" );
	}
	if( machineName ) {
		shadow->dprintf( debugLevel, "\tmachineName: %s\n",
						 machineName );
	}
	if( starterAddress ) {
		shadow->dprintf( debugLevel, "\tstarterAddr: %s\n", 
						 starterAddress );
	}
	shadow->dprintf( debugLevel, "\texit_reason: %d\n", exit_reason );
	shadow->dprintf( debugLevel, "\texited_by_signal: %s\n", 
					 exited_by_signal ? "True" : "False" );
	if( exited_by_signal ) {
		shadow->dprintf( debugLevel, "\texit_signal: %d\n", 
						 exit_value );
	} else {
		shadow->dprintf( debugLevel, "\texit_code: %d\n", 
						 exit_value );
	}
}

int
RemoteResource::attemptShutdownTimeout()
{
	m_attempt_shutdown_tid = -1;
	attemptShutdown();
	return TRUE;
}

void
RemoteResource::abortFileTransfer()
{
	filetrans.stopServer();
	if( state == RR_PENDING_TRANSFER ) {
		state = RR_FINISHED;
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
	shadow->shutDown( exit_reason );
}

int
RemoteResource::handleSysCalls( Stream * /* sock */ )
{

		// change value of the syscall_sock to correspond with that of
		// this claim sock right before do_REMOTE_syscall().

	syscall_sock = claim_sock;
	thisRemoteResource = this;

	if (do_REMOTE_syscall() < 0) {
		shadow->dprintf(D_SYSCALLS,"Shadow: do_REMOTE_syscall returned < 0\n");
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
		mName = strnewp( machineName );
	} else {
		if ( machineName ) {
			delete [] mName;
			mName = strnewp( machineName );
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
	char* addr = dc_startd->addr();
	if( ! addr ) {
		return;
	}
	if( sinful ) {
		delete [] sinful;
	}
	sinful = strnewp( addr );
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
	char* localName = dc_startd->name();
	if( ! localName ) {
		return;
	}
	if( remoteName ) {
		delete [] remoteName;
	}
	remoteName = strnewp( localName );
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
	char* my_id = dc_startd->getClaimId();
	if( ! my_id ) {
		return;
	}
	if( id ) {
		delete[] id;
	}
	id = strnewp( my_id );
}

void
RemoteResource::getStarterAddress( char *& starterAddr )
{

	if (!starterAddr) {
		starterAddr = strnewp( starterAddress );
	} else {
		if ( starterAddress ) {
			delete[] starterAddr;
			starterAddr = strnewp( starterAddress );
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


int
RemoteResource::getExitReason()
{
	return exit_reason;
}

bool
RemoteResource::claimIsClosing()
{
	return claim_is_closing;
}


int
RemoteResource::exitSignal( void )
{
	if( exited_by_signal ) {
		return exit_value;
	}
	return -1;
}


int
RemoteResource::exitCode( void )
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
	if( param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION",false) ) {
		if( m_claim_session.secSessionId() == NULL ) {
			dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: warning - failed to create security session from claim id %s because claim has no session information, likely because the matched startd has SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION set to False\n",m_claim_session.publicClaimId());
		}
		else {
			bool rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				DAEMON,
				m_claim_session.secSessionId(),
				m_claim_session.secSessionKey(),
				m_claim_session.secSessionInfo(),
				EXECUTE_SIDE_MATCHSESSION_FQU,
				dc_startd->addr(),
				0 /*don't expire*/ );

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

			MyString filetrans_claimid;
				// prepend something to the claim id so that the session id
				// is different for file transfer than for the claim session
			filetrans_claimid.formatstr("filetrans.%s",claim_id);
			m_filetrans_session = ClaimIdParser(filetrans_claimid.Value());

				// Get rid of session parameters set by startd.
				// We will set our own based on the shadow WRITE policy.
			m_filetrans_session.setSecSessionInfo(NULL);

				// Since we just removed the session info, we must
				// set ignore_session_info=true in the following call or
				// we will get NULL for the session id.
			MyString filetrans_session_id =
				m_filetrans_session.secSessionId(/*ignore_session_info=*/ true);

			rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
				WRITE,
				filetrans_session_id.Value(),
				m_filetrans_session.secSessionKey(),
				NULL,
				EXECUTE_SIDE_MATCHSESSION_FQU,
				NULL,
				0 /*don't expire*/ );

			if( !rc ) {
				dprintf(D_ALWAYS,"SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create security session for %s, so will fall back on security negotiation\n",m_filetrans_session.publicClaimId());
			}
			else {
					// fill in session_info so that starter will have
					// enough info to create a security session
					// compatible with the one we just created.
				MyString session_info;
				rc = daemonCore->getSecMan()->ExportSecSessionInfo(
					filetrans_session_id.Value(),
					session_info );

				if( !rc ) {
					dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to get session info for claim id %s\n",m_filetrans_session.publicClaimId());
				}
				else {
					m_filetrans_session.setSecSessionInfo( session_info.Value() );
				}
			}
		}
	}
}


void
RemoteResource::setStarterInfo( ClassAd* ad )
{

	char* tmp = NULL;

	if( ad->LookupString(ATTR_STARTER_IP_ADDR, &tmp) ) {
		setStarterAddress( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_STARTER_IP_ADDR, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_UID_DOMAIN, &tmp) ) {
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_UID_DOMAIN, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &tmp) ) {
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_FILE_SYSTEM_DOMAIN,
				 tmp );  
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_NAME, &tmp) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				delete [] machineName;
				machineName = strnewp( tmp );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, tmp );
		free( tmp );
		tmp = NULL;
	} else if( ad->LookupString(ATTR_MACHINE, &tmp) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				delete [] machineName;
				machineName = strnewp( tmp );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_ARCH, &tmp) ) {
		if( starterArch ) {
			delete [] starterArch;
		}	
		starterArch = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_ARCH, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_OPSYS, &tmp) ) {
		if( starterOpsys ) {
			delete [] starterOpsys;
		}	
		starterOpsys = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_OPSYS, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	char* starter_version;
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

	int tmp_int;
	if( ad->LookupBool(ATTR_HAS_RECONNECT, tmp_int) ) {
			// Whatever the starter defines in its own classad
			// overrides whatever we might think...
		supports_reconnect = tmp_int;
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_HAS_RECONNECT, 
				 supports_reconnect ? "TRUE" : "FALSE" );
	} else {
		dprintf( D_SYSCALLS, "  %s = FALSE (not specified)\n",
				 ATTR_HAS_RECONNECT );
	}
}


void
RemoteResource::setMachineName( const char * mName )
{

	if ( machineName )
		delete [] machineName;
	
	machineName = strnewp ( mName );
}

void
RemoteResource::setStarterAddress( const char * starterAddr )
{
	if( starterAddress ) {
		delete [] starterAddress;
	}	
	starterAddress = strnewp( starterAddr );
}


void
RemoteResource::setExitReason( int reason )
{
	// Set the exit_reason, but not if the reason is JOB_KILLED.
	// This prevents exit_reason being reset from JOB_KILLED to
	// JOB_NOT_CKPTED or some such when the starter gets killed
	// and the syscall sock goes away.

	if( exit_reason != JOB_KILLED && -1 == exit_reason) {
		shadow->dprintf( D_FULLDEBUG, "setting exit reason on %s to %d\n", 
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
RemoteResource::bytesSent()
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
RemoteResource::bytesReceived()
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

	int int_value;
	int64_t int64_value;
	float float_value;

	if( jA->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, float_value) ) {
		remote_rusage.ru_stime.tv_sec = (int) float_value; 
	}
			
	if( jA->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, float_value) ) {
		remote_rusage.ru_utime.tv_sec = (int) float_value; 
	}

	if( jA->LookupInteger(ATTR_IMAGE_SIZE, int64_value) ) {
		image_size_kb = int64_value;
	}

	if( jA->LookupInteger(ATTR_MEMORY_USAGE, int64_value) ) {
		memory_usage_mb = int64_value;
	}

	if( jA->LookupInteger(ATTR_RESIDENT_SET_SIZE, int_value) ) {
		remote_rusage.ru_maxrss = int_value;
	}

	if( jA->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE, int64_value) ) {
		proportional_set_size_kb = int64_value;
	}

			
	if( jA->LookupInteger(ATTR_DISK_USAGE, int_value) ) {
		disk_usage = int_value;
	}

	if( jA->LookupInteger(ATTR_LAST_JOB_LEASE_RENEWAL, int_value) ) {
		last_job_lease_renewal = (time_t)int_value;
	}

	jA->LookupBool( ATTR_WANT_IO_PROXY, m_want_chirp );
	jA->LookupBool( ATTR_WANT_REMOTE_UPDATES, m_want_remote_updates );
	jA->LookupBool( ATTR_WANT_VOLATILE_UPDATES, m_want_volatile );

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
RemoteResource::updateFromStarter( ClassAd* update_ad )
{
	int int_value;
	int64_t int64_value;
	float float_value;
	MyString string_value;

	dprintf( D_FULLDEBUG, "Inside RemoteResource::updateFromStarter()\n" );
	hadContact();

	if( IsDebugLevel(D_MACHINE) ) {
		dprintf( D_MACHINE, "Update ad:\n" );
		dPrintAd( D_MACHINE, *update_ad );
		dprintf( D_MACHINE, "--- End of ClassAd ---\n" );
	}

	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, float_value) ) {
		remote_rusage.ru_stime.tv_sec = (int) float_value; 
		jobAd->Assign(ATTR_JOB_REMOTE_SYS_CPU, float_value);
	}
			
	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, float_value) ) {
		remote_rusage.ru_utime.tv_sec = (int) float_value; 
		jobAd->Assign(ATTR_JOB_REMOTE_USER_CPU, float_value);
	}

	if( update_ad->LookupInteger(ATTR_IMAGE_SIZE, int64_value) ) {
		if( int64_value > image_size_kb ) {
			image_size_kb = int64_value;
			jobAd->Assign(ATTR_IMAGE_SIZE, image_size_kb);
		}
	}

	classad::ExprTree * tree = update_ad->Lookup(ATTR_MEMORY_USAGE);
	if( tree ) {
		tree = tree->Copy();
		jobAd->Insert(ATTR_MEMORY_USAGE, tree, false);
	}

	if( update_ad->LookupFloat(ATTR_JOB_VM_CPU_UTILIZATION, float_value) ) { 
		  jobAd->Assign(ATTR_JOB_VM_CPU_UTILIZATION, float_value);
	}
	
	if( update_ad->LookupInteger(ATTR_RESIDENT_SET_SIZE, int_value) ) {
		int rss = remote_rusage.ru_maxrss;
		if( !jobAd->LookupInteger(ATTR_RESIDENT_SET_SIZE,rss) || rss < int_value ) {
			remote_rusage.ru_maxrss = int_value;
			jobAd->Assign(ATTR_RESIDENT_SET_SIZE, int_value);
		}
	}

	if( update_ad->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE, int64_value) ) {
		int64_t pss = proportional_set_size_kb;
		if( !jobAd->LookupInteger(ATTR_PROPORTIONAL_SET_SIZE,pss) || pss < int64_value ) {
			proportional_set_size_kb = int64_value;
			jobAd->Assign(ATTR_PROPORTIONAL_SET_SIZE, int64_value);
		}
	}

	if( update_ad->LookupInteger(ATTR_BLOCK_READ_KBYTES, int_value) ) {
		jobAd->Assign(ATTR_BLOCK_READ_KBYTES, int_value);
	}

	if( update_ad->LookupInteger(ATTR_BLOCK_WRITE_KBYTES, int_value) ) {
		jobAd->Assign(ATTR_BLOCK_WRITE_KBYTES, int_value);
	}

	if( update_ad->LookupInteger(ATTR_DISK_USAGE, int_value) ) {
		if( int_value > disk_usage ) {
			disk_usage = int_value;
			jobAd->Assign(ATTR_DISK_USAGE, int_value);
		}
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_HIERARCHY,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_HIERARCHY, string_value.Value());
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_NAME,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_NAME, string_value.Value());
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_TYPE,string_value) ) {
		jobAd->Assign(ATTR_EXCEPTION_TYPE, string_value.Value());
	}

	if( update_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
		exited_by_signal = (bool)int_value;
		jobAd->Assign(ATTR_ON_EXIT_BY_SIGNAL, (bool)exited_by_signal);
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
		jobAd->Assign(ATTR_ON_EXIT_SIGNAL, int_value);
		exit_value = int_value;
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
		jobAd->Assign(ATTR_ON_EXIT_CODE, int_value);
		exit_value = int_value;
	}

	if( update_ad->LookupString(ATTR_EXIT_REASON,string_value) ) {
		jobAd->Assign(ATTR_EXIT_REASON, string_value.Value());
	}

	if( update_ad->LookupBool(ATTR_JOB_CORE_DUMPED, int_value) ) {
		jobAd->Assign(ATTR_JOB_CORE_DUMPED, (bool)int_value);
	}

		// The starter sends this attribute whether or not we are spooling
		// output (because it doesn't know if we are).  Technically, we
		// only need to write this attribute into the job ClassAd if we
		// are spooling output.  However, it doesn't hurt to have it there
		// otherwise.
	if( update_ad->LookupString(ATTR_SPOOLED_OUTPUT_FILES,string_value) ) {
		jobAd->Assign(ATTR_SPOOLED_OUTPUT_FILES,string_value.Value());
	}
	else if( jobAd->LookupString(ATTR_SPOOLED_OUTPUT_FILES,string_value) ) {
		jobAd->AssignExpr(ATTR_SPOOLED_OUTPUT_FILES,"UNDEFINED");
	}

		// Process all chrip-based updates from the starter.
	for (classad::ClassAd::const_iterator it = update_ad->begin(); it != update_ad->end(); it++) {
		if (allowRemoteWriteAttributeAccess(it->first))
		{
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

	if (jobAd->EvalInteger(ATTR_MEMORY_USAGE, NULL, int64_value)) {
		memory_usage_mb = int64_value;
	}

	MyString starter_addr;
	update_ad->LookupString( ATTR_STARTER_IP_ADDR, starter_addr );
	if( !starter_addr.IsEmpty() ) {
		// The starter sends updated contact info along with the job
		// update (useful if CCB info changes).  It's a bit of a hack
		// to do it through this channel, but better than nothing.
		setStarterAddress( starter_addr.Value() );
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
	int now = (int)time(NULL);
	int total_suspensions = 0;
	char tmp[256];

	jobAd->LookupInteger( ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	total_suspensions++;
	sprintf( tmp, "%s = %d", ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	jobAd->Insert( tmp );

	sprintf( tmp, "%s = %d", ATTR_LAST_SUSPENSION_TIME, now );
	jobAd->Insert( tmp );

	// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );
	
	// TSTCLAIR: In promotion to 1st class status we *must* 
	// update the job in the queue
	sprintf( tmp, "%s = %d", ATTR_JOB_STATUS , SUSPENDED );
	jobAd->Insert( tmp );
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
	int now = (int)time(NULL);
	int cumulative_suspension_time = 0;
	int last_suspension_time = 0;
	char tmp[256];

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

	sprintf( tmp, "%s = %d", ATTR_CUMULATIVE_SUSPENSION_TIME,
			 cumulative_suspension_time );
	jobAd->Insert( tmp );

		// set the current suspension time to zero, meaning not suspended
	sprintf(tmp, "%s = 0", ATTR_LAST_SUSPENSION_TIME );
	jobAd->Insert( tmp );

		// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );

	// TSTCLAIR: In promotion to 1st class status we *must* 
	// update the job in the queue
	sprintf( tmp, "%s = %d", ATTR_JOB_STATUS , RUNNING );
	jobAd->Insert( tmp );
	shadow->updateJobInQueue(U_STATUS);

	return rval;
}


bool
RemoteResource::recordCheckpointEvent( ClassAd* update_ad )
{
	bool rval = true;
	MyString string_value;
	int int_value = 0;
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
	int now = (int)time(NULL);

	// Increase the total count of checkpoint
	// by default, we round ATTR_NUM_CKPTS, so fetch the raw value
	// here (if available) for us to increment later.
	int ckpt_count = 0;
	if( !jobAd->LookupInteger(ATTR_NUM_CKPTS_RAW, ckpt_count) || !ckpt_count ) {
		jobAd->LookupInteger(ATTR_NUM_CKPTS, ckpt_count );
	}
	ckpt_count++;
	jobAd->Assign(ATTR_NUM_CKPTS, ckpt_count);

	int last_ckpt_time = 0;
	jobAd->LookupInteger(ATTR_LAST_CKPT_TIME, last_ckpt_time);

	int current_start_time = 0;
	jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, current_start_time);

	int_value = (last_ckpt_time > current_start_time) ? 
						last_ckpt_time : current_start_time;

	// Update Job committed time
	if( int_value > 0 ) {
		int job_committed_time = 0;
		jobAd->LookupInteger(ATTR_JOB_COMMITTED_TIME, job_committed_time);
		job_committed_time += now - int_value;
		jobAd->Assign(ATTR_JOB_COMMITTED_TIME, job_committed_time);

		float slot_weight = 1;
		jobAd->LookupFloat(ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0, slot_weight);
		float slot_time = 0;
		jobAd->LookupFloat(ATTR_COMMITTED_SLOT_TIME, slot_time);
		slot_time += slot_weight * (now - int_value);
		jobAd->Assign(ATTR_COMMITTED_SLOT_TIME, slot_time);
	}

	// Update timestamp of the last checkpoint
	jobAd->Assign(ATTR_LAST_CKPT_TIME, now);

	// Update CkptArch and CkptOpSys
	if( starterArch ) {
		jobAd->Assign(ATTR_CKPT_ARCH, starterArch);
	}
	if( starterOpsys ) {
		jobAd->Assign(ATTR_CKPT_OPSYS, starterOpsys);
	}

	// Update Ckpt MAC and IP address of VM
	if( update_ad->LookupString(ATTR_VM_CKPT_MAC,string_value) ) {
		jobAd->Assign(ATTR_VM_CKPT_MAC, string_value.Value());
	}
	if( update_ad->LookupString(ATTR_VM_CKPT_IP,string_value) ) {
		jobAd->Assign(ATTR_VM_CKPT_IP, string_value.Value());
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
	MyString string_attr;

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

	// CkptArch and CkptOpSys
	string_attr = "";
	jobAd->LookupString(ATTR_CKPT_ARCH, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_CKPT_ARCH, string_attr.Value());

	string_attr = "";
	jobAd->LookupString(ATTR_CKPT_OPSYS, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_CKPT_OPSYS, string_attr.Value());

	// MAC and IP address of VM
	string_attr = "";
	jobAd->LookupString(ATTR_VM_CKPT_MAC, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_VM_CKPT_MAC, string_attr.Value());

	string_attr = "";
	jobAd->LookupString(ATTR_VM_CKPT_IP, string_attr);
	dprintf( debug_level, "%s = %s\n", ATTR_VM_CKPT_IP, string_attr.Value());
}


void
RemoteResource::resourceExit( int reason_for_exit, int exit_status )
{
	dprintf( D_FULLDEBUG, "Inside RemoteResource::resourceExit()\n" );
	setExitReason( reason_for_exit );

	// record the start time of transfer output into the job ad.
	time_t tStart = -1;
	if (filetrans.GetDownloadTimestamps(&tStart)) {
		jobAd->Assign(ATTR_JOB_CURRENT_START_TRANSFER_OUTPUT_DATE, (int)tStart);
	}

#if 0 // tj: this seems to record only transfer output time, turn it off for now.
	FileTransfer::FileTransferInfo fi = filetrans.GetInfo();
	if (fi.duration) {
		float cumulativeDuration = 0.0;
		if ( ! jobAd->LookupFloat(ATTR_CUMULATIVE_TRANSFER_TIME, cumulativeDuration)) { 
			cumulativeDuration = 0.0; 
		}
		jobAd->Assign(ATTR_CUMULATIVE_TRANSFER_TIME, cumulativeDuration + fi.duration );
	}
#endif

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
		char tmp[64];
		if( WIFSIGNALED(exit_status) ) {
			exited_by_signal = true;
			sprintf( tmp, "%s=TRUE", ATTR_ON_EXIT_BY_SIGNAL );
			jobAd->Insert( tmp );

			exit_value = WTERMSIG( exit_status );
			sprintf( tmp, "%s=%d", ATTR_ON_EXIT_SIGNAL, exit_value );
			jobAd->Insert( tmp );

		} else {
			exited_by_signal = false;
			sprintf( tmp, "%s=FALSE", ATTR_ON_EXIT_BY_SIGNAL );
			jobAd->Insert( tmp );

			exit_value = WEXITSTATUS( exit_status );
			sprintf( tmp, "%s=%d", ATTR_ON_EXIT_CODE, exit_value );
			jobAd->Insert( tmp );
		}
	}
}


void 
RemoteResource::setResourceState( ResourceState s )
{
	if ( state != s )
	{
		shadow->dprintf( D_FULLDEBUG,
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
	if( !proxy_path.IsEmpty() ) {
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
	if( began_execution ) {
			// Only call this function once per remote resource
		return;
	}

	began_execution = true;
	setResourceState( RR_EXECUTING );

	// add the execution start time into the job ad. 
	int now = (int)time(NULL);
	jobAd->Assign( ATTR_JOB_CURRENT_START_EXECUTING_DATE , now);

	startCheckingProxy();
	
		// Let our shadow know so it can make global decisions (for
		// example, should it log a JOB_EXECUTE event)
	shadow->resourceBeganExecution( this );
}

void
RemoteResource::hadContact( void )
{
		// Length: ATTR_LAST_JOB_LEASE_RENEWAL is 19, '=' is 1,
		// MAX_INT is 10, and another 10 to spare should plenty... 
	char contact_buf[40];
	last_job_lease_renewal = time(0);
	snprintf( contact_buf, 32, "%s=%d", ATTR_LAST_JOB_LEASE_RENEWAL,
			  (int)last_job_lease_renewal );
	jobAd->Insert( contact_buf );
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
	if( ! jobAd->LookupInteger(ATTR_JOB_LEASE_DURATION, tmp) ) {
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
		dprintf( D_ALWAYS, "%s: %d %s", ATTR_LAST_JOB_LEASE_RENEWAL,
				 (int)last_job_lease_renewal, 
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
		MyString reason = "Job disconnected too long: ";
		reason += ATTR_JOB_LEASE_DURATION;
		reason += " (";
		reason += lease_duration;
		reason += " seconds) expired";
		shadow->reconnectFailed( reason.Value() );
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
RemoteResource::attemptReconnect( void )
{
		// now that the timer went off, clear out this variable so we
		// don't get confused later.
	next_reconnect_tid = -1;

		// if if this attempt fails, we need to remember we tried
	reconnect_attempts++;

	
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
	int now = (int)time(0);
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
			delete[] claimid;
			return true;
		} else {
			EXCEPT( "impossible: locateStarter() returned success "
					"but %s not found", ATTR_STARTER_IP_ADDR );
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
	delete[] claimid;
	return false;
}

void
RemoteResource::getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status)
{
	upload_status = m_upload_xfer_status;
	download_status = m_download_xfer_status;
}

int
RemoteResource::transferStatusUpdateCallback(FileTransfer *transobject)
{
	ASSERT(jobAd);

	FileTransfer::FileTransferInfo info = transobject->GetInfo();
	dprintf(D_FULLDEBUG,"RemoteResource::transferStatusUpdateCallback(in_progress=%d)\n",info.in_progress);

	if( info.type == FileTransfer::DownloadFilesType ) {
		m_download_xfer_status = info.xfer_status;
	}
	else {
		m_upload_xfer_status = info.xfer_status;
	}
	shadow->updateJobInQueue(U_PERIODIC);
	return 0;
}

void
RemoteResource::initFileTransfer()
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
	filetrans.Init( jobAd, false, PRIV_USER, spool_time != 0 );

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
	if( jobAd->EvalInteger(ATTR_MAX_TRANSFER_INPUT_MB,NULL,ad_max_upload_mb) ) {
		max_upload_mb = ad_max_upload_mb;
	}
	if( jobAd->EvalInteger(ATTR_MAX_TRANSFER_OUTPUT_MB,NULL,ad_max_download_mb) ) {
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
	MyString msg;

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
		msg.formatstr("%s=\"%s\"",ATTR_TRANSFER_KEY,value);
		req.Insert(msg.Value());
		free(value);
		value = NULL;
	} else {
		dprintf( D_ALWAYS,"requestReconnect(): failed to determine %s\n",
			ATTR_TRANSFER_KEY );
	}
	jobAd->LookupString(ATTR_TRANSFER_SOCKET,&value);
	if (value) {
		msg.formatstr("%s=\"%s\"",ATTR_TRANSFER_SOCKET,value);
		req.Insert(msg.Value());
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
			shadow->reconnectFailed( msg.Value() );
			break;

				// All the errors that can only be programmer
				// mistakes: the starter should never return them...  
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

	began_execution = true;
	setResourceState( RR_EXECUTING );
	reconnect_attempts = 0;
	hadContact();

	int proxy_expiration = 0;
	if( jobAd->LookupInteger(ATTR_DELEGATED_PROXY_EXPIRATION,proxy_expiration) ) {
		setRemoteProxyRenewTime(proxy_expiration);
	}
	startCheckingProxy();

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
	jobAd->Assign(ATTR_DELEGATED_PROXY_EXPIRATION, (int)expiration_time);
}

void
RemoteResource::setRemoteProxyRenewTime()
{
		// When the proxy was (initially) delegated via the file
		// transfer object, we have to do our best here to compute the
		// expiration time of the delegated proxy.  To know what it
		// actually is, we would need to have a better interface for
		// obtaining that information from the file transfer object.

	if( proxy_path.IsEmpty() ) {
		return;
	}

	time_t desired_expiration_time = GetDesiredDelegatedJobCredentialExpiration(jobAd);
	time_t proxy_expiration_time = x509_proxy_expiration_time(proxy_path.Value());
	time_t expiration_time = desired_expiration_time;

	if( proxy_expiration_time == (time_t)-1 ) {
		char const *errmsg = x509_error_string();
		dprintf(D_ALWAYS,"setRemoteProxyRenewTime: failed to get proxy expiration time for %s: %s\n",
				proxy_path.Value(),
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
RemoteResource::checkX509Proxy( void )
{
	if( state != RR_EXECUTING ) {
		dprintf(D_FULLDEBUG,"checkX509Proxy() doing nothing, because resource is not in EXECUTING state.\n");
		return;
	}
	if(proxy_path.IsEmpty()) {
		/* Harmless, but suspicious. */
		return;
	}
	
	StatInfo si(proxy_path.Value());
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


	// if the proxy has been modified, attempt to reload various attributes
	// and update them in the jobAd.  we do this on the submit side regardless
	// of whether or not the remote side succesfully receives the proxy, since
	// this allows us to use the attributes in job policy (periodic_hold, etc.)

	// first, do the DN and expiration time, which all proxies have
	char* proxy_subject = x509_proxy_identity_name(proxy_path.Value());
	time_t proxy_expiration_time = x509_proxy_expiration_time(proxy_path.Value());
	jobAd->Assign(ATTR_X509_USER_PROXY_SUBJECT, proxy_subject);
	jobAd->Assign(ATTR_X509_USER_PROXY_EXPIRATION, proxy_expiration_time);
	if (proxy_subject) {
		free(proxy_subject);
	}

#if defined(HAVE_EXT_VOMS)
	// second, worry about the VOMS attributes, which may or may not be present
	char * voname = NULL;
	char * firstfqan = NULL;
	char * quoted_DN_and_FQAN = NULL;
	int vomserr = extract_VOMS_info_from_file(
			proxy_path.Value(),
			0 /*do not verify*/,
			&voname,
			&firstfqan,
			&quoted_DN_and_FQAN);
	if (vomserr == 0) {
		// VOMS attributes were found
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf(D_SECURITY, "VOMS attributes were found\n");
		}
		jobAd->Assign(ATTR_X509_USER_PROXY_VONAME, voname);
		jobAd->Assign(ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan);
		jobAd->Assign(ATTR_X509_USER_PROXY_FQAN, quoted_DN_and_FQAN);
		free(voname);
		free(firstfqan);
		free(quoted_DN_and_FQAN);
	} else {
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf(D_SECURITY, "VOMS attributes were not found\n");
		}
	}
#endif
	shadow->updateJobInQueue(U_X509);

	// Proxy file updated.  Time to upload
	last_proxy_timestamp = lastmod;
	updateX509Proxy(proxy_path.Value());
}

bool
RemoteResource::getSecSessionInfo(
	char const *,
	MyString &reconnect_session_id,
	MyString &reconnect_session_info,
	MyString &reconnect_session_key,
	char const *,
	MyString &filetrans_session_id,
	MyString &filetrans_session_info,
	MyString &filetrans_session_key)
{
	if( !param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION",false) ) {
		dprintf(D_ALWAYS,"Request for security session info from starter, but SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION is not True, so ignoring.\n");
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
	bool response = m_want_chirp || m_want_streaming_io;
	logRemoteAccessCheck(response,"read access to file",filename);
	return response;
}

bool
RemoteResource::allowRemoteWriteFileAccess( char const * filename )
{
	bool response = m_want_chirp || m_want_streaming_io;
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
	if (!response && m_want_volatile)
	{
                response = strcasecmp(name.substr(0, m_remote_update_prefix.length()).c_str(), m_remote_update_prefix.c_str()) == 0;
	}
	logRemoteAccessCheck(response,"write access to attribute",name.c_str());
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
