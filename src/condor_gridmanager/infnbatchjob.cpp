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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_string.h"
#include "basename.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "nullfile.h"
#include "ipv6_hostname.h"
#include "condor_netaddr.h"
#include "directory.h"
#include "spooled_job_files.h"

#include "gridmanager.h"
#include "infnbatchjob.h"


// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT				2
#define GM_SUBMIT_SAVE			3
#define GM_SUBMITTED			4
#define GM_DONE_SAVE			5
#define GM_DONE_COMMIT			6
#define GM_CANCEL				7
#define GM_DELETE				8
#define GM_CLEAR_REQUEST		9
#define GM_HOLD					10
#define GM_REFRESH_PROXY		11
#define GM_START				12
#define GM_POLL_ACTIVE			13
#define GM_SAVE_SANDBOX_ID		14
#define GM_TRANSFER_INPUT		15
#define GM_TRANSFER_OUTPUT		16
#define GM_DELETE_SANDBOX		17
#define GM_TRANSFER_PROXY		18

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_REFRESH_PROXY",
	"GM_START",
	"GM_POLL_ACTIVE",
	"GM_SAVE_SANDBOX_ID",
	"GM_TRANSFER_INPUT",
	"GM_TRANSFER_OUTPUT",
	"GM_DELETE_SANDBOX",
	"GM_TRANSFER_PROXY",
};

#define JOB_STATE_UNKNOWN				-1
#define JOB_STATE_UNSUBMITTED			0

#define LINK_BUFSIZE 4096

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void INFNBatchJobInit()
{
}

void INFNBatchJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	INFNBatchJob::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	INFNBatchJob::setConnectFailureRetry( tmp_int );

	FileTransfer::SetServerShouldBlock( false );
}

bool INFNBatchJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
		// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
		//   system names should be used (pbs, lsf). Glite are the only
		//   people who care about the old value. This changed happend in
		//   Condor 6.7.12.
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 ( strncasecmp( resource.c_str(), "blah", 4 ) == 0 ||
		   strncasecmp( resource.c_str(), "batch", 5 ) == 0 ||
		   strncasecmp( resource.c_str(), "pbs", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "lsf", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "nqs", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "sge", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "naregi", 6 ) == 0 ) ) {

		return true;
	}
	return false;
}

BaseJob *INFNBatchJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new INFNBatchJob( jobad );
}


int INFNBatchJob::submitInterval = 300;			// default value
int INFNBatchJob::gahpCallTimeout = 300;		// default value
int INFNBatchJob::maxConnectFailures = 3;		// default value

