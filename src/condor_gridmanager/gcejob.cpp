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
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "nullfile.h"
#include "filename_tools.h"

#ifdef WIN32
#else
#include <uuid/uuid.h>
#endif

#include "gridmanager.h"
#include "gcejob.h"
#include "condor_config.h"

using namespace std;

#define GM_INIT							0
#define GM_START_VM						1
#define GM_SAVE_INSTANCE_ID				2
#define GM_SUBMITTED					3
#define GM_DONE_SAVE					4
#define GM_CANCEL						5
#define GM_DELETE						6
#define GM_CLEAR_REQUEST				7
#define GM_HOLD							8
#define GM_PROBE_JOB					9
#define GM_SAVE_INSTANCE_NAME			10
#define GM_SEEK_INSTANCE_ID				11
#define GM_CANCEL_CHECK					12

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_START_VM",
	"GM_SAVE_INSTANCE_ID",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_CANCEL",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROBE_JOB",
	"GM_SAVE_INSTANCE_NAME",
	"GM_SEEK_INSTANCE_ID",
	"GM_CANCEL_CHECK",
};

#define GCE_INSTANCE_PROVISIONING		"PROVISIONING"
#define GCE_INSTANCE_STAGING			"STAGING"
#define GCE_INSTANCE_RUNNING			"RUNNING"
#define GCE_INSTANCE_STOPPING			"STOPPING"
#define GCE_INSTANCE_STOPPED			"STOPPED"
#define GCE_INSTANCE_TERMINATED			"TERMINATED"

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void GCEJobInit()
{
}


void GCEJobReconfig()
{
	// Tell all the resource objects to deal with their new config values
	GCEResource *next_resource;

	GCEResource::ResourcesByName.startIterations();

	while ( GCEResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}


bool GCEJobAdMatch( const ClassAd *job_ad )
{
	int universe;
	string resource;

	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) &&
		 (strncasecmp( resource.c_str(), "gce", 3 ) == 0) )
	{
		return true;
	}
	return false;
}


BaseJob* GCEJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GCEJob( jobad );
}

int GCEJob::gahpCallTimeout = 600;
int GCEJob::submitInterval = 300;
int GCEJob::maxConnectFailures = 3;
int GCEJob::funcRetryInterval = 15;
int GCEJob::pendingWaitTime = 15;
int GCEJob::maxRetryTimes = 3;

