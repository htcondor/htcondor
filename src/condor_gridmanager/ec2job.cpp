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
#include "ec2job.h"
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
#define GM_SAVE_CLIENT_TOKEN			10

#define GM_SPOT_START                   11
#define GM_SPOT_CANCEL                  12
#define GM_SPOT_SUBMITTED               13
#define GM_SPOT_QUERY                   14
#define GM_SPOT_CHECK                   15

#define GM_SEEK_INSTANCE_ID             16

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
	"GM_SAVE_CLIENT_TOKEN",
	"GM_SPOT_START",
	"GM_SPOT_CANCEL",
	"GM_SPOT_SUBMITTED",
	"GM_SPOT_QUERY",
	"GM_SPOT_CHECK",
	"GM_SEEK_INSTANCE_ID"
};

#define EC2_VM_STATE_RUNNING			"running"
#define EC2_VM_STATE_PENDING			"pending"
#define EC2_VM_STATE_SHUTTINGDOWN	"shutting-down"
#define EC2_VM_STATE_TERMINATED		"terminated"


// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void EC2JobInit()
{
}


void EC2JobReconfig()
{
	// Tell all the resource objects to deal with their new config values
	EC2Resource *next_resource;

	EC2Resource::ResourcesByName.startIterations();

	while ( EC2Resource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}	
}


bool EC2JobAdMatch( const ClassAd *job_ad )
{
	int universe;
	string resource;
	
	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) &&
		 (strncasecmp( resource.c_str(), "ec2", 3 ) == 0) ) 
	{
		return true;
	}
	return false;
}


BaseJob* EC2JobCreate( ClassAd *jobad )
{
	return (BaseJob *)new EC2Job( jobad );
}

int EC2Job::gahpCallTimeout = 600;
int EC2Job::submitInterval = 300;
int EC2Job::maxConnectFailures = 3;
int EC2Job::funcRetryInterval = 15;
int EC2Job::pendingWaitTime = 15;
int EC2Job::maxRetryTimes = 3;

