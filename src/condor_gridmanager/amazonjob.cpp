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
#include "amazonjob.h"
#include "condor_config.h"
  
#define GM_INIT							0
#define GM_UNSUBMITTED					1
#define GM_START_VM						2
#define GM_SAVE_INSTANCE_ID				3
#define GM_SUBMITTED					4
#define GM_DONE_SAVE					5
#define GM_CANCEL						6
#define GM_FAILED						7
#define GM_DELETE						8
#define GM_CLEAR_REQUEST				9
#define GM_HOLD							10
#define GM_PROBE_JOB					11
#define GM_START						12
#define GM_CREATE_KEYPAIR				13
#define GM_DESTROY_KEYPAIR				14
#define GM_DESTROY_KEYPAIR_SUBMIT		15
#define GM_SAVE_KEYPAIR_NAME			16
#define GM_CHECK_VM						17

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_START_VM",
	"GM_SAVE_INSTANCE_ID",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROBE_JOB",
	"GM_START",
	"GM_CREATE_KEYPAIR",
	"GM_DESTROY_KEYPAIR",
	"GM_DESTROY_KEYPAIR_SUBMIT",
	"GM_SAVE_KEYPAIR_NAME",
	"GM_CHECK_VM",
};

#define AMAZON_VM_STATE_RUNNING			"running"
#define AMAZON_VM_STATE_PENDING			"pending"
#define AMAZON_VM_STATE_SHUTTINGDOWN	"shutting-down"
#define AMAZON_VM_STATE_TERMINATED		"terminated"

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_strcmp _stricmp
#	define file_contains contains_anycase
#else
#	define file_strcmp strcmp
#	define file_contains contains
#endif

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void AmazonJobInit()
{
}


void AmazonJobReconfig()
{
	// change interval time for 5 minute
	int tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 60 * 5 ); 
	AmazonJob::setProbeInterval( tmp_int );
		
	// Tell all the resource objects to deal with their new config values
	AmazonResource *next_resource;

	AmazonResource::ResourcesByName.startIterations();

	while ( AmazonResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}	
}


bool AmazonJobAdMatch( const ClassAd *job_ad )
{
	int universe;
	MyString resource;
	
	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) && (strncasecmp( resource.Value(), "amazon", 6 ) == 0) ) 
	{
		return true;
	}
	return false;
}


BaseJob* AmazonJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new AmazonJob( jobad );
}

int AmazonJob::gahpCallTimeout = 600;
int AmazonJob::probeInterval = 300;
int AmazonJob::submitInterval = 300;
int AmazonJob::maxConnectFailures = 3;
int AmazonJob::funcRetryInterval = 15;
int AmazonJob::pendingWaitTime = 15;
int AmazonJob::maxRetryTimes = 3;