INFNBatchJob::INFNBatchJob( ClassAd *classad )
	: BaseJob( classad )
{
	std::string buff;
	std::string error_string = "";
	char *gahp_path = NULL;
	ArgList gahp_args;
	std::string args_str;

	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	numStatusCheckAttempts = 0;
	remoteSandboxId = NULL;
	remoteJobId = NULL;
	lastPollTime = 0;
	pollNow = false;
	m_remoteIwdSet = false;
	myResource = NULL;
	gahp = NULL;
	m_xfer_gahp = NULL;
	jobProxy = NULL;
	remoteProxyExpireTime = 0;
	m_filetrans = NULL;
	batchType = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	time_t int_value = 0;
	if ( jobAd->LookupInteger( ATTR_DELEGATED_PROXY_EXPIRATION, int_value ) ) {
		remoteProxyExpireTime = int_value;
	}

	if (jobAd->LookupString("REMOTE_Iwd", m_sandboxPath) ||
	    jobAd->LookupString(ATTR_JOB_REMOTE_IWD, m_sandboxPath)) {
		m_remoteIwdSet = true;
	}

	buff = "";
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff != "" ) {
		const char *token;
		Tokenize( buff );

		token = GetNextToken( " ", false );
		if ( token && !strcmp( "batch", token ) ) {
			token = GetNextToken( " ", false );
		}
		if ( token ) {
			batchType = strdup( token );
		}

		while ( (token = GetNextToken( " ", false )) ) {
			gahp_args.AppendArg( token );
		}
	}
	if ( !batchType ) {
		formatstr( error_string, "%s is not set properly in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff = "";
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff != "" ) {
		const char *token;
		Tokenize( buff );

			// TODO Do we want to handle values from older versions
			//   i.e. ones with no sandbox id?

			// This should be 'batch'
		token = GetNextToken( " ", false );
			// This should be the batch system type
		token = GetNextToken( " ", false );
			// This should be the sandbox id
		if ( (token = GetNextToken( " ", false )) ) {
			SetRemoteSandboxId( token );
		}
			// This should be the batch system job id
		if ( (token = GetNextToken( " ", false )) ) {
			SetRemoteJobId( token );
		}
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	strlwr( batchType );

	if ( gahp_args.Count() > 0 ) {
		gahp_path = param( "REMOTE_GAHP" );
		if ( gahp_path == NULL ) {
			formatstr( error_string, "REMOTE_GAHP not defined" );
			goto error_exit;
		}
		gahp_args.GetArgsStringV2Raw( args_str );
	} else {
		// CRUFT: BATCH_GAHP was added in 7.7.6.
		//   Checking <batch-type>_GAHP should be removed at some
		//   point in the future.
		if ( strcasecmp( batchType, "condor" ) ) {
			formatstr( buff, "%s_GAHP", batchType );
			gahp_path = param(buff.c_str());
		}
		if ( gahp_path == NULL ) {
			gahp_path = param( "BATCH_GAHP" );
			if ( gahp_path == NULL ) {
				formatstr( error_string, "Neither %s nor %s defined", buff.c_str(),
						 "BATCH_GAHP" );
				goto error_exit;
			}
		}
	}

	buff = batchType;
	if ( gahp_args.Count() > 0 ) {
		formatstr_cat( buff, "/%s", args_str.c_str() );
		gahp_args.AppendArg( "blahpd" );
	}
	gahp = new GahpClient( buff.c_str(), gahp_path, &gahp_args );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	if ( gahp_args.Count() > 0 ) {
		gahp_path = param( "REMOTE_GAHP" );
		if ( gahp_path == NULL ) {
			formatstr( error_string, "REMOTE_GAHP not defined" );
			goto error_exit;
		}

		formatstr( buff, "xfer/%s/%s", batchType, args_str.c_str() );
		gahp_args.RemoveArg( gahp_args.Count() - 1 );
		gahp_args.AppendArg( "condor_ft-gahp" );
		m_xfer_gahp = new GahpClient( buff.c_str(), gahp_path, &gahp_args );
		free( gahp_path );

		m_xfer_gahp->SetCondorInherit(true);
		m_xfer_gahp->setNotificationTimerId( evaluateStateTid );
		m_xfer_gahp->setMode( GahpClient::normal );
		// TODO: This can't be the normal gahp timeout value. Does it need to
		//   be configurable?
		m_xfer_gahp->setTimeout( 60*60 );
	}

	myResource = INFNBatchResource::FindOrCreateResource( batchType,
		gahp_args.GetArg(0), args_str.c_str() );
	myResource->RegisterJob( this );
	if ( remoteJobId ) {
		myResource->AlreadySubmitted( this );
	}

		// Does this have to be lower-case for SetRemoteJobId()?

	strlwr( batchType );

	jobProxy = AcquireProxy( jobAd, error_string,
							 (CallbackType)&BaseJob::SetEvaluateState, this );
	// If we're removing the job, ignore a bad/missing proxy file.
	if ( jobProxy == NULL && error_string != "" && condorState != REMOVED) {
		goto error_exit;
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

INFNBatchJob::~INFNBatchJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, (CallbackType)&BaseJob::SetEvaluateState, this );
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( batchType != NULL ) {
		free( batchType );
	}
	free( remoteSandboxId );
	if ( remoteJobId != NULL ) {
		free( remoteJobId );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	delete m_xfer_gahp;
	delete m_filetrans;
}

void INFNBatchJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void INFNBatchJob::doEvaluateState( int /* timerID */ )
{
	int old_gm_state;
	int old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, remoteState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],remoteState);

	GahpClient::mode gahp_mode = GahpClient::normal;
	if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
		gahp_mode = GahpClient::results_only;
	}
	if (gahp) {
		gahp->setMode(gahp_mode);
	}
	if (m_xfer_gahp) {
		m_xfer_gahp->setMode(gahp_mode);
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_remote_state = remoteState;

		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).

			// We let the Resource object do the spawning of the gahp
			// servers in the ping code. This allows us to treat an ssh
			// failure for a remote gahp as a failed ping of the resource.
			if (!myResource->didFirstPing()) {
				break;
			}

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			errorString = "";
			if ( condorState == COMPLETED ) {
				gmState = GM_DONE_COMMIT;
			} else if (remoteJobId == nullptr && remoteSandboxId == nullptr) {
				gmState = GM_CLEAR_REQUEST;
			} else if (!gahp->isStarted() || (m_xfer_gahp && !m_xfer_gahp->isStarted())) {
				// Wait to see if remote gahps can be started on retry
				// We do not want to try issuing any gahp commands if the
				// gahp(s) aren't running.
			} else if ( remoteJobId != NULL ) {
				gmState = GM_SUBMITTED;
			} else {
				// Due to the preceding checks, we know that
				// remoteSandboxId is non-null here
				if (condorState == REMOVED) {
					// There may be a remote sandbox to clean up
					gmState = GM_DELETE_SANDBOX;
				} else {
					// We don't know if the input sandbox transfer completed
					// successfully before. Try it now.
					gmState = GM_TRANSFER_INPUT;
				}
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding remote submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else if ( condorState == COMPLETED ) {
				gmState = GM_DELETE;
			} else if (!resourceDown) {
				gmState = GM_SAVE_SANDBOX_ID;
			}
			} break;
		case GM_SAVE_SANDBOX_ID: {
			// Create and save sandbox id for bosco
			if ( remoteSandboxId == NULL ) {
				CreateSandboxId();
			}
			if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
				requestScheddUpdate( this, true );
				break;
			}
			gmState = GM_TRANSFER_INPUT;
		} break;
		case GM_TRANSFER_INPUT: {
			// Transfer input sandbox
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				if ( errorString == "" ) {
					std::string error_string = "Attempts to submit failed: ";
					error_string += gahp->getGahpStderr();
					jobAd->Assign( ATTR_HOLD_REASON, error_string );
				}
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now < lastSubmitAttempt + submitInterval ) {
				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_UNSUBMITTED;
					break;
				}
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
				break;
			}

			// Once RequestSubmit() is called at least once, you must
			// CancelRequest() once you're done with the request call
			if ( myResource->RequestSubmit( this ) == false ) {
				break;
			}

			// Can't break in the middle of a transfer, because we need
			// to clean up any files we've started to move.
			if ( (condorState == REMOVED || condorState == HELD) && gahpAd == NULL ) {
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			// If our blahp is local, we don't have to do any file transfer,
			// so go straight to submitting the job.
			if ( !myResource->GahpIsRemote() ) {
				gmState = GM_SUBMIT;
				break;
			}

			if ( gahpAd == NULL ) {
				gahpAd = buildTransferAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}
			if ( m_filetrans == NULL ) {
				m_filetrans = new FileTransfer();
				// TODO Do we really not want a file catalog?
				if ( m_filetrans->Init( gahpAd, false, PRIV_USER, false ) == 0 ) {
					errorString = "Failed to initialize FileTransfer";
					gmState = GM_HOLD;
					break;
				}

				// Set the Condor version of the FT GAHP. If we don't know
				// its version, then assume it's pre-8.1.0.
				// In 8.1, the file transfer protocol changed and we added
				// a command to exchange Condor version strings with the
				// FT GAHP.
				// Also, never rename the executable during transfer, even
				// to an older FT GAHP.
				const char *ver_str = m_xfer_gahp->getCondorVersion();
				if ( ver_str && ver_str[0] ) {
					m_filetrans->setPeerVersion( ver_str );
					m_filetrans->setRenameExecutable(false);
				} else {
					CondorVersionInfo ver( 8, 0, 0 );
					m_filetrans->setPeerVersion( ver );
				}
			}

			// If available, use SSH tunnel for file transfer connections.
			// Take our sinful string and replace the IP:port with
			// the one that should be used on the remote side for
			// tunneling.
			if ( m_xfer_gahp->getSshForwardPort() ) {
				std::string new_addr;
				// TODO We're ignoring IPv6 for now.
				Sinful our_sinful(daemonCore->InfoCommandSinfulString());
				condor_sockaddr local_addr;
				our_sinful.setHost("127.0.0.1");
				our_sinful.setPort(m_xfer_gahp->getSshForwardPort());
				our_sinful.clearAddrs();
				local_addr.set_ipv4();
				local_addr.set_loopback();
				local_addr.set_port(m_xfer_gahp->getSshForwardPort());
				our_sinful.addAddrToAddrs(local_addr);
				new_addr = our_sinful.getSinful();
				gahpAd->Assign( ATTR_TRANSFER_SOCKET, new_addr );
			}

			std::string sandbox_path;
			rc = m_xfer_gahp->blah_download_sandbox( m_remoteIwdSet ? m_sandboxPath.c_str() : remoteSandboxId, gahpAd,
													 sandbox_path );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			lastSubmitAttempt = time(NULL);
			numSubmitAttempts++;
			if ( rc != 0 ) {
				// Failure!
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_download_sandbox() failed: %s\n",
						 procID.cluster, procID.proc,
						 m_xfer_gahp->getErrorString() );
				errorString = m_xfer_gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
			} else {
				if (!m_remoteIwdSet) {
					m_sandboxPath = sandbox_path;
				}
				gmState = GM_SUBMIT;
			}
		} break;
		case GM_SUBMIT: {
			// Start a new remote submission for this job.

			// Can't break in the middle of a submit, because the job will
			// end up running if the submit succeeds
			if ( (condorState == REMOVED || condorState == HELD) && gahpAd == NULL ) {
				gmState = GM_DELETE_SANDBOX;
				break;
			}
			char *job_id_string = NULL;

			if ( gahpAd == NULL ) {
				gahpAd = buildSubmitAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}

			rc = gahp->blah_job_submit( gahpAd, &job_id_string );
			lastSubmitAttempt = time(NULL);
			if ( !myResource->GahpIsRemote() ) {
				numSubmitAttempts++;
			}
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GAHP_SUCCESS ) {
				SetRemoteJobId( job_id_string );
				if(jobProxy) {
					remoteProxyExpireTime = jobProxy->expiration_time;
					jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
								   remoteProxyExpireTime );
				}
				WriteGridSubmitEventToUserLog( jobAd );
				gmState = GM_SUBMIT_SAVE;
			} else {
				// unhandled error
				// TODO Only request ping if failure doesn't look job-specific
				// TODO Wait and retry command if resource is down and
				//   eventually comes back up
				myResource->RequestPing(nullptr);
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_job_submit() failed: %s\n",
						 procID.cluster, procID.proc,
						 gahp->getErrorString() );
				errorString = gahp->getErrorString();
				myResource->CancelSubmit( this );
				gmState = GM_HOLD;
				reevaluate_state = true;
			}
			if ( job_id_string != NULL ) {
				free( job_id_string );
			}
			} break;
		case GM_SUBMIT_SAVE: {
			// Save the job id for a new remote submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			// The resetting of the evaluateStateTid timer should be the
			// last thing we do, after checking everything of interest.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( remoteState == COMPLETED ) {
				gmState = GM_TRANSFER_OUTPUT;
			} else if ( remoteState == REMOVED ) {
				errorString = "Job removed from batch queue manually";
				SetRemoteJobId( NULL );
				gmState = GM_HOLD;
			} else if ( jobProxy && myResource->GahpCanRefreshProxy() &&
						remoteProxyExpireTime < jobProxy->expiration_time ) {
					gmState = GM_TRANSFER_PROXY;
			} else {
				if ( lastPollTime < enteredCurrentGmState ) {
					lastPollTime = enteredCurrentGmState;
				}
				if ( pollNow ) {
					lastPollTime = 0;
					pollNow = false;
				}
				int poll_interval = myResource->GetJobPollInterval();
				if ( now >= lastPollTime + poll_interval ) {
					gmState = GM_POLL_ACTIVE;
					break;
				}
				unsigned int delay = 0;
				if ( (lastPollTime + poll_interval) > now ) {
					delay = (lastPollTime + poll_interval) - now;
				}
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_POLL_ACTIVE: {
			ClassAd *status_ad = NULL;
			rc = gahp->blah_job_status( remoteJobId, &status_ad );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GAHP_SUCCESS ) {
				// unhandled error
				numStatusCheckAttempts++;
				if (numStatusCheckAttempts < param_integer("BATCH_GAHP_CHECK_STATUS_ATTEMPTS", 5)) {
					// We'll check again soon
					lastPollTime = time(NULL);
					gmState = GM_SUBMITTED;
					break;
				} else {
					// TODO Only request ping if failure doesn't look job-specific
					// TODO Wait and retry command if resource is down and
					//   eventually comes back up
					myResource->RequestPing(nullptr);
					dprintf( D_ALWAYS,
							"(%d.%d) blah_job_status() failed: %s\n",
							procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();
					gmState = GM_HOLD;
					break;
				}
			}
			numStatusCheckAttempts = 0;
			ProcessRemoteAd( status_ad );
			delete status_ad;
			lastPollTime = time(NULL);
			gmState = GM_SUBMITTED;
			} break;
		case GM_TRANSFER_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_SUBMITTED;
			} else if ( !myResource->GahpIsRemote() ) {
				gmState = GM_REFRESH_PROXY;
			} else {
				// We should never end up here if we don't have a
				// proxy already!
				if( ! jobProxy ) {
					EXCEPT( "(%d.%d) Requested to refresh proxy, but no proxy present. ", procID.cluster,procID.proc);
				}

				if ( m_sandboxPath.empty() ) {
					m_xfer_gahp->blah_get_sandbox_path( remoteSandboxId,
														m_sandboxPath );
				}
				if ( m_sandboxPath.empty() ) {
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_get_sandbox_path() failed: %s\n",
							 procID.cluster, procID.proc, m_xfer_gahp->getErrorString() );
					errorString = m_xfer_gahp->getErrorString();
					gmState = GM_HOLD;
					break;
				}
				if ( gahpAd == NULL ) {
					gahpAd = buildTransferAd();
				}
				if ( gahpAd == NULL ) {
					gmState = GM_HOLD;
					break;
				}

				m_xferId = remoteSandboxId;
				gahpAd->Assign( ATTR_TRANSFER_KEY, m_xferId );
				FetchProxyList[m_xferId] = this;

				// If available, use SSH tunnel for file transfer connections.
				// Take our sinful string and replace the IP:port with
				// the one that should be used on the remote side for
				// tunneling.
				if ( m_xfer_gahp->getSshForwardPort() ) {
					std::string new_addr;
					// TODO We're ignoring IPv6 for now.
					Sinful our_sinful(daemonCore->InfoCommandSinfulString());
					condor_sockaddr local_addr;
					our_sinful.setHost("127.0.0.1");
					our_sinful.setPort(m_xfer_gahp->getSshForwardPort());
					our_sinful.clearAddrs();
					local_addr.set_ipv4();
					local_addr.set_loopback();
					local_addr.set_port(m_xfer_gahp->getSshForwardPort());
					our_sinful.addAddrToAddrs(local_addr);
					new_addr = our_sinful.getSinful();
					gahpAd->Assign( ATTR_TRANSFER_SOCKET, new_addr );
				}

				rc = m_xfer_gahp->blah_download_proxy( m_remoteIwdSet ? m_sandboxPath.c_str() : remoteSandboxId, gahpAd );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GAHP_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_download_proxy() failed: %s\n",
							 procID.cluster, procID.proc, m_xfer_gahp->getErrorString() );
					errorString = m_xfer_gahp->getErrorString();
					gmState = GM_HOLD;
					break;
				}
				gmState = GM_REFRESH_PROXY;
			}
		} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_SUBMITTED;
			} else {
				// We should never end up here if we don't have a
				// proxy already!
				if( ! jobProxy ) {
					EXCEPT( "(%d.%d) Requested to refresh proxy, but no proxy present. ", procID.cluster,procID.proc);
				}

				std::string proxy_path = jobProxy->proxy_filename;
				if ( myResource->GahpIsRemote() ) {
					proxy_path = m_sandboxPath;
					proxy_path += DIR_DELIM_CHAR;
					proxy_path += condor_basename( jobProxy->proxy_filename );
				}
				rc = gahp->blah_job_refresh_proxy( remoteJobId,
												   proxy_path.c_str() );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GAHP_SUCCESS ) {
					// unhandled error
					// TODO Only request ping if failure doesn't look job-specific
					// TODO Wait and retry command if resource is down and
					//   eventually comes back up
					myResource->RequestPing(nullptr);
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_job_refresh_proxy() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();
					gmState = GM_HOLD;
					break;
				}
				remoteProxyExpireTime = jobProxy->expiration_time;
				jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
							   remoteProxyExpireTime );
				gmState = GM_SUBMITTED;
			}
		} break;
		case GM_TRANSFER_OUTPUT: {
			if ( !myResource->GahpIsRemote() ) {
				gmState = GM_DONE_SAVE;
				break;
			}
			// Transfer output sandbox
			if ( gahpAd == NULL ) {
				gahpAd = buildTransferAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}

			if ( m_filetrans == NULL ) {
				m_filetrans = new FileTransfer();
				// TODO Do we really not want a file catalog?
				if ( m_filetrans->Init( gahpAd, false, PRIV_USER, false ) == 0 ) {
					errorString = "Failed to initialize FileTransfer";
					gmState = GM_HOLD;
					break;
				}

				// Set the Condor version of the FT GAHP. If we don't know
				// its version, then assume it's pre-8.1.0.
				// In 8.1, the file transfer protocol changed and we added
				// a command to exchange Condor version strings with the
				// FT GAHP.
				const char *ver_str = m_xfer_gahp->getCondorVersion();
				if ( ver_str && ver_str[0] ) {
					m_filetrans->setPeerVersion( ver_str );
				} else {
					CondorVersionInfo ver( 8, 0, 0 );
					m_filetrans->setPeerVersion( ver );
				}

				// Add extra remaps for the canonical stdout/err filenames.
				// If using the FileTransfer object, the starter will rename the
				// stdout/err files, and we need to remap them back here.
				std::string file;
				if ( jobAd->LookupString( ATTR_JOB_OUTPUT, file ) &&
					 strcmp( file.c_str(), StdoutRemapName ) ) {

					m_filetrans->AddDownloadFilenameRemap( StdoutRemapName,
														   file.c_str() );
				}
				if ( jobAd->LookupString( ATTR_JOB_ERROR, file ) &&
					 strcmp( file.c_str(), StderrRemapName ) ) {

					m_filetrans->AddDownloadFilenameRemap( StderrRemapName,
														   file.c_str() );
				}
			}

			// If available, use SSH tunnel for file transfer connections.
			// Take our sinful string and replace the IP:port with
			// the one that should be used on the remote side for
			// tunneling.
			if ( m_xfer_gahp->getSshForwardPort() ) {
				std::string new_addr;
				// TODO We're ignoring IPv6 for now.
				Sinful our_sinful(daemonCore->InfoCommandSinfulString());
				condor_sockaddr local_addr;
				our_sinful.setHost("127.0.0.1");
				our_sinful.setPort(m_xfer_gahp->getSshForwardPort());
				our_sinful.clearAddrs();
				local_addr.set_ipv4();
				local_addr.set_loopback();
				local_addr.set_port(m_xfer_gahp->getSshForwardPort());
				our_sinful.addAddrToAddrs(local_addr);
				new_addr = our_sinful.getSinful();
				gahpAd->Assign( ATTR_TRANSFER_SOCKET, new_addr );
			}

			rc = m_xfer_gahp->blah_upload_sandbox( m_remoteIwdSet ? m_sandboxPath.c_str() : remoteSandboxId, gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != 0 ) {
				// Failure!
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_upload_sandbox() failed: %s\n",
						 procID.cluster, procID.proc,
						 m_xfer_gahp->getErrorString() );
				errorString = m_xfer_gahp->getErrorString();
				gmState = GM_HOLD;
			} else {
				gmState = GM_DONE_SAVE;
			}
		} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				if ( jobAd->IsAttributeDirty( ATTR_JOB_STATUS ) ) {
					requestScheddUpdate( this, true );
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the remote scheduler it can remove the job from the queue.

			// Nothing to tell the blahp
			// TODO Combine this state with GM_DELETE_SANDBOX
			gmState = GM_DELETE_SANDBOX;
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.

			// Should this if-stmt be here? Even if the job is completed,
			// we may still want to remove it (say if we have trouble
			// fetching the output files).
			if ( remoteState != COMPLETED ) {
				rc = gahp->blah_job_cancel( remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GAHP_SUCCESS ) {
					// unhandled error
					// TODO Only request ping if failure doesn't look job-specific
					// TODO Wait and retry command if resource is down and
					//   eventually comes back up
					myResource->RequestPing(nullptr);
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_job_cancel() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				if ( condorState != COMPLETED && condorState != REMOVED ) {
					SetRemoteJobId( NULL );
				}
				myResource->CancelSubmit( this );
			}
			gmState = GM_DELETE_SANDBOX;
			} break;
		case GM_DELETE_SANDBOX: {
			// Clean up any files left behind by the job execution.

			if ( myResource->GahpIsRemote() && !m_remoteIwdSet ) {

				// Delete the remote sandbox
				if ( gahpAd == NULL ) {
					gahpAd = buildTransferAd();
				}
				if ( gahpAd == NULL ) {
					gmState = GM_HOLD;
					break;
				}

				rc = m_xfer_gahp->blah_destroy_sandbox( remoteSandboxId, gahpAd );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != 0 ) {
					// Failure!
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_destroy_sandbox() failed: %s\n",
							 procID.cluster, procID.proc,
							 m_xfer_gahp->getErrorString() );
					errorString = m_xfer_gahp->getErrorString();
					gmState = GM_HOLD;
					break;
				}

			} else {

#if !defined(WIN32)
				// Local blahp
				// Check whether the blahp left behind a job execute directory.
				// The blahp's job wrapper should remove this directory
				// after the job exits, but sometimes the directory gets
				// left behind.
				struct passwd *pw = getpwuid( get_user_uid() );
				if ( pw && pw->pw_dir ) {
					Directory dir( pw->pw_dir, PRIV_USER );
					if ( dir.Find_Named_Entry( BlahpJobDir() ) ) {
						if ( !dir.Remove_Current_File() ) {
							dprintf( D_ALWAYS, "(%d.%d) Failed to remove blahp directory %s\n", procID.cluster, procID.proc, remoteSandboxId );
						}
					}

					// Check whether the blahp left behind a proxy symlink.
					// If a job has a proxy, the blahp creates a limited
					// proxy from it and makes a symlink to the new file
					// under ~/.blah_jobproxy_dir/.
					// It often doesn't remove these files.
					// Remove the symlink, and remove the limited proxy
					// if it's in the spool directory.
					// If the limited proxy isn't in the spool directory,
					// then it might be shared by multiple jobs, so we
					// need to leave it alone.
					if ( jobAd->Lookup( ATTR_X509_USER_PROXY ) && remoteJobId ) {
						TemporaryPrivSentry sentry(PRIV_USER);

						std::string job_id;
						for (auto& token : StringTokenIterator(remoteJobId, "/")) {
							job_id = token;
						}
						ASSERT( !job_id.empty() );
						std::string proxy;
						formatstr( proxy, "%s/.blah_jobproxy_dir/%s.proxy",
								   pw->pw_dir, job_id.c_str() );
						struct stat statbuf;
						int rc = lstat( proxy.c_str(), &statbuf );
						if ( rc < 0 ) {
							proxy += ".norenew";
							rc = lstat( proxy.c_str(), &statbuf );
						}
						if ( rc == 0 ) {
							char target[LINK_BUFSIZE];
							rc = readlink( proxy.c_str(), target, LINK_BUFSIZE );
							if ( rc > 0 && rc < LINK_BUFSIZE ) {
								target[rc] = '\0';

								char *spooldir = param("SPOOL");
								if ( !strncmp( spooldir, target, strlen( spooldir ) ) ) {
									unlink( target );
								}
								free( spooldir );
							}
							unlink( proxy.c_str() );
						}

					}
				}
#endif
			}