MSC_DISABLE_WARNING(6262) // function uses more than 16k of stack
EC2Job::EC2Job( ClassAd *classad )
	: BaseJob( classad )
{
dprintf( D_ALWAYS, "================================>  EC2Job::EC2Job 1 \n");
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
	m_group_names = NULL;
	m_should_gen_key_pair = false;
	m_keypair_created = false;
	
	// check the public_key_file
	jobAd->LookupString( ATTR_EC2_ACCESS_KEY_ID, m_public_key_file );
	
	if ( m_public_key_file.empty() ) {
		error_string = "Public key file not defined";
		goto error_exit;
	}

	// check the private_key_file
	jobAd->LookupString( ATTR_EC2_SECRET_ACCESS_KEY, m_private_key_file );
	
	if ( m_private_key_file.empty() ) {
		error_string = "Private key file not defined";
		goto error_exit;
	}

    // look for a spot price
    jobAd->LookupString( ATTR_EC2_SPOT_PRICE, m_spot_price );
    dprintf( D_FULLDEBUG, "Found EC2 spot price: %s\n", m_spot_price.c_str() );

    // look for a spot ID
    jobAd->LookupString( ATTR_EC2_SPOT_REQUEST_ID, m_spot_request_id );
    dprintf( D_FULLDEBUG, "Found EC2 spot request ID: %s\n", m_spot_request_id.c_str() );

    // Check for failure injections.
    m_failure_injection = getenv( "GM_FAILURE_INJECTION" );
    if( m_failure_injection == NULL ) { m_failure_injection = ""; }
    dprintf( D_FULLDEBUG, "GM_FAILURE_INJECTION = %s\n", m_failure_injection );

    // lookup the elastic IP
    jobAd->LookupString( ATTR_EC2_ELASTIC_IP, m_elastic_ip );
	
    jobAd->LookupString( ATTR_EC2_EBS_VOLUMES, m_ebs_volumes );
	
	// lookup the elastic IP
    jobAd->LookupString( ATTR_EC2_AVAILABILITY_ZONE, m_availability_zone );
	
    jobAd->LookupString( ATTR_EC2_VPC_SUBNET, m_vpc_subnet );
	
    jobAd->LookupString( ATTR_EC2_VPC_IP, m_vpc_ip );
	
	
	// if user assigns both user_data and user_data_file, the two will
	// be concatenated by the gahp
	jobAd->LookupString( ATTR_EC2_USER_DATA_FILE, m_user_data_file );

	jobAd->LookupString( ATTR_EC2_USER_DATA, m_user_data );
	
	// get VM instance type
	// if clients don't assign this value in condor submit file,
	// we should set the default value to NULL and gahp_server
	// will start VM in EC2 using m1.small mode.
	jobAd->LookupString( ATTR_EC2_INSTANCE_TYPE, m_instance_type );
	
	m_vm_check_times = 0;

	// Only generate a keypair if the user asked for one.
	jobAd->LookupString( ATTR_EC2_KEY_PAIR, m_key_pair );
	jobAd->LookupString( ATTR_EC2_KEY_PAIR_FILE, m_key_pair_file );
	if( m_key_pair.empty() && ! m_key_pair_file.empty() ) {
	    m_should_gen_key_pair = true;
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
		if ( !token || strcasecmp( token, "ec2" ) ) {
			formatstr( error_string, "%s not of type ec2",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			m_serviceUrl = token;
		} else {
			formatstr( error_string, "%s missing EC2 service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		formatstr( error_string, "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	gahp_path = param( "EC2_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "EC2_GAHP not defined";
		goto error_exit;
	}

	gahp_log = param( "EC2_GAHP_LOG" );
	if ( gahp_log == NULL ) {
		dprintf(D_ALWAYS, "Warning: No EC2_GAHP_LOG defined\n");
	} else {
		args.AppendArg("-f");
		args.AppendArg(gahp_log);
		free(gahp_log);
	}

	args.AppendArg("-w");
	gahp_worker_cnt = param_integer( "EC2_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-m");
	gahp_worker_cnt = param_integer( "EC2_GAHP_WORKER_MAX_NUM", 5 );
	args.AppendArg(gahp_worker_cnt);

	args.AppendArg("-d");
	gahp_debug = param( "EC2_GAHP_DEBUG" );
	if (!gahp_debug) {
		args.AppendArg("D_ALWAYS");
	} else {
		args.AppendArg(gahp_debug);
		free(gahp_debug);
	}

	gahp = new GahpClient( EC2_RESOURCE_NAME, gahp_path, &args );
	free(gahp_path);
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource =
		EC2Resource::FindOrCreateResource( m_serviceUrl.c_str(),
										   m_public_key_file.c_str(),
										   m_private_key_file.c_str() );
	myResource->RegisterJob( this );

	jobAd->LookupString( ATTR_GRID_JOB_ID, value );
	if ( !value.empty() ) {
		const char *token;

		dprintf(D_ALWAYS, "Recovering job from %s\n", value.c_str());

		Tokenize( value );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "ec2" ) ) {
			formatstr( error_string, "%s not of type ec2", ATTR_GRID_JOB_ID );
			goto error_exit;
		}

			// Skip the service URL
		GetNextToken( " ", false );

		token = GetNextToken( " ", false );
		if ( token ) {
			m_client_token = token;
		}

		token = GetNextToken( " ", false );
		if ( token ) {
			m_remoteJobId = token;
		}
	}

	if ( !m_remoteJobId.empty() ) {
		myResource->AlreadySubmitted( this );
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
MSC_RESTORE_WARNING(6262) // function uses more than 16k of stack

EC2Job::~EC2Job()
{
	if ( myResource ) myResource->UnregisterJob( this );
	delete gahp;
	delete m_group_names;
}


void EC2Job::Reconfig()
{
	BaseJob::Reconfig();
}


void EC2Job::doEvaluateState()
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
				// This is the state all jobs start in when the EC2Job object
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
                    if( condorState == REMOVED ) {
                        gmState = GM_DELETE;
                        break;
                    } else {
                        formatstr( errorString, "Failed to authenticate %s.",
                                   myResource->authFailureMessage.c_str() );
                        dprintf( D_ALWAYS, "(%d.%d) %s\n",
                                 procID.cluster, procID.proc, errorString.c_str() );
                        jobAd->Assign( ATTR_HOLD_REASON, errorString );
                        gmState = GM_HOLD;
                        break;
                    }
                }

                // If we're not doing recovery, start with GM_CLEAR_REQUEST.
				gmState = GM_CLEAR_REQUEST;

                if( ! m_client_token.empty() ) {
                    // Recovery happens differently for spot instances.
	    			if( ! m_spot_price.empty() ) {
	    			    // If we have only a client token, check to see if
	    			    // a corresponding request exists.  We can't just
	    			    // ignore the possibility and go to GM_SPOT_START
	    			    // (like a normal instance) because indempotence
	    			    // isn't baked into the EC2 API for spot instances.
	    			    // (Although, see the question below; we might have
	    			    // to jump to GM_SAVE_CLIENT_TOKEN anyway.)
	    			    gmState = GM_SPOT_CHECK;
	    			    
	    			    if( ! m_spot_request_id.empty() ) {
	    			        // If we have a request ID, GM_SPOT_SUBMITTED
	    			        // will check to see if the request has spawned
	    			        // an instance.
	    			        gmState = GM_SPOT_SUBMITTED;
	    			    }
	    			    
	    			    if( ! m_remoteJobId.empty() ) {
	    			        // If we've already spawned an instance, cancel
	    			        // the spot request immediately.
	    			        gmState = GM_SPOT_CANCEL;
	    			    }
	    		    } else {
	    		        // Because we have the client token, we know that
	    		        // we still have the SSH keypair from the previous
	    		        // execution of GM_SAVE_CLIENT_TOKEN.  We can therefore
	    		        // jump directly to GM_START_VM, where (using the
	    		        // client token), we will call RunInstances() 
	    		        // idempotently.
           				gmState = GM_START_VM;
	    		        
	    		        // As an optimization, if we already have the instance
	    		        // ID, we can jump directly to GM_SUBMITTED.  This
	    		        // also allows us to avoid logging the submission and
	    		        // execution events twice.
	    		        //
	    		        // FIXME: If a job were removed before we begin
	    		        // recovery, would the execute event be logged twice?
	    		    	if (!m_remoteJobId.empty()) {
		    		    	submitLogged = true;
    		    			if ( condorState == RUNNING || condorState == COMPLETED ) {
	    		    			executeLogged = true;
		    		    	}
    			    		gmState = GM_SUBMITTED;
	    			    } else if( condorState == REMOVED ) {
	    			        // We don't know if the corresponding instance
	    			        // exists or not.  Rather than unconditionally
	    			        // create it (in GM_START_VM), check to see if
	    			        // exists first.  While this may be more efficient,
	    			        // the real benefit is that invalid jobs won't
	    			        // stay on hold when the user removes them
	    			        // (because we won't try to start them).
	    			        gmState = GM_SEEK_INSTANCE_ID;
	    			    }
                    }
                }
				break;


			case GM_SAVE_CLIENT_TOKEN: {
				if (m_client_token.empty()) {
					SetClientToken(build_client_token().c_str());
				}
				
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}
				
				int gmTargetState = GM_START_VM;
				if( ! m_spot_price.empty() ) {
				    gmTargetState = GM_SPOT_START;
                }
				
				////////////////////////////////
				// Here we create the keypair only 
				// if we need to.  
				if ( m_should_gen_key_pair && !m_keypair_created )
				{	
				    if (m_key_pair.empty())
				    {
				      SetKeypairId( build_keypair().c_str() );
				    }
				
				    rc = gahp->ec2_vm_create_keypair(m_serviceUrl, 
								     m_public_key_file, 
								     m_private_key_file, 
								     m_key_pair, 
								     m_key_pair_file, 
								     gahp_error_code);

				    if ( rc == GAHPCLIENT_COMMAND_PENDING || 
				      rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ) {
					    break;
				    }

				    if (rc == 0) {
				      m_keypair_created = true;
				      gmState = gmTargetState;
				    } else {
				
					    // May need to add back retry logic, but why?
					    errorString = gahp->getErrorString();
					    dprintf(D_ALWAYS,"(%d.%d) job create keypair failed: %s: %s\n",
							    procID.cluster, procID.proc, gahp_error_code,
							    errorString.c_str() );
					    gmState = GM_HOLD;
					    break;
				    }
				}
				else
				{
				  gmState = gmTargetState;
				}
				
				} break;


			case GM_START_VM:

				// XXX: This is always false, why is numSubmitAttempts
				// not incremented after ec2_vm_start? Same for
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

					// construct input parameters for ec2_vm_start()
					char* instance_id = NULL;
					
					// For a given EC2 Job, in its life cycle, the attributes will not change
					
					m_ami_id = build_ami_id();
					if ( m_group_names == NULL ) {
						m_group_names = build_groupnames();
					}
					
					// ec2_vm_start() will check the input arguments
					rc = gahp->ec2_vm_start( m_serviceUrl,
											 m_public_key_file,
											 m_private_key_file,
											 m_ami_id,
											 m_key_pair,
											 m_user_data,
											 m_user_data_file,
											 m_instance_type,
											 m_availability_zone,
											 m_vpc_subnet,
											 m_vpc_ip,
											 m_client_token,
											 *m_group_names,
											 instance_id,
											 gahp_error_code);
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					lastSubmitAttempt = time(NULL);

					if ( rc != 0 &&
						 strcmp( gahp_error_code, "NEED_CHECK_VM_START" ) == 0 ) {
						// get an error code from gahp server said that we should check if 
						// the VM has been started successfully in EC2
						
						// Maxmium retry times is 3, if exceeds this limitation, we fall through
						if ( m_vm_check_times++ < maxRetryTimes ) {
							gmState = GM_START_VM;
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
						daemonCore->Reset_Timer( evaluateStateTid,
												 submitInterval );
					 } else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s: %s\n",
								procID.cluster, procID.proc, gahp_error_code,
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
				
				if( ! m_spot_price.empty() ) {
				    gmState = GM_SPOT_CANCEL;
                } else {				    
    				gmState = GM_SUBMITTED;
                }
				break;
				
			
			case GM_SUBMITTED:

				if ( remoteJobState == EC2_VM_STATE_TERMINATED ) {
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
					int interval = myResource->GetJobPollInterval();
					if ( remoteJobState != EC2_VM_STATE_RUNNING ) {
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
				
				myResource->CancelSubmit( this );
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					SetInstanceId( NULL );
					SetClientToken( NULL );
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
				if ( !m_remoteJobId.empty() && condorState != REMOVED 
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
				SetClientToken( NULL );

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

				if ( m_spot_request_id != "" ) {
					m_spot_request_id = "";
					jobAd->AssignExpr( ATTR_EC2_SPOT_REQUEST_ID, "Undefined" );
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
					gmState = GM_SAVE_CLIENT_TOKEN;
				}

				break;				


			case GM_PROBE_JOB:

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_SUBMITTED; // GM_SUBMITTED knows how to handle this
				} else {
					StringList returnStatus;

					// need to call ec2_vm_status(), ec2_vm_status()
					// will check input arguments
					// The VM status we need is saved in the second
					// string of the returned status StringList
					rc = gahp->ec2_vm_status(m_serviceUrl,
											 m_public_key_file,
											 m_private_key_file,
											 m_remoteJobId,
											 returnStatus,
											 gahp_error_code );
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					
					// processing error code received
					if ( rc != 0 ) {
						// What to do about failure?
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s: %s\n",
								 procID.cluster, procID.proc, gahp_error_code,
								 errorString.c_str() );
						gmState = GM_HOLD;
						break;
					} else {
                        if ( returnStatus.number() == 0 ) {
							// The instance has been purged, act like we
							// got back 'terminated'
							returnStatus.append( m_remoteJobId.c_str() );
							returnStatus.append( EC2_VM_STATE_TERMINATED );
						}

                        //
                        // Grab everything out of returnStatus in one pass.
                        //
                        returnStatus.rewind();
                        
                        // Required; srCode may be null.
                        std::string instance_id = returnStatus.next();
                        std::string new_status = returnStatus.next();
                        std::string ami_id = returnStatus.next();
                        std::string srCode = returnStatus.next();
                        
                        // Optional.
                        std::string public_dns, private_dns, keyname;
                        if( returnStatus.number() >= 5 ) {
                            public_dns = returnStatus.next();
                            private_dns = returnStatus.next();
                            keyname = returnStatus.next();
                        }
                        
                        // Any remaining values are the security groups.

						// if ec2 VM's state is "running" or beyond,
						// change condor job status to Running.
						if ( new_status != remoteJobState &&
							 ( new_status == EC2_VM_STATE_RUNNING ||
							   new_status == EC2_VM_STATE_SHUTTINGDOWN ||
							   new_status == EC2_VM_STATE_TERMINATED ) ) 
						{
							JobRunning();
                            
                            // On a state change to running we perform all associations
							// the are non-blocking and we continue even if they fail.
                            if ( new_status == EC2_VM_STATE_RUNNING )
                            {
                                associate_n_attach(returnStatus);
                            }
						}

						remoteJobState = new_status;
						SetRemoteJobStatus( new_status.c_str() );

                        if( ! public_dns.empty() ) {
                            SetRemoteVMName( public_dns.c_str() );
                        }

                        // dprintf( D_ALWAYS, "DEBUG: srCode = %s (assuming 'NULL')\n", srCode.c_str() );
                        if( srCode != "NULL" ) {
                            // Send the user a copy of the reason code.
                            jobAd->Assign( ATTR_EC2_STATUS_REASON_CODE, srCode.c_str() );                            
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
                                 srCode == "Client.InstanceInitiatedShutdown"
                              || srCode == "Client.UserInitiatedShutdown"
                              || srCode == "Server.SpotInstanceTermination" ) {
                                // Normal instance terminations are normal.
                            } else if(
                                 srCode == "Server.InternalError"
                              || srCode == "Server.InsufficientInstanceCapacity"
                              || srCode == "Client.VolumeLimitExceeded"
                              || srCode == "Client.InternalError"
                              || srCode == "Client.InvalidSnapshot.NotFound" ) {
                                // Put abnormal instance terminations on hold.
                                formatstr( errorString, "Abnormal instance termination: %s.", srCode.c_str() );
                                dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                                gmState = GM_HOLD;
                                break;
                            } else {
                                // Treat all unrecognized reasons as abnormal.
                                formatstr( errorString, "Unrecognized reason for instance termination: %s.  Treating as abnormal.", srCode.c_str() );
                                dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                                gmState = GM_HOLD;
                                break;
                            }
                        }
					}
					
                    lastProbeTime = now;
                    gmState = GM_SUBMITTED;
				}

				break;				
				
			case GM_CANCEL:
			    // Rather than duplicate code or cleverness in the spot
			    // instance subgraph, just handle the case where we're
			    // cancelling a spot request which has no corresponding
			    // instance here.
			    if( ! m_remoteJobId.empty() ) {
    				// need to call ec2_vm_stop(), it will only return
	    			// STOP operation is success or failed
		    		// ec2_vm_stop() will check the input arguments
			    	rc = gahp->ec2_vm_stop(m_serviceUrl,
				    					   m_public_key_file,
					    				   m_private_key_file,
						    			   m_remoteJobId,
							    		   gahp_error_code);
			
    				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
	    				 rc == GAHPCLIENT_COMMAND_PENDING ) {
		    			break;
			    	} 
			    } else {
			        rc = 0;
			    }
				
				if ( rc == 0 ) {
					if ( condorState == COMPLETED || condorState == REMOVED ) {
						gmState = GM_DELETE;
					} else {
							// If the job was not Completed or Removed
							// then it might run again, clear
							// everything in preparation.
							//
							// This case is hit when a job is Held,
							// when it is later released it should run
							// again. If the GridJobId is not cleared,
							// the second run attempt will find the
							// first terminated instance and consider
							// itself complete.

							// Clear the contact string here because it may not get
							// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
						myResource->CancelSubmit( this );
						SetInstanceId( NULL );
						SetClientToken( NULL );
						gmState = GM_CLEAR_REQUEST;
					}
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s: %s\n",
							 procID.cluster, procID.proc, gahp_error_code,
							 errorString.c_str() );
					gmState = GM_HOLD;
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
				if (m_keypair_created)
				{
				  // Yes, now let's destroy the temporary keypair 
				  rc = gahp->ec2_vm_destroy_keypair(m_serviceUrl,
								    m_public_key_file, 
								    m_private_key_file, 
								    m_key_pair, gahp_error_code);

				  if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					  break;
				  }

				  if( rc == 0 ) {
					// Forget about the keypair.
					SetKeypairId( NULL );
					m_keypair_created = false;
					
					// Preserve the user's copy of the private key unless
					// we're sure the instance is going away.  This will
					// be overwritten on instance restart, if it occurs.
					//
					// Note that what we actually do here is check to see
					// if the gridmanager will pay attention to the job the
					// next time the gridmanager runs.  If it won't,
					// we clean up.
					//
					// Except that, empirically, condor_rm doesn't clear
					// the job ad.  *sigh* 
					std::string gridJobID;
					jobAd->LookupString( ATTR_GRID_JOB_ID, gridJobID );
					if( gridJobID.empty() || condorState == REMOVED || condorState == COMPLETED ) {
						// remove temporary keypair local output file
						if ( !remove_keypair_file(m_key_pair_file.c_str()) ) {
							dprintf(D_ALWAYS,"(%d.%d) job destroy keypair local file failed.\n", procID.cluster, procID.proc);
						}
					}
				  } else {
					  errorString = gahp->getErrorString();
					  dprintf( D_ALWAYS,"(%d.%d) job destroy keypair failed: %s: %s\n",
							  procID.cluster, procID.proc, gahp_error_code,
							  errorString.c_str() );
				  }
				}
								
				// We are done with the job. Propagate any remaining updates
				// to the schedd, then delete this object.
				DoneWithJob();
				// This object will be deleted when the update occurs
				break;

            //
            // Except for the three exceptions above (search for GM_SPOT),
            // and exits to terminal states, the state machine for spot
            // instance requests is a disjoint subgraph.  Although this
            // enforces certain limits (e.g., at most one instance per
            // spot request), it makes reasoning about them much simpler.
            //

            // Actually submit the spot request to the cloud.  Note that
            // this occurs *after* we've saved the client token, so we
            // can never lose track of it.
            
            case GM_SPOT_START: {
                // Because we can't (easily) check if we've already made
                // a spot instance request, we can't check if the job's been
                // held or removed here; we might leak a request.  Instead,
                // the user will just have to wait until we get (or don't)
                // a spot request ID.
            
                // Why is this necessary?
                m_ami_id = build_ami_id();
                if( m_group_names == NULL ) {
                    m_group_names = build_groupnames();
                }
                
                // Send a command to the GAHP, or poll for its result(s).
                char * spot_request_id = NULL;
                rc = gahp->ec2_spot_start(  m_serviceUrl,
                                            m_public_key_file,
                                            m_private_key_file,
                                            m_ami_id,
                                            m_spot_price,
                                            m_key_pair,
                                            m_user_data, m_user_data_file,
                                            m_instance_type,
                                            m_availability_zone,
                                            m_vpc_subnet, m_vpc_ip,
                                            m_client_token,
                                            * m_group_names,
                                            spot_request_id,
                                            gahp_error_code );

                dprintf( D_ALWAYS, "GM_FAILURE_INJECTION = '%s'\n", m_failure_injection );
                if( strcmp( m_failure_injection, "1" ) == 0 ) {
                    rc = 1;
                    // gahp_error_code is free()d below.
                    gahp_error_code = strdup( "E_TESTING" );
                    gahp->setErrorString( "GM_FAILURE_INJECTION #1" );
                }

                // If the command hasn't terminated yet, return to this
                // state and poll again.
                if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
                    rc == GAHPCLIENT_COMMAND_PENDING ) {
                    break;
                }

                // If the command succeeded, write the request ID back to
                // the schedd, but don't block.  We'll discover the request
                // ID based on the client token during recovery.  Note that
                // we don't overload the remote job ID, because we need to
                // distinguish between instance and spot request IDs, and
                // it's not clear they're required to be distinguishable.
                if( rc == 0 ) {
                    ASSERT( spot_request_id != NULL );
                    m_spot_request_id = spot_request_id;
                    free( spot_request_id );
                    
                    jobAd->Assign( ATTR_EC2_SPOT_REQUEST_ID, m_spot_request_id );
                    requestScheddUpdate( this, false );
                    
                    gmState = GM_SPOT_SUBMITTED;
                    break;
                } else {
                    errorString = gahp->getErrorString();
                    dprintf( D_ALWAYS, "(%d.%d) spot instance request failed: %s: %s\n",
                                procID.cluster, procID.proc, gahp_error_code,
                                errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_START + <GAHP failure> = GM_HOLD\n" );
                    
                    // I make the argument that even if the user removed the
                    // job, or put in on hold, before the spot instance
                    // request failed, they should learn about the failure.
                    gmState = GM_HOLD;
                    break;
                }

                } break;

            // Cancel the request with the cloud.  Once we're done with
            // the request proper, handle any instance(s) that it may
            // have spawned.
            case GM_SPOT_CANCEL:
                if( ! m_spot_request_id.empty() ) {
                    // Send a command to the GAHP, or poll for its result(s).
                    rc = gahp->ec2_spot_stop(   m_serviceUrl,
                                                m_public_key_file,
                                                m_private_key_file,
                                                m_spot_request_id,
                                                gahp_error_code );

                    if( strcmp( m_failure_injection, "2" ) == 0 ) {
                        rc = 1;
                        // gahp_error_code is free()d below.
                        gahp_error_code = strdup( "E_TESTING" );
                        gahp->setErrorString( "GM_FAILURE_INJECTION #2" );
                    }

                    // If the command hasn't terminated yet, return to this
                    // state and poll again.
                    if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
                        rc == GAHPCLIENT_COMMAND_PENDING ) {
                        break;
                    }
                    
                    if( rc != 0 ) {
                        errorString = gahp->getErrorString();
                        dprintf( D_ALWAYS, "(%d.%d) spot request stop failed: %s: %s\n",
                                    procID.cluster, procID.proc, gahp_error_code,
                                    errorString.c_str() );
                        dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_CANCEL + <GAHP failure> = GM_HOLD\n" );
                        gmState = GM_HOLD;
                        break;
                    }
                    
                    // Clear the request ID, now that it's been cancelled.
                    m_spot_request_id.clear();
                    
                    // Since we know the request is gone, forget about it.
                    jobAd->AssignExpr( ATTR_EC2_SPOT_REQUEST_ID, "Undefined" );
                    requestScheddUpdate( this, false );
                    
                    // Rather than decide if we crashed after cancelling a
                    // request but before removing its ID from the job ad,
                    // just never remove it.  It won't hurt to try to cancel
                    // the request again, and since we already have the
                    // instance ID, we know we're not done when we cancel it.
                }
            
                gmState = GM_SUBMITTED;
                break;

            // Alternates with GM_SPOT_QUERY to watch for an instance start.
            case GM_SPOT_SUBMITTED:
                // If the Condor job has been held or removed, we need to
                // know what the state of the remote job is (whether it's
                // spawned an instance) before we can decide what to do.
                if( condorState == HELD || condorState == REMOVED ) {
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_SUBMITTED + HELD|REMOVED = GM_SPOT_QUERY\n" );
                    gmState = GM_SPOT_QUERY;
                    break;
                }
                
                // Always wait at least interval before probing. 
                if( lastProbeTime < enteredCurrentGmState ) {
                    lastProbeTime = enteredCurrentGmState;
                }

                if( now >= lastProbeTime + myResource->GetJobPollInterval() ) {
                    gmState = GM_SPOT_QUERY;
                    break;
                } else {
                    // Why is this more complicated in GM_SUBMITTED?
                    unsigned int delay = (lastProbeTime + myResource->GetJobPollInterval()) - now;
                    daemonCore->Reset_Timer( evaluateStateTid, delay );
                }
                break;

            // Alternates with GM_SPOT_SUBMITTED to watch for instance start.
            case GM_SPOT_QUERY: {
                // Send a command to the GAHP, or poll for its result(s).
                StringList returnStatus;
                rc = gahp->ec2_spot_status( m_serviceUrl,
                                            m_public_key_file,
                                            m_private_key_file,
                                            m_spot_request_id,
                                            returnStatus,
                                            gahp_error_code );

                if( strcmp( m_failure_injection, "3" ) == 0 ) {
                    rc = 1;
                    // gahp_error_code is free()d below.
                    gahp_error_code = strdup( "E_TESTING" );
                    gahp->setErrorString( "GM_FAILURE_INJECTION #3" );
                }

                // If the command hasn't terminated yet, return to this
                // state and poll again.
                if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
                    rc == GAHPCLIENT_COMMAND_PENDING ) {
                    break;
                }

                // If the command fails, put the job on hold.
                if( rc != 0 ) {
                    errorString = gahp->getErrorString();
                    dprintf( D_ALWAYS, "(%d.%d) spot request probe failed: %s: %s\n",
                                procID.cluster, procID.proc, gahp_error_code,
                                errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + <GAHP failure> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;
                }
                
                if( strcmp( m_failure_injection, "4" ) == 0 ) { returnStatus.clearAll(); }

                // Update the job state.
                if( returnStatus.number() == 0 ) {
                    // The spot instance request has been purged.  This should
                    // only happen during recovery.  Put the job on hold so
                    // we get a chance to tell the user that an instance may
                    // have escaped.  (We can't tag or set a ClientToken for
                    // an instance in the spot request, so we can't just check
                    // all instances to see if we own any of them.)
                    errorString = "Spot request purged; an instance may still be running.";
                    dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + <spot purged> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;
                }
                
                // Spot status results are 4-tuples of request IDs, status
                // strings, AMI IDs (probably superflous), and instance IDs.
                // There should only ever be one result when probing a job.
                std::string status;
                std::string requestID;
                std::string instanceID;
                returnStatus.rewind();
                
                // The GAHP client will EXCEPT() returnStatus.number()
                // isn't 0 or a multiple 4.  (gahp-client.cpp:7070-86)
                // Should we EXCEPT again here?
                for( int i = 0; i < returnStatus.number(); i += 4 ) {
                    requestID = returnStatus.next();
                    status = returnStatus.next();
                    std::string launchGroup = returnStatus.next();
                    instanceID = returnStatus.next();
                    
                    if( requestID == m_spot_request_id ) { break; }
                }
                
                // The single result should always be the job we asked about.
                if( requestID != m_spot_request_id ) {
                    formatstr( errorString, "GM_SPOT_QUERY asked about %s, got %s instead.", m_spot_request_id.c_str(), requestID.c_str() );
                    dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + <bogus query data> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;                    
                }
                
                // If the request spawned an instance, we must save the
                // instance ID.  This (GM_SAVE_INSTANCE_ID) will then cancel
                // the request (GM_SPOT_CANCEL) and after checking the job
                // state (GM_SUBMITTED), cancel the instance.
                if( ! instanceID.empty() ) {
                    SetInstanceId( instanceID.c_str() );
                    gmState = GM_SAVE_INSTANCE_ID;
                    break;
                }

                // All 'active'-status requests have instance IDs.
                ASSERT( status != "active" );
                
                // If the request didn't spawn an instance, but the Condor
                // job has been held or removed, cancel the request.  (It's
                // OK to cancel the request twice.)
                if( condorState == HELD || condorState == REMOVED ) {
                    // Force GM_SUBMITTED (from GM_SPOT_CANCEL) to skip
                    // an instance-status probe.
                    remoteJobState = EC2_VM_STATE_TERMINATED;
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + HELD|REMOVED = GM_SPOT_CANCEL\n" );
                    gmState = GM_SPOT_CANCEL;
                    break;
                }

                if( strcmp( m_failure_injection, "5" ) == 0 ) { status = "not-open"; }

                if( status == "open" ) {
                    // Nothing interesting happened, so ask again latter.
                    gmState = GM_SPOT_SUBMITTED;
                    break;
                } else if( status == "cancelled" ) {
                    // We'll never see the cancelled state from GM_SPOT_CANCEL
                    // because it exits the SPOT subgraph.  Thus, this cancel
                    // must have come from the user (because we don't specify
                    // a bid expiration date in any our requsts) -- or some
                    // other person with the appropriate credentials.
                    // 
                    // For now, then, we'll put the job on hold, saying
                    // "You, or somebody like you, cancelled this request."
                    // 
                    // I'll leave this case broken out in case we change
                    // our minds later.
                    formatstr( errorString, "You, or somebody like you, cancelled this request." );
                    dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + <cancelled> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;                    
                } else {
                    // We should notify the user about a 'failed' job.
                    // The other possible status is 'closed', which we should
                    // never see -- we don't submit requests with durations,
                    // so they can't expire, and even if it somehow started
                    // and finished instance while we weren't paying attention,
                    // we still would have seen the instance ID already.
                    formatstr( errorString, "Request status '%s' unexpected in GM_SPOT_QUERY.", status.c_str() );
                    dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_QUERY + <unexpected state> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;
                }
                
                } break;

            // If, during recovery of a spot request, we have a client token
            // but not a spot instance ID, look at all spot requests to see
            // if we actually made the request or not.
            case GM_SPOT_CHECK: {
                // Send a command to the GAHP, or poll for its result(s).
                StringList returnStatus;
                rc = gahp->ec2_spot_status_all( m_serviceUrl,
                                                m_public_key_file,
                                                m_private_key_file,
                                                returnStatus,
                                                gahp_error_code );

                if( strcmp( m_failure_injection, "6" ) == 0 ) {
                    rc = 1;
                    // gahp_error_code is free()d below.
                    gahp_error_code = strdup( "E_TESTING" );
                    gahp->setErrorString( "GM_FAILURE_INJECTION #6" );
                }

                // If the command hasn't terminated yet, return to this
                // state and poll again.
                if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
                    rc == GAHPCLIENT_COMMAND_PENDING ) {
                    break;
                }
                
                // Place the job on hold if we can't look for the corresponding
                // spot instance; this may not be optimal, but at least the
                // user could notice and try again later.
                if( rc != 0 ) {
                    errorString = "Spot check failed.";
                    dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
                    dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_CHECK + <spot check failed> = GM_HOLD\n" );
                    gmState = GM_HOLD;
                    break;                    
                }

                // Look for the stray SIR.
                int originalState = gmState;
                returnStatus.rewind();
                for( int i = 0; i < returnStatus.number(); i += 4 ) {
                    std::string requestID = returnStatus.next();
                    std::string status = returnStatus.next();
                    std::string launchGroup = returnStatus.next();
                    std::string instanceID = returnStatus.next();
                    
                    if( launchGroup == m_client_token ) {
                        m_spot_request_id = requestID;

                        jobAd->Assign( ATTR_EC2_SPOT_REQUEST_ID, m_spot_request_id );
                        requestScheddUpdate( this, false );

                        if( ! instanceID.empty() ) {
                            SetInstanceId( instanceID.c_str() );
                            gmState = GM_SAVE_INSTANCE_ID;
                            // dprintf( D_FULLDEBUG, "Recovery transition: GM_SPOT_CHECK -> GM_SAVE_INSTANCE_ID\n" );
                            break;
                        }
                        
                        gmState = GM_SPOT_SUBMITTED;
                        // dprintf( D_FULLDEBUG, "Recovery transition: GM_SPOT_CHECK -> GM_SUBMITTED\n" );
                        break;
                    }
                }
                if( originalState != gmState ) { break; }
                
                //
                // We didn't find the SIR.  Since we never submit requests
                // with leases, if the SIR doesn't exist, either it never
                // did or the instance it spawned terminated long enough
                // ago for the request to have been purged.  The usual
                // Condor semantics of "it didn't succeed if we didn't see
                // it succeed" apply here, so it makes sense to submit
                // in both cases.
                // 
                gmState = GM_SPOT_START;
                } break;

            case GM_SEEK_INSTANCE_ID: {
                //
                // During recovery, if we have a client token but not an
                // instance ID, and we don't want to start a VM (that is,
                // we're removing the job), we need to be able to check
                // if our client token has a corresponding instance.
                //
                StringList returnStatus;
                rc = gahp->ec2_vm_status_all(   m_serviceUrl,
                                                m_public_key_file,
                                                m_private_key_file,
                                                returnStatus,
                                                gahp_error_code );
                if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED
                 || rc == GAHPCLIENT_COMMAND_PENDING ) {
                    break;
                }
                
                if( rc != 0 ) {
                    errorString = gahp->getErrorString();
                    dprintf( D_ALWAYS, "(%d.%d) Attempt to locate job during recovery failed: %s: %s\n",
                             procID.cluster, procID.proc, gahp_error_code,
                             errorString.c_str() );
                    gmState = GM_HOLD;
                    break;
                } else {
                    ASSERT( returnStatus.number() % 4 == 0 );
                    
                    returnStatus.rewind();
                    std::string instanceID;
                    std::string iid, status, amiID, clientToken;
                    for( int i = 0; i < returnStatus.number(); i += 4 ) {
                        iid = returnStatus.next();
                        status = returnStatus.next();
                        amiID = returnStatus.next();
                        clientToken = returnStatus.next();
                        
                        if( clientToken == m_client_token ) {
                            instanceID = iid;
                            break;
                        }
                    }
                        
                    if( ! instanceID.empty() ) {
                        // Yes, this duplicates work, but it also ensures
                        // that we do things like logging the submit and/or
                        // execute events.
                        gmState = GM_START_VM;
                    } else {
                        gmState = GM_DELETE;
                    }
                }
                } break;
                    
			default:
				EXCEPT( "(%d.%d) Unknown gmState %d!",
						procID.cluster, procID.proc, gmState );
				break;
		} // end of switch_case
		
			// This string is used for gahp calls, but is never needed beyond
			// this point. This should really be a MyString.
		free( gahp_error_code );
		gahp_error_code = NULL;

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