AmazonJob::AmazonJob( ClassAd *classad )
	: BaseJob( classad )
{
dprintf( D_ALWAYS, "================================>  AmazonJob::AmazonJob 1 \n");
	char buff[16385]; // user data can be 16K, this is 16K+1
	MyString error_string = "";
	char *gahp_path = NULL;
	char *gahp_log = NULL;
	int gahp_worker_cnt = 0;
	char *gahp_debug = NULL;
	ArgList args;
	
	remoteJobId = NULL;
	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	myResource = NULL;
	gahp = NULL;
	
	// check the public_key_file
	buff[0] = '\0';
	jobAd->LookupString( ATTR_AMAZON_PUBLIC_KEY, buff );
	m_public_key_file = strdup(buff);
	
	if ( strlen(m_public_key_file) == 0 ) {
		error_string = "Public key file not defined";
		goto error_exit;
	}

	// check the private_key_file
	buff[0] = '\0';
	jobAd->LookupString( ATTR_AMAZON_PRIVATE_KEY, buff );
	m_private_key_file = strdup(buff);
	
	if ( strlen(m_private_key_file) == 0 ) {
		error_string = "Private key file not defined";
		goto error_exit;
	}

		// XXX: Buffer Overflow if the user_data is > 16K? This code
		// should be unprivileged.

		// XXX: It is bad to assume the buff is initialized to 0s,
		// always use memset? All this code should be changed to get
		// at the attribute in a better way.

	memset(buff, 0, 16385);
	m_user_data = NULL;
	m_user_data_file = NULL;	
	
	// if user assigns both user_data and user_data_file, only user_data_file
	// will be used.
	if ( jobAd->LookupString( ATTR_AMAZON_USER_DATA_FILE, buff ) ) {
		m_user_data_file = strdup(buff);	
	} else {
		if ( jobAd->LookupString( ATTR_AMAZON_USER_DATA, buff ) ) {
			m_user_data = strdup(buff);
		}
	}
	
	// get VM instance type
	memset(buff, 0, 16385);
	m_instance_type = NULL; // if clients don't assign this value in condor submit file,
							// we should set the default value to NULL and gahp_server
							// will start VM in Amazon using m1.small mode.
	if ( jobAd->LookupString( ATTR_AMAZON_INSTANCE_TYPE, buff ) ) {
		m_instance_type = strdup(buff);	
	}
	
	m_group_names = NULL;
	m_vm_check_times = 0;
	m_keypair_check_times = 0;

	// for SSH keypair output file
	{
	char* buffer = NULL;
	
	// Notice:
	// 	we can have two kinds of SSH keypair output file names or the place where the 
	// output private file should be written to, 
	// 	1. the name assigned by client in the condor submit file with attribute "AmazonKeyPairFile"
	// 	2. if there is no attribute "AmazonKeyPairFile" in the condor submit file, we 
	// 	   should discard this private file by writing to NULL_FILE
	if ( jobAd->LookupString( ATTR_AMAZON_KEY_PAIR_FILE, &buffer ) ) {
		// clinet define the location where this SSH keypair file will be written to
		m_key_pair_file = buffer;
	} else {
		// If client doesn't assign keypair output file name, we should discard it by 
		// writing this private file to /dev/null
		m_key_pair_file = NULL_FILE;
	}
	free (buffer);
	}

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD && jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	gahp_path = param( "AMAZON_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "AMAZON_GAHP not defined";
		goto error_exit;
	}

	snprintf( buff, sizeof(buff), AMAZON_RESOURCE_NAME ); // for client's ID

	gahp_log = param( "AMAZON_GAHP_LOG" );
	if ( gahp_log == NULL ) {
		dprintf(D_ALWAYS, "Warning: No AMAZON_GAHP_LOG defined\n");
	} else {
		args.AppendArg("-f");
		args.AppendArg(gahp_log);
		free(gahp_log);
	}

	args.AppendArg("-w");
	gahp_worker_cnt = param_integer( "AMAZON_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-m");
	gahp_worker_cnt = param_integer( "AMAZON_GAHP_WORKER_MAX_NUM", 5 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-d");
	gahp_debug = param( "AMAZON_GAHP_DEBUG" );
	if (!gahp_debug) {
		args.AppendArg("D_ALWAYS");
	} else {
		args.AppendArg(gahp_debug);
		free(gahp_debug);
	}

	gahp = new GahpClient( buff, gahp_path, &args );
	free(gahp_path);
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource = AmazonResource::FindOrCreateResource( AMAZON_RESOURCE_NAME, m_public_key_file, m_private_key_file );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "amazon" ) ) {
			error_string.sprintf( "%s not of type amazon",
								  ATTR_GRID_JOB_ID );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			m_key_pair = token;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			remoteJobId = strdup( token );
		}
	}
	
	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

	// JEF: Increment a GMSession attribute for use in letting the job
	// ad crash the gridmanager on request
	if ( jobAd->Lookup( "CrashGM" ) != NULL ) {
		int session = 0;
		jobAd->LookupInteger( "GMSession", session );
		session++;
		jobAd->Assign( "GMSession", session );
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.IsEmpty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.Value() );
	}
	
	return;
}

