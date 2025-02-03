/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "basename.h"

#ifdef WIN32
#else
#include <uuid/uuid.h>
#endif

#include "gridmanager.h"
#include "azurejob.h"
#include "condor_config.h"

#define GM_INIT							0
#define GM_START_VM						1
#define GM_SAVE_VM_INFO					2
#define GM_SUBMITTED					3
#define GM_DONE_SAVE					4
#define GM_CANCEL						5
#define GM_DELETE						6
#define GM_CLEAR_REQUEST				7
#define GM_HOLD							8
#define GM_PROBE_JOB					9
#define GM_SAVE_VM_NAME					10
#define GM_CHECK_SUBMISSION				11

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_START_VM",
	"GM_SAVE_VM_INFO",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_CANCEL",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROBE_JOB",
	"GM_SAVE_VM_NAME",
	"GM_CHECK_SUBMISSION",
};

#define AZURE_VM_STARTING		"starting"
#define AZURE_VM_RUNNING		"running"
#define AZURE_VM_DEALLOCATING	"deallocating"
#define AZURE_VM_DEALLOCATED	"deallocated"

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void AzureJobInit()
{
}


void AzureJobReconfig()
{
    int gct = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 10 * 60 );
	AzureJob::setGahpCallTimeout( gct );

	int cfrc = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT", 3);
	AzureJob::setConnectFailureRetry( cfrc );

	// Tell all the resource objects to deal with their new config values
	for (auto &elem : AzureResource::ResourcesByName) {
		elem.second->Reconfig();
	}
}


bool AzureJobAdMatch( const ClassAd *job_ad )
{
	int universe;
	std::string resource;

	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) &&
		 (strncasecmp( resource.c_str(), "azure", 3 ) == 0) )
	{
		return true;
	}
	return false;
}


BaseJob* AzureJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new AzureJob( jobad );
}

int AzureJob::gahpCallTimeout = 600;
int AzureJob::submitInterval = 300;
int AzureJob::maxConnectFailures = 3;
int AzureJob::funcRetryInterval = 15;
int AzureJob::maxRetryTimes = 3;

AzureJob::AzureJob( ClassAd *classad ) :
	BaseJob( classad ),
	m_retry_times( 0 ),
	probeNow( false )
{
	std::string error_string = "";
	char *gahp_path = NULL;
	std::string value;

	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	myResource = NULL;
	gahp = NULL;

	// Check for failure injections.
	m_failure_injection = getenv( "GM_FAILURE_INJECTION" );
	if( m_failure_injection == NULL ) { m_failure_injection = ""; }
	dprintf( D_FULLDEBUG, "GM_FAILURE_INJECTION = %s\n", m_failure_injection );

	// check the auth_file
	jobAd->LookupString( ATTR_AZURE_AUTH_FILE, m_authFile );

	if ( m_authFile.empty() ) {
		// This means attempt to grab the default Azure CLI credentials
		m_authFile = "NULL";
	}

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
		if ( !token || strcasecmp( token, "azure" ) ) {
			formatstr( error_string, "%s not of type azure",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			m_subscription = token;
		} else {
			formatstr( error_string, "%s missing Azure subscription",
									  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		formatstr( error_string, "%s is not set in the job ad",
								  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	gahp_path = param( "AZURE_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "AZURE_GAHP not defined";
		goto error_exit;
	}

	gahp = new GahpClient( AZURE_RESOURCE_NAME, gahp_path );
	free(gahp_path);
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource =
		AzureResource::FindOrCreateResource( "azure",
		                                     m_subscription.c_str(),
		                                     m_authFile.c_str() );
	myResource->RegisterJob( this );

	value.clear();
	jobAd->LookupString( ATTR_GRID_JOB_ID, value );
	if ( !value.empty() ) {
		const char *token;

		dprintf(D_ALWAYS, "Recovering job from %s\n", value.c_str());

		Tokenize( value );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "azure" ) ) {
			formatstr( error_string, "%s not of type azure", ATTR_GRID_JOB_ID );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token ) {
			m_vmName = token;
			dprintf( D_FULLDEBUG, "Found vm name '%s'.\n", m_vmName.c_str() );
		}
	}

//	if ( !m_instanceId.empty() ) {
//		myResource->AlreadySubmitted( this );
//		SetInstanceId( m_instanceId.c_str() );
//	}

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
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}

	return;
}

AzureJob::~AzureJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	delete gahp;
}