#if !defined(WIN32)
			// Check whether the blahp left behind a limited proxy.
			// If a job has a proxy, the blahp creates a limited
			// proxy from it with the same name plus ".lmt".
			// It often doesn't remove this file.
			// If there's a .lmt file and it's in the spool directory,
			// then remove it. If it's somewhere else,
			// then it might be shared by multiple jobs, so we
			// need to leave it alone.
			// The code above may delete this same file through the
			// symlink under ~/.blah_jobproxy_dir/.
			// We need this additional check in case the symlink is
			// deleted but the .lmt file is left behind, and for
			// a remote blahp, when file transfer may send back the
			// .lmt file.
			if ( jobProxy ) {
				std::string limited_proxy;
				formatstr( limited_proxy, "%s.lmt", jobProxy->proxy_filename );
				struct stat statbuf;
				int rc = lstat( limited_proxy.c_str(), &statbuf );
				if ( rc == 0 ) {
					char *spooldir = param("SPOOL");
					if ( !strncmp( spooldir, limited_proxy.c_str(), strlen( spooldir ) ) ) {
						unlink( limited_proxy.c_str() );
					}
					free( spooldir );
				}
			}
#endif

			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				SetRemoteIds( NULL, NULL );
				gmState = GM_CLEAR_REQUEST;
			}
		} break;
		case GM_DELETE: {
			// We are done with the job. Propagate any remaining updates
			// to the schedd, then delete this object.
			DoneWithJob();
			// This object will be deleted when the update occurs
			} break;
		case GM_CLEAR_REQUEST: {
			// Remove all knowledge of any previous or present job
			// submission, in both the gridmanager and the schedd.

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if (  (remoteSandboxId != NULL || remoteJobId != NULL) && condorState != REMOVED ) {
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteIds( NULL, NULL );
			myResource->CancelSubmit( this );
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
			}

			if ( remoteProxyExpireTime != 0 ) {
				remoteProxyExpireTime = 0;
				jobAd->AssignExpr( ATTR_DELEGATED_PROXY_EXPIRATION, "Undefined" );
			}

			// If there are no updates to be done when we first enter this
			// state, requestScheddUpdate will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, requestScheddUpdate won't return done until
			// they've been committed to the schedd.
			if ( jobAd->dirtyBegin() != jobAd->dirtyEnd() ) {
				requestScheddUpdate( this, true );
				break;
			}
			if (!m_remoteIwdSet) {
				m_sandboxPath = "";
			}
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			remoteState = JOB_STATE_UNSUBMITTED;
			SetRemoteJobStatus( NULL );
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?

			// If the condor state is already HELD, then someone already
			// HELD it, so don't update anything else.
			if ( condorState != HELD ) {

				// Set the hold reason as best we can
				// TODO: set the hold reason in a more robust way.
				char holdReason[1024];
				holdReason[0] = '\0';
				holdReason[sizeof(holdReason)-1] = '\0';
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason,
									 sizeof(holdReason) );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.c_str(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || remoteState != old_remote_state ) {
			reevaluate_state = true;
		}
		if ( remoteState != old_remote_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) remote state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					JobStatusNames(old_remote_state),
//					JobStatusNames(remoteState));
			enteredCurrentRemoteState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending gahp call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a gahp func that used gahpAd, we're done
			// with it now, so free it.
			if ( gahpAd ) {
				delete gahpAd;
				gahpAd = NULL;
			}
			if ( m_filetrans ) {
				delete m_filetrans;
				m_filetrans = NULL;
			}
			if ( !m_xferId.empty() ) {
				FetchProxyList.erase( m_xferId );
				m_xferId.clear();
			}
			resourcePingComplete = false;
		}

	} while ( reevaluate_state );
}