AmazonJob::~AmazonJob()
{
	if ( myResource ) myResource->UnregisterJob( this );
	if ( remoteJobId ) free( remoteJobId );
	
	if ( gahp != NULL ) delete gahp;
	free (m_public_key_file);
	free (m_private_key_file);
	free (m_user_data);
	if (m_group_names != NULL) delete m_group_names;
	free(m_user_data_file);
	free(m_instance_type);
}


void AmazonJob::Reconfig()
{
	BaseJob::Reconfig();
}


int AmazonJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;

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
		
		char *gahp_error_code = NULL;

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
				// This is the state all jobs start in when the AmazonJob object
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
					dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n", procID.cluster, procID.proc );
					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
					gmState = GM_HOLD;
					break;
				}
				
				gmState = GM_START;
				break;
				
			case GM_START:

				errorString = "";
				
				if ( m_key_pair == "" ) {
					gmState = GM_CLEAR_REQUEST;
				} else if ( remoteJobId == NULL ) {
					gmState = GM_CHECK_VM;
				} else {
					submitLogged = true;
					if ( condorState == RUNNING || condorState == COMPLETED ) {
						executeLogged = true;
					}
					gmState = GM_SUBMITTED;
				}
				
				break;
				
			case GM_UNSUBMITTED:

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_SAVE_KEYPAIR_NAME;
				}
				
				break;
				
			case GM_SAVE_KEYPAIR_NAME:
				// Create a unique name for the ssh keypair for this job
				// in EC2 and save it in GridJobId in the schedd. This
				// will be our handle to the job until we get the instance
				// id at the end of the submission process.

				if ( (condorState == REMOVED) ||
					 (condorState == HELD) ) {

					gmState = GM_DELETE;
				}

				// Once RequestSubmit() is called at least once, you must
				// CancelSubmit() once the submission process is complete
				// or aborted.
				if ( myResource->RequestSubmit( this ) == false ) {
					break;
				}

				if ( m_key_pair == "" ) {
					SetKeypairId( build_keypair().Value() );
				}
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
						// The keypair name still needs to be saved to
						//the schedd
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_CREATE_KEYPAIR;
				break;

			case GM_START_VM:
				
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
							gmState = GM_DESTROY_KEYPAIR;
						}
						break;
					}

					// construct input parameters for amazon_vm_start()
					char* instance_id = NULL;
					
					// For a given Amazon Job, in its life cycle, the attributes will not change 					
					
					
					m_ami_id = build_ami_id();
					if ( m_key_pair == "" ) {
						m_key_pair = build_keypair();
					}
					if ( m_group_names == NULL )	m_group_names = build_groupnames();
					
					// amazon_vm_start() will check the input arguments
					rc = gahp->amazon_vm_start( m_public_key_file, m_private_key_file, 
												m_ami_id.Value(), m_key_pair.Value(), 
												m_user_data, m_user_data_file, m_instance_type, 
												*m_group_names, instance_id, gahp_error_code);
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					lastSubmitAttempt = time(NULL);

					if ( rc != 0 && strcmp( gahp_error_code, "NEED_CHECK_VM_START" ) == 0 ) {
						// get an error code from gahp server said that we should check if 
						// the VM has been started successfully in EC2
						
						// Maxmium retry times is 3, if exceeds this limitation, we fall through
						if ( m_vm_check_times++ < maxRetryTimes ) {
							gmState = GM_CHECK_VM;
						}
						break;
					}

					if ( rc == 0 ) {
						
						ASSERT( instance_id != NULL );
						SetInstanceId( instance_id );
						WriteGridSubmitEventToUserLog(jobAd);
						free( instance_id );
											
						gmState = GM_SAVE_INSTANCE_ID;
						
					} else if ( strcmp( gahp_error_code, "InstanceLimitExceeded" ) == 0 ) {
						// meet the resource limitation (maximum 20 instances)
						// should retry this command later
						myResource->CancelSubmit( this );
						daemonCore->Reset_Timer( evaluateStateTid, submitInterval );
					 } else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s: %s\n",
								procID.cluster, procID.proc, gahp_error_code,
								errorString.Value() );
						gmState = GM_HOLD;
					}
					
				} else {
					if ( (condorState == REMOVED) || (condorState == HELD) ) {
						gmState = GM_DESTROY_KEYPAIR;
						break;
					}

					unsigned int delay = 0;
					if ( (lastSubmitAttempt + submitInterval) > now ) {
						delay = (lastSubmitAttempt + submitInterval) - now;
					}				
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}

				break;
				
			
			case GM_CHECK_VM:
				
				{
					
				// check if the VM has been started successfully
				StringList returnStatus;
							
				rc = gahp->amazon_vm_vm_keypair_all(m_public_key_file, m_private_key_file,
												    returnStatus, gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
				}

				if (rc == 0) {

					// now we should check, corresponding to a given SSH keypair, in EC2, is there
					// existing any running VM instances? If these are some ones, we just return the
					// first one we found.
					bool is_running = false;
					char* instance_id = NULL;
					char* keypair_name = NULL;
								
					int size = returnStatus.number();
					returnStatus.rewind();
								
					for (int i=0; i<size/2; i++) {
									
						instance_id = returnStatus.next();
						keypair_name = returnStatus.next();

						if (strcmp(m_key_pair.Value(), keypair_name) == 0) {
							is_running = true;
							break;
						}
					}
								
					if ( is_running ) {

						// there is a running VM instance corresponding to the given SSH keypair
						gmState = GM_SAVE_INSTANCE_ID;
						// save the instance ID which will be used when delete VM instance
						SetInstanceId( instance_id );
									
					} else {
						// we shoudl re-start the VM again with the corresponding SSH keypair
						// TODO If we know we successfully created the
						//   keypair during this instance, we can go
						//   to GM_START_VM instead.
						myResource->CancelSubmit( this );
						gmState = GM_DESTROY_KEYPAIR_SUBMIT;
					}
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) VM check failed: %s: %s\n",
							procID.cluster, procID.proc, gahp_error_code,
							errorString.Value() );
					gmState = GM_HOLD;
				}

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

				if ( remoteJobState == AMAZON_VM_STATE_TERMINATED ) {
					gmState = GM_DONE_SAVE;
				} 

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} 
				else {
					if ( lastProbeTime < enteredCurrentGmState ) {
						lastProbeTime = enteredCurrentGmState;
					}
					
					// if current state isn't "running", we should check its state
					// every "funcRetryInterval" seconds. Otherwise the interval should
					// be "probeInterval" seconds.  
					int interval = probeInterval;
					if ( remoteJobState != AMAZON_VM_STATE_RUNNING ) {
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
					gmState = GM_DESTROY_KEYPAIR;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					if ( remoteJobId != NULL ) {
						SetInstanceId( NULL );
						SetKeypairId( NULL );
					}
					gmState = GM_CLEAR_REQUEST;
				}
			
				break;
						
				
			case GM_CLEAR_REQUEST:

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
				if ( remoteJobId != NULL && condorState != REMOVED 
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
				if ( remoteJobId != NULL ) {
					SetInstanceId( NULL );
					SetKeypairId( NULL );
				}

				JobIdle();

				if ( submitLogged ) {
					JobEvicted();
					if ( !evictLogged ) {
						WriteEvictEventToUserLog( jobAd );
						evictLogged = true;
					}
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
				jobAd->ResetExpr();
				if ( jobAd->NextDirtyExpr() ) {
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
				gmState = GM_UNSUBMITTED;

				break;				
				
			case GM_PROBE_JOB:

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} else {
					MyString new_status;
					MyString public_dns;
					StringList returnStatus;

					// need to call amazon_vm_status(), amazon_vm_status() will check input arguments
					// The VM status we need is saved in the second string of the returned status StringList
					rc = gahp->amazon_vm_status(m_public_key_file, m_private_key_file, remoteJobId, returnStatus, gahp_error_code );
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					
					// processing error code received
					if ( rc != 0 ) {
						// What to do about failure?
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s: %s\n",
								 procID.cluster, procID.proc, gahp_error_code,
								 errorString.Value() );
						gmState = GM_HOLD;
						break;
					} else {
						if ( returnStatus.number() == 0 ) {
							// The instance has been purged, act like we
							// got back 'terminated'
							returnStatus.append( remoteJobId );
							returnStatus.append( AMAZON_VM_STATE_TERMINATED );
						}

						// VM Status is the second value in the return string list
						returnStatus.rewind();
						// jump to the value I need
						for (int i=0; i<1; i++) {
							returnStatus.next();
						}
						new_status = returnStatus.next();
						
						// if amazon VM's state is "running" or beyond,
						// change condor job status to Running.
						if ( new_status != remoteJobState &&
							 ( new_status == AMAZON_VM_STATE_RUNNING ||
							   new_status == AMAZON_VM_STATE_SHUTTINGDOWN ||
							   new_status == AMAZON_VM_STATE_TERMINATED ) ) {
							JobRunning();
						}
												
						remoteJobState = new_status;
						SetRemoteJobStatus( new_status.Value() );
										
						
						returnStatus.rewind();
						int size = returnStatus.number();
						// only when status changed to running, can we have the public dns name
						// at this situation, the number of return value is larger than 4
						if (size >=4 ) {
							for (int i=0; i<3; i++) {
								returnStatus.next();							
							}
							public_dns = returnStatus.next();
							SetRemoteVMName( public_dns.Value() );
						}
					}

					lastProbeTime = now;
					gmState = GM_SUBMITTED;
				}

				break;				
				
			case GM_CANCEL:

				// need to call amazon_vm_stop(), it will only return STOP operation is success or failed
				// amazon_vm_stop() will check the input arguments
				rc = gahp->amazon_vm_stop(m_public_key_file, m_private_key_file, remoteJobId, gahp_error_code);
			
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} 
				
				if ( rc == 0 ) {
					gmState = GM_DESTROY_KEYPAIR;
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s: %s\n",
							 procID.cluster, procID.proc, gahp_error_code,
							 errorString.Value() );
					gmState = GM_HOLD;
				}
				
				break;
				

			case GM_CREATE_KEYPAIR:
				{
				// Once RequestSubmit() is called at least once, you must
				// CancelSubmit() once the submission process is complete
				// or aborted.
				if ( myResource->RequestSubmit( this ) == false ) {
					// If we haven't started the CREATE_KEYPAIR call yet,
					// we can abort the submission here for held and
					// removed jobs.
					if ( (condorState == REMOVED) ||
						 (condorState == HELD) ) {

						gmState = GM_DELETE;
					}
					break;
				}

				// now create and register this keypair by using amazon_vm_create_keypair()
				rc = gahp->amazon_vm_create_keypair(m_public_key_file, m_private_key_file, 
													m_key_pair.Value(), m_key_pair_file.Value(), gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ) {
					if ( (condorState == REMOVED) ||
						 (condorState == HELD) ) {
						gmState = GM_DELETE;
					}
					break;
				}

				if (rc == 0) {
					if ( (condorState == REMOVED) ||
						 (condorState == HELD) ) {

						gmState = GM_DESTROY_KEYPAIR;
					} else {
						gmState = GM_START_VM;
					}
				} else {
					if ( strcmp(gahp_error_code, "NEED_CHECK_SSHKEY" ) == 0 ) {
						
						// get an error code from gahp server said that we should check if 
						// the SSH keypair has been registered successfully in EC2

						// Maxmium retry times is 3, if exceeds this limitation, we will go to HOLD state
						if ( m_keypair_check_times++ < maxRetryTimes ) {
							gmState = GM_DESTROY_KEYPAIR_SUBMIT;
							break;
						}
					}

					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job create keypair failed: %s: %s\n",
							procID.cluster, procID.proc, gahp_error_code,
							errorString.Value() );
					gmState = GM_HOLD;
					break;
				}
								
				}				
				
				break;


			case GM_DESTROY_KEYPAIR_SUBMIT:
				{
				// Something went wrong during the submit process and
				// we need to destroy the keypair
				rc = gahp->amazon_vm_destroy_keypair(m_public_key_file, m_private_key_file, m_key_pair.Value(), gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				if (rc == 0) {
					// remove temporary keypair local output file
					if ( !remove_keypair_file(m_key_pair_file.Value()) ) {
						dprintf(D_ALWAYS,"(%d.%d) job destroy temporary keypair local file failed.\n", procID.cluster, procID.proc);
					}
					if ( condorState == REMOVED || condorState == HELD ) {
						SetInstanceId( NULL );
						SetKeypairId( NULL );
						gmState = GM_DELETE;
					} else {
						gmState = GM_CREATE_KEYPAIR;
					}
					
				} else {
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS,"(%d.%d) job destroy keypair failed: %s: %s\n",
							 procID.cluster, procID.proc, gahp_error_code,
							 errorString.Value() );
					gmState = GM_HOLD;
				}
									
				}

				break; 	
				

			case GM_DESTROY_KEYPAIR:
				{
				// Yes, now let's destroy the temporary keypair 
				rc = gahp->amazon_vm_destroy_keypair(m_public_key_file, m_private_key_file, m_key_pair.Value(), gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				if (rc == 0) {
					// remove temporary keypair local output file
					if ( remove_keypair_file(m_key_pair_file.Value()) ) {
						gmState = GM_FAILED;
					} else {
						dprintf(D_ALWAYS,"(%d.%d) job destroy keypair local file failed.\n", procID.cluster, procID.proc);
						gmState = GM_FAILED;
					}
					
				} else {
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS,"(%d.%d) job destroy keypair failed: %s: %s\n",
							 procID.cluster, procID.proc, gahp_error_code,
							 errorString.Value() );
					gmState = GM_HOLD;
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
					jobAd->LookupString( ATTR_HOLD_REASON, holdReason, sizeof(holdReason) - 1 );
					if ( holdReason[0] == '\0' && errorString != "" ) {
						strncpy( holdReason, errorString.Value(), sizeof(holdReason) - 1 );
					} else if ( holdReason[0] == '\0' ) {
						strncpy( holdReason, "Unspecified gridmanager error", sizeof(holdReason) - 1 );
					}

					JobHeld( holdReason );
				}
			
				gmState = GM_DELETE;
				
				break;
				
				
			case GM_FAILED:

				myResource->CancelSubmit( this );
				SetInstanceId( NULL );
				SetKeypairId( NULL );

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
		
			// This string is used for gahp calls, but is never needed beyond
			// this point. This should really be a MyString.
		free( gahp_error_code );
		gahp_error_code = NULL;

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state], GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}
		
	} // end of do_while
	while ( reevaluate_state );	

	return TRUE;
}