GCEJob::GCEJob( ClassAd *classad ) :
	BaseJob( classad ),
	m_was_job_completion( false ),
	m_retry_times( 0 ),
	probeNow( false )
{
	string error_string = "";
	char *gahp_path = NULL;
	char *gahp_log = NULL;
	int gahp_worker_cnt = 0;
	char *gahp_debug = NULL;
	ArgList args;
	string value;

	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	myResource = NULL;
	gahp = NULL;

	// check the auth_file
	jobAd->LookupString( ATTR_GCE_AUTH_FILE, m_authFile );

	if ( m_authFile.empty() ) {
		error_string = "Auth file not defined";
		goto error_exit;
	}

	// Check for failure injections.
	m_failure_injection = getenv( "GM_FAILURE_INJECTION" );
	if( m_failure_injection == NULL ) { m_failure_injection = ""; }
	dprintf( D_FULLDEBUG, "GM_FAILURE_INJECTION = %s\n", m_failure_injection );

	jobAd->LookupString( ATTR_GCE_IMAGE, m_image );

	// if user assigns both metadata and metadata_file, the two will
	// be concatenated by the gahp
	// TODO Do we need to process this attribute value before passing it
	//   to the gahp?
	jobAd->LookupString( ATTR_GCE_METADATA_FILE, m_metadataFile );

	jobAd->LookupString( ATTR_GCE_METADATA, m_metadata );

	// get VM machine type
	jobAd->LookupString( ATTR_GCE_MACHINE_TYPE, m_machineType );

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD &&
		 jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobAd->LookupString( ATTR_GRID_RESOURCE, value );
	if ( !value.empty() ) {
		const char *token;

		Tokenize( value );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "gce" ) ) {
			formatstr( error_string, "%s not of type gce",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			m_serviceUrl = token;
		} else {
			formatstr( error_string, "%s missing GCE service URL",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			m_project = token;
		} else {
			formatstr( error_string, "%s missing GCE project",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			m_zone = token;
		} else {
			formatstr( error_string, "%s missing GCE zone",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}
	} else {
		formatstr( error_string, "%s is not set in the job ad",
								  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	gahp_path = param( "GCE_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "GCE_GAHP not defined";
		goto error_exit;
	}

	gahp_log = param( "GCE_GAHP_LOG" );
	if ( gahp_log == NULL ) {
		dprintf(D_ALWAYS, "Warning: No GCE_GAHP_LOG defined\n");
	} else {
		args.AppendArg("-f");
		args.AppendArg(gahp_log);
		free(gahp_log);
	}

	args.AppendArg("-w");
	gahp_worker_cnt = param_integer( "GCE_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-m");
	gahp_worker_cnt = param_integer( "GCE_GAHP_WORKER_MAX_NUM", 5 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-d");
	gahp_debug = param( "GCE_GAHP_DEBUG" );
	if (!gahp_debug) {
		args.AppendArg("D_ALWAYS");
	} else {
		args.AppendArg(gahp_debug);
		free(gahp_debug);
	}

	gahp = new GahpClient( GCE_RESOURCE_NAME, gahp_path, &args );
	free(gahp_path);
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource =
		GCEResource::FindOrCreateResource( m_serviceUrl.c_str(),
										   m_project.c_str(),
										   m_zone.c_str(),
										   m_authFile.c_str() );
	myResource->RegisterJob( this );

	value.clear();
	jobAd->LookupString( ATTR_GRID_JOB_ID, value );
	if ( !value.empty() ) {
		const char *token;

		dprintf(D_ALWAYS, "Recovering job from %s\n", value.c_str());

		Tokenize( value );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "gce" ) ) {
			formatstr( error_string, "%s not of type gce", ATTR_GRID_JOB_ID );
			goto error_exit;
		}

			// Skip the service URL
		GetNextToken( " ", false );

		token = GetNextToken( " ", false );
		if ( token ) {
			m_instanceName = token;
			dprintf( D_FULLDEBUG, "Found instance name '%s'.\n", m_instanceName.c_str() );
		}

		token = GetNextToken( " ", false );
		if ( token ) {
			m_instanceId = token;
			dprintf( D_FULLDEBUG, "Found instance ID '%s'.\n", m_instanceId.c_str() );
		}
	}

	if ( !m_instanceId.empty() ) {
		myResource->AlreadySubmitted( this );
		SetInstanceId( m_instanceId.c_str() );
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

	// JEF: Increment a GMSession attribute for use in letting the job
	// ad crash the gridmanager on request
	if ( jobAd->LookupExpr( "CrashGM" ) != NULL ) {
		int session = 0;
		jobAd->LookupInteger( "GMSession", session );
		session++;
		jobAd->Assign( "GMSession", session );
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.c_str() );
	}

	return;
}

GCEJob::~GCEJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
		if( ! m_instanceId.empty() ) {
			myResource->jobsByInstanceID.remove( HashKey( m_instanceId.c_str() ) );
		}
	}
	delete gahp;
}


void GCEJob::Reconfig()
{
	BaseJob::Reconfig();
}


void GCEJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;
	std::string gahp_error_code;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

	dprintf(D_ALWAYS, "(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],condorState);

	if ( gahp ) {
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

	do {

		gahp_error_code = "";

		// JEF: Crash the gridmanager if requested by the job
		int should_crash = 0;
		jobAd->Assign( "GMState", gmState );
		jobAd->SetDirtyFlag( "GMState", false );

		if ( jobAd->EvalBool( "CrashGM", NULL, should_crash ) && should_crash ) {
			EXCEPT( "Crashing gridmanager at the request of job %d.%d",
					procID.cluster, procID.proc );
		}

		reevaluate_state = false;
		old_gm_state = gmState;

		switch ( gmState )
		{
			case GM_INIT:
				// This is the state all jobs start in when the GCEJob object
				// is first created. Here, we do things that we didn't want to
				// do in the constructor because they could block (the
				// constructor is called while we're connected to the schedd).

				// JEF: Save GMSession to the schedd if needed
				jobAd->GetDirtyFlag( "GMSession", &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}

				if ( gahp->Startup() == false ) {
					dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n",
							 procID.cluster, procID.proc );
					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
					gmState = GM_HOLD;
					break;
				}

				errorString = "";

				//
				// Put the job on hold for auth failures, but only if
				// we aren't trying to remove it.
				//
				if( ! myResource->didFirstPing() ) { break; }
				if( myResource->hadAuthFailure() ) {
					if( condorState == REMOVED && m_instanceName.empty() && m_instanceId.empty() ) {
						gmState = GM_DELETE;
						break;
					} else {
						formatstr( errorString, "Failed to authenticate %s.",
									myResource->authFailureMessage.c_str() );
						dprintf( D_ALWAYS, "(%d.%d) %s\n",
								procID.cluster, procID.proc, errorString.c_str() );
						gmState = GM_HOLD;
						break;
					}
				}

				// If we're not doing recovery, start with GM_CLEAR_REQUEST.
				gmState = GM_CLEAR_REQUEST;

				if( ! m_instanceName.empty() ) {

					// Because we have the instance name, we know that
					// we still have the SSH keypair from the previous
					// execution of GM_SAVE_INSTANCE_NAME.  We can therefore
					// jump directly to GM_START_VM, where (using the
					// instance name), we will call RunInstances()
					// idempotently.
					gmState = GM_START_VM;

					// As an optimization, if we already have the instance
					// ID, we can jump directly to GM_SUBMITTED.  This
					// also allows us to avoid logging the submission and
					// execution events twice.
					//
					// FIXME: If a job were removed before we begin
					// recovery, would the execute event be logged twice?
					if (!m_instanceId.empty()) {
						submitLogged = true;
						if ( condorState == RUNNING || condorState == COMPLETED ) {
							executeLogged = true;
						}
						// Do NOT set probeNow; if we're recovering from
						// a queue with 5000 jobs, we'd hit the service
						// with 5000 status update requests, which is
						// precisely what we're trying to avoid.
						gmState = GM_SUBMITTED;
					} else if( condorState == REMOVED ) {
						// We don't know if the corresponding instance
						// exists or not. And if we're creating the
						// ssh keypair, we don't know if that exists.
						// Rather than unconditionally
						// create it (in GM_START_VM), check to see if
						// exists first.  While this may be more efficient,
						// the real benefit is that invalid jobs won't
						// stay on hold when the user removes them
						// (because we won't try to start them).
						gmState = GM_SEEK_INSTANCE_ID;
					}
				}
				break;


			case GM_SAVE_INSTANCE_NAME: {
				// If we don't know yet what type of server we're talking
				// to (e.g. all pings have failed because the server's
				// down), we have to wait here, as that affects how we'll
				// submit the job.
				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CLEAR_REQUEST;
					break;
				}

				if (m_instanceName.empty()) {
					SetInstanceName(build_instance_name().c_str());
				}

				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );

				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}

				gmState = GM_START_VM;

				} break;

			case GM_START_VM:

				// XXX: This is always false, why is numSubmitAttempts
				// not incremented after gce_instance_insert? Same for
				// dcloudjob.
				if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
					gmState = GM_HOLD;
					break;
				}

				// After a submit, wait at least submitInterval before trying another one.
				if ( now >= lastSubmitAttempt + submitInterval ) {

					// Once RequestSubmit() is called at least once, you must
					// CancelSubmit() once you're done with the request call
					if ( myResource->RequestSubmit( this ) == false ) {
						// If we haven't started the START_VM call yet,
						// we can abort the submission here for held and
						// removed jobs.
						if ( (condorState == REMOVED) ||
							 (condorState == HELD) ) {
							myResource->CancelSubmit( this );
							gmState = GM_DELETE;
						}
						break;
					}

					// construct input parameters for gce_instance_insert()
					std::string instance_id = "";

					// gce_instance_insert() will check the input arguments
					rc = gahp->gce_instance_insert( m_serviceUrl,
													m_authFile,
													m_project,
													m_zone,
													m_instanceName,
													m_machineType,
													m_image,
													m_metadata,
													m_metadataFile,
													instance_id );

					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					lastSubmitAttempt = time(NULL);

					if ( rc != 0 &&
						 gahp_error_code == "NEED_CHECK_VM_START" ) {
						// get an error code from gahp server said that we should check if
						// the VM has been started successfully in EC2

						// Maxmium retry times is 3, if exceeds this limitation, we fall through
						if ( m_retry_times++ < maxRetryTimes ) {
							gmState = GM_START_VM;
						}
						break;
					}

					if ( rc == 0 ) {

						ASSERT( instance_id != "" );
						SetInstanceId( instance_id.c_str() );
						WriteGridSubmitEventToUserLog(jobAd);

						gmState = GM_SAVE_INSTANCE_ID;

					} else if ( gahp_error_code == "InstanceLimitExceeded" ) {
						// meet the resource limitation (maximum 20 instances)
						// should retry this command later
						myResource->CancelSubmit( this );
						daemonCore->Reset_Timer( evaluateStateTid,
												 submitInterval );
					 } else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s: %s\n",
								procID.cluster, procID.proc,
								gahp_error_code.c_str(),
								errorString.c_str() );
						gmState = GM_HOLD;
					}

				} else {
					if ( (condorState == REMOVED) || (condorState == HELD) ) {
						gmState = GM_CANCEL;
						break;
					}

					unsigned int delay = 0;
					if ( (lastSubmitAttempt + submitInterval) > now ) {
						delay = (lastSubmitAttempt + submitInterval) - now;
					}
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}

				break;


			case GM_SAVE_INSTANCE_ID:

				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID,
									 &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					// Wait for the instance id to be saved to the schedd
					requestScheddUpdate( this, true );
					break;
				}

				// If we just now, for the first time, discovered
				// an instance's ID, we don't presently know its
				// address(es) (or hostnames).  It seems reasonable
				// to learn this as quickly as possible.
				probeNow = true;

				gmState = GM_SUBMITTED;
				break;


			case GM_SUBMITTED:
				if ( remoteJobState == GCE_INSTANCE_TERMINATED ) {
					gmState = GM_DONE_SAVE;
				}

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				}
				else {
					// Don't go to GM probe until asked (when the remote
					// job status changes).
					if( ! probeNow ) { break; }

					if ( lastProbeTime < enteredCurrentGmState ) {
						lastProbeTime = enteredCurrentGmState;
					}

					// if current state isn't "running", we should check its state
					// every "funcRetryInterval" seconds. Otherwise the interval should
					// be "probeInterval" seconds.
					int interval = myResource->GetJobPollInterval();
					if ( remoteJobState != GCE_INSTANCE_RUNNING ) {
						interval = funcRetryInterval;
					}

					if ( now >= lastProbeTime + interval ) {
						gmState = GM_PROBE_JOB;
						break;
					}

					unsigned int delay = 0;
					if ( (lastProbeTime + interval) > now ) {
						delay = (lastProbeTime + interval) - now;
					}
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}
				break;


			case GM_DONE_SAVE:

					// XXX: Review this

				if ( condorState != HELD && condorState != REMOVED ) {
					JobTerminated();
					if ( condorState == COMPLETED ) {
						jobAd->GetDirtyFlag( ATTR_JOB_STATUS,
											 &attr_exists, &attr_dirty );
						if ( attr_exists && attr_dirty ) {
							requestScheddUpdate( this, true );
							break;
						}
					}
				}

				// TODO Need to delete instance!

				myResource->CancelSubmit( this );
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					SetInstanceId( NULL );
					SetInstanceName( NULL );
					gmState = GM_CLEAR_REQUEST;
				}

				break;


			case GM_CLEAR_REQUEST: {
				m_retry_times = 0;

				// Remove all knowledge of any previous or present job
				// submission, in both the gridmanager and the schedd.

				// If we are doing a rematch, we are simply waiting around
				// for the schedd to be updated and subsequently this globus job
				// object to be destroyed.  So there is nothing to do.
				if ( wantRematch ) {
					break;
				}

				// For now, put problem jobs on hold instead of
				// forgetting about current submission and trying again.
				// TODO: Let our action here be dictated by the user preference
				// expressed in the job ad.
				if ( !m_instanceId.empty() && condorState != REMOVED
					 && wantResubmit == 0 && doResubmit == 0 ) {
					gmState = GM_HOLD;
					break;
				}

				// Only allow a rematch *if* we are also going to perform a resubmit
				if ( wantResubmit || doResubmit ) {
					jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
				}

				if ( wantResubmit ) {
					wantResubmit = 0;
					dprintf(D_ALWAYS, "(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
				}

				if ( doResubmit ) {
					doResubmit = 0;
					dprintf(D_ALWAYS, "(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
				}

				errorString = "";
				myResource->CancelSubmit( this );
				SetInstanceId( NULL );
				SetInstanceName( NULL );

				JobIdle();

				if ( submitLogged ) {
					JobEvicted();
					if ( !evictLogged ) {
						WriteEvictEventToUserLog( jobAd );
						evictLogged = true;
					}
				}

				if ( remoteJobState != "" ) {
					remoteJobState = "";
					SetRemoteJobStatus( NULL );
				}

				if ( wantRematch ) {
					dprintf(D_ALWAYS, "(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

					// Set ad attributes so the schedd finds a new match.
					int dummy;
					if ( jobAd->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
						jobAd->Assign( ATTR_JOB_MATCHED, false );
						jobAd->Assign( ATTR_CURRENT_HOSTS, 0 );
					}

					// If we are rematching, we need to forget about this job
					// cuz we wanna pull a fresh new job ad, with a fresh new match,
					// from the all-singing schedd.
					gmState = GM_DELETE;
					break;
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
				const char *name;
				ExprTree *expr;
				jobAd->ResetExpr();
				if ( jobAd->NextDirtyExpr(name, expr) ) {
					requestScheddUpdate( this, true );
					break;
				}

				submitLogged = false;
				executeLogged = false;
				submitFailedLogged = false;
				terminateLogged = false;
				abortLogged = false;
				evictLogged = false;
				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_SAVE_INSTANCE_NAME;
				}
			} break;


			case GM_PROBE_JOB:
				// Note that we do an individual-job probe because it can
				// return information (e.g., the public DNS name) that the
				// user should now about it.  It also simplifies the coding,
				// since the status-handling code can stay here.
				probeNow = false;

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_SUBMITTED; // GM_SUBMITTED knows how to handle this
				} else {

					// We don't check for a status change, because this
					// state is now only entered if we had one.
					if( remoteJobState == GCE_INSTANCE_RUNNING ||
						remoteJobState == GCE_INSTANCE_STOPPING ||
						remoteJobState == GCE_INSTANCE_STOPPED ||
						remoteJobState == GCE_INSTANCE_TERMINATED ) {
						JobRunning();
					}

					// dprintf( D_ALWAYS, "DEBUG: m_state_reason_code = %s (assuming 'NULL')\n", m_state_reason_code.c_str() );
					if( ! m_state_reason_code.empty() ) {
						// Send the user a copy of the reason code.
						jobAd->Assign( ATTR_EC2_STATUS_REASON_CODE, m_state_reason_code.c_str() );
						requestScheddUpdate( this, false );

							//
							// http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-ItemType-StateReasonType.html
							// defines the state [transition] reason codes.
							//
							// We consider the following reasons to be normal
							// termination conditions:
							//
							// - Client.InstanceInitiatedShutdown
							// - Client.UserInitiatedShutdown
							// - Server.SpotInstanceTermination
							//
							// the last because the user will be able to ask
							// Condor, via on_exit_remove (and the attribute
							// updated above), to resubmit the job.
							//
							// We consider the following reasons to be abnormal
							// termination conditions, and thus put the job
							// on hold:
							//
							// - Server.InternalError
							// - Server.InsufficientInstanceCapacity
							// - Client.VolumeLimitExceeded
							// - Client.InternalError
							// - Client.InvalidSnapshot.NotFound
							//
							// The first three are likely to be transient; if
							// the distinction becomes important, we can add
							// it later.
							//

						if(
							m_state_reason_code == "Client.InstanceInitiatedShutdown"
						 || m_state_reason_code == "Client.UserInitiatedShutdown"
						 || m_state_reason_code == "Server.SpotInstanceTermination" ) {
							// Normal instance terminations are normal.
						} else if(
							m_state_reason_code == "purged" ) {
							// This isn't normal, but if the job was purged,
							// there's no reason to hold onto it.  Added this
							// so that we wouldn't complain but still write
							// purged as the EC2StatusReasonCode to the
							// history file.
						} else if(
							m_state_reason_code == "Server.InternalError"
						 || m_state_reason_code == "Server.InsufficientInstanceCapacity"
						 || m_state_reason_code == "Client.VolumeLimitExceeded"
						 || m_state_reason_code == "Client.InternalError"
						 || m_state_reason_code == "Client.InvalidSnapshot.NotFound" ) {
							// Put abnormal instance terminations on hold.
							formatstr( errorString, "Abnormal instance termination: %s.", m_state_reason_code.c_str() );
							dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
							gmState = GM_HOLD;
							break;
						} else {
							// Treat all unrecognized reasons as abnormal.
							formatstr( errorString, "Unrecognized reason for instance termination: %s.  Treating as abnormal.", m_state_reason_code.c_str() );
							dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
							gmState = GM_HOLD;
							break;
						}
					}

					lastProbeTime = now;
					gmState = GM_SUBMITTED;
				}
				break;

			case GM_CANCEL:

				// Rather than duplicate code in the spot instance subgraph,
				// just handle jobs with no corresponding instance here.
				if( m_instanceId.empty() ) {
					// Bypasses the polling delay in GM_CANCEL_CHECK.
					probeNow = true;
				} else {
					rc = gahp->gce_instance_delete( m_serviceUrl,
													m_authFile,
													m_project,
													m_zone,
													m_instanceName );

					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					if( rc != 0 ) {
						if ( strstr( gahp->getErrorString(), "was not found" ) ) {
							// The instance is gone. Don't wait in
							// GM_CANCEL_CHECK for a bulk status update.
							SetRemoteJobStatus( GCE_INSTANCE_TERMINATED );
							remoteJobState = GCE_INSTANCE_TERMINATED;
							probeNow = true;
							gmState = GM_CANCEL_CHECK;
							break;
						}
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s: %s\n",
								 procID.cluster, procID.proc,
								 gahp_error_code.c_str(),
								errorString.c_str() );
						gmState = GM_HOLD;
						break;
					}

				}

				gmState = GM_CANCEL_CHECK;
				break;

			case GM_CANCEL_CHECK:
				// Wait for the job's state to change.  This should happen
				// on the next poll, because the job is in an invalid state.
				if( ! probeNow ) { break; }
				probeNow = false;

				// We cancel (terminate) a job for one of two reasons: it
				// either entered one of the two semi-terminated states
				// (STOPPED or SHUTOFF) of its own accord, or the job went
				// on hold or was removed.
				//
				// In either case, we need to confirm that the cancel
				// command succeeded.  However, since Amazon permits
				// STOPPED instances to enter the TERMINATED state, we
				// can, presuming the same for SHUTOFF instances, just
				// retry until the instance becomes TERMINATED.  An
				// instance that has been purged must have successfully
				// terminated first, so we will treat it the same.
				//
				// All other states, therefore, will cause us to try
				// terminating the instance again.
				//
				// Once we've successfully terminated the instance,
				// we need to go to GM_DONE_SAVE for instances that were
				// STOPPED or SHUTOFF, and either GM_DELETE or
				// GM_CLEAR_REQUEST, depending on the condor state of the
				// job, otherwise.
				//

				// TODO When do we see STOPPED? Do we have to do something
				//   when we see this state?
				if( remoteJobState == GCE_INSTANCE_TERMINATED ) {
					if( m_was_job_completion ) {
						gmState = GM_DONE_SAVE;
					} else {
						if( condorState == COMPLETED || condorState == REMOVED ) {
							gmState = GM_DELETE;
						} else {
							// The job may run again, so clear the job state.
							//
							// (If a job is held and then released, the job
							// could find its previous (terminated) instance
							// and consider itself complete.)
							//
							// Clear the contact string here first because
							// we may enter GM_HOLD before GM_CLEAR_REQUEST
							// actually clears the request. (*sigh*)
							myResource->CancelSubmit( this );
							SetInstanceId( NULL );
							SetInstanceName( NULL );
							gmState = GM_CLEAR_REQUEST;
						}
					}
				} else {
					if( m_retry_times++ < maxRetryTimes ) {
						gmState = GM_CANCEL;
					} else {
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job cancel did not succeed after %d tries, giving up.\n",
								 procID.cluster, procID.proc, maxRetryTimes );
						gmState = GM_HOLD;
						break;
					}
				}
				break;


			case GM_HOLD:
				// Put the job on hold in the schedd.
				// If the condor state is already HELD, then someone already
				// HELD it, so don't update anything else.
				if ( condorState != HELD ) {

					// Set the hold reason as best we can
					// TODO: set the hold reason in a more robust way.
					char holdReason[1024];
					holdReason[0] = '\0';
					holdReason[sizeof(holdReason)-1] = '\0';
					jobAd->LookupString( ATTR_HOLD_REASON,
										 holdReason, sizeof(holdReason) - 1 );
					if ( holdReason[0] == '\0' && errorString != "" ) {
						strncpy( holdReason, errorString.c_str(),
								 sizeof(holdReason) - 1 );
					} else if ( holdReason[0] == '\0' ) {
						strncpy( holdReason, "Unspecified gridmanager error",
								 sizeof(holdReason) - 1 );
					}

					JobHeld( holdReason );
				}

				gmState = GM_DELETE;

				break;


			case GM_DELETE:
				// We are done with the job. Propagate any remaining updates
				// to the schedd, then delete this object.
				DoneWithJob();
				// This object will be deleted when the update occurs
				break;


			case GM_SEEK_INSTANCE_ID: {
				// Wait for the next scheduled bulk query.
				if( ! probeNow ) { break; }

				// If the bulk query found this job, it has an instance ID.
				// (If the job had an instance ID before, we would be in
				// an another state.)  Otherwise, the service doesn't know
				// about this job, and we can remove it from the queue.
				if( ! m_instanceId.empty() ) {
					WriteGridSubmitEventToUserLog( jobAd );
					gmState = GM_SAVE_INSTANCE_ID;
				} else if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_START_VM;
				}

				} break;

			default:
				EXCEPT( "(%d.%d) Unknown gmState %d!",
						procID.cluster, procID.proc, gmState );
				break;
		} // end of switch_case

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc,
					GMStateNames[old_gm_state], GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}

	} // end of do_while
	while ( reevaluate_state );
}