void AzureJob::Reconfig()
{
	BaseJob::Reconfig();
}


void AzureJob::doEvaluateState( int /* timerID */ )
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;
	std::string gahp_error_code;
	std::string tmp_str;

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
		bool should_crash = false;
		jobAd->Assign( "GMState", gmState );
		jobAd->MarkAttributeClean( "GMState" );

		if ( jobAd->LookupBool( "CrashGM", should_crash ) && should_crash ) {
			EXCEPT( "Crashing gridmanager at the request of job %d.%d",
					procID.cluster, procID.proc );
		}

		reevaluate_state = false;
		old_gm_state = gmState;
		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState )
		{
			case GM_INIT:
				// This is the state all jobs start in when the AzureJob object
				// is first created. Here, we do things that we didn't want to
				// do in the constructor because they could block (the
				// constructor is called while we're connected to the schedd).

				// JEF: Save GMSession to the schedd if needed
				if ( jobAd->IsAttributeDirty( "GMSession" ) ) {
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
					if( condorState == REMOVED && m_vmName.empty() ) {
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

				if ( m_vmName.empty() ) {
					// This is a fresh job submission.
					gmState = GM_CLEAR_REQUEST;
				} else if ( !jobAd->LookupString( ATTR_AZURE_VM_ID, tmp_str ) ) {
					// We died during submission. There may be a running
					// vm whose ID we don't know.
					// Wait for a bulk query of vms and see if a vm
					// that matches our m_vmName appears.
					gmState = GM_CHECK_SUBMISSION;
				} else {
					// There is (or was) a running vm and we know
					// its ID.
					submitLogged = true;
					if ( condorState == RUNNING || condorState == COMPLETED ) {
						executeLogged = true;
					}
					gmState = GM_SUBMITTED;
				}
				break;

			case GM_SAVE_VM_NAME: {
				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CLEAR_REQUEST;
					break;
				}

				if (m_vmName.empty()) {
					SetRemoteJobId(build_vm_name().c_str());
				}

				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
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

				if ( ( condorState == REMOVED || condorState == HELD ) &&
					 !gahp->pendingRequestIssued() ) {
					gmState = GM_CLEAR_REQUEST;
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

					std::string vm_id;
					std::string ip_address;

					if ( m_vmParams.empty() ) {
						if ( !BuildVmParams() ) {
							myResource->CancelSubmit(this);
							gmState = GM_HOLD;
							break;
						}
					}

					rc = gahp->azure_vm_create( m_authFile,
					                            m_subscription,
					                            m_vmParams,
					                            vm_id,
					                            ip_address );

					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					lastSubmitAttempt = time(NULL);

					if ( rc == 0 ) {

						ASSERT( vm_id != "" );
						jobAd->Assign( ATTR_AZURE_VM_ID, vm_id );
						jobAd->Assign( "VmIpAddress", ip_address );
						WriteGridSubmitEventToUserLog(jobAd);
						submitLogged = true;

						gmState = GM_SAVE_VM_INFO;

					 } else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s: %s\n",
								procID.cluster, procID.proc,
								gahp_error_code.c_str(),
								errorString.c_str() );
						gmState = GM_HOLD;
					}

				} else {
					gmState = GM_CANCEL;
					break;
				}

				break;


			case GM_SAVE_VM_INFO:

				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
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
				if ( remoteJobState == AZURE_VM_DEALLOCATED ) {
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
					if ( remoteJobState != AZURE_VM_RUNNING ) {
						interval = funcRetryInterval;
					}

					if ( now >= lastProbeTime + interval ) {
						gmState = GM_PROBE_JOB;
						break;
					}

					time_t delay = 0;
					if ( (lastProbeTime + interval) > now ) {
						delay = (lastProbeTime + interval) - now;
					}
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}
				break;


			case GM_DONE_SAVE:

				if ( condorState != HELD && condorState != REMOVED ) {
					JobTerminated();
					if ( condorState == COMPLETED ) {
						if ( jobAd->IsAttributeDirty( ATTR_JOB_STATUS ) ) {
							requestScheddUpdate( this, true );
							break;
						}
					}
				}

				gmState = GM_CANCEL;

				break;


			case GM_CLEAR_REQUEST: {
				m_retry_times = 0;

				// Remove all knowledge of any previous or present job
				// submission, in both the gridmanager and the schedd.

				// For now, put problem jobs on hold instead of
				// forgetting about current submission and trying again.
				// TODO: Let our action here be dictated by the user preference
				// expressed in the job ad.
				if (!m_vmName.empty() && condorState != REMOVED) {
					gmState = GM_HOLD;
					break;
				}

				errorString = "";
				myResource->CancelSubmit( this );
				// TODO Can we make SetRemoteJobId() more intelligent
				//   about not dirtying the job if the value won't
				//   change?
				if ( !m_vmName.empty() ) {
					SetRemoteJobId( NULL );
				}
				if ( jobAd->LookupString( ATTR_AZURE_VM_ID, tmp_str ) ) {
					jobAd->AssignExpr( ATTR_AZURE_VM_ID, "Undefined" );
				}
				if ( jobAd->LookupString( "VmIpAddress", tmp_str ) ) {
					jobAd->AssignExpr( "VmIpAddress", "Undefined" );
				}

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

				submitLogged = false;
				executeLogged = false;
				submitFailedLogged = false;
				terminateLogged = false;
				abortLogged = false;
				evictLogged = false;
				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_SAVE_VM_NAME;
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
					if( remoteJobState == AZURE_VM_RUNNING ||
						remoteJobState == AZURE_VM_DEALLOCATING ||
						remoteJobState == AZURE_VM_DEALLOCATED ) {
						JobRunning();
					}

					lastProbeTime = now;
					gmState = GM_SUBMITTED;
				}
				break;

			case GM_CANCEL:
				// Delete the vm on the server. This can be due
				// to the user running condor_rm/condor_hold or the
				// the instance shutting down (job completion).

				rc = gahp->azure_vm_delete( m_authFile,
											m_subscription,
											m_vmName );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				if( rc != 0 && !strstr( gahp->getErrorString(), "could not be found" ) ) {
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s: %s\n",
							 procID.cluster, procID.proc,
							 gahp_error_code.c_str(),
							 errorString.c_str() );
					gmState = GM_HOLD;
					break;
				}

				myResource->CancelSubmit( this );
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					SetRemoteJobId( NULL );
					gmState = GM_CLEAR_REQUEST;
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


			case GM_CHECK_SUBMISSION: {
				// Wait for the next scheduled bulk query.
				if( ! probeNow ) { break; }

				// If the bulk query found this job, it has an instance ID.
				// (If the job had an instance ID before, we would be in
				// an another state.)  Otherwise, the service doesn't know
				// about this job, and we can submit the job or let it
				// leave the queue.
				std::string vm_id;
				if( !remoteJobState.empty() ) {
					WriteGridSubmitEventToUserLog( jobAd );
					submitLogged = true;
						// TODO add and invoke an AZURE_VM_INFO command to
						//   get vm id and public ip address.
						//   For now, set AzureVmId to an empty string to
						//   indicate a successful submission.
					jobAd->Assign( ATTR_AZURE_VM_ID, "" );
					gmState = GM_SAVE_VM_INFO;
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


BaseResource* AzureJob::GetResource()
{
	return (BaseResource *)myResource;
}


void AzureJob::SetRemoteJobId( const char *vm_name )
{
	std::string full_job_id;
	if ( vm_name && vm_name[0] ) {
		m_vmName = vm_name;
		formatstr( full_job_id, "azure %s", vm_name );
	} else {
		m_vmName.clear();
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}


// Instance name is max 64 characters, alphanumberic, underscore, and hyphen
std::string AzureJob::build_vm_name()
{
	// TODO Currently, a storage account name is derived from our
	//   unique name, and those have max length 24 and must be
	//   alphanumeric.
	//   Once the gahp can support using managed disks in Azure,
	//   this naming restriction will go away.
#if 1
	std::string final_str;
	formatstr( final_str, "condorc%dp%d", procID.cluster, procID.proc );
	return final_str;
#else
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
#endif
}

bool AzureJob::BuildVmParams()
{
	std::string value;
	std::string name_value;

	m_vmParams.clear();

	name_value = "name=";
	name_value += m_vmName;
	m_vmParams.emplace_back(name_value);

	if ( !jobAd->LookupString( ATTR_AZURE_LOCATION, value ) ) {
		errorString = "Missing attribute " ATTR_AZURE_LOCATION;
		return false;
	}
	name_value = "location=";
	name_value += value;
	m_vmParams.emplace_back(name_value);

	if ( !jobAd->LookupString( ATTR_AZURE_IMAGE, value ) ) {
		errorString = "Missing attribute " ATTR_AZURE_IMAGE;
		return false;
	}
	name_value = "image=";
	name_value += value;
	m_vmParams.emplace_back(name_value);

	if ( !jobAd->LookupString( ATTR_AZURE_SIZE, value ) ) {
		errorString = "Missing attribute " ATTR_AZURE_SIZE;
		return false;
	}
	name_value = "size=";
	name_value += value;
	m_vmParams.emplace_back(name_value);

	if ( !jobAd->LookupString( ATTR_AZURE_ADMIN_USERNAME, value ) ) {
		errorString = "Missing attribute " ATTR_AZURE_ADMIN_USERNAME;
		return false;
	}
	name_value = "adminUsername=";
	name_value += value;
	m_vmParams.emplace_back(name_value);

	if ( !jobAd->LookupString( ATTR_AZURE_ADMIN_KEY, value ) ) {
		errorString = "Missing attribute " ATTR_AZURE_ADMIN_KEY;
		return false;
	}
	name_value = "key=";
	name_value += value;
	m_vmParams.emplace_back(name_value);

	return true;
}

void AzureJob::StatusUpdate( const char * status )
{
	// Currently, the AZURE_VM_LIST command returns a series of State
	// attributes for the vm, as illustrated below.
	// We only care about hte PowerState, which we assume will be
	// last.
	// Example:
	//   ProvisioningState/succeeded,OSState/generalized,PowerState/deallocated
	if ( status ) {
		const char *ptr = strrchr( status, '/' );
		if ( ptr ) {
			status = ptr + 1;
		}
	}

	// If the bulk status update didn't find this job, assume it's gone.
	// We use submitLogged as an indicator that we know that the vm
	// was successfully created.
	if( submitLogged && status == NULL ) {
		status = AZURE_VM_DEALLOCATED;
	}

	// SetRemoteJobStatus() sets the last-update timestamp, but
	// only returns true if the status has changed.
	// If we're in GM_CHECK_SUBMISSION, always trigger a reaction to the
	// updated status.
	if( SetRemoteJobStatus( status ) || gmState == GM_CHECK_SUBMISSION ) {
		remoteJobState = status ? status : "";
		probeNow = true;
		SetEvaluateState();
	}
}