void INFNBatchJob::SetRemoteSandboxId( const char *sandbox_id )
{
	SetRemoteIds( sandbox_id, remoteJobId );
}

void INFNBatchJob::SetRemoteJobId( const char *job_id )
{
	SetRemoteIds( remoteSandboxId, job_id );
}

void INFNBatchJob::SetRemoteIds( const char *sandbox_id, const char *job_id )
{
	if ( sandbox_id != remoteSandboxId ) {
		free( remoteSandboxId );
		if ( sandbox_id ) {
			remoteSandboxId = strdup( sandbox_id );
		} else {
			remoteSandboxId = NULL;
		}
	}

	if ( job_id != remoteJobId ) {
		free( remoteJobId );
		if ( job_id ) {
			ASSERT( remoteSandboxId );
			remoteJobId = strdup( job_id );
		} else {
			remoteJobId = NULL;
		}
	}

	std::string full_job_id;
	if ( remoteSandboxId ) {
		formatstr( full_job_id, "batch %s %s", batchType, remoteSandboxId );
	}
	if ( remoteJobId ) {
		formatstr_cat( full_job_id, " %s", remoteJobId );
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

void INFNBatchJob::ProcessRemoteAd( ClassAd *remote_ad )
{
	int new_remote_state;
	ExprTree *new_expr, *old_expr;

	int index;
	const char *attrs_to_copy[] = {
		ATTR_BYTES_SENT,
		ATTR_BYTES_RECVD,
		ATTR_ON_EXIT_BY_SIGNAL,
		ATTR_ON_EXIT_SIGNAL,
		ATTR_ON_EXIT_CODE,
		ATTR_EXIT_REASON,
		ATTR_JOB_REMOTE_SYS_CPU,
		ATTR_JOB_REMOTE_USER_CPU,
		ATTR_NUM_RESTARTS,
		ATTR_JOB_REMOTE_WALL_CLOCK,
		"CpusProvisioned",
		"DiskProvisioned",
		"MemoryProvisioned",
		NULL };		// list must end with a NULL

	if ( remote_ad == NULL ) {
		return;
	}

	dprintf( D_FULLDEBUG, "(%d.%d) ***ProcessRemoteAd\n",
			 procID.cluster, procID.proc );

	remote_ad->LookupInteger( ATTR_JOB_STATUS, new_remote_state );

	if ( new_remote_state == IDLE ) {
		JobIdle();
	}
	if ( new_remote_state == RUNNING ) {
		JobRunning();
	}
	// If the job has been removed locally, don't propagate a hold from
	// the remote schedd. 
	// If HELD is the first job status we get from the remote schedd,
	// assume that it's an old hold that was also reflected in the local
	// schedd and has since been released locally (and should be released
	// remotely as well). This won't always be true, but releasing the
	// remote job anyway shouldn't cause any major trouble.
	if ( new_remote_state == HELD && condorState != REMOVED &&
		 remoteState != JOB_STATE_UNKNOWN ) {
		char *reason = NULL;
		int code = 0;
		int subcode = 0;
		if ( remote_ad->LookupString( ATTR_HOLD_REASON, &reason ) ) {
			remote_ad->LookupInteger( ATTR_HOLD_REASON_CODE, code );
			remote_ad->LookupInteger( ATTR_HOLD_REASON_SUBCODE, subcode );
			JobHeld( reason, code, subcode );
			free( reason );
		} else {
			JobHeld( "held remotely with no hold reason" );
		}
	}
	remoteState = new_remote_state;
	SetRemoteJobStatus( getJobStatusString( remoteState ) );


	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		old_expr = jobAd->LookupExpr( attrs_to_copy[index] );
		new_expr = remote_ad->LookupExpr( attrs_to_copy[index] );

		if ( new_expr != NULL && ( old_expr == NULL || !(*old_expr == *new_expr) ) ) {
			ExprTree * pTree =  new_expr->Copy();
			jobAd->Insert( attrs_to_copy[index], pTree );
		}
	}

	requestScheddUpdate( this, false );

	return;
}

BaseResource *INFNBatchJob::GetResource()
{
	return (BaseResource *)myResource;
}

ClassAd *INFNBatchJob::buildSubmitAd()
{
	std::string expr;
	ClassAd *submit_ad;
	ExprTree *next_expr;
	classad::Value cereq_val;

	int index;
	const char *attrs_to_copy[] = {
		ATTR_JOB_ARGUMENTS1,
		ATTR_JOB_ARGUMENTS2,
		ATTR_JOB_ENV_V1,
		ATTR_JOB_ENV_V1_DELIM,
		ATTR_JOB_ENVIRONMENT,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_TRANSFER_OUTPUT_REMAPS,
		ATTR_JOB_IWD,
		ATTR_GRID_RESOURCE,
		ATTR_REQUEST_MEMORY,
		"NodeNumber",
		"HostNumber",
		"SMPGranularity",
		"WholeNodes",
		"HostSMPSize",
		ATTR_BATCH_EXTRA_SUBMIT_ARGS,
		"StageCmd",
		ATTR_BATCH_PROJECT,
		ATTR_BATCH_RUNTIME,
		"GPUNumber",
		"GPUMode",
		"GPUModel",
		"MICNumber",
		NULL };		// list must end with a NULL

	submit_ad = new ClassAd;

	classad::EvalState state;
	state.SetScopes(jobAd);
	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		classad::Value val;
		classad::Literal *new_literal;
		if ( (next_expr = jobAd->LookupExpr(attrs_to_copy[index])) &&
		     next_expr->Evaluate(state, val) &&
		     (new_literal = classad::Literal::MakeLiteral(val)) )
		{
			submit_ad->Insert(attrs_to_copy[index], new_literal);
		}
	}

	if ( jobAd->EvaluateAttr( ATTR_CE_REQUIREMENTS, cereq_val ) ) {
		classad::ClassAd *cereq_ad;
		std::string cereq_str;

		if ( cereq_val.IsClassAdValue( cereq_ad ) ) {

			// CERequirements is a nested ad. Build the goofy blahp
			// expression from the names and values of the nested ad.
			ExprTree *new_cereq = NULL;
			for (auto & next_attr : *cereq_ad) {
				classad::Value val;
				next_attr.second->Evaluate( val );
				classad::Literal *new_literal = classad::Literal::MakeLiteral( val );
				if ( new_literal == NULL ) {
					continue;
				}
				classad::AttributeReference *new_ref = classad::AttributeReference::MakeAttributeReference( NULL, next_attr.first, false );
				classad::Operation *new_op = classad::Operation::MakeOperation( classad::Operation::EQUAL_OP, new_ref, new_literal );
				if ( new_cereq == NULL ) {
					new_cereq = new_op;
				} else {
					new_cereq = classad::Operation::MakeOperation( classad::Operation::LOGICAL_AND_OP, new_cereq, new_op );
				}
			}
			submit_ad->Insert( ATTR_CE_REQUIREMENTS, new_cereq );

		} else if ( cereq_val.IsStringValue( cereq_str ) ) {

			// CERequirements is string. Split on comma/space and lookup
			// the resulting names in the job ad to build the goofy
			// blahp expression.
			ExprTree *new_cereq = NULL;
			for (const auto& next_attr : StringTokenIterator(cereq_str)) {
				classad::Value val;
				jobAd->EvaluateAttr( next_attr, val );
				classad::Literal *new_literal = classad::Literal::MakeLiteral( val );
				if ( new_literal == NULL ) {
					continue;
				}
				classad::AttributeReference *new_ref = classad::AttributeReference::MakeAttributeReference( NULL, next_attr, false );
				classad::Operation *new_op = classad::Operation::MakeOperation( classad::Operation::EQUAL_OP, new_ref, new_literal );
				if ( new_cereq == NULL ) {
					new_cereq = new_op;
				} else {
					new_cereq = classad::Operation::MakeOperation( classad::Operation::LOGICAL_AND_OP, new_cereq, new_op );
				}
			}
			submit_ad->Insert( ATTR_CE_REQUIREMENTS, new_cereq );

		} else if ( (next_expr = jobAd->LookupExpr( ATTR_CE_REQUIREMENTS )) ) {

			// CERequirements is neither a ClassAd nor a string.
			// Forward the unevaluated expression to the blahp.
			ExprTree * pTree = next_expr->Copy();
			submit_ad->Insert( ATTR_CE_REQUIREMENTS, pTree );

		}
	}

	expr = "";
	jobAd->LookupString( ATTR_JOB_REMOTE_IWD, expr );
	if ( !expr.empty() ) {
		submit_ad->Assign( ATTR_JOB_IWD, expr );
	}

	expr = "";
	jobAd->LookupString( ATTR_BATCH_QUEUE, expr );
	if ( !expr.empty() ) {
		submit_ad->Assign( "Queue", expr );
	}

	int gpus = 0;
	jobAd->LookupInteger(ATTR_REQUEST_GPUS, gpus);
	if ( gpus > 0 ) {
		submit_ad->Assign("GPUNumber", gpus);
	}

	GetJobExecutable( jobAd, expr );
	submit_ad->Assign( ATTR_JOB_CMD, expr );

	// The blahp expects the proxy attribute to contain the full pathname
	// of the proxy file.
	if ( jobAd->LookupString( ATTR_X509_USER_PROXY, expr ) ) {
		if ( expr[0] != '/' ) {
			std::string fullpath;
			submit_ad->LookupString( ATTR_JOB_IWD, fullpath );
			formatstr_cat( fullpath, "/%s", expr.c_str() );
			submit_ad->Assign( ATTR_X509_USER_PROXY, fullpath );
		} else {
			submit_ad->Assign( ATTR_X509_USER_PROXY, expr );
		}
	}

	submit_ad->Assign( "JobDirectory", BlahpJobDir() );

		// CRUFT: In the current glite code, jobs have a grid-type of 'blah'
		//   and the blahp looks at the attribute "gridtype" for 'pbs' or
		//   'lsf'. Until the blahp changes to look at GridResource, set
		//   'gridtype' for non-glite users.
	if ( strcmp( batchType, "blah" ) != 0 ) {
		submit_ad->Assign( "gridtype", batchType );
	}

	bool cleared_environment = false;
	bool cleared_arguments = false;

		// Remove all remote_* attributes from the new ad before
		// translating remote_* attributes from the original ad.
		// See gittrac #376 for why we have two loops here.
	auto itr = submit_ad->begin();
	while (itr != submit_ad->end()) {
		// This convoluted setup is an attempt to avoid invalidating
		// the iterator when deleting the attribute.
		if ( strncasecmp( itr->first.c_str(), "REMOTE_", 7 ) == 0 &&
		     itr->first.size() > 7 ) {

			itr = submit_ad->erase(itr);
		} else {
			itr++;
		}
	}

	for (auto & itr : *jobAd) {
		const char *next_name = itr.first.c_str();
		if ( strncasecmp( next_name, "REMOTE_", 7 ) == 0 &&
			 strlen( next_name ) > 7 ) {

			char const *attr_name = &(next_name[7]);

			if(strcasecmp(attr_name,ATTR_JOB_ENV_V1) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ENV_V1_DELIM) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ENVIRONMENT) == 0)
			{
				//Any remote environment settings indicate that we
				//should clear whatever environment was already copied
				//over from the non-remote settings, so the non-remote
				//settings can never trump the remote settings.
				if(!cleared_environment) {
					cleared_environment = true;
					submit_ad->Delete(ATTR_JOB_ENV_V1);
					submit_ad->Delete(ATTR_JOB_ENV_V1_DELIM);
					submit_ad->Delete(ATTR_JOB_ENVIRONMENT);
				}
			}

			if(strcasecmp(attr_name,ATTR_JOB_ARGUMENTS1) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ARGUMENTS2) == 0)
			{
				//Any remote arguments settings indicate that we
				//should clear whatever arguments was already copied
				//over from the non-remote settings, so the non-remote
				//settings can never trump the remote settings.
				if(!cleared_arguments) {
					cleared_arguments = true;
					submit_ad->Delete(ATTR_JOB_ARGUMENTS1);
					submit_ad->Delete(ATTR_JOB_ARGUMENTS2);
				}
			}

			ExprTree * pTree = itr.second->Copy();
			submit_ad->Insert( attr_name, pTree );
		}
	}

	if ( myResource->GahpIsRemote() ) {
		// Rewrite ad so that everything is relative to m_sandboxPath
		std::string old_value;
		std::string new_value;
		bool xfer_exec = true;
		bool xfer_stdin = true;
		bool xfer_stdout = true;
		bool xfer_stderr = true;

		submit_ad->InsertAttr( ATTR_JOB_IWD, m_sandboxPath );

		jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, xfer_exec );
		if ( xfer_exec ) {
			submit_ad->LookupString( ATTR_JOB_CMD, old_value );
			formatstr( new_value, "%s/%s", m_sandboxPath.c_str(), condor_basename( old_value.c_str() ) );
			//formatstr( new_value, "%s/%s", m_sandboxPath.c_str(), CONDOR_EXEC );
			submit_ad->InsertAttr( ATTR_JOB_CMD, new_value );
		}

		old_value = "";
		submit_ad->LookupString( ATTR_JOB_INPUT, old_value );
		jobAd->LookupBool(ATTR_TRANSFER_INPUT, xfer_stdin);
		if ( xfer_stdin && !old_value.empty() && !nullFile( old_value.c_str() ) ) {
			submit_ad->InsertAttr( ATTR_JOB_INPUT, condor_basename( old_value.c_str() ) );
		}

		old_value = "";
		submit_ad->LookupString( ATTR_JOB_OUTPUT, old_value );
		jobAd->LookupBool(ATTR_TRANSFER_OUTPUT, xfer_stdout);
		if ( xfer_stdout && !old_value.empty() && !nullFile( old_value.c_str() ) ) {
			submit_ad->InsertAttr( ATTR_JOB_OUTPUT, StdoutRemapName );
		}

		new_value = "";
		submit_ad->LookupString( ATTR_JOB_ERROR, new_value );
		jobAd->LookupBool(ATTR_TRANSFER_ERROR, xfer_stderr);
		if ( xfer_stderr && !new_value.empty() && !nullFile( new_value.c_str() ) ) {
			if ( xfer_stdout && old_value == new_value ) {
				submit_ad->InsertAttr( ATTR_JOB_ERROR, StdoutRemapName );
			} else {
				submit_ad->InsertAttr( ATTR_JOB_ERROR, StderrRemapName );
			}
		}

		// Older versions of the blahp will fail if the x509userproxy
		// attribute isn't a full path.
		old_value = "";
		submit_ad->LookupString( ATTR_X509_USER_PROXY, old_value );
		if ( !old_value.empty() ) {
			new_value = m_sandboxPath + DIR_DELIM_CHAR + condor_basename( old_value.c_str() );
			submit_ad->InsertAttr( ATTR_X509_USER_PROXY, new_value.c_str() );
		}

		old_value = "";
		submit_ad->LookupString( ATTR_TRANSFER_INPUT_FILES, old_value );
		if ( !old_value.empty() ) {
			std::vector<std::string> file_list = split(old_value, ",");
			for (auto& file : file_list) {
				// We make a copy of file, since condor_basename() will
				// return a pointer inside the passed string
				std::string tmp_name = file;
				file = condor_basename( tmp_name.c_str() );
			}
			submit_ad->InsertAttr( ATTR_TRANSFER_INPUT_FILES, join(file_list, ",") );
		}

		submit_ad->Delete( ATTR_TRANSFER_OUTPUT_REMAPS );
	}

	return submit_ad;
}