BaseResource* GCEJob::GetResource()
{
	return (BaseResource *)myResource;
}


// setup the public name of gce remote VM, which can be used the clients
void GCEJob::SetRemoteVMName( const char * newName )
{
	if( newName == NULL ) {
		newName = "Undefined";
	}

	std::string oldName;
	jobAd->LookupString( ATTR_EC2_REMOTE_VM_NAME, oldName );
	if( oldName != newName ) {
		jobAd->Assign( ATTR_EC2_REMOTE_VM_NAME, newName );
		requestScheddUpdate( this, false );
	}
}

void GCEJob::SetInstanceName(const char *instance_name)
{
	if( instance_name != NULL ) {
		m_instanceName = instance_name;
	} else {
		m_instanceName.clear();
	}
	GCESetRemoteJobId(m_instanceName.empty() ? NULL : m_instanceName.c_str(),
				   m_instanceId.c_str());
}

void GCEJob::SetInstanceId( const char *instance_id )
{
	// Don't unconditionally clear the remote job ID -- if we do,
	// SetInstanceId( m_instanceId.c_str() ) does exactly the opposite
	// of what you'd expect, because the c_str() is cleared as well.
	if( instance_id == NULL ) {
		m_instanceId.clear();
	} else {
		m_instanceId = instance_id;
	}
	GCESetRemoteJobId( m_instanceName.c_str(),
					m_instanceId.empty() ? NULL : m_instanceId.c_str() );
}