BaseResource* EC2Job::GetResource()
{
	return (BaseResource *)myResource;
}


// steup the public name of ec2 remote VM, which can be used the clients 
void EC2Job::SetRemoteVMName(const char * name)
{
	if ( name ) {
		jobAd->Assign( ATTR_EC2_REMOTE_VM_NAME, name );
	} else {
		jobAd->AssignExpr( ATTR_EC2_REMOTE_VM_NAME, "Undefined" );
	}
	
	requestScheddUpdate( this, false );
}

void EC2Job::SetKeypairId( const char *keypair_id )
{
	if ( keypair_id == NULL ) {
		m_key_pair = "";
	} else {
		m_key_pair = keypair_id;
	}
	
	jobAd->Assign( ATTR_EC2_KEY_PAIR, m_key_pair );
	
	requestScheddUpdate( this, false );
}

void EC2Job::SetClientToken(const char *client_token)
{
	m_client_token.clear();
	if ( client_token ) {
		m_client_token = client_token;
	}
	EC2SetRemoteJobId(m_client_token.empty() ? NULL : m_client_token.c_str(),
				   m_remoteJobId.c_str());
}

void EC2Job::SetInstanceId( const char *instance_id )
{
	m_remoteJobId.clear();
	if ( instance_id ) {
		m_remoteJobId = instance_id;
        jobAd->Assign( ATTR_EC2_INSTANCE_NAME, m_remoteJobId );
	}
	EC2SetRemoteJobId( m_client_token.c_str(),
					m_remoteJobId.empty() ? NULL : m_remoteJobId.c_str() );
}