ClassAd *INFNBatchJob::buildTransferAd()
{
	int index;
	const char *attrs_to_copy[] = {
		ATTR_CLUSTER_ID,
		ATTR_PROC_ID,
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_TRANSFER_EXECUTABLE,
		ATTR_TRANSFER_INPUT,
		ATTR_TRANSFER_OUTPUT,
		ATTR_TRANSFER_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_TRANSFER_OUTPUT_REMAPS,
		ATTR_X509_USER_PROXY,
		ATTR_OUTPUT_DESTINATION,
		ATTR_ENCRYPT_INPUT_FILES,
		ATTR_ENCRYPT_OUTPUT_FILES,
		ATTR_DONT_ENCRYPT_INPUT_FILES,
		ATTR_DONT_ENCRYPT_OUTPUT_FILES,
		ATTR_JOB_IWD,
		NULL };		// list must end with a NULL
	// TODO Here are other attributes the FileTransfer object looks at.
	//   They may be important:
	//   ATTR_OWNER
	//   ATTR_NT_DOMAIN
	//   ATTR_ULOG_FILE
	//   ATTR_CLUSTER_ID
	//   ATTR_PROC_ID
	//   ATTR_SPOOLED_OUTPUT_FILES
	//   ATTR_STREAM_OUTPUT
	//   ATTR_STREAM_ERROR
	//   ATTR_ENCRYPT_INPUT_FILES
	//   ATTR_ENCRYPT_OUTPUT_FILES
	//   ATTR_STAGE_IN_FINISH
	//   ATTR_TRANSFER_INTERMEDIATE_FILES
	//   ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME

	ClassAd *xfer_ad = new ClassAd();
	ExprTree *next_expr;

	std::string stdout_path;
	std::string stderr_path;
	bool xfer_stdout = true;
	bool xfer_stderr = true;
	jobAd->LookupBool(ATTR_TRANSFER_OUTPUT, xfer_stdout);
	jobAd->LookupBool(ATTR_TRANSFER_ERROR, xfer_stderr);
	jobAd->LookupString( ATTR_JOB_OUTPUT, stdout_path );
	if ( xfer_stdout && !stdout_path.empty() && !nullFile( stdout_path.c_str() ) ) {
		xfer_ad->InsertAttr( ATTR_JOB_OUTPUT, StdoutRemapName );
	}
	jobAd->LookupString( ATTR_JOB_ERROR, stderr_path );
	if ( xfer_stderr && !stderr_path.empty() && !nullFile( stderr_path.c_str() ) ) {
		if ( xfer_stdout && stdout_path == stderr_path ) {
			xfer_ad->InsertAttr( ATTR_JOB_ERROR, StdoutRemapName );
		} else {
			xfer_ad->InsertAttr( ATTR_JOB_ERROR, StderrRemapName );
		}
	}

	// Initialize ATTR_TRANSFER_OUTPUT_FILES to an empty string.
	// Right now, we don't support transferring all new/modified output
	// files. In that case, we need ATTR_TRANSFER_OUTPUT_FILES to always
	// be set. Otherwise, stdout and stderr aren't transferred.
	xfer_ad->InsertAttr( ATTR_TRANSFER_OUTPUT_FILES, "" );

	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		if ( ( next_expr = jobAd->LookupExpr( attrs_to_copy[index] ) ) != NULL ) {
			ExprTree * pTree = next_expr->Copy();
			xfer_ad->Insert( attrs_to_copy[index], pTree );
		}
	}

	// Don't process "REMOTE_*" attributes like we do for the ad given
	// to the blahp.
	// This ad is used for both local and remote ends of FileTransfer.
	// We don't want Remote_Iwd to apply to the local ad, and the ft-gahp
	// will overwrite Iwd in the remote ad.

	// TODO This may require some additional attributes to be set.

	return xfer_ad;
}