BaseResource* AmazonJob::GetResource()
{
	return (BaseResource *)myResource;
}


// steup the public name of amazon remote VM, which can be used the clients 
void AmazonJob::SetRemoteVMName(const char * name)
{
	if ( name ) {
		jobAd->Assign( ATTR_AMAZON_REMOTE_VM_NAME, name );
	} else {
		jobAd->AssignExpr( ATTR_AMAZON_REMOTE_VM_NAME, "Undefined" );
	}
	
	requestScheddUpdate( this, false );
}


void AmazonJob::SetKeypairId( const char *keypair_id )
{
	if ( keypair_id == NULL ) {
		m_key_pair = "";
	} else {
		m_key_pair = keypair_id;
	}
	SetRemoteJobId( m_key_pair.Value(), remoteJobId );
}

void AmazonJob::SetInstanceId( const char *instance_id )
{
	free( remoteJobId );
	if ( instance_id ) {
		remoteJobId = strdup( instance_id );
	} else {
		remoteJobId = NULL;
	}
	SetRemoteJobId( m_key_pair.Value(), remoteJobId );
}

// SetRemoteJobId() is used to set the value of global variable "remoteJobID"
void AmazonJob::SetRemoteJobId( const char *keypair_id, const char *instance_id )
{
	MyString full_job_id;
	if ( keypair_id && keypair_id[0] ) {
		full_job_id.sprintf( "amazon %s", keypair_id );
		if ( instance_id && instance_id[0] ) {
			full_job_id.sprintf_cat( " %s", instance_id );
		}
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}


// private functions to construct ami_id, keypair, keypair output file and groups info from ClassAd

// if ami_id is empty, client must have assigned upload file name value
// otherwise the condor_submit will report an error.
MyString AmazonJob::build_ami_id()
{
	MyString ami_id;
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_AMI_ID, &buffer ) ) {
		ami_id = buffer;
		free (buffer);
	}
	return ami_id;
}


