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

#include "gridmanager.h"
#include "dcloudjob.h"
#include "condor_config.h"

#include <uuid/uuid.h>

#define GM_INIT			0
#define GM_CREATE_VM		1
#define GM_SAVE_INSTANCE_ID	2
#define GM_SUBMITTED		3
#define GM_DONE_SAVE		4
#define GM_CANCEL		5
#define GM_STOPPED		6
#define GM_DELETE		7
#define GM_CLEAR_REQUEST	8
#define GM_HOLD			9
#define GM_PROBE_JOB		10
#define GM_SAVE_INSTANCE_NAME	11
#define GM_CHECK_VM		12
#define GM_START_VM		13
#define GM_CHECK_AUTOSTART	14

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_CREATE_VM",
	"GM_SAVE_INSTANCE_ID",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_CANCEL",
	"GM_STOPPED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROBE_JOB",
	"GM_SAVE_INSTANCE_NAME",
	"GM_CHECK_VM",
	"GM_START_VM",
	"GM_CHECK_AUTOSTART",
};

#define DCLOUD_VM_STATE_RUNNING			"RUNNING"
#define DCLOUD_VM_STATE_PENDING			"PENDING"
#define DCLOUD_VM_STATE_STOPPED			"STOPPED"
#define DCLOUD_VM_STATE_FINISH			"FINISH"

HashTable<HashKey, DCloudJob *> DCloudJob::JobsByInstanceId( hashFunction );

void DCloudJobInit()
{ }

void DCloudJobReconfig()
{
	// Tell all the resource objects to deal with their new config values
	DCloudResource *next_resource;

	DCloudResource::ResourcesByName.startIterations();

	while ( DCloudResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

bool DCloudJobAdMatch( const ClassAd *job_ad )
{
	int universe;
	bool ret=false;
	MyString resource;

	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) && (strncasecmp( resource.Value(), "deltacloud", 6 ) == 0 ) )
	{
		ret=true;
	}
	return ret;
}

BaseJob* DCloudJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new DCloudJob( jobad );
}

int DCloudJob::gahpCallTimeout = 600;
int DCloudJob::submitInterval = 300;
int DCloudJob::maxConnectFailures = 3;
int DCloudJob::funcRetryInterval = 15;
int DCloudJob::pendingWaitTime = 15;
int DCloudJob::maxRetryTimes = 3;

