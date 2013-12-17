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

#define GM_SPOT_START					11
#define GM_SPOT_CANCEL					12
#define GM_SPOT_SUBMITTED				13
#define GM_SPOT_QUERY					14
#define GM_SPOT_CHECK					15

#define GM_SEEK_INSTANCE_ID				16
#define GM_CREATE_KEY_PAIR				17
#define GM_CANCEL_CHECK					18

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
	"GM_SEEK_INSTANCE_ID",
	"GM_CREATE_KEY_PAIR",
	"GM_CANCEL_CHECK",
};

#define EC2_VM_STATE_RUNNING			"running"
#define EC2_VM_STATE_PENDING			"pending"
#define EC2_VM_STATE_SHUTTINGDOWN		"shutting-down"
#define EC2_VM_STATE_TERMINATED			"terminated"
#define EC2_VM_STATE_SHUTOFF			"shutoff"
#define EC2_VM_STATE_STOPPED			"stopped"

// These are pseduostates used internally by the grid manager.
#define EC2_VM_STATE_PURGED				"purged"
#define EC2_VM_STATE_CANCELLING			"cancelling"

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
EC2Job::EC2Job( ClassAd *classad ) :
	BaseJob( classad ),
	m_was_job_completion( false ),
	m_retry_times( 0 ),
	probeNow( false ),
	resourceLeaseTID( -1 )
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

	// Only generate a keypair if the user asked for one.
	// Note: We assume that if both the key_pair and key_pair_file
	//   attributes exist, then we created the keypair in a previous
	//   incarnation (and need to destroy them during job cleanup).
	//   This requires that the user not submit a job with
	//   both attributes set. There's no reason for the user to do so,
	//   and condor_submit will not allow it.
	// Note: We also want to generate the keypair if the client token
	//   can't be used for failure recovery during submission. But we
	//   may not know about that until we've pinged the server at least
	//   once. So we check for that in GM_INIT.
	jobAd->LookupString( ATTR_EC2_KEY_PAIR, m_key_pair );
	jobAd->LookupString( ATTR_EC2_KEY_PAIR_FILE, m_key_pair_file );
	if ( !m_key_pair_file.empty() ) {
		m_should_gen_key_pair = true;
	}
	if ( !m_key_pair.empty() && !m_key_pair_file.empty() ) {
		m_keypair_created = true;
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
		EC2Resource::FindOrCreateResource(	m_serviceUrl.c_str(),
											m_public_key_file.c_str(),
											m_private_key_file.c_str() );
	myResource->RegisterJob( this );

	// Registering the job isn't sufficient, because we need to be able
	// find jobs which gain these IDs during execution.  If they already
	// have one, we'll go into recovery, skipping the usual insert.
	if( ! m_spot_request_id.empty() ) {
		SetRequestID( m_spot_request_id.c_str() );
	}

	value.clear();
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
			dprintf( D_FULLDEBUG, "Found client token '%s'.\n", m_client_token.c_str() );
		}

		token = GetNextToken( " ", false );
		if ( token ) {
			m_remoteJobId = token;
			dprintf( D_FULLDEBUG, "Found remote job ID '%s'.\n", m_remoteJobId.c_str() );
		}
	}

	if ( !m_remoteJobId.empty() ) {
		myResource->AlreadySubmitted( this );
		// See comment above SetRequestID().
		SetInstanceId( m_remoteJobId.c_str() );
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
	if ( myResource ) {
		myResource->UnregisterJob( this );
		if( ! m_spot_request_id.empty() ) {
			myResource->spotJobsByRequestID.remove( HashKey( m_spot_request_id.c_str() ) );
		}
		if( ! m_remoteJobId.empty() ) {
			myResource->jobsByInstanceID.remove( HashKey( m_remoteJobId.c_str() ) );
		}
	}

	if( resourceLeaseTID != -1 ) {
		daemonCore->Cancel_Timer( resourceLeaseTID );
		resourceLeaseTID = -1;
	}

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
					if( condorState == REMOVED && m_client_token.empty() && m_remoteJobId.empty() ) {
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

				if( ! m_client_token.empty() ) {

					// If we can't rely on the client token for failure
					// recovery during submission, then we need to use the
					// old ssh keypair trick.
					if ( !myResource->ClientTokenWorks( this ) ) {
						m_should_gen_key_pair = true;
						m_keypair_created = true;
					}


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
						// Our starting assumption is that we still have
						// to start the instance, but if it already exists,
						// the ClientToken will prevent us from creating
						// a second one.
						// We may change our mind based on the checks below.
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
							// Do NOT set probeNow; if we're recovering from
							// a queue with 5000 jobs, we'd hit the service
							// with 5000 status update requests, which is
							// precisely what we're trying to avoid.
							gmState = GM_SUBMITTED;
						} else if( condorState == REMOVED ||
								   m_should_gen_key_pair ) {
							// We don't know if the corresponding instance
							// exists or not. And if we're creating the
							// ssh keypair, we don't know if that exists.
							// Rather than unconditionally
							// create it (in GM_START_VM), check to see if
							// exists first.  While this may be more efficient,
							// the real benefit is that invalid jobs won't
							// stay on hold when the user removes them
							// (because we won't try to start them).
							// Additionally, if the ClientToken can't be
							// used for idempotent submission, we have to
							// check for existence of the instance.
							gmState = GM_SEEK_INSTANCE_ID;
						}
					}
				}
				break;


			case GM_SAVE_CLIENT_TOKEN: {
				// If we don't know yet what type of server we're talking
				// to (e.g. all pings have failed because the server's
				// down), we have to wait here, as that affects how we'll
				// submit the job.
				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				if ( !myResource->ServerTypeQueried() ) {
					dprintf( D_FULLDEBUG, "(%d.%d) Don't know server type yet, waiting...\n", procID.cluster, procID.proc );
					break;
				}
				if ( !myResource->ClientTokenWorks( this ) ) {
					m_should_gen_key_pair = true;

					if ( m_client_token.empty() && m_key_pair_file.empty() &&
						 !m_key_pair.empty() ) {
						formatstr( errorString, "Can't use existing ssh keypair for server type %s", myResource->m_serverType.c_str() );
						gmState = GM_HOLD;
						break;
					}
				}

				if (m_client_token.empty()) {
					SetClientToken(build_client_token().c_str());
				}

				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );

				std::string type;
				jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type );
				if ( type != myResource->m_serverType ) {
					jobAd->Assign( ATTR_EC2_SERVER_TYPE, myResource->m_serverType );
					attr_exists = true;
					attr_dirty = true;
				}

				if ( m_should_gen_key_pair && m_key_pair.empty() ) {
					SetKeypairId( build_keypair().c_str() );
					attr_exists = true;
					attr_dirty = true;
				}

				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}

				gmState = GM_CREATE_KEY_PAIR;

				} break;

			case GM_CREATE_KEY_PAIR: {
				// Create the ssh keypair for this instance, if we need
				// to.

				int gmTargetState = GM_START_VM;
				if( ! m_spot_price.empty() ) {
					gmTargetState = GM_SPOT_START;
				}

				////////////////////////////////
				// Here we create the keypair only
				// if we need to.

				// We save the client token in the job ad first so
				// that if we fail and then the user holds or removes
				// the job, we'll still clean up the keypair properly.
				if ( m_should_gen_key_pair )
				{
					if ( ( condorState == REMOVED || condorState == HELD ) &&
						 !gahp->pendingRequestIssued() ) {
						gahp->purgePendingRequests();
						gmState = GM_CLEAR_REQUEST;
						break;
					}

					rc = gahp->ec2_vm_create_keypair(m_serviceUrl,
									m_public_key_file,
									m_private_key_file,
									m_key_pair,
									m_key_pair_file,
									gahp_error_code);

					if( rc == GAHPCLIENT_COMMAND_PENDING ||
						rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ) {
						break;
					}

					// If the keypair already exists, treat it as a
					// success. We do this instead of checking whether
					// the keypair exists during recovery.
					// Each server type uses a different error message.
					// Amazon, OpenSatck(Havana): "InvalidKeyPair.Duplicate"
					// Eucalyptus: "Keypair already exists"
					// OpenStack(pre-Havana): "KeyPairExists"
					// Nimbus: No error
					if ( rc == 0 ||
						 strstr( gahp->getErrorString(), "KeyPairExists" ) ||
						 strstr( gahp->getErrorString(), "Keypair already exists" ) ||
						 strstr( gahp->getErrorString(), "InvalidKeyPair.Duplicate" ) ) {
						m_keypair_created = true;
						gmState = gmTargetState;
					} else {

						// May need to add back retry logic, but why?
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job create keypair failed: %s: %s\n",
								procID.cluster, procID.proc,
								gahp_error_code.c_str(),
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
					std::string instance_id = "";

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

				if( ! m_spot_price.empty() ) {
					gmState = GM_SPOT_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
				break;


			case GM_SUBMITTED:
				if( remoteJobState == EC2_VM_STATE_SHUTOFF
				 || remoteJobState == EC2_VM_STATE_STOPPED ) {
					// SHUTOFF is an OpenStack-specific state where the VM
					// is no longer running, but retains its reserved resources.
					//
					// We simplify by considering this job complete and letting
					// it exit the queue.
					//
					// According to Amazon's documentation, the stopped state
					// only occurs for EBS-backed instances for which the
					// parameter InstanceInstantiateShutdownBehavior is not
					// set to terminated.  OpenStack blithely ignores either
					// this parameter or this restriction, and so we can see
					// the stopped state.  For the present, our users just
					// want OpenStack instances to act like Amazon instances
					// when they shut themselves down, so we'll work-around
					// the OpenStack bug here -- doing exactly the same thing
					// as we do for the SHUTOFF state.
					//
					// Note that we must set this flag so that we enter
					// GM_DONE_SAVE after terminating the job, instead of
					// GM_DELETE.  We don't need to record this in job ad:
					// either the job is TERMINATED when we come back (and
					// we go into GM_DONE_SAVE anyway), or it's still
					// SHUTOFF or STOPPED, and we return here; since cancel
					// is idempotent, this is OK.
					//
					// Since we'd like to retry the cancel no matter how
					// many times we retried the GM_START_VM, reset the count.
					//

					m_retry_times = 0;
					m_was_job_completion = true;
					gmState = GM_CANCEL;
					break;
				}

				if ( remoteJobState == EC2_VM_STATE_TERMINATED ) {
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
					SetRequestID( NULL );
				}

				std::string type;
				if ( jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type ) ) {
					jobAd->AssignExpr( ATTR_EC2_SERVER_TYPE, "Undefined" );
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
					if( remoteJobState == EC2_VM_STATE_PURGED ) {
						// The instance has been purged, act like we
						// got back 'terminated'
						remoteJobState = EC2_VM_STATE_TERMINATED;
						m_state_reason_code = "purged";
					}

					// We don't check for a status change, because this
					// state is now only entered if we had one.
					if( remoteJobState == EC2_VM_STATE_RUNNING ||
						remoteJobState == EC2_VM_STATE_SHUTTINGDOWN ||
						remoteJobState == EC2_VM_STATE_TERMINATED ) {
						JobRunning();

						// On a state change to running we perform all associations
						// the are non-blocking and we continue even if they fail.
						if ( remoteJobState == EC2_VM_STATE_RUNNING ) {
							associate_n_attach();
						}
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
				remoteJobState = EC2_VM_STATE_CANCELLING;
				SetRemoteJobStatus( EC2_VM_STATE_CANCELLING );

				// Rather than duplicate code in the spot instance subgraph,
				// just handle jobs with no corresponding instance here.
				if( m_remoteJobId.empty() ) {
					// Bypasses the polling delay in GM_CANCEL_CHECK.
					probeNow = true;
				} else {
					rc = gahp->ec2_vm_stop( m_serviceUrl,
											m_public_key_file,
											m_private_key_file,
											m_remoteJobId,
											gahp_error_code);

					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}

					if( rc != 0 ) {
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s: %s\n",
								 procID.cluster, procID.proc,
								 gahp_error_code.c_str(),
							 	errorString.c_str() );
						gmState = GM_HOLD;
						break;
					}

					// We could save some time here for Amazon users by
					// changing the EC2 GAHP to return the (new) state of
					// the job.  If it's SHUTTINGDOWN, set probeNow.  We
					// can't do this for OpenStack because we know they lie.
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

				if( remoteJobState == EC2_VM_STATE_TERMINATED
				 || remoteJobState == EC2_VM_STATE_PURGED
				 || ( remoteJobState == EC2_VM_STATE_SHUTTINGDOWN
				   && myResource->ShuttingDownTrusted( this ) ) ) {
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
							SetClientToken( NULL );
							gmState = GM_CLEAR_REQUEST;
						}
					}
				} else {
					if( m_retry_times++ < maxRetryTimes ) {
						gmState = GM_CANCEL;
					} else {
						formatstr( errorString, "Job cancel did not succeed after %d tries, giving up.", maxRetryTimes );
						dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, errorString.c_str() );
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
				//
				// Clean up the keypair as late as possible -- that is, the
				// last time we see this job.  That's logically identical to
				// when we will never see the job again, and we can check
				// that by looking the the condorState and the job attributes
				// we (would) use for recovery.
				//
				if( condorState == REMOVED
					|| condorState == COMPLETED
					|| ( m_client_token.empty() && m_remoteJobId.empty() ) ) {

					if( m_keypair_created && ! m_key_pair.empty() ) {
						rc = gahp->ec2_vm_destroy_keypair( m_serviceUrl,
									m_public_key_file, m_private_key_file,
									m_key_pair, gahp_error_code );

						if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
							break;
						}

						// If we don't remove the file until after we
						// remove the server-side copy, we'll try to delete
						// it twice, the first time before issuing the
						// command to the GAHP, the second after.
						if( !m_key_pair_file.empty() && ! remove_keypair_file( m_key_pair_file.c_str() ) ) {
							dprintf( D_ALWAYS, "(%d.%d) job destroy keypair local file (%s) failed.\n", procID.cluster, procID.proc, m_key_pair_file.c_str() );
						}

						// In order for putting the job on hold to make
						// sense, we'd need to be sure that the gridmanager
						// saw the job again.  While we may not have the
						// original job information, if all we want to do
						// is try to remove the keypair again, we could
						// ask the gahp('s resource) where it lives and
						// cons up an artificial gridJobID, something like
						// 'ec2 URL remove-keypair', which we'd recognize
						// during recovery (initialization/parsing)
						// and react to appropriately -- since we haven't
						// deleted the keypair ID yet, it'll still be in
						// the job ad...
						if( rc != 0 ) {
							errorString = gahp->getErrorString();
							dprintf( D_ALWAYS, "(%d.%d) job destroy keypair (%s) failed: %s: %s\n",
									 procID.cluster, procID.proc, m_key_pair.c_str(),
									 gahp_error_code.c_str(),
									 errorString.c_str() );
						}
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
				std::string spot_request_id;
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
					gahp_error_code = "E_TESTING";
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
					ASSERT( spot_request_id != "" );

					SetRequestID( spot_request_id.c_str() );
					requestScheddUpdate( this, false );

					gmState = GM_SPOT_SUBMITTED;
					break;
				} else {
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) spot instance request failed: %s: %s\n",
								procID.cluster, procID.proc,
								gahp_error_code.c_str(),
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
						gahp_error_code = "E_TESTING";
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
									procID.cluster, procID.proc,
									gahp_error_code.c_str(),
									errorString.c_str() );
						dprintf( D_FULLDEBUG, "Error transition: GM_SPOT_CANCEL + <GAHP failure> = GM_HOLD\n" );
						gmState = GM_HOLD;
						break;
					}

					// Since we know the request is gone, forget about it.
					SetRequestID( NULL );
					requestScheddUpdate( this, false );

					// Rather than decide if we crashed after cancelling a
					// request but before removing its ID from the job ad,
					// just never remove it.  It won't hurt to try to cancel
					// the request again, and since we already have the
					// instance ID, we know we're not done when we cancel it.
				}

				// Do NOT set probeNow here.  If we came from
				// GM_SAVE_INSTANCE_ID, it's already set.  If we came from
				// recovery, see the argument in recovery as to why not.
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

				// Don't go to GM probe until asked (when the remote
				// job status changes).
				if( ! probeNow ) { break; }

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
				probeNow = false;

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
					gahp_error_code = "E_TESTING";
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
								procID.cluster, procID.proc,
								gahp_error_code.c_str(),
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

				// Spot status results are 5-tuples of request IDs, status
				// strings, AMI IDs (probably superflous), instance IDs,
				// and status codes.
				// There should only ever be one result when probing a job.
				std::string status;
				std::string requestID;
				std::string instanceID;
				std::string statusCode;
				returnStatus.rewind();

				// The GAHP client will EXCEPT() returnStatus.number()
				// isn't 0 or a multiple of 5.  (gahp-client.cpp:7070-86)
				// Should we EXCEPT again here?
				for( int i = 0; i < returnStatus.number(); i += 5 ) {
					requestID = returnStatus.next();
					status = returnStatus.next();
					std::string launchGroup = returnStatus.next();
					instanceID = returnStatus.next();
					statusCode = returnStatus.next();

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

				//
				// Now that we're sure we have the right status, update it.
				//
				remoteJobState = status;
				SetRemoteJobStatus( status.c_str() );

				if( ! statusCode.empty() ) {
					SetRemoteJobStatus( statusCode.c_str() );
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
					gahp_error_code = "E_TESTING";
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
				for( int i = 0; i < returnStatus.number(); i += 5 ) {
					std::string requestID = returnStatus.next();
					std::string state = returnStatus.next();
					std::string launchGroup = returnStatus.next();
					std::string instanceID = returnStatus.next();
					std::string statusCode = returnStatus.next();

					if( launchGroup == m_client_token ) {
						SetRequestID( requestID.c_str() );
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
				// However, if we're removing the job, this just means
				// that we have nothing else to do.
				//
				if( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_SPOT_START;
				}
				} break;

			case GM_SEEK_INSTANCE_ID: {
				// Wait for the next scheduled bulk query.
				if( ! probeNow ) { break; }

				// If the bulk query found this job, it has an instance ID.
				// (If the job had an instance ID before, we would be in
				// an another state.)  Otherwise, the service doesn't know
				// about this job, and we can remove it from the queue.
				if( ! m_remoteJobId.empty() ) {
					WriteGridSubmitEventToUserLog( jobAd );
					gmState = GM_SAVE_INSTANCE_ID;
				} else if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CREATE_KEY_PAIR;
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


BaseResource* EC2Job::GetResource()
{
	return (BaseResource *)myResource;
}


// setup the public name of ec2 remote VM, which can be used the clients
void EC2Job::SetRemoteVMName( const char * newName )
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
	if( client_token != NULL ) {
		m_client_token = client_token;
	} else {
		m_client_token.clear();
	}
	EC2SetRemoteJobId(m_client_token.empty() ? NULL : m_client_token.c_str(),
				   m_remoteJobId.c_str());
}

void EC2Job::SetInstanceId( const char *instance_id )
{
	// Don't unconditionally clear the remote job ID -- if we do,
	// SetInstanceId( m_remoteJobId.c_str() ) does exactly the opposite
	// of what you'd expect, because the c_str() is cleared as well.
	if( instance_id == NULL ) {
		m_remoteJobId.clear();
	} else {
		m_remoteJobId = instance_id;
		jobAd->Assign( ATTR_EC2_INSTANCE_NAME, m_remoteJobId );
	}
	EC2SetRemoteJobId( m_client_token.c_str(),
					m_remoteJobId.empty() ? NULL : m_remoteJobId.c_str() );
}

// EC2SetRemoteJobId() is used to set the value of global variable "remoteJobID"
// Don't call this function directly!
// It doesn't update m_client_token or m_remoteJobId!
// Use SetClientToken() or SetInstanceId() instead.
void EC2Job::EC2SetRemoteJobId( const char *client_token, const char *instance_id )
{
	string full_job_id;
	if ( client_token && client_token[0] ) {
		formatstr( full_job_id, "ec2 %s %s", m_serviceUrl.c_str(), client_token );
		if ( instance_id && instance_id[0] ) {
			// We need this to do bulk status queries.
			myResource->jobsByInstanceID.insert( HashKey( instance_id ), this );
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
		group_names = new StringList( buffer, ", " );
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

void EC2Job::associate_n_attach()
{

	std::string gahp_error_code;
	StringList returnStatus;
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
					gahp_error_code.c_str());
			break;
		}
		gahp_error_code = "";
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
						gahp_error_code.c_str());
				break;
		}
		gahp_error_code = "";
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
							gahp_error_code.c_str());
					break;
			}
			gahp_error_code = "";
		}
	}
}