MyString AmazonJob::build_keypair()
{
	// Build a name for the ssh keypair that will be unique to this job.
	// Our pattern is SSH_<collector name>_<GlobalJobId>

	// get condor pool name
	// In case there are multiple collectors, strip out the spaces
	// If there's no collector, insert a dummy name
	char* pool_name = param( "COLLECTOR_HOST" );
	if ( pool_name ) {
		StringList collectors( pool_name );
		free( pool_name );
		pool_name = collectors.print_to_string();
	} else {
		pool_name = strdup( "NoPool" );
	}

	// use "ATTR_GLOBAL_JOB_ID" to get unique global job id
	MyString job_id;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );

	MyString key_pair;
	key_pair.sprintf( "SSH_%s_%s", pool_name, job_id.Value() );

	free( pool_name );
	return key_pair;
}

StringList* AmazonJob::build_groupnames()
{
	StringList* group_names = NULL;
	char* buffer = NULL;
	
	// Notice:
	// Based on the meeting in 04/01/2008, now we will not create any temporary security groups
	// 1. clients assign ATTR_AMAZON_SECURITY_GROUPS in condor_submit file, then we will use those 
	//    security group names.
	// 2. clients don't assign ATTR_AMAZON_SECURITY_GROUPS in condor_submit file, then we will use
	//    the default security group (by just keeping group_names is empty).
	
	if ( jobAd->LookupString( ATTR_AMAZON_SECURITY_GROUPS, &buffer ) ) {
		group_names = new StringList( buffer, " " );
	} else {
		group_names = new StringList();
	}
	
	free (buffer);
	
	return group_names;
}


// After keypair is destroyed, we need to call this function. In temporary keypair
// scenario, we should delete the temporarily created keypair output file.
bool AmazonJob::remove_keypair_file(const char* filename)
{
	if (filename == NULL) {
		// not create temporary keypair output file
		// return success directly.
		return true;
	} else {
		// check if the file name is what we should create
		if ( strcmp(filename, NULL_FILE) == 0 ) {
			// no need to delete it since it is /dev/null
			return true;
		} else {
			if (remove(filename) == 0) 	
				return true;
			else 
				return false;
		}
	}
}


// print out the error code received from grid_manager
void AmazonJob::print_error_code( const char* error_code,
								  const char* function_name )
{
	dprintf( D_ALWAYS, "Receiving error code = %s from function %s !", error_code, function_name );	
}