DCloudJob::DCloudJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[16385]; // user data can be 16K, this is 16K+1
	std::string error_string;
	char *gahp_path = NULL;

	m_serviceUrl = NULL;
	m_instanceId = NULL;
	m_instanceName = NULL;
	m_imageId = NULL;
	m_realmId = NULL;
	m_hwpId = NULL;
	m_hwpMemory = NULL;
	m_hwpCpu = NULL;
	m_hwpStorage = NULL;
	m_username = NULL;
	m_password = NULL;
	m_keyname = NULL;
	m_userdata = NULL;

	remoteJobState = "";
	gmState = GM_INIT;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	probeErrorTime = 0;
	lastProbeTime = 0;
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	myResource = NULL;
	gahp = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD && jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	gahp_path = param( "DELTACLOUD_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "DELTACLOUD_GAHP not defined";
		goto error_exit;
	}

	gahp = new GahpClient( DCLOUD_RESOURCE_NAME, gahp_path );
	free(gahp_path);
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff, sizeof(buff) );
	if ( buff[0] ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "deltacloud" ) ) {
			formatstr( error_string, "%s not of type deltacloud",
					 ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			m_serviceUrl = strdup( token );
		} else {
			formatstr( error_string, "%s missing Deltacloud service URL",
					 ATTR_GRID_RESOURCE );
		}
	} else {
		formatstr( error_string, "%s is not set in the job ad",
				 ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	if ( !jobAd->LookupString( ATTR_DELTACLOUD_USERNAME, &m_username ) ) {
		formatstr( error_string, "%s is not set in the job ad",
				 ATTR_DELTACLOUD_USERNAME );
		goto error_exit;
	}

	if ( !jobAd->LookupString( ATTR_DELTACLOUD_PASSWORD_FILE, &m_password ) ) {
		formatstr( error_string, "%s is not set in the job ad",
				 ATTR_DELTACLOUD_PASSWORD_FILE );
		goto error_exit;
	}
	
	// inclusion of instance named items
	jobAd->LookupString( ATTR_DELTACLOUD_INSTANCE_NAME, &m_instanceName );

	// only fail if no imageid && no 
	if ( !jobAd->LookupString( ATTR_DELTACLOUD_IMAGE_ID, &m_imageId ) && m_instanceName == NULL) {
		formatstr( error_string, "%s is not set in the job ad",
				 ATTR_DELTACLOUD_IMAGE_ID );
		goto error_exit;
	}

	jobAd->LookupString( ATTR_DELTACLOUD_REALM_ID, &m_realmId );
	jobAd->LookupString( ATTR_DELTACLOUD_HARDWARE_PROFILE, &m_hwpId );
	jobAd->LookupString( ATTR_DELTACLOUD_HARDWARE_PROFILE_CPU, &m_hwpCpu );
	jobAd->LookupString( ATTR_DELTACLOUD_HARDWARE_PROFILE_MEMORY, &m_hwpMemory );
	jobAd->LookupString( ATTR_DELTACLOUD_HARDWARE_PROFILE_STORAGE, &m_hwpStorage );
	jobAd->LookupString( ATTR_DELTACLOUD_KEYNAME, &m_keyname );
	jobAd->LookupString( ATTR_DELTACLOUD_USER_DATA, &m_userdata );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff, sizeof(buff) );
	if ( buff[0] ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "deltacloud" ) ) {
			formatstr( error_string, "%s not of type deltacloud",
					 ATTR_GRID_JOB_ID );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			SetInstanceName( token );
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			SetInstanceId( token );
		}
	}

	if ( !jobAd->LookupBool( ATTR_DELTACLOUD_NEEDS_START, m_needstart ) ) {
		m_needstart = false;
	}

	myResource = DCloudResource::FindOrCreateResource( m_serviceUrl, m_username, m_password );
	myResource->RegisterJob( this );
	if ( m_instanceId ) {
		myResource->AlreadySubmitted( this );
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.c_str() );
	}

	return;
}

DCloudJob::~DCloudJob()
{
	if ( myResource ) myResource->UnregisterJob( this );

	if ( gahp != NULL ) delete gahp;

	if ( m_instanceId ) {
		MyString hashname;
		hashname.formatstr( "%s#%s", m_serviceUrl, m_instanceId );
		JobsByInstanceId.insert( HashKey( hashname.Value() ), this );
	}

	free( m_serviceUrl );
	free( m_instanceId );
	free( m_instanceName );
	free( m_imageId );
	free( m_realmId );
	free( m_hwpId );
	free( m_hwpCpu );
	free( m_hwpMemory );
	free( m_hwpStorage );
	free( m_username );
	free( m_password );
	free( m_keyname );
	free( m_userdata );
}

void DCloudJob::Reconfig()
{
	BaseJob::Reconfig();
}

void DCloudJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

	dprintf( D_ALWAYS, "(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			 procID.cluster,procID.proc,GMStateNames[gmState],condorState);

	if ( gahp ) 
	{
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) 
		{
			gahp->setMode( GahpClient::results_only );
		} else 
		{
			gahp->setMode( GahpClient::normal );
		}
	}

	do {

		reevaluate_state = false;
		old_gm_state = gmState;

		switch ( gmState )
		{
			case GM_INIT:
				// This is the state all jobs start in when the DCloudJob
				// object is first created. Here, we do things that we didn't
				// want to do in the constructor because they could block (the
				// constructor is called while we're connected to the schedd).
				if ( gahp->Startup() == false ) 
				{
					dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n",
							 procID.cluster, procID.proc );
					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
					gmState = GM_HOLD;
					break;
				}

				errorString = "";

				if ( m_instanceName == NULL || strcmp(m_instanceName, "NULL") == 0 ) 
				{
					gmState = GM_CLEAR_REQUEST;
				} 
				else 
				{
					gmState = GM_CHECK_VM;
				}

				break;

			case GM_CHECK_VM: 
			{
				char *instance_id = NULL;
				
				// check if the VM has been started successfully
				rc = gahp->dcloud_find( m_serviceUrl,
							m_username,
							m_password,
							m_instanceName,
							&instance_id );
				
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) 
				{
					break;
				}

				if (rc == 0 ) 
				{
					if ( instance_id && *instance_id ) 
					{
						SetInstanceId( instance_id );
						free( instance_id );
						myResource->AlreadySubmitted( this );
						probeNow = true;
						gmState = GM_CHECK_AUTOSTART; //GM_SAVE_INSTANCE_ID;
					}
					else if ( (condorState == REMOVED) || (condorState == HELD) )  {
						gmState = GM_DELETE;
					}
					else 
					{
						gmState = GM_CREATE_VM;
					}
				} 
				else 
				{
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS,"(%d.%d) VM check failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.Value() );
					gmState = GM_HOLD;
				}

			} break;

			case GM_SAVE_INSTANCE_NAME:
				// Create a unique name for this job and save it in GridJobId
				// in the schedd. This will be our handle to the job until we
				// get the instance id at the end of the submission process.

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
					break;
				}

				// Once RequestSubmit() is called at least once, you must
				// CancelSubmit() once the submission process is complete or
				// aborted.
				if ( myResource->RequestSubmit( this ) == false ) {
					break;
				}

				if ( m_instanceName == NULL || strcmp(m_instanceName, "NULL") == 0 ) {
					// start with the assumption that the maximum name length
					// is 1024.  If the provider doesn't specify otherwise,
					// this is long enough
					int max_length = 1024;

					rc = gahp->dcloud_get_max_name_length( m_serviceUrl,
										m_username,
										m_password,
										&max_length);

					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING )
						break;

					if ( rc != 0 ) {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) Maximum name length failed: %s\n",
								procID.cluster, procID.proc,
								errorString.Value() );
						gmState = GM_HOLD;
						break;
					}

					SetInstanceName( build_instance_name( max_length ).Value() );
				}
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					// The instance name still needs to be saved to the schedd
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_CREATE_VM;
				break;

			case GM_CREATE_VM: 
			{
				// After a submit, wait at least submitInterval before trying
				// another one.
				if ( now < lastSubmitAttempt + submitInterval ) 
				{
					unsigned int delay = 0;

					if ( (condorState == REMOVED) || (condorState == HELD) ) {
						gmState = GM_STOPPED;
						break;
					}

					delay = (lastSubmitAttempt + submitInterval) - now;
					daemonCore->Reset_Timer( evaluateStateTid, delay );
					break;
				}

				// Once RequestSubmit() is called at least once, you must
				// CancelSubmit() once you're done with the request call
				if ( myResource->RequestSubmit( this ) == false ) 
				{
					// If we haven't started the START_VM call yet, we can
					// abort the submission here for held and removed jobs.
					if ( (condorState == REMOVED) || (condorState == HELD) ) {

						myResource->CancelSubmit( this );
						gmState = GM_STOPPED;
					}
					break;
				}

				StringList instance_attrs;

				rc = gahp->dcloud_submit( m_serviceUrl,
										  m_username,
										  m_password,
										  m_imageId,
										  m_instanceName,
										  m_realmId,
										  m_hwpId,
										  m_hwpMemory,
										  m_hwpCpu,
										  m_hwpStorage,
										  m_keyname,
										  m_userdata,
										  instance_attrs );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				lastSubmitAttempt = time(NULL);

				if ( rc != 0 ) {
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS,"(%d.%d) job submit failed: %s\n",
							 procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
					break;
				}

				WriteGridSubmitEventToUserLog(jobAd);
				ProcessInstanceAttrs( instance_attrs );
				ASSERT( m_instanceId );
				
				gmState = GM_CHECK_AUTOSTART;

				break;
			}

		case GM_CHECK_AUTOSTART: {
			bool autostart;

			rc = gahp->dcloud_start_auto( m_serviceUrl,
										  m_username,
										  m_password,
										  &autostart );

			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING )
				break;

			if ( rc != 0 ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS,"(%d.%d) Finding autostart failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_HOLD;
				break;
			}

			m_needstart = !autostart;
			jobAd->Assign( ATTR_DELTACLOUD_NEEDS_START, m_needstart );

			gmState = GM_SAVE_INSTANCE_ID;

			break;
		}

		case GM_START_VM:
				rc = gahp->dcloud_action( m_serviceUrl,
										  m_username,
										  m_password,
										  m_instanceId,
										  "start" );
				jobAd->Assign( ATTR_DELTACLOUD_NEEDS_START, false );
				m_needstart = false;

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				if ( rc == 0 ) {
					gmState = GM_PROBE_JOB;
				} else {
					// What to do about a failed start?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job start failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.Value() );
					gmState = GM_HOLD;
				}
				break;

			case GM_SAVE_INSTANCE_ID:

				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					// Wait for the instance id to be saved to the schedd
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_SUBMITTED;

				break;

			case GM_SUBMITTED:
				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} else if ( remoteJobState == DCLOUD_VM_STATE_STOPPED ) {
					if ( m_needstart ) {
						gmState = GM_START_VM;
					} else {
						gmState = GM_DONE_SAVE;
					}
				} else if ( remoteJobState == DCLOUD_VM_STATE_FINISH ) {
					gmState = GM_DONE_SAVE;
				} else if ( probeNow ) {
					gmState = GM_PROBE_JOB;
				}

				break;

			case GM_DONE_SAVE:

				if ( condorState != HELD && condorState != REMOVED ) {
					JobTerminated();
					if ( condorState == COMPLETED ) {
						jobAd->GetDirtyFlag( ATTR_JOB_STATUS, &attr_exists, &attr_dirty );
						if ( attr_exists && attr_dirty ) {
							requestScheddUpdate( this, true );
							break;
						}
					}
				}

				myResource->CancelSubmit( this );
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_STOPPED;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD
					// first).
					if ( m_instanceId != NULL ) {
						SetInstanceId( NULL );
						SetInstanceName( NULL );
					}
					gmState = GM_CLEAR_REQUEST;
				}

				break;


			case GM_CLEAR_REQUEST:

				// Remove all knowledge of any previous or present job
				// submission, in both the gridmanager and the schedd.  If we
				// are doing a rematch, we are simply waiting around for the
				// schedd to be updated and subsequently this dcloud job
				// object to be destroyed.  So there is nothing to do.
				if ( wantRematch ) {
					break;
				}

				// For now, put problem jobs on hold instead of forgetting
				// about current submission and trying again.
				// TODO: Let our action here be dictated by the user preference
				// expressed in the job ad.
				if ( m_instanceId != NULL && condorState != REMOVED
					&& wantResubmit == 0 && doResubmit == 0 ) {
					gmState = GM_HOLD;
					break;
				}

				// Only allow a rematch *if* we are also going to perform a
				// resubmit
				if ( wantResubmit || doResubmit ) {
					jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
				}

				if ( wantResubmit ) {
					wantResubmit = 0;
					dprintf( D_ALWAYS, "(%d.%d) Resubmitting to Deltacloud because %s==TRUE\n",
							 procID.cluster, procID.proc,
							 ATTR_GLOBUS_RESUBMIT_CHECK );
				}

				if ( doResubmit ) {
					doResubmit = 0;
					dprintf( D_ALWAYS, "(%d.%d) Resubmitting to Deltacloud (last submit failed)\n",
							 procID.cluster, procID.proc );
				}

				errorString = "";
				myResource->CancelSubmit( this );
				if ( m_instanceId != NULL ) {
					SetInstanceId( NULL );
					SetInstanceName( NULL );
				}

				JobIdle();

				if ( m_needstart ) {
					jobAd->Assign( ATTR_DELTACLOUD_NEEDS_START, false );
					m_needstart = false;
				}

				if ( submitLogged ) {
					JobEvicted();
					if ( !evictLogged ) {
						WriteEvictEventToUserLog( jobAd );
						evictLogged = true;
					}
				}

				if ( wantRematch ) {
					dprintf( D_ALWAYS, "(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
							 procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

					// Set ad attributes so the schedd finds a new match.
					int dummy;
					if ( jobAd->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
						jobAd->Assign( ATTR_JOB_MATCHED, false );
						jobAd->Assign( ATTR_CURRENT_HOSTS, 0 );
					}

					// If we are rematching, we need to forget about this job
					// because we want to pull a fresh new job ad, with a
					// fresh new match, from the all-singing schedd.
					gmState = GM_DELETE;
					break;
				}

				// If there are no updates to be done when we first enter this
				// state, requestScheddUpdate will return done immediately and
				// not waste time with a needless connection to the schedd.
				// If updates need to be made, they won't show up in
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

				if ( remoteJobState != "" ) {
					remoteJobState = "";
					SetRemoteJobStatus( NULL );
				}

				submitLogged = false;
				executeLogged = false;
				submitFailedLogged = false;
				terminateLogged = false;
				abortLogged = false;
				evictLogged = false;
				gmState = GM_SAVE_INSTANCE_NAME;

				break;

			case GM_PROBE_JOB: {

				probeNow = false;

				if ( lastProbeTime + 30 > now ) {
					// Wait before trying another probe
					unsigned int delay = ( lastProbeTime + 30 ) - now;
					daemonCore->Reset_Timer( evaluateStateTid, delay );
					break;
				}

				StringList attrs;
				rc = gahp->dcloud_info( m_serviceUrl,
										m_username,
										m_password,
										m_instanceId,
										attrs );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				lastProbeTime = now;
				// processing error code received
				if ( rc != 0 ) {
					// What to do about failure?  We want to wait a little
					// bit before declaring a complete failure and going to
					// HELD as some providers don't immediately show the
					// instance in the list after creating it.
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s\n",
							 procID.cluster, procID.proc, errorString.Value() );

					if ( probeErrorTime == 0 ) {
						probeErrorTime = time(NULL);
						dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s: Delaying HELD state, waiting for another probe..\n",
								 procID.cluster, procID.proc,
								 errorString.Value());
					} else {
						int retry_time;

						// probeErrorTime was set previously, check how long it
						// has been and see if we have to move to HELD.
						if ( jobAd->LookupInteger( ATTR_DELTACLOUD_RETRY_TIMEOUT, retry_time ) == FALSE ) {
							// Set default retry to 90s
							retry_time = 90;
						}
						if ( now - probeErrorTime > retry_time ) {
							dprintf( D_ALWAYS, "(%d.%d): Moving job to HELD\n",
									 procID.cluster, procID.proc );
							gmState = GM_HOLD;
						} else {
							dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s: HELD state delayed for %d of %d seconds.\n",
									 procID.cluster, procID.proc,
									 errorString.Value(),
									 (int) (now - probeErrorTime), retry_time );
							daemonCore->Reset_Timer( evaluateStateTid, 30 );
						}
					}
					break;
				} else {
					probeErrorTime = 0;
				}

				ProcessInstanceAttrs( attrs );

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				}
				else {
					gmState = GM_SUBMITTED;
				}

				break;
			}

			case GM_CANCEL:

				if (remoteJobState == DCLOUD_VM_STATE_RUNNING )
				{
				    
				    
				  rc = gahp->dcloud_action( m_serviceUrl,
										    m_username,
										    m_password,
										    m_instanceId,
										    "stop" );
				  if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					  rc == GAHPCLIENT_COMMAND_PENDING ) {
					  break;
				  }

				  if ( rc == 0 ) {
					  gmState = GM_PROBE_JOB;
				  } else {
					  // What to do about a failed cancel?
					  errorString = gahp->getErrorString();
					  dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n",
							  procID.cluster, procID.proc,
							  errorString.Value() );
					  gmState = GM_HOLD;
				  }
				}
				else if (remoteJobState == DCLOUD_VM_STATE_PENDING)
				{
				    gmState = GM_PROBE_JOB;
				}
				else 
				{
				    gmState = GM_STOPPED;
				}
				
				break;

			case GM_HOLD:
				// Put the job on hold in the schedd.  If the condor state is
				// already HELD, then someone already HELD it, so don't update
				// anything else.
				if ( condorState != HELD ) {

					// Set the hold reason as best we can
					// TODO: set the hold reason in a more robust way.
					char holdReason[1024];
					holdReason[0] = '\0';
					holdReason[sizeof(holdReason)-1] = '\0';
					jobAd->LookupString( ATTR_HOLD_REASON, holdReason, sizeof(holdReason) );
					if ( holdReason[0] == '\0' && errorString != "" ) {
						strncpy( holdReason, errorString.Value(), sizeof(holdReason) - 1 );
					} else if ( holdReason[0] == '\0' ) {
						strncpy( holdReason, "Unspecified gridmanager error", sizeof(holdReason) - 1 );
					}

					JobHeld( holdReason );
				}

				gmState = GM_DELETE;

				break;


			case GM_STOPPED:

				/*
				if ( remoteJobState != DCLOUD_VM_STATE_FINISH ) {
					rc = gahp->dcloud_action( m_serviceUrl,
											  m_username,
											  m_password,
											  m_instanceId,
											  "destroy" );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					// We could check for a failed destroy here, but on some
					// providers failure is normal as the instance is
					// destroyed when it is stopped.  Instead, we just say we
					// are done.  If the destroy failed for some other reason,
					// I'm not sure what we can really do about it.
					// Note that we do catch the 'pending' and not submitted
					// case above so I think that should cover network
					// issues etc.
					StatusUpdate( DCLOUD_VM_STATE_FINISH );
				}*/
				StatusUpdate( DCLOUD_VM_STATE_FINISH );
				
				myResource->CancelSubmit( this );
				SetInstanceId( NULL );
				SetInstanceName( NULL );

				if ( (condorState == REMOVED) || (condorState == COMPLETED) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}

				break;

			case GM_DELETE:

				// We are done with the job. Propagate any remaining updates
				// to the schedd, then delete this object.
				DoneWithJob();
				// This object will be deleted when the update occurs
				break;


			default:
				EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster, procID.proc, gmState );
				break;
		} // end of switch_case

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf( D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					 procID.cluster, procID.proc, GMStateNames[old_gm_state], GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}

	} // end of do_while
	while ( reevaluate_state );
}

