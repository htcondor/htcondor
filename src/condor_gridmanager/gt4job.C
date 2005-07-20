/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "environ.h"  // for Environ object
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"

#include "gridmanager.h"
#include "gt4job.h"
#include "condor_config.h"
#include "my_username.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMIT_COMMIT		6
#define GM_SUBMITTED			7
#define GM_DONE_SAVE			8
#define GM_DONE_COMMIT			9
#define GM_CANCEL				10
#define GM_FAILED				11
#define GM_DELETE				12
#define GM_CLEAR_REQUEST		13
#define GM_HOLD					14
#define GM_PROXY_EXPIRED		15
#define GM_EXTEND_LIFETIME		16
#define GM_PROBE_JOBMANAGER		17
#define GM_START				18
#define GM_GENERATE_ID			19
#define GM_DELEGATE_PROXY		20
#define GM_SUBMIT_ID_SAVE		21

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_EXTEND_LIFETIME",
	"GM_PROBE_JOBMANAGER",
	"GM_START",
	"GM_GENERATE_ID",
	"GM_DELEGATE_PROXY",
	"GM_SUBMIT_ID_SAVE"
};

#define GT4_JOB_STATE_PENDING		1
#define GT4_JOB_STATE_ACTIVE		2
#define GT4_JOB_STATE_FAILED		4
#define GT4_JOB_STATE_DONE			8
#define GT4_JOB_STATE_SUSPENDED		16
#define GT4_JOB_STATE_UNSUBMITTED	32
#define GT4_JOB_STATE_STAGE_IN		64
#define GT4_JOB_STATE_STAGE_OUT		128
#define GT4_JOB_STATE_CLEAN_UP		256
#define GT4_JOB_STATE_UNKNOWN		0


// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s %s\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error==GAHPCLIENT_COMMAND_TIMED_OUT?"timed out":"failed")

#define CHECK_PROXY \
{ \
	if ( PROXY_NEAR_EXPIRED( jobProxy ) && gmState != GM_PROXY_EXPIRED ) { \
		dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire\n", \
				 procID.cluster, procID.proc ); \
		gmState = GM_PROXY_EXPIRED; \
		break; \
	} \
}

static bool WriteGT4SubmitEventToUserLog( ClassAd *job_ad );
static bool WriteGT4SubmitFailedEventToUserLog( ClassAd *job_ad,
												const char *failure_string );
static bool WriteGT4ResourceUpEventToUserLog( ClassAd *job_ad );
static bool WriteGT4ResourceDownEventToUserLog( ClassAd *job_ad );

#define HASH_TABLE_SIZE			500

HashTable <HashKey, GT4Job *> GT4JobsByContact( HASH_TABLE_SIZE,
												hashFunction );

const char *
gt4JobId( const char *contact )
{
/*
	static char buff[1024];
	char *first_end;
	char *second_begin;

	ASSERT( strlen(contact) < sizeof(buff) );

	first_end = strrchr( contact, ':' );
	ASSERT( first_end );

	second_begin = strchr( first_end, '/' );
	ASSERT( second_begin );

	strncpy( buff, contact, first_end - contact );
	strcpy( buff + ( first_end - contact ), second_begin );

	return buff;
*/
	return contact;
}

void
gt4GramCallbackHandler( void *user_arg, const char *job_contact,
						const char *state, const char *fault,
						const int exit_code )
{
	int rc;
	GT4Job *this_job;

	// Find the right job object
	rc = GT4JobsByContact.lookup( HashKey( gt4JobId(job_contact) ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gt4GramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %s, ignoring\n",
			job_contact, state );
		return;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %s, fault %s, exit code %d\n",
			 this_job->procID.cluster, this_job->procID.proc, state,
			 fault ? fault : "(null)", exit_code );

	this_job->GramCallback( state, fault, exit_code );
}

void GT4JobInit()
{
}

void GT4JobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	GT4Job::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	GT4Resource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GT4Job::setGahpCallTimeout( tmp_int );
	GT4Resource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GT4Job::setConnectFailureRetry( tmp_int );

	// Tell all the resource objects to deal with their new config values
	GT4Resource *next_resource;

	GT4Resource::ResourcesByName.startIterations();

	while ( GT4Resource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

const char *GT4JobAdConst = "JobUniverse =?= 9 && (JobGridType == \"gt4\") =?= True";

bool GT4JobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);
	if ( !must_expand ) {
		char resource_name[800];
		jobad->LookupString(ATTR_GLOBUS_RESOURCE, resource_name);
		if ( strstr(resource_name,"$$") ) {
			must_expand = 1;
		}
	}

	return must_expand != 0;
}

BaseJob *GT4JobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GT4Job( jobad );
}


static
const char *xml_stringify( const char *string )
{
	static MyString dst;

	dst = string;
	dst.replaceString( "&", "&amp;" );
	dst.replaceString( "<", "&lt;" );
	dst.replaceString( ">", "&gt;" );

	return dst.Value();
}

static
const char *xml_stringify( const MyString& src )
{
	return xml_stringify( src.Value() );
}