// EC2SetRemoteJobId() is used to set the value of global variable "remoteJobID"
void EC2Job::EC2SetRemoteJobId( const char *client_token, const char *instance_id )
{
	string full_job_id;
	if ( client_token && client_token[0] ) {
		formatstr( full_job_id, "ec2 %s %s", m_serviceUrl.c_str(), client_token );
		if ( instance_id && instance_id[0] ) {
			formatstr_cat( full_job_id, " %s", instance_id );
		}
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}


// private functions to construct ami_id, keypair, keypair output file and groups info from ClassAd

// if ami_id is empty, client must have assigned upload file name value
// otherwise the condor_submit will report an error.
string EC2Job::build_ami_id()
{
	string ami_id;
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_EC2_AMI_ID, &buffer ) ) {
		ami_id = buffer;
		free (buffer);
	}
	return ami_id;
}

// Client token is max 64 ASCII chars
// http://docs.amazonwebservices.com/AWSEC2/latest/UserGuide/Run_Instance_Idempotency.html
string EC2Job::build_client_token()
{
#ifdef WIN32
	GUID guid;
	if (S_OK != CoCreateGuid(&guid))
		return NULL;
	WCHAR wsz[40];
	StringFromGUID2(guid, wsz, COUNTOF(wsz));
	char uuid_str[40];
	WideCharToMultiByte(CP_ACP, 0, wsz, -1, uuid_str, COUNTOF(uuid_str), NULL, NULL);
	return string(uuid_str);
#else
	char uuid_str[37];
	uuid_t uuid;

	uuid_generate(uuid);

	uuid_unparse(uuid, uuid_str);
	uuid_str[36] = '\0';
	return string(uuid_str);
#endif
}