void INFNBatchJob::CreateSandboxId()
{
	// Build a name for the sandbox that will be unique to this job.
	// Our pattern is <collector name>_<GlobalJobId>

	// get condor pool name
	// If there's no collector, insert a dummy name
	char* pool_name = param( "COLLECTOR_HOST" );
	if ( !pool_name ) {
		pool_name = strdup( "NoPool" );
	}

	// The sandbox id becomes a directory on the remote side.
	// Having ':', '?', ' ', or ',' in a path can mess up PBS and
	// other systems, so we need to remove them.
	// If pool_name has a list of collectors, just the first will be used.
	for ( char *ptr = pool_name; *ptr != '\0'; ptr++ ) {
		switch( *ptr ) {
		case ':':
			*ptr = '_';
			break;
		case '?':
		case ',':
		case ' ':
		case '\t':
			*ptr = '\0';
			break;
		}
	}

	// use "ATTR_GLOBAL_JOB_ID" to get unique global job id
	std::string job_id;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );
	size_t pos = 0;
	while ( (pos = job_id.find_first_of("@#", pos)) != std::string::npos ) {
		job_id[pos] = '_';
		pos++;
	}

	std::string unique_id;
	formatstr( unique_id, "%s_%s", pool_name, job_id.c_str() );

	free( pool_name );

	SetRemoteSandboxId( unique_id.c_str() );
}

const char *INFNBatchJob::BlahpJobDir()
{
	static std::string dir;
	ASSERT( remoteSandboxId );
	formatstr( dir, "home_bl_%s", remoteSandboxId );
	return dir.c_str();
}