// GCESetRemoteJobId() is used to set the value of global variable "remoteJobID"
// Don't call this function directly!
// It doesn't update m_instanceName or m_instanceId!
// Use SetInstanceName() or SetInstanceId() instead.
void GCEJob::GCESetRemoteJobId( const char *instance_name, const char *instance_id )
{
	string full_job_id;
	if ( instance_name && instance_name[0] ) {
		formatstr( full_job_id, "gce %s %s", m_serviceUrl.c_str(), instance_name );
		if ( instance_id && instance_id[0] ) {
			// We need this to do bulk status queries.
			myResource->jobsByInstanceID.insert( HashKey( instance_id ), this );
			formatstr_cat( full_job_id, " %s", instance_id );
		}
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}


// Instance name is max 63 characters, matching this pattern:
//    [a-z]([-a-z0-9]*[a-z0-9])?
string GCEJob::build_instance_name()
{
#ifdef WIN32
	GUID guid;
	if (S_OK != CoCreateGuid(&guid))
		return NULL;
	WCHAR wsz[40];
	StringFromGUID2(guid, wsz, COUNTOF(wsz));
	char uuid_str[40];
	WideCharToMultiByte(CP_ACP, 0, wsz, -1, uuid_str, COUNTOF(uuid_str), NULL, NULL);
	string final_str = "condor-";
	final_str += uuid_str;
	return final_str;
#else
	char uuid_str[37];
	uuid_t uuid;

	uuid_generate(uuid);

	uuid_unparse(uuid, uuid_str);
	uuid_str[36] = '\0';
	for ( char *c = uuid_str; *c != '\0'; c++ ) {
		*c = tolower( *c );
	}
	std::string final_str = "condor-";
	final_str += uuid_str;
	return final_str;
#endif
}

void GCEJob::StatusUpdate( const char * instanceID,
						   const char * status,
						   const char * stateReasonCode,
						   const char * publicDNSName ) {
	// This avoids having to store the public DNS name for GM_PROBE_JOB.
	if( publicDNSName != NULL && strlen( publicDNSName ) != 0
	 && strcmp( publicDNSName, "NULL" ) != 0 ) {
		SetRemoteVMName( publicDNSName );
	}

	if( stateReasonCode != NULL && strlen( stateReasonCode ) != 0
	 && strcmp( stateReasonCode, "NULL" ) != 0 ) {
		m_state_reason_code = stateReasonCode;
	} else {
		m_state_reason_code.clear();
	}

	// To avoid concurrency issues, we could delay calling SetEvaluateState()
	// until just before we exit the function.

	// If the bulk status update didn't find this job, assume it's gone.
	// The job will be unblocked after the SetRemoteStatus() call below
	// if it wasn't previously purged.
	if( !m_instanceId.empty() && status == NULL ) {
		status = GCE_INSTANCE_TERMINATED;
	}

	// Update the instance ID, if this is the first time we've seen it.
	if( m_instanceId.empty() ) {
		if( instanceID && *instanceID ) {
			SetInstanceId( instanceID );
		}

		// We only consider discovering the instance ID a status change
		// when it occurs while we're blocked in GM_SEEK_INSTANCE_ID.
		if( gmState == GM_SEEK_INSTANCE_ID ) {
			probeNow = true;
			SetEvaluateState();
		}
	}

	// SetRemoteJobStatus() sets the last-update timestamp, but
	// only returns true if the status has changed.
	if( SetRemoteJobStatus( status ) ) {
		remoteJobState = status;
		probeNow = true;
		SetEvaluateState();
	}
}