int Gt4JobStateToInt( const char *status ) {
	if ( status == NULL ) {
		return GT4_JOB_STATE_UNKNOWN;
	} else if ( !strcmp( status, "Pending" ) ) {
		return GT4_JOB_STATE_PENDING;
	} else if ( !strcmp( status, "Active" ) ) {
		return GT4_JOB_STATE_ACTIVE;
	} else if ( !strcmp( status, "Failed" ) ) {
		return GT4_JOB_STATE_FAILED;
	} else if ( !strcmp( status, "Done" ) ) {
		return GT4_JOB_STATE_DONE;
	} else if ( !strcmp( status, "Suspended" ) ) {
		return GT4_JOB_STATE_SUSPENDED;
	} else if ( !strcmp( status, "Unsubmitted" ) ) {
		return GT4_JOB_STATE_UNSUBMITTED;
	} else if ( !strcmp( status, "StageIn" ) ) {
		return GT4_JOB_STATE_STAGE_IN;
	} else if ( !strcmp( status, "StageOut" ) ) {
		return GT4_JOB_STATE_STAGE_OUT;
	} else if ( !strcmp( status, "CleanUp" ) ) {
		return GT4_JOB_STATE_CLEAN_UP;
	} else {
		return GT4_JOB_STATE_UNKNOWN;
	}
}

const char *Gt4JobStateToString( int status )
{
	switch ( status ) {
	case GT4_JOB_STATE_PENDING:
		return "Pending";
	case GT4_JOB_STATE_ACTIVE:
		return "Active";
	case GT4_JOB_STATE_FAILED:
		return "Failed";
	case GT4_JOB_STATE_DONE:
		return "Done";
	case GT4_JOB_STATE_SUSPENDED:
		return "Suspended";
	case GT4_JOB_STATE_UNSUBMITTED:
		return "Unsubmitted";
	case GT4_JOB_STATE_STAGE_IN:
		return "StageIn";
	case GT4_JOB_STATE_STAGE_OUT:
		return "StageOut";
	case GT4_JOB_STATE_CLEAN_UP:
		return "CleanUp";
	case GT4_JOB_STATE_UNKNOWN:
		return "Unknown";
	default:
		return "??????";
	}
}

int GT4Job::probeInterval = 300;			// default value
int GT4Job::submitInterval = 300;			// default value
int GT4Job::gahpCallTimeout = 300;			// default value
int GT4Job::maxConnectFailures = 3;			// default value