std::string EC2Job::build_keypair()
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
	std::string job_id;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );

	std::string key_pair;
	formatstr( key_pair, "SSH_%s_%s", pool_name, job_id.c_str() );
	free( pool_name );
	
    // Some EC2 implementations (OpenStack) restrict the keypair name to
    // "alphanumeric character, spaces, dashes, and underscore."  Convert
    // everything else to spaces, since we don't presently use them for
    // anything.
    
    size_t loc = 0;
    #define KEYPAIR_FILTER "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_"
    while( (loc = key_pair.find_first_not_of( KEYPAIR_FILTER, loc )) != string::npos ) {
        key_pair[loc] = ' ';
    }        
	
	return key_pair;
}

StringList* EC2Job::build_groupnames()
{
	StringList* group_names = NULL;
	char* buffer = NULL;
	
	// Notice:
	// Based on the meeting in 04/01/2008, now we will not create any temporary security groups
	// 1. clients assign ATTR_EC2_SECURITY_GROUPS in condor_submit file, then we will use those 
	//    security group names.
	// 2. clients don't assign ATTR_EC2_SECURITY_GROUPS in condor_submit file, then we will use
	//    the default security group (by just keeping group_names is empty).
	
	if ( jobAd->LookupString( ATTR_EC2_SECURITY_GROUPS, &buffer ) ) {
		group_names = new StringList( buffer, " " );
	} else {
		group_names = new StringList();
	}
	
	free (buffer);
	
	return group_names;
}