void EC2Job::StatusUpdate( const char * instanceID,
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
	//
	// I've seen this state fire after a GM_SPOT_START - > GM_SPOT_SUBMITTED
	// transition, and after a GM_SPOT_SUBMITTED -> GM_SPOT_QUERY transition,
	// both of which had updated their EC2SpotRequestIDs -- that is, the
	// SIRs we knew about weren't immediately in the server's response.
	// What's truly disconcerting is that (after GM_SPOT_QUERY ignores the
	// status field) and makes it own query, it gets the right answer...
	// .. for now, let's not scare the user by passing through the "purged"
	// state on spot instances.
	if( m_spot_price.empty() && !m_remoteJobId.empty() && status == NULL ) {
		status = EC2_VM_STATE_PURGED;
	}

	// Update the instance ID, if this is the first time we've seen it.
	if( m_spot_price.empty() && m_remoteJobId.empty() ) {
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
	// only returns true if the status has changed.  SetRemoteJobStatus()
	// can handle NULL statuses, but remoteJobState's assignment operator
	// can't.  One way to get a status update with a NULL status is if
	// a spot instance was purged (e.g., recovery after a long downtime).
	if( SetRemoteJobStatus( status ) ) {
		if( status != NULL ) { remoteJobState = status; }
		probeNow = true;
		SetEvaluateState();
	}
}

// Take a const char * rather than a const std::string & because
// std::string( NULL ) is probably the same as std::string( "" ),
// but those two are not the same in ClassAds.
void EC2Job::SetRequestID( const char * requestID ) {
	if( requestID == NULL ) {
		if( ! m_spot_request_id.empty() ) {
			// If the job is forgetting about its request ID, make sure that
			// the resource does, as well; otherwise, we can have one job
			// updates by both the dedicated and spot batch status processes.
			myResource->spotJobsByRequestID.remove( HashKey( m_spot_request_id.c_str() ) );
		}
		jobAd->AssignExpr( ATTR_EC2_SPOT_REQUEST_ID, "Undefined" );
		m_spot_request_id = std::string();
	} else {
		jobAd->Assign( ATTR_EC2_SPOT_REQUEST_ID, requestID );
		myResource->spotJobsByRequestID.insert( HashKey( requestID ), this );
		m_spot_request_id = requestID;
	}
}

//
// Don't wait forever for resources to come back up.  Particularly useful
// for the test suite (if the simulator breaks).
//
// JaimeF recalls no wisdom as to why the Globus job type implements this
// as a per-job timer, instead of a per-resource timer.
//
// This function executes asynchronously with respect to doEvaluateState(),
// but since we're putting the job on hold, we know we're not losing any
// information.  If the job goes on hold with a GAHP command pending, it's
// no different than if that command had timed out.  (The only cases where
// doEvaluateState() doesn't immediately put the job on hold when a GAHP
// command fails are ec2_vm_destroy_keypair() and ec2_vm_start(). For
// the former, if the job is removed or completed, it will be removed or
// completed when released; if we have no job ID, then we'll restart it
// from scratch.  This can happen if, forex, we put the job on hold before
// it starts.  However, in this case, restarting the job is the right thing
// to do.  More generally, the keypair ID we generate is entirely determined
// by invariant information about the job, so we'll try to delete it again
// later if we do resubmit.  For the latter, we won't execute any retries
// (delayed or not), but since the resource is down, that's OK.  (We'll
// start trying again when the job is released.))
//

void EC2Job::ResourceLeaseExpired() {
	errorString = "Resource was down for too long.";
	dprintf( D_ALWAYS, "(%d.%d) Putting job on hold: resource was down for too long.\n", procID.cluster, procID.proc );
	gmState = GM_HOLD;
	resourceLeaseTID = -1;
	SetEvaluateState();
}

void EC2Job::NotifyResourceDown() {
	BaseJob::NotifyResourceDown();

	if( resourceLeaseTID != -1 ) { return; }

	time_t now = time( NULL );
	int leaseDuration = param_integer( "EC2_RESOURCE_TIMEOUT", -1 );
	if( leaseDuration == -1 ) {
		return;
	}

	// We don't need to update/maintain ATTR_GRID_RESOURCE_UNAVAILABLE_TIME;
	// BaseJob::NotifyResource[Down|Up] does that for us.
	int leaseBegan = 0;
	jobAd->LookupInteger( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME, leaseBegan );
	if( leaseBegan != 0 ) {
		leaseDuration = leaseDuration - (now - leaseBegan);
	}

	if( leaseDuration > 0 ) {
		resourceLeaseTID = daemonCore->Register_Timer( leaseDuration,
			(TimerHandlercpp) & EC2Job::ResourceLeaseExpired,
			"ResourceLeaseExpired", (Service *) this );
	} else {
		ResourceLeaseExpired();
	}
}

void EC2Job::NotifyResourceUp() {
	BaseJob::NotifyResourceUp();

	if( resourceLeaseTID != -1 ) {
		daemonCore->Cancel_Timer( resourceLeaseTID );
		resourceLeaseTID = -1;
	}
}