GT4Job::GT4Job( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	bool job_already_submitted = false;
	char *error_string = NULL;
	char *gahp_path = NULL;

	RSL = NULL;
	callbackRegistered = false;
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	globusStateFaultString = 0;
	callbackGlobusState = 0;
	callbackGlobusStateFaultString = "";
	gmState = GM_INIT;
	globusState = GT4_JOB_STATE_UNSUBMITTED;
	resourcePingPending = false;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	jmProxyExpireTime = 0;
	jmLifetime = 0;
	connect_failure_counter = 0;
	resourceManagerString = NULL;
	jobmanagerType = NULL;
	myResource = NULL;
	jobProxy = NULL;
	gramCallbackContact = NULL;
	gahp = NULL;
	submit_id = NULL;
	delegatedCredentialURI = NULL;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_X509_USER_PROXY, buff );
	if ( buff[0] != '\0' ) {
		jobProxy = AcquireProxy( buff, evaluateStateTid );
		if ( jobProxy == NULL ) {
			dprintf( D_ALWAYS, "(%d.%d) error acquiring proxy!\n",
					 procID.cluster, procID.proc );
			error_string = "Failed to acquire proxy";
			goto error_exit;
		}
	} else {
		dprintf( D_ALWAYS, "(%d.%d) %s not set in job ad!\n",
				 procID.cluster, procID.proc, ATTR_X509_USER_PROXY );
	}

	gahp_path = param("GT4_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "GT4_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "GT4/%s", jobProxy->subject->subject_name );
	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GLOBUS_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		resourceManagerString = strdup( buff );
	} else {
		error_string = "GT4Resource is not set in the job ad";
		goto error_exit;
	}

		// Find/create an appropriate GT4Resource for this job
	myResource = GT4Resource::FindOrCreateResource( resourceManagerString,
													jobProxy->subject->subject_name);
	if ( myResource == NULL ) {
		error_string = "Failed to initialized GT4Resource object";
		goto error_exit;
	}

	resourceDown = false;
	resourceStateKnown = false;
	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		SetJobContact( buff );
		jobAd->SetDirtyFlag( ATTR_GLOBUS_CONTACT_STRING, false );
		job_already_submitted = true;
	}

	if (jobAd->LookupString ( ATTR_GLOBUS_JOBMANAGER_TYPE, buff )) {
		jobmanagerType = strdup( buff );
	}

	if (jobAd->LookupString ( ATTR_GLOBUS_SUBMIT_ID, buff )) {
		submit_id = strdup ( buff );
	}

	if ( jobAd->LookupString( ATTR_GLOBUS_DELEGATION_URI, buff ) ) {
		delegatedCredentialURI = strdup( buff );
		myResource->registerDelegationURI( delegatedCredentialURI, jobProxy );
	}

	jobAd->LookupInteger( ATTR_GLOBUS_STATUS, globusState );

	globusErrorString = "";

	iwd[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( error_string ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

GT4Job::~GT4Job()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( jobContact ) {
		GT4JobsByContact.remove( HashKey( gt4JobId(jobContact) ) );
		free( jobContact );
	}
	if ( RSL ) {
		delete RSL;
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( jobProxy ) {
		ReleaseProxy( jobProxy, evaluateStateTid );
	}
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( submit_id != NULL ) {
		free( submit_id );
	}
	if ( jobmanagerType != NULL ) {
		free( jobmanagerType );
	}
	if ( delegatedCredentialURI != NULL) {
		free( delegatedCredentialURI );
	}

}

void GT4Job::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int GT4Job::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

	if ( gahp ) {
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( jobProxy );

			gahp->setMode( GahpClient::blocking );

			err = gahp->gt4_gram_client_callback_allow( gt4GramCallbackHandler,
														NULL,
														&gramCallbackContact );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) Error enabling GRAM callback, err=%d\n", 
						 procID.cluster, procID.proc, err );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setMode( GahpClient::normal );

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			// If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				if ( globusState == GT4_JOB_STATE_STAGE_IN ||
					 globusState == GT4_JOB_STATE_PENDING ||
					 globusState == GT4_JOB_STATE_ACTIVE ||
					 globusState == GT4_JOB_STATE_SUSPENDED ||
					 globusState == GT4_JOB_STATE_STAGE_OUT ||
					 globusState == GT4_JOB_STATE_DONE ) {
					submitLogged = true;
				}
				if ( globusState == GT4_JOB_STATE_ACTIVE ||
					 globusState == GT4_JOB_STATE_SUSPENDED ||
					 globusState == GT4_JOB_STATE_STAGE_OUT ||
					 globusState == GT4_JOB_STATE_DONE ) {
					executeLogged = true;
				}

				gmState = GM_REGISTER;
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_callback_register( jobContact,
														gramCallbackContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_callback_register()", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			callbackRegistered = true;
			probeNow = true;
			//gmState = GM_STDIO_UPDATE;
//			gmState = GM_CANCEL;
			gmState = GM_SUBMITTED;
			} break;
		case GM_STDIO_UPDATE: {
/*
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			CHECK_PROXY;
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->Value(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_UPDATE)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
			if ( globusState == GT4_JOB_STATE_UNSUBMITTED ) {
				gmState = GM_SUBMIT_COMMIT;
			} else {
				gmState = GM_SUBMITTED;
			}
*/
			gmState = GM_CANCEL;
			} break;
 		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else {
				gmState = GM_DELEGATE_PROXY;
			}
			} break;
 		case GM_DELEGATE_PROXY: {
			const char *deleg_uri;
				// TODO What happens if Gt4Resource can't delegate proxy?
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			}
			if ( delegatedCredentialURI != NULL ) {
				gmState = GM_GENERATE_ID;
				break;
			}
			deleg_uri = myResource->getDelegationURI( jobProxy );
			if ( deleg_uri == NULL ) {
					// proxy still needs to be delegated. Wait.
				break;
			}
			delegatedCredentialURI = strdup( deleg_uri );
			gmState = GM_GENERATE_ID;
			jobAd->Assign( ATTR_GLOBUS_DELEGATION_URI,
						   delegatedCredentialURI );
		} break;
		case GM_GENERATE_ID: {

			// TODO: allow REMOVED or HELD jobs to break out of this state
			if (submit_id) {
				gmState = GM_SUBMIT_ID_SAVE;
				break;
			}

			rc = gahp->gt4_generate_submit_id( &submit_id );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error generating submit id\n",
						 procID.cluster, procID.proc );
				errorString = "Failed to generate submit id";
				gmState = GM_HOLD;
			} else {
				jobAd->Assign( ATTR_GLOBUS_SUBMIT_ID, submit_id );
				gmState = GM_SUBMIT_ID_SAVE;
			}
		} break;
		case GM_SUBMIT_ID_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT;
			}
		} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
 			char *job_contact = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				if ( globusErrorString == "" ) {
					globusErrorString = "Attempts to submit failed";
				}
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					myResource->CancelSubmit(this);
					gmState = GM_HOLD;
					break;
				}
				
				if (!jobmanagerType) {
					jobmanagerType = strdup ( "Fork" );
				}

				rc = gahp->gt4_gram_client_job_create( 
													  submit_id,
										resourceManagerString,
										jobmanagerType,
										gramCallbackContact,
										RSL->Value(),
										NULL,
										&job_contact );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				jmProxyExpireTime = jobProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					jmLifetime = time(NULL) + 12*60*60;
					callbackRegistered = true;
					SetJobContact( job_contact );
					free( job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_create()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					globusErrorString = gahp->getErrorString();
					WriteGT4SubmitFailedEventToUserLog( jobAd,
														gahp->getErrorString() );
					myResource->CancelSubmit( this );
					gmState = GM_UNSUBMITTED;
					reevaluate_state = true;
				}
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_UNSUBMITTED;
			} else {
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_SUBMIT_SAVE: {
			// Save the jobmanager's contact for a new gram submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_COMMIT;
			}
			} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the jobmanager's contact, commit the
			// gram job submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_start( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_start()", rc );
					globusErrorString = gahp->getErrorString();
					WriteGT4SubmitFailedEventToUserLog( jobAd, gahp->getErrorString() );
					gmState = GM_CANCEL;
				} else {
						// We don't want an old or zeroed lastProbeTime
						// make us do a probe immediately after submitting
						// the job, so set it to now
					lastProbeTime = time(NULL);
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( globusState == GT4_JOB_STATE_DONE ) {
				gmState = GM_DONE_SAVE;
			} else if ( globusState == GT4_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( GetCallbacks() == true ) {
					reevaluate_state = true;
					break;
				}
				if ( jmLifetime < time(NULL) + 11*60*60 ) {
					gmState = GM_EXTEND_LIFETIME;
					break;
				}
				now = time(NULL);
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_EXTEND_LIFETIME: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				time_t new_lifetime = 12*60*60;
				rc = gahp->gt4_set_termination_time( jobContact,
													 new_lifetime );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					break;
				}
				jmLifetime = new_lifetime;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status = NULL;
				char *fault = NULL;
				int exit_code = -1;
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_status( jobContact, &status,
													   &fault, &exit_code );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_status()", rc );
					globusErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					if ( status ) {
						free( status );
					}
					break;
				}
				UpdateGlobusState( Gt4JobStateToInt( status ), fault );
				if ( status ) {
					free( status );
				}
				if ( fault ) {
					free( fault );
				}
				ClearCallbacks();
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy()", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				SetJobContact( NULL );
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( jobContact != NULL ) {
					SetJobContact( NULL );
					globusState = GT4_JOB_STATE_UNSUBMITTED;
					jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
					requestScheddUpdate( this );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GT4_JOB_STATE_DONE &&
				 globusState != GT4_JOB_STATE_FAILED ) {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_destroy( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy()", rc );
					globusErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				myResource->CancelSubmit( this );
				SetJobContact( NULL );
			}
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.

			// Sending a COMMIT_END here means we no longer care
			// about this job submission. Either we know the job
			// isn't pending/running or the user has told us to
			// forget lost job submissions.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			SetJobContact( NULL );
			myResource->CancelSubmit( this );
			globusState = GT4_JOB_STATE_UNSUBMITTED;
			jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
			requestScheddUpdate( this );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				//gmState = GM_CLEAR_REQUEST;
				gmState= GM_HOLD;
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
			if ( (jobContact != NULL || globusState == GT4_JOB_STATE_FAILED) 
				     && condorState != REMOVED 
					 && wantResubmit == 0 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = 0;
				dprintf(D_ALWAYS,
						"(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			if ( globusState != GT4_JOB_STATE_UNSUBMITTED ) {
				globusState = GT4_JOB_STATE_UNSUBMITTED;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
			}
			globusStateFaultString = "";
			globusErrorString = "";
			errorString = "";
			ClearCallbacks();
			myResource->CancelSubmit( this );
			if ( jobContact != NULL ) {
				SetJobContact( NULL );
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
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
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
			done = requestScheddUpdate( this );
			if ( !done ) {
				break;
			}
			DeleteOutput();
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?
			if ( jobContact &&
				 globusState != GT4_JOB_STATE_UNKNOWN ) {
				globusState = GT4_JOB_STATE_UNKNOWN;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
				//UpdateGlobusState( GT4_JOB_STATE_UNKNOWN, NULL );
			} else if ( !jobContact &&
						globusState != GT4_JOB_STATE_UNSUBMITTED ) {
				globusState = GT4_JOB_STATE_UNSUBMITTED;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
			}
			// If the condor state is already HELD, then someone already
			// HELD it, so don't update anything else.
			if ( condorState != HELD ) {

				// Set the hold reason as best we can
				// TODO: set the hold reason in a more robust way.
				char holdReason[1024];
				holdReason[0] = '\0';
				holdReason[sizeof(holdReason)-1] = '\0';
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason,
									 sizeof(holdReason) - 1 );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.Value(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' &&
					 !globusStateFaultString.IsEmpty() ) {

					snprintf( holdReason, 1024, "Globus error: %s",
							  globusStateFaultString.Value() );
				}
				if ( holdReason[0] == '\0' && !globusErrorString.IsEmpty() ) {
					snprintf( holdReason, 1024, "Globus error: %s",
							  globusErrorString.Value() );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		case GM_PROXY_EXPIRED: {
			// The proxy for this job is either expired or about to expire.
			// If requested, put the job on hold. Otherwise, wait for the
			// proxy to be refreshed, then resume handling the job.
			now = time(NULL);
			if ( jobProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
				gmState = GM_START;
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || globusState != old_globus_state ) {
			reevaluate_state = true;
		}
		if ( globusState != old_globus_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					Gt4JobStateToString(old_globus_state),
//					Gt4JobStateToString(globusState));
			enteredCurrentGlobusState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending globus call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( connect_failure && !resourceDown ) {
		if ( connect_failure_counter < maxConnectFailures ) {
				// We are seeing a lot of failures to connect
				// with Globus 2.2 libraries, often due to GSI not able 
				// to authenticate.
			connect_failure_counter++;
			int retry_secs = param_integer(
				"GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL",5);
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure (try #%d), retrying in %d secs\n",
				procID.cluster,procID.proc,connect_failure_counter,retry_secs);
			daemonCore->Reset_Timer( evaluateStateTid, retry_secs );
		} else {
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure, requesting a ping of the resource\n",
				procID.cluster,procID.proc);
			resourcePingPending = true;
			myResource->RequestPing( this );
		}
	}

	return TRUE;
}

void GT4Job::SetJobContact( const char *job_contact )
{
		// Clear the delegation URI whenever the job contact string
		// is cleared
	if ( job_contact == NULL && delegatedCredentialURI != NULL ) {
		free( delegatedCredentialURI );
		delegatedCredentialURI = NULL;
		jobAd->AssignExpr( ATTR_GLOBUS_DELEGATION_URI, "Undefined" );
	}

	if ( jobContact != NULL && job_contact != NULL &&
		 strcmp( jobContact, job_contact ) == 0 ) {
		return;
	}
	if ( jobContact != NULL ) {
		GT4JobsByContact.remove( HashKey( gt4JobId(jobContact) ) );
		free( jobContact );
		jobContact = NULL;
		jobAd->Assign( ATTR_GLOBUS_CONTACT_STRING, NULL_JOB_CONTACT );
	}
	if ( job_contact != NULL ) {
		jobContact = strdup( job_contact );
		GT4JobsByContact.insert( HashKey( gt4JobId(jobContact) ), this );
		jobAd->Assign( ATTR_GLOBUS_CONTACT_STRING, jobContact );
	}
	requestScheddUpdate( this );
}

void GT4Job::NotifyResourceDown()
{
	resourceStateKnown = true;
	if ( resourceDown == false ) {
		WriteGT4ResourceDownEventToUserLog( jobAd );
	}
	resourceDown = true;
	resourcePingPending = false;
	// set downtime timestamp?
	SetEvaluateState();
}

void GT4Job::NotifyResourceUp()
{
	resourceStateKnown = true;
	if ( resourceDown == true ) {
		WriteGT4ResourceUpEventToUserLog( jobAd );
	}
	resourceDown = false;
	resourcePingPending = false;
	SetEvaluateState();
}

bool GT4Job::AllowTransition( int new_state, int old_state )
{

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
    if ( new_state == old_state ||
		 new_state == GT4_JOB_STATE_UNSUBMITTED ||
		 old_state == GT4_JOB_STATE_DONE ||
		 old_state == GT4_JOB_STATE_FAILED ||
		 ( new_state == GT4_JOB_STATE_STAGE_IN &&
		   old_state != GT4_JOB_STATE_UNSUBMITTED) ||
		 ( new_state == GT4_JOB_STATE_PENDING &&
		   old_state != GT4_JOB_STATE_UNSUBMITTED &&
		   old_state != GT4_JOB_STATE_STAGE_IN) ||
		 ( old_state == GT4_JOB_STATE_STAGE_OUT &&
		   new_state != GT4_JOB_STATE_DONE &&
		   new_state != GT4_JOB_STATE_FAILED ) ) {
		return false;
	}

	return true;
}


void GT4Job::UpdateGlobusState( int new_state, const char *new_fault )
{
	bool allow_transition;

	allow_transition = AllowTransition( new_state, globusState );

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				Gt4JobStateToString(globusState),
				Gt4JobStateToString(new_state));

		if ( ( new_state == GT4_JOB_STATE_ACTIVE ||
			   new_state == GT4_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state == GT4_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		if ( globusState == GT4_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GT4_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				if ( !submitFailedLogged ) {
					WriteGT4SubmitFailedEventToUserLog( jobAd,
														new_fault );
					submitFailedLogged = true;
				}
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				if ( !submitLogged ) {
					WriteGT4SubmitEventToUserLog( jobAd );
					submitLogged = true;
				}
				jobAd->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
									  num_globus_submits );
				num_globus_submits++;
				jobAd->Assign( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GT4_JOB_STATE_ACTIVE ||
			  new_state == GT4_JOB_STATE_STAGE_OUT ||
			  new_state == GT4_JOB_STATE_DONE ||
			  new_state == GT4_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		jobAd->Assign( ATTR_GLOBUS_STATUS, new_state );

		globusState = new_state;
		globusStateFaultString = new_fault;
		enteredCurrentGlobusState = time(NULL);

		requestScheddUpdate( this );

		SetEvaluateState();
	}
}

void GT4Job::GramCallback( const char *new_state, const char *new_fault,
						   const int new_exit_code )
{
	int new_state_int = Gt4JobStateToInt( new_state );
	if ( AllowTransition(new_state_int,
						 callbackGlobusState ?
						 callbackGlobusState :
						 globusState ) ) {

		callbackGlobusState = new_state_int;
		callbackGlobusStateFaultString = new_fault ? new_fault : "";

//		if ( new_exit_code != GT4_NO_EXIT_CODE ) {
		if ( new_state_int == GT4_JOB_STATE_DONE ) {
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
			jobAd->Assign( ATTR_ON_EXIT_CODE, new_exit_code );
		}

		SetEvaluateState();
	}
}

bool GT4Job::GetCallbacks()
{
	if ( callbackGlobusState != 0 ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateFaultString.Value() );

		ClearCallbacks();
		return true;
	} else {
		return false;
	}
}

void GT4Job::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateFaultString = "";
}

BaseResource *GT4Job::GetResource()
{
	return (BaseResource *)myResource;
}


// Build submit RSL.. er... XML
MyString *GT4Job::buildSubmitRSL()
{
	MyString *rsl = new MyString;
	MyString local_iwd = "";
	MyString remote_iwd = "";
	MyString local_executable = "";
	MyString remote_executable = "";
	bool create_remote_iwd = false;
	bool transfer_executable = true;
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;
	StringList stage_in_list;
	StringList stage_out_list;
	bool staging_input = false;

	char *local_url_base = param("GRIDFTP_URL_BASE");
	if ( local_url_base == NULL ) {
		errorString = "GRIDFTP_URL_BASE not defined";
		return NULL;
	}

		// Once we add streaming support, remove this
	if ( streamOutput || streamError ) {
		errorString = "Streaming not supported";
		return NULL;
	}

	MyString delegation_epr;
	MyString delegation_uri = delegatedCredentialURI;
	int pos = delegation_uri.FindChar( '?' );
	delegation_epr.sprintf( "<ns1:Address xsi:type=\"ns1:AttributedURI\">%s</ns1:Address>", delegation_uri.Substr( 0, pos-1 ).Value() );
	delegation_epr.sprintf_cat( "<ns1:ReferenceProperties xsi:type=\"ns1:ReferencePropertiesType\">" );
	delegation_epr.sprintf_cat( "<ns1:DelegationKey xmlns:ns1=\"http://www.globus.org/08/2004/delegationService\">%s</ns1:DelegationKey>", delegation_uri.Substr( pos+1, delegation_uri.Length()-1 ).Value() );
	delegation_epr.sprintf_cat( "</ns1:ReferenceProperties><ns1:ReferenceParameters xsi:type=\"ns1:ReferenceParametersType\"/>" );
/*
	delegation_epr.sprintf( "<Address>%s</Address>", delegation_uri.Substr( 0, pos-1 ).Value() );
	delegation_epr.sprintf_cat( "<ReferenceProperties>" );
	delegation_epr.sprintf_cat( "<DelegationKey>%s</DelegationKey>", delegation_uri.Substr( pos+1, delegation_uri.Length()-1 ).Value() );
	delegation_epr.sprintf_cat( "</ReferenceProperties><ReferenceParameters/>" );
*/

		// Set our local job working directory
	if ( jobAd->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
		local_iwd = attr_value;
		int len = strlen(attr_value);
		if ( len > 1 && attr_value[len - 1] != '/' ) {
			local_iwd += '/';
		}
	} else {
		local_iwd = "/";
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Set our remote job working directory
	if ( jobAd->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
			// The user gave us a remote IWD, use it.
		create_remote_iwd = false;
		remote_iwd = attr_value;
	} else {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.

		// Note that ${SCRATCH_DIR} is no longer supported by globus
		// Instead we'll upload an empty directory to be our scratch dir
		
		create_remote_iwd = true;
		ASSERT (submit_id);	// append submit_id for uniqueness, fool
		char *my_submit_id = submit_id;
			// Avoid having a colon in our job scratch directory name, for
			// any systems that may not like that.
		if ( strncmp( my_submit_id, "uuid:", 5 ) == 0 ) {
			my_submit_id = &my_submit_id[5];
		}
		// The GT4 server expects a leading slash *before* variable
		// substitution is completed for some reason. See globus bugzilla
		// ticket 2846 for details. To keep it happy, throw in an extra
		// slash, even though this'll result in file:////....
		remote_iwd.sprintf ("/${GLOBUS_SCRATCH_DIR}/job_%s", my_submit_id);
	}

	if ( remote_iwd[remote_iwd.Length()-1] != '/' ) {
		remote_iwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	//Start off the RSL
	rsl->sprintf( "<job>" );


	//We're assuming all job clasads have a command attribute
	jobAd->LookupString( ATTR_JOB_CMD, &attr_value );

	transfer_executable = true;
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_executable );

	if ( transfer_executable == false ) {
			// The executable is already on the remote machine. Leave the
			// path alone.
		remote_executable = attr_value;
	} else {
			// The executable needs to be transferred. Construct the
			// pathname of the transferred file.
		remote_executable = remote_iwd;
		remote_executable += condor_basename( attr_value );

			// Figure out where the local copy of the executable is
		char *spooldir = param("SPOOL");
		if ( spooldir ) {
			char *source = gen_ckpt_name(spooldir,procID.cluster,ICKPT,0);
			free(spooldir);
			if ( access(source,F_OK | X_OK) >= 0 ) {
					// we can access an executable in the spool dir
				local_executable = source;
			}
		}
		if ( local_executable.IsEmpty() ) {
				// didn't find any executable in the spool directory,
				// so use what is explicitly stated in the job ad

				// We're assuming here that the submitter gives us full
				// paths for executables that are to be transferred.
			local_executable = attr_value;
		}
	}

	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	*rsl += printXMLParam( "executable", remote_executable.Value() );
	
	*rsl += printXMLParam( "directory", remote_iwd.Value() );

		// parse arguments
	if ( jobAd->LookupString(ATTR_JOB_ARGUMENTS, &attr_value) && *attr_value ) {
		StringList arg_list( attr_value, " " );
		char *arg;

		arg_list.rewind();
		while ( (arg = arg_list.next()) != NULL ) {
			*rsl += printXMLParam ("argument", xml_stringify(arg));
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Start building the list of files to be staged in
	if ( jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		stage_in_list.initializeFromString( attr_value );
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Start building the list of files to be staged out
	if ( jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		 *attr_value ) {
		stage_out_list.initializeFromString( attr_value );
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

		// For stdin/out/err, always give a full pathname, because GT4
		// doesn't resolve relative paths before handing to the local
		// scheduler, which can interpret relative paths as relative to
		// the user's home directory. Details in globus bugzilla ticket
		// 2886.

		// standard input
	if ( jobAd->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		bool transfer = true;
		jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer );

		if ( transfer == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stdin", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stdin", buff.Value() );
			stage_in_list.append( attr_value );
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// standard output
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		bool transfer = true;
		jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, transfer );

		if ( transfer == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stdout", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stdout", buff.Value() );
			stage_out_list.append( attr_value );
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// standard error
	if ( jobAd->LookupString(ATTR_JOB_ERROR, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		bool transfer = true;
		jobAd->LookupBool( ATTR_TRANSFER_ERROR, transfer );

		if ( transfer == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stderr", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stderr", buff.Value() );
			stage_out_list.append( attr_value );
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Now add the input staging directives to the job description
	if ( create_remote_iwd || transfer_executable ||
		 !stage_in_list.isEmpty() ) {

		staging_input = true;

		*rsl += "<fileStageIn>";
		*rsl += "<maxAttempts>5</maxAttempts>";

		*rsl += "<transferCredentialEndpoint xsi:type=\"ns1:EndpointReferenceType\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:ns1=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";

		// First upload an emtpy dummy directory
		// This will be the job's sandbox directory
		if ( create_remote_iwd ) {
			*rsl += "<transfer>";
			buff.sprintf( "%s%s/", local_url_base, getDummyJobScratchDir() );
			*rsl += printXMLParam( "sourceUrl", buff.Value() );
			buff.sprintf( "file://%s", remote_iwd.Value() );
			*rsl += printXMLParam( "destinationUrl", buff.Value());
			*rsl += "</transfer>";
		}

		// Next, add the executable if needed
		if ( transfer_executable ) {
			*rsl += "<transfer>";
			buff.sprintf( "%s%s", local_url_base, local_executable.Value() );
			*rsl += printXMLParam( "sourceUrl", buff.Value() );
			buff.sprintf( "file://%s", remote_executable.Value() );
			*rsl += printXMLParam( "destinationUrl", buff.Value());
			*rsl += "</transfer>";
		}

		// Finally, deal with any other files we might wish to transfer
		if ( !stage_in_list.isEmpty() ) {
			char *filename;

			stage_in_list.rewind();
			while ( (filename = stage_in_list.next()) != NULL ) {

				*rsl += "<transfer>";
				buff.sprintf( "%s%s%s", local_url_base,
							  filename[0] == '/' ? "" : local_iwd.Value(),
							  filename );
				*rsl += printXMLParam ("sourceUrl", 
									   buff.Value());
				buff.sprintf ("file://%s%s",
							  remote_iwd.Value(),
							  condor_basename (filename));
				*rsl += printXMLParam ("destinationUrl", 
									   buff.Value());
				*rsl += "</transfer>";

			}
		}

		*rsl += "</fileStageIn>";
	}

		// Now add the output staging directives to the job description
		// All the files we need are in stage_out_list
	if ( !stage_out_list.isEmpty() ) {
		char *filename;

		*rsl += "<fileStageOut>";
		*rsl += "<maxAttempts>5</maxAttempts>";

		*rsl += "<transferCredentialEndpoint xsi:type=\"ns1:EndpointReferenceType\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:ns1=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";

		stage_out_list.rewind();
		while ( (filename = stage_out_list.next()) != NULL ) {

			*rsl += "<transfer>";
			buff.sprintf ("file://%s%s",
						  remote_iwd.Value(),
						  condor_basename (filename));
			*rsl += printXMLParam ("sourceUrl", 
								   buff.Value());
			buff.sprintf( "%s%s%s", local_url_base,
						  filename[0] == '/' ? "" : local_iwd.Value(),
						  filename );
			*rsl += printXMLParam ("destinationUrl", 
								   buff.Value());
			*rsl += "</transfer>";

		}

		*rsl += "</fileStageOut>";
	}

		// Add a cleanup directive to remove the remote iwd,
		// if we created one
	if ( create_remote_iwd ) {
		*rsl += "<fileCleanUp>";
		*rsl += "<transferCredentialEndpoint xsi:type=\"ns1:EndpointReferenceType\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:ns1=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";
		*rsl += "<deletion><file>file://" + remote_iwd +
			"</file></deletion></fileCleanUp>";
	}

	if ( jobAd->LookupString(ATTR_JOB_ENVIRONMENT, &attr_value) && *attr_value ) {
		Environ env_obj;
		env_obj.add_string(attr_value);
		char **env_vec = env_obj.get_vector();

		for ( int i = 0; env_vec[i]; i++ ) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			
			*equals = '\0';

			*rsl += "<environment>";
			*rsl += printXMLParam ("name", env_vec[i]);
			*rsl += printXMLParam ("value", equals + 1);
			*rsl += "</environment>";

			*equals = '=';
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	*rsl += "<jobCredentialEndpoint xsi:type=\"ns1:EndpointReferenceType\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:ns1=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">";
//	*rsl += "<jobCredentialEndpoint>";
	*rsl += delegation_epr;
	*rsl += "</jobCredentialEndpoint>";

	*rsl += "<stagingCredentialEndpoint xsi:type=\"ns1:EndpointReferenceType\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:ns1=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">";
//	*rsl += "<stagingCredentialEndpoint>";
	*rsl += delegation_epr;
	*rsl += "</stagingCredentialEndpoint>";

	if ( jobAd->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

		// Start the job on hold
	if ( staging_input ) {
		*rsl += printXMLParam( "holdState", "StageIn" );
	} else {
		*rsl += printXMLParam( "holdState", "Pending" );
	}

	*rsl += "</job>";

	free( local_url_base );

	return rsl;
}

void GT4Job::DeleteOutput()
{
	int rc;
	struct stat fs;

	mode_t old_umask = umask(0);

	if ( streamOutput ) {
		rc = stat( localOutput, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localOutput );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
		}
		rc = creat( localOutput, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	if ( streamError ) {
		rc = stat( localError, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localError );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
		}
		rc = creat( localError, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	umask( old_umask );
}

static bool
WriteGT4SubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	int version;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus submit record to user logfile\n",
			 cluster, proc );

	GlobusSubmitEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact = strnewp(contact);

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, contact,
						   sizeof(contact) - 1 );
	event.jmContact = strnewp(contact);

	version = 0;
	job_ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, version );
	event.restartableJM = version >= GRAM_V_1_5;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

static bool
WriteGT4SubmitFailedEventToUserLog( ClassAd *job_ad,
									const char *failure_string )
{
	int cluster, proc;

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing submit-failed record to user logfile\n",
			 cluster, proc );

	GlobusSubmitFailedEvent event;

	if ( failure_string ) {
		event.reason =  strnewp(failure_string);
	}

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT_FAILED event\n",
				 cluster, proc);
		return false;
	}

	return true;
}

static bool
WriteGT4ResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus up record to user logfile\n",
			 cluster, proc );

	GlobusResourceUpEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_UP event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

static bool
WriteGT4ResourceDownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus down record to user logfile\n",
			 cluster, proc );

	GlobusResourceDownEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_DOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}


const char * 
GT4Job::printXMLParam (const char * name, const char * value) {
	static MyString buff;
		// TODO should perform escaping of special characters in value
	buff.sprintf ("<%s>%s</%s>", name, value, name);
	return buff.Value();
}

// Create an per-user empty directory, or return one if
// one already exist. The directory will be created under
// gridmanger scratch dir. NOT thread-safe.
const char*
GT4Job::getDummyJobScratchDir() {
	static MyString dirname;
#ifndef WIN32 
	dirname.sprintf ("%s%cempty_dir_u%d", // <scratch>/empty_dir_u<uid>
					 GridmanagerScratchDir, 
					 DIR_DELIM_CHAR,
					 geteuid());
#else // Windows
	char *user = my_username();
	dirname.sprintf ("%s%cempty_dir_u%s", // <scratch>\empty_dir_u<username>
					 GridmanagerScratchDir, 
					 DIR_DELIM_CHAR,
					 user);
	free(user);
	user = NULL;
#endif
	
	struct stat stat_buff;
	if ( stat(dirname.Value(), &stat_buff) < 0 ) {
		int rc;
		if ( (rc = mkdir (dirname.Value(), 0700)) < 0 ) {
			dprintf (D_ALWAYS, "Unable to create scratch directory %s, rc=%d\n", 
					 dirname.Value(), rc);
			return NULL;
		}
	}
		// Work-around for globus rft bug: rft hangs when you try to
		// transfer an empty directory. So create an innocuous file in
		// our dummy directory
	MyString filename;
	filename.sprintf( "%s%c.ignoreme", dirname.Value(), DIR_DELIM_CHAR );
	if ( stat(filename.Value(), &stat_buff) < 0 ) {
		int rc;
		if ( (rc = creat( filename.Value(), 0600 )) < 0 ) {
			dprintf (D_ALWAYS, "Unable to create file in scratch directory %s, rc=%d\n", 
					 dirname.Value(), rc);
			return NULL;
		}
		close( rc );
	}

	return dirname.Value();
}