BaseResource* DCloudJob::GetResource()
{
	return (BaseResource *)myResource;
}

void DCloudJob::SetInstanceName( const char *instance_name )
{
	free( m_instanceName );
	if ( instance_name ) {
		m_instanceName = strdup( instance_name );
	} else {
		m_instanceName = NULL;
	}
	SetRemoteJobId( m_instanceName, m_instanceId );
}

void DCloudJob::SetInstanceId( const char *instance_id )
{
	MyString hashname;
	if ( m_instanceId ) {
		hashname.formatstr( "%s#%s", m_serviceUrl, m_instanceId );
		JobsByInstanceId.remove( HashKey( hashname.Value() ) );
		free( m_instanceId );
	}
	if ( instance_id ) {
		m_instanceId = strdup( instance_id );
		hashname.formatstr( "%s#%s", m_serviceUrl, m_instanceId );
		JobsByInstanceId.insert( HashKey( hashname.Value() ), this );
	} else {
		m_instanceId = NULL;
	}
	SetRemoteJobId( m_instanceName, m_instanceId );
}

// SetRemoteJobId() is used to set the value of global variable "remoteJobID"
void DCloudJob::SetRemoteJobId( const char *instance_name, const char *instance_id )
{
	MyString full_job_id;
	if ( instance_name && instance_name[0] ) {
		full_job_id.formatstr( "deltacloud %s", instance_name );
		if ( instance_id && instance_id[0] ) {
			full_job_id.formatstr_cat( " %s", instance_id );
		}
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

void DCloudJob::ProcessInstanceAttrs( StringList &attrs )
{
	// TODO
	// NOTE: If attrlist is empty, treat as completed job
	// Call StatusUpdate() with status from list
	const char *line;
	// We use a new_status flag here because we want to parse everything
	// and get all the info into the classad before updating the status
	// and producing an event log entry.
	const char *new_status = NULL;

	attrs.rewind();
	while ( (line = attrs.next()) ) {
		if ( strncmp( line, "state=", 6 ) == 0 ) {
			new_status = &line[6];
		} else if ( strncmp( line, "id=", 3 ) == 0 ) {
			SetInstanceId( &line[3] );
			jobAd->Assign(ATTR_DELTACLOUD_PROVIDER_ID, &line[3]);
		} else if ( strncmp( line, "public_addresses=", 17 ) == 0 ) {
			jobAd->Assign(ATTR_DELTACLOUD_PUBLIC_NETWORK_ADDRESSES, &line[17]);
		} else if ( strncmp( line, "private_addresses=", 18 ) == 0 ) {
			jobAd->Assign(ATTR_DELTACLOUD_PRIVATE_NETWORK_ADDRESSES, &line[18]);
		} else if ( strncmp( line, "actions=", 8) == 0 ) {
			jobAd->Assign(ATTR_DELTACLOUD_AVAILABLE_ACTIONS, &line[8]);
		}
	}

	if ( new_status ) {
		// Now that we have everything in the classad, do the job
		// status update.
		StatusUpdate( new_status );
		if ( strcasecmp( new_status, DCLOUD_VM_STATE_RUNNING ) == 0 ) {
			JobRunning();
		}
	}

	if ( attrs.isEmpty() ) {
		StatusUpdate( DCLOUD_VM_STATE_FINISH );
	}
}

void DCloudJob::StatusUpdate( const char *new_status )
{
	if ( new_status == NULL ) {
		// TODO May need to prevent this firing if job was just
		//   submitted, so it's not in the query
		probeNow = true;
		SetEvaluateState();
	} else if ( SetRemoteJobStatus( new_status ) ) {
		remoteJobState = new_status;
		probeNow = true;
		SetEvaluateState();
	}
}

MyString DCloudJob::build_instance_name( int max_length )
{
	// Build a name that will be unique to this job.
	// We use a generated UUID

	uuid_t uuid;
	char *instance_name;

	uuid_generate(uuid);

	// A UUID parses out to 36 characters plus a \0, so allocate 37 here
	instance_name = (char *)malloc(37);

	uuid_unparse(uuid, instance_name);

	if (max_length < 36) {
		// if the maximum length we can generate is less than 36
		// characters, we have to truncate the UUID.  There's not much
		// we can do to improve the uniqueness, so just lop off
		// some bytes
		instance_name[max_length] = '\0';
	}

	return instance_name;
}