// After keypair is destroyed, we need to call this function. In temporary keypair
// scenario, we should delete the temporarily created keypair output file.
bool EC2Job::remove_keypair_file(const char* filename)
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
void EC2Job::print_error_code( const char* error_code,
								  const char* function_name )
{
	dprintf( D_ALWAYS, "Receiving error code = %s from function %s !",
			 error_code, function_name );	
}

void EC2Job::associate_n_attach(StringList & returnStatus)
{

	char *gahp_error_code = NULL;
	int rc;

	char *buffer = NULL;
	if (jobAd->LookupString(ATTR_EC2_TAG_NAMES, &buffer)) {
		StringList tags;
		StringList tagNames(buffer);
		char *tagName;
		tagNames.rewind();
		while ((tagName = tagNames.next())) {
				// XXX: Check that tagName does not contain an equal sign (=)
			string tag;
			string tagAttr(ATTR_EC2_TAG_PREFIX);
			tagAttr.append(tagName);
			char *value = NULL;
			if (!jobAd->LookupString(tagAttr.c_str(), &value)) {
				dprintf(D_ALWAYS, "(%d.%d) Error: %s not defined, no value for tag, skipping\n",
						procID.cluster, procID.proc,
						tagAttr.c_str());
				continue;
			}
			tag.append(tagName).append("=").append(value);
			tags.append(tag.c_str());
			dprintf(D_FULLDEBUG, "(%d.%d) Tag found: %s\n",
					procID.cluster, procID.proc,
					tag.c_str());
			free(value);
		}

		rc = gahp->ec2_create_tags(m_serviceUrl.c_str(),
								   m_public_key_file.c_str(),
								   m_private_key_file.c_str(),
								   m_remoteJobId.c_str(),
								   tags,
								   returnStatus,
								   gahp_error_code );

		switch (rc) {
		case 0:
			break;
		case GAHPCLIENT_COMMAND_PENDING:
			break;
		case GAHPCLIENT_COMMAND_NOT_SUBMITTED:
			if ((condorState == REMOVED) || (condorState == HELD)) 
				gmState = GM_DELETE;
		default:
			dprintf(D_ALWAYS,
					"Failed ec2_create_tags returned %s continuing w/job\n",
					gahp_error_code);
			break;
		}
	}
	if (buffer) { free(buffer); buffer = NULL; }

	// associate the elastic ip with the now running instance.
	if ( !m_elastic_ip.empty() )
	{
		rc = gahp->ec2_associate_address(m_serviceUrl,
										 m_public_key_file,
										 m_private_key_file,
										 m_remoteJobId,
										 m_elastic_ip,
										 returnStatus,
										 gahp_error_code );

		switch (rc)
		{
			case 0:
				break;
			case GAHPCLIENT_COMMAND_PENDING:
				break;
			case GAHPCLIENT_COMMAND_NOT_SUBMITTED:
				if ( (condorState == REMOVED) || (condorState == HELD) ) 
					gmState = GM_DELETE;
			default:
				dprintf(D_ALWAYS,
						"Failed ec2_associate_address returned %s continuing w/job\n",
						gahp_error_code);
				break;
		}
	}

	if (!m_ebs_volumes.empty())
	{
		bool bcontinue=true;
		StringList vols(m_ebs_volumes.c_str(), ",");
		// Need to loop through here parsing the volumes which we will send to the gahp
		vols.rewind();
		
		const char *volume_str = NULL;
		while( bcontinue && (volume_str = vols.next() ) != NULL ) 
		{
			StringList ebs_volume_params(volume_str, ":");
			ebs_volume_params.rewind();

			// Volumes consist of volume_id:device_id similar to vm_disks
			char * volume_id = ebs_volume_params.next();
			char * device_id = ebs_volume_params.next();

			rc = gahp->ec2_attach_volume(m_serviceUrl,
										 m_public_key_file,
										 m_private_key_file,
										 volume_id,
										 m_remoteJobId,
										 device_id,
										 returnStatus,
										 gahp_error_code );

			switch (rc)
			{
				case 0:
					break;
				case GAHPCLIENT_COMMAND_PENDING:
					break;
				case GAHPCLIENT_COMMAND_NOT_SUBMITTED:
					if ( (condorState == REMOVED) || (condorState == HELD) ) 
						gmState = GM_DELETE;
				default:
					bcontinue=false;
					dprintf(D_ALWAYS,
							"Failed ec2_attach_volume returned %s continuing w/job\n",
							gahp_error_code);
					break;
			}
		}
	}
}
