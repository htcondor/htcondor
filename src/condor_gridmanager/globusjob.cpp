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



/* TODO XXX adesmet: Notes on gridshell, TODO List

- Writing input classad to file in BuildSubmitRSL seems wrong.  But where?
  (And how to pass filename to BuildSubmitRSL)
- Log retrieval:
	- propagating things from the submit file to allow the user to
	  specify (it's all hard coded now).

*/


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "env.h"
#include "condor_daemon_core.h"
#include "basename.h"
#include "spooled_job_files.h"
#include "condor_holdcodes.h"
#include "string_list.h"
#include "filename_tools.h"
//#include "myproxy_manager.h"

#include "gridmanager.h"
#include "globusjob.h"
#include "condor_config.h"

// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMIT_COMMIT		6
#define GM_SUBMITTED			7
#define GM_CHECK_OUTPUT			8
#define GM_DONE_SAVE			9
#define GM_DONE_COMMIT			10
#define GM_STOP_AND_RESTART		11
#define GM_RESTART				12
#define GM_RESTART_SAVE			13
#define GM_RESTART_COMMIT		14
#define GM_CANCEL				15
#define GM_CANCEL_WAIT			16
#define GM_FAILED				17
#define GM_DELETE				18
#define GM_CLEAR_REQUEST		19
#define GM_HOLD					20
#define GM_PROXY_EXPIRED		21
#define GM_REFRESH_PROXY		22
#define GM_PROBE_JOBMANAGER		23
#define GM_OUTPUT_WAIT			24
#define GM_PUT_TO_SLEEP			25
#define GM_JOBMANAGER_ASLEEP	26
#define GM_START				27
#define GM_CLEANUP_RESTART		28
#define GM_CLEANUP_COMMIT		29
#define GM_CLEANUP_CANCEL		30
#define GM_CLEANUP_WAIT			31

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_CHECK_OUTPUT",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_STOP_AND_RESTART",
	"GM_RESTART",
	"GM_RESTART_SAVE",
	"GM_RESTART_COMMIT",
	"GM_CANCEL",
	"GM_CANCEL_WAIT",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_REFRESH_PROXY",
	"GM_PROBE_JOBMANAGER",
	"GM_OUTPUT_WAIT",
	"GM_PUT_TO_SLEEP",
	"GM_JOBMANAGER_ASLEEP",
	"GM_START",
	"GM_CLEANUP_RESTART",
	"GM_CLEANUP_COMMIT",
	"GM_CLEANUP_CANCEL",
	"GM_CLEANUP_WAIT",
};

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
// #define JM_MIN_PROXY_TIME		(minProxy_time + 60)
#define JM_MIN_PROXY_TIME		(180 + 60)

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	3

#define OUTPUT_WAIT_POLL_INTERVAL 1

#define CANCEL_AFTER_RESTART_DELAY	15

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s returned Globus error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error)

#define GOTO_RESTART_IF_JM_DOWN \
{ \
    if ( jmDown == true ) { \
        myResource->JMComplete( this ); \
        gmState = GM_RESTART; \
        globusError =  GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER; \
		break; \
    } \
}

#define CHECK_PROXY \
{ \
	if ( jobProxy && ((PROXY_NEAR_EXPIRED( jobProxy ) && condorState != REMOVED) || \
		  (PROXY_IS_EXPIRED( jobProxy ) && condorState == REMOVED)) \
		 && gmState != GM_PROXY_EXPIRED ) { \
		dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire, changing state to GM_PROXY_EXPIRED\n", \
				 procID.cluster, procID.proc ); \
		gmState = GM_PROXY_EXPIRED; \
		break; \
	} \
}

struct OrphanCallback_t {
	char *job_contact;
	int state;
	int errorcode;
};


HashTable <std::string, GlobusJob *> JobsByContact( hashFunction );

static List<OrphanCallback_t> OrphanCallbackList;

/* Take the job id returned by the jobmanager
 * (https://foo.edu:123/456/7890) and remove the port, so that we
 * end up with an id that won't change if the jobmanager restarts.
 */
char *
globusJobId( const char *contact )
{
	static char buff[1024];
	std::string url_scheme;
	std::string url_host;
	std::string url_path;
	int url_port;

	snprintf( buff, sizeof(buff), "%s", contact );
	filename_url_parse( buff, url_scheme, url_host, &url_port, url_path );

	snprintf( buff, sizeof(buff), "%s://%s%s", url_scheme.c_str(), url_host.c_str(), url_path.c_str() );
	return buff;
}

void
orphanCallbackHandler()
{
	int rc;
	GlobusJob *this_job;
	OrphanCallback_t *orphan;

	// Remove the first element in the list
	OrphanCallbackList.Rewind();
	if ( OrphanCallbackList.Next( orphan ) == false ) {
		// Empty list
		return;
	}
	OrphanCallbackList.DeleteCurrent();

	// Find the right job object
	rc = JobsByContact.lookup( globusJobId(orphan->job_contact), this_job );
	if ( rc == 0 && this_job != NULL ) {
		dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
				 this_job->procID.cluster, this_job->procID.proc,
				 orphan->state, orphan->errorcode );

		this_job->GramCallback( orphan->state, orphan->errorcode );

		free( orphan->job_contact );
		delete orphan;
		return;
	}

	for (auto &elem : GlobusResource::ResourcesByName) {
		GlobusResource *next_resource = elem.second;
		if ( next_resource->monitorGramJobId && !strcmp( orphan->job_contact, next_resource->monitorGramJobId ) ) {
			next_resource->gridMonitorCallback( orphan->state,
												orphan->errorcode );

			free( orphan->job_contact );
			delete orphan;
			return;
		}
	}

	dprintf( D_ALWAYS, 
			 "orphanCallbackHandler: Can't find record for globus job with "
			 "contact %s on globus state %d, errorcode %d, ignoring\n",
			 orphan->job_contact, orphan->state, orphan->errorcode );
	free( orphan->job_contact );
	delete orphan;
}

void
gramCallbackHandler( void * /* user_arg */, char *job_contact, int state,
					 int errorcode )
{
	int rc;
	GlobusJob *this_job;

	// Find the right job object
	rc = JobsByContact.lookup( globusJobId(job_contact), this_job );
	if ( rc == 0 && this_job != NULL ) {
		dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
				 this_job->procID.cluster, this_job->procID.proc, state,
				 errorcode );

		this_job->GramCallback( state, errorcode );
		return;
	}

	for (auto &elem : GlobusResource::ResourcesByName) {
		GlobusResource *next_resource = elem.second;
		if ( next_resource->monitorGramJobId && !strcmp( job_contact, next_resource->monitorGramJobId ) ) {
			next_resource->gridMonitorCallback( state, errorcode );
			return;
		}
	}

	dprintf( D_ALWAYS, 
			 "gramCallbackHandler: Can't find record for globus job with "
			 "contact %s on globus state %d, errorcode %d, delaying\n",
			 job_contact, state, errorcode );
	OrphanCallback_t *new_orphan = new OrphanCallback_t;
	new_orphan->job_contact = strdup( job_contact );
	new_orphan->state = state;
	new_orphan->errorcode = errorcode;
	OrphanCallbackList.Append( new_orphan );
	daemonCore->Register_Timer( 1, orphanCallbackHandler,
								"orphanCallbackHandler" );
}

/////////////////////////interface functions to gridmanager.C
void GlobusJobInit()
{
}

void GlobusJobReconfig()
{
	bool tmp_bool;
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GlobusJob::setGahpCallTimeout( tmp_int );
	GlobusResource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GlobusJob::setConnectFailureRetry( tmp_int );

	tmp_bool = param_boolean("ENABLE_GRID_MONITOR",true);
	GlobusResource::setEnableGridMonitor( tmp_bool );

	tmp_int = param_integer("GRID_MONITOR_DISABLE_TIME",
							DEFAULT_GM_DISABLE_LENGTH);
	GlobusResource::setGridMonitorDisableLength( tmp_int );

	// Tell all the resource objects to deal with their new config values
	for (auto &elem : GlobusResource::ResourcesByName) {
		elem.second->Reconfig();
	}
}

bool GlobusJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 ( strncasecmp( resource.c_str(), "gt2 ", 4 ) == 0 ||
		   strncasecmp( resource.c_str(), "gt5 ", 4 ) == 0  ) ) {

		return true;
	}
	return false;
}

BaseJob *GlobusJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GlobusJob( jobad );
}
////////////////////////////////////////

/* Given a classad, write it to a file for later staging to the gridshell.
Returns true/false on success/failure.  If successful, out_filename contains
the filename of the classad.  If not successful, out_filename's contents are
undefined.
*/
static bool write_classad_input_file( ClassAd *classad, 
	const std::string & executable_path,
	std::string & out_filename )
{
	if( ! classad ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (null)\n");
		return false;
	}

	ClassAd tmpclassad(*classad);

	// TODO: Store old Cmd as OrigCmd?
	tmpclassad.Assign(ATTR_JOB_CMD, condor_basename( executable_path.c_str() ));

	PROC_ID procID;
	if( ! tmpclassad.LookupInteger( ATTR_CLUSTER_ID, procID.cluster ) ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_CLUSTER_ID);
		return false;
	}
	if( ! tmpclassad.LookupInteger( ATTR_PROC_ID, procID.proc ) ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_PROC_ID);
		return false;
	}

	formatstr(out_filename, "_condor_private_classad_in_%d.%d", 
		procID.cluster, procID.proc);

	std::string out_filename_full;
	{
		char * buff = NULL;
		if( ! tmpclassad.LookupString( ATTR_JOB_IWD, &buff ) ) {

			dprintf(D_ALWAYS,"(%d.%d) Internal Error: "
				"write_classad_input_file handed "
				"invalid ClassAd (Missing or malformed %s)\n",
				procID.cluster, procID.proc, ATTR_JOB_IWD);

			return false;
		}
		out_filename_full = buff;
		free(buff);
		out_filename_full += "/";
		out_filename_full += out_filename;
	}

		// Fix the universe, too, since the starter is going to expect
		// "VANILLA", not "GLOBUS"...
	tmpclassad.Assign( ATTR_JOB_UNIVERSE, 5 );

	dprintf(D_FULLDEBUG,"(%d.%d) Writing ClassAd to file %s\n",
		procID.cluster, procID.proc, out_filename.c_str());

	// TODO: Test for file's existance, complain and die on existance?
	FILE * fp = safe_fopen_wrapper_follow(out_filename_full.c_str(), "w");

	if( ! fp )
	{
		dprintf(D_ALWAYS,"(%d.%d) Failed to write ClassAd to file %s. "
			"Error number %d (%s).\n",
			procID.cluster, procID.proc, out_filename.c_str(),
			errno, strerror(errno));
		return false;
	}

	if( fPrintAd(fp, tmpclassad) ) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to write ClassAd to file %s. "
			"Unknown error in fPrintAd.\n",
			procID.cluster, procID.proc, out_filename.c_str());
		fclose(fp);
		return false;
	} 

	fclose(fp);
	return true;
}

const char *rsl_stringify( const std::string& src )
{
	size_t src_len = src.length();
	size_t src_pos = 0;
	size_t var_pos1;
	size_t var_pos2;
	size_t quote_pos;
	static std::string dst;

	if ( src_len == 0 ) {
		dst = "''";
	} else {
		dst = "";
	}

	while ( src_pos < src_len ) {
		var_pos1 = src.find( "$(", src_pos );
		var_pos2 = var_pos1 == std::string::npos ? var_pos1 : src.find( ")", var_pos1 );
		quote_pos = src.find( "'", src_pos );
		if ( var_pos2 == std::string::npos && quote_pos == std::string::npos ) {
			dst += "'";
			dst += src.substr( src_pos, src.npos );
			dst += "'";
			src_pos = src.length();
		} else if ( var_pos2 == std::string::npos ||
					(quote_pos != std::string::npos && quote_pos < var_pos1 ) ) {
			if ( src_pos < quote_pos ) {
				dst += "'";
				dst += src.substr( src_pos, quote_pos - src_pos );
				dst += "'#";
			}
			dst += '"';
			while ( src[quote_pos] == '\'' ) {
				dst += "'";
				quote_pos++;
			}
			dst += '"';
			if ( quote_pos < src_len ) {
				dst += '#';
			}
			src_pos = quote_pos;
		} else {
			if ( src_pos < var_pos1 ) {
				dst += "'";
				dst += src.substr( src_pos, var_pos1 - src_pos );
				dst += "'#";
			}
			dst += src.substr( var_pos1, (var_pos2 - var_pos1) + 1 );
			if ( var_pos2 + 1 < src_len ) {
				dst += '#';
			}
			src_pos = var_pos2 + 1;
		}
	}

	return dst.c_str();
}

const char *rsl_stringify( const char *string )
{
	static std::string src;
	if ( string ) {
		src = string;
	}
	return rsl_stringify( src );
}

static bool merge_file_into_classad(const char * filename, ClassAd * ad)
{
	if( ! ad ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called without ClassAd\n");
		return false;
	}
	if( ! filename ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called without filename\n");
		return false;
	}
	if( ! strlen(filename) ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called with empty filename\n");
		return false;
	}

	PROC_ID procID;
	if( ! ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster ) ) {
		dprintf(D_ALWAYS,"Internal Error: merge_file_into_classad handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_CLUSTER_ID);
		return false;
	}
	if( ! ad->LookupInteger( ATTR_PROC_ID, procID.proc ) ) {
		dprintf(D_ALWAYS,"Internal Error: merge_file_into_classad handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_PROC_ID);
		return false;
	}

	/* TODO: Is this the right solution?  I'm basically reimplementing
	   a subset of the ClassAd reading code.  Perhaps load into a ClassAd
	   and scan that? */
	{
		StringList SAVE_ATTRS;
		SAVE_ATTRS.append(ATTR_JOB_REMOTE_SYS_CPU);
		SAVE_ATTRS.append(ATTR_JOB_REMOTE_USER_CPU);
		SAVE_ATTRS.append(ATTR_IMAGE_SIZE);
		SAVE_ATTRS.append(ATTR_JOB_STATE);
		SAVE_ATTRS.append(ATTR_NUM_PIDS);
		SAVE_ATTRS.append(ATTR_ON_EXIT_BY_SIGNAL);
		SAVE_ATTRS.append(ATTR_ON_EXIT_CODE);
		SAVE_ATTRS.append(ATTR_ON_EXIT_SIGNAL);
		SAVE_ATTRS.append(ATTR_JOB_START_DATE);
		SAVE_ATTRS.append(ATTR_JOB_DURATION);


		/* TODO: COMPLETION_DATE isn't currently returned.  Who deals with it?
		   Is it our job?  gridshell's?  Condor-G never really supported it,
		   but it would be nice to have.  Update: Normally
		   condor_schedd.V6/qmgmt.C does it in DestroyProc.  Why isn't it?
		   */

		SAVE_ATTRS.append(ATTR_COMPLETION_DATE);

		std::string full_filename;
		{
			char * buff = NULL;
			if( ! ad->LookupString( ATTR_JOB_IWD, &buff ) ) {
				dprintf(D_ALWAYS,"(%d.%d) Internal Error: "
					"merge_file_into_classad handed "
					"invalid ClassAd (Missing or malformed %s)\n",
					procID.cluster, procID.proc, ATTR_JOB_IWD);
				return false;
			}
			full_filename = buff;
			free(buff);
			full_filename += "/";
			full_filename += filename;
		}

		FILE * fp = safe_fopen_wrapper_follow(full_filename.c_str(), "r");
		if( ! fp ) {
			dprintf(D_ALWAYS, "Unable to read output ClassAd at %s.  "
				"Error number %d (%s).  "
				"Results will not be integrated into history.\n",
				filename, errno, strerror(errno));
			return false;
		}

		std::string line;
		while( readLine(line, fp) ) {
			chomp(line);
			size_t n = line.find(" = ");
			if(n == 0 || n == std::string::npos) {
				dprintf( D_ALWAYS,
					"Failed to parse \"%s\", ignoring.", line.c_str());
				continue;
			}
			std::string attr = line.substr(0, n);

			dprintf( D_ALWAYS, "FILE: %s\n", line.c_str() );
			if( ! SAVE_ATTRS.contains_anycase(attr.c_str()) ) {
				continue;
			}

			if( ! ad->Insert(line.c_str()) ) {
				dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
						 "ignoring.\n", line.c_str() );
			}
		}
		fclose( fp );
	}

	return true;
}

int GlobusJob::submitInterval = 300;	// default value
int GlobusJob::restartInterval = 60;	// default value
int GlobusJob::gahpCallTimeout = 300;	// default value
int GlobusJob::maxConnectFailures = 3;	// default value
int GlobusJob::outputWaitGrowthTimeout = 15;	// default value

GlobusJob::GlobusJob( ClassAd *classad )
	: BaseJob( classad )
{
	bool bool_value;
	int int_value;
	std::string iwd;
	std::string job_output;
	std::string job_error;
	std::string grid_resource;
	std::string grid_job_id;
	std::string grid_proxy_subject;
	bool job_already_submitted = false;
	std::string error_string = "";
	char *gahp_path = NULL;
	ArgList gahp_args;
	bool is_gt5 = false;

	RSL = NULL;
	callbackRegistered = false;
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	globusStateErrorCode = 0;
	globusStateBeforeFailure = 0;
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
	restartingJM = false;
	restartWhen = 0;
	gmState = GM_INIT;
	globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	jmUnreachable = false;
	jmDown = false;
	lastProbeTime = 0;
	lastRemoteStatusUpdate = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	previousGlobusError = 0;
	lastRestartReason = 0;
	lastRestartAttempt = 0;
	numRestartAttempts = 0;
	numRestartAttemptsThisSubmit = 0;
	jmProxyExpireTime = 0;
	proxyRefreshRefused = false;
	connect_failure_counter = 0;
	outputWaitLastGrowth = 0;
	// HACK!
	retryStdioSize = true;
	resourceManagerString = NULL;
	myResource = NULL;
	jobProxy = NULL;
	gassServerUrl = NULL;
	gramCallbackContact = NULL;
	communicationTimeoutTid = -1;
	gahp = NULL;

	useGridShell = false;
	mergedGridShellOutClassad = false;
	jmShouldBeStoppingTime = 0;

	outputWaitOutputSize = 0;
	outputWaitErrorSize = 0;
	useGridJobMonitor = true;
	globusError = 0;

	{
		bool use_gridshell;
		if( classad->LookupBool(ATTR_USE_GRID_SHELL, use_gridshell) ) {
			useGridShell = use_gridshell;
		}
	}

	m_lightWeightJob = false;
	jobAd->LookupBool( ATTR_JOB_NONESSENTIAL, m_lightWeightJob );

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobProxy = AcquireProxy( jobAd, error_string,
							 (TimerHandlercpp)&GlobusJob::ProxyCallback, this );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			formatstr( error_string, "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		goto error_exit;
	}

	gahp_path = param( "GT2_GAHP" );
	if ( gahp_path == NULL ) {
		gahp_path = param( "GAHP" );
		if ( gahp_path == NULL ) {
			error_string = "GT2_GAHP not defined";
			goto error_exit;
		}

		char *args = param("GAHP_ARGS");
		std::string args_error;
		if(!gahp_args.AppendArgsV1RawOrV2Quoted(args, args_error)) {
			dprintf(D_ALWAYS,"Failed to parse gahp arguments: %s",args_error.c_str());
			error_string = "ERROR: failed to parse GAHP arguments.";
			free(args);
			goto error_exit;
		}
		free(args);
	}
	formatstr( grid_proxy_subject, "GT2/%s",
			 jobProxy->subject->fqan );
	gahp = new GahpClient( grid_proxy_subject.c_str(), gahp_path, &gahp_args );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	jobAd->LookupString( ATTR_GRID_RESOURCE, grid_resource );
	if ( grid_resource.length() ) {
		const char *token;

		Tokenize( grid_resource );

		token = GetNextToken( " ", false );
		if ( !token || ( strcasecmp( token, "gt2" ) && strcasecmp( token, "gt5" ) ) ) {
			formatstr( error_string, "%s not of type gt2 or gt5",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}
		if ( !strcasecmp( token, "gt5" ) ) {
			is_gt5 = true;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			resourceManagerString = strdup( token );
		} else {
			formatstr( error_string, "%s missing GRAM service name",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		formatstr( error_string, "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	jobAd->LookupString( ATTR_GRID_JOB_ID, grid_job_id );
	if ( grid_job_id.length() ) {
		const char *token;

		Tokenize( grid_job_id );

		token = GetNextToken( " ", false );
		if ( !token || ( strcasecmp( token, "gt2" ) && strcasecmp( token, "gt5" ) ) ) {
			formatstr( error_string, "%s not of type gt2 or gt5",
								  ATTR_GRID_JOB_ID );
			goto error_exit;
		}
		if ( !strcasecmp( token, "gt5" ) ) {
			is_gt5 = true;
		}

		token = GetNextToken( " ", false );
		token = GetNextToken( " ", false );
		if ( !token ) {
			formatstr( error_string, "%s missing job ID",
								  ATTR_GRID_JOB_ID );
			goto error_exit;
		}
		GlobusSetRemoteJobId( token, is_gt5 );
		job_already_submitted = true;
	}

	// Find/create an appropriate GlobusResource for this job
	myResource = GlobusResource::FindOrCreateResource( resourceManagerString,
													   jobProxy,
													   is_gt5 );
	if ( myResource == NULL ) {
		error_string = "Failed to initialized GlobusResource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	useGridJobMonitor = true;

	jobAd->LookupInteger( ATTR_GLOBUS_STATUS, globusState );

	globusError = GLOBUS_SUCCESS;

	if ( jobAd->LookupInteger( ATTR_DELEGATED_PROXY_EXPIRATION, int_value ) ) {
		jmProxyExpireTime = (time_t)int_value;
	}

	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && iwd.length() ) {
		int len = iwd.length();
		if ( len > 1 && iwd[len - 1] != '/' ) {
			iwd += "/";
		}
	} else {
		iwd = "/";
	}

	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, job_output) && job_output.length() &&
		 strcmp( job_output.c_str(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {
			std::string full_job_output;

			if ( job_output[0] != '/' ) {
				 full_job_output = iwd;
			}

			full_job_output += job_output;
			localOutput = strdup( full_job_output.c_str() );

			bool_value = false;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = bool_value;
			stageOutput = !streamOutput;
		}
	}

	if ( jobAd->LookupString(ATTR_JOB_ERROR, job_error) && job_error.length() &&
		 strcmp( job_error.c_str(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {
			std::string full_job_error;

			if ( job_error[0] != '/' ) {
				full_job_error = iwd;
			}

			full_job_error += job_error;
			localError = strdup( full_job_error.c_str() );

			bool_value = false;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = bool_value;
			stageError = !streamError;
		}
	}

	if ( gahp_path ) {
		free( gahp_path );
	}

	return;

 error_exit:
	if ( gahp_path ) {
		free( gahp_path );
	}
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

GlobusJob::~GlobusJob()
{
	if ( gahp ) {
		delete gahp;
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( jobContact ) {
		JobsByContact.remove(globusJobId(jobContact));
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
		ReleaseProxy( jobProxy, (TimerHandlercpp)&GlobusJob::ProxyCallback, this );
	}
	if ( gassServerUrl ) {
		free( gassServerUrl );
	}
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
	}
	if ( communicationTimeoutTid != -1 ) {
		daemonCore->Cancel_Timer(communicationTimeoutTid);
		communicationTimeoutTid = -1;
	}
}

void GlobusJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void GlobusJob::ProxyCallback()
{
	if ( ( gmState == GM_JOBMANAGER_ASLEEP && !JmShouldSleep() ) ||
		 ( gmState == GM_SUBMITTED && jmProxyExpireTime < jobProxy->expiration_time ) ||
		 gmState == GM_PROXY_EXPIRED ) {
		SetEvaluateState();
	}
}

void GlobusJob::doEvaluateState()
{
	bool connect_failure_jobmanager = false;
	bool connect_failure_gatekeeper = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;
	int status;
	int error;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

	if ( gahp ) {
		// We don't include jmDown here because we don't want it to block
		// connections to the gatekeeper (particularly restarts) and any
		// state that contacts to the jobmanager should be jumping to
		// GM_RESTART instead.
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

		// Special light-weight job semantics for BNL
	if ( condorState == REMOVED && m_lightWeightJob ) {
		if ( resourceDown && jobContact ) {
			dprintf( D_ALWAYS, "(%d.%d) Immediately removing light-weight "
					 "job whose resource is down.\n", procID.cluster,
					 procID.proc );
			GlobusSetRemoteJobId( NULL, false );
			gmState = GM_CLEAR_REQUEST;
		}
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( !jobProxy ) {
				jobAd->Assign( ATTR_HOLD_REASON,
							   "Proxy file missing or corrupted" );
				jobAd->Assign(ATTR_HOLD_REASON_CODE,
							   CONDOR_HOLD_CODE_CorruptedCredential);
				jobAd->Assign(ATTR_HOLD_REASON_SUBCODE, 0);
				gmState = GM_HOLD;
				break;
			}

			/* This code was added for 'use best proxy' support, which I
			 * don't believe anyone is using. It affects jobs that aren't
			 * explicitly using this 'feature' and keeps the gridmanager
			 * managing jobs even when they've become HELD or REMOVED in
			 * the schedd.
			if ( !PROXY_IS_VALID(jobProxy) ) {
				dprintf( D_FULLDEBUG, "(%d.%d) proxy not valid, waiting\n",
						 procID.cluster, procID.proc );
				break;
			}
			*/

			if ( gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( jobProxy );

			GahpClient::mode saved_mode = gahp->getMode();
			gahp->setMode( GahpClient::blocking );

			err = gahp->globus_gram_client_callback_allow( gramCallbackHandler,
														   NULL,
														   &gramCallbackContact );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error enabling GRAM callback, err=%d - %s\n", 
						 procID.cluster, procID.proc, err,
						 gahp->globus_gram_client_error_string(err) );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			err = gahp->globus_gass_server_superez_init( &gassServerUrl, 0 );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error enabling GASS server, err=%d\n",
						 procID.cluster, procID.proc, err );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setMode( saved_mode );

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			// If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// There are two ways a job can end up back in this state:
			// 1. If we attempt a restart of a jobmanager only to be told
			//    that the old jobmanager process is still alive.
			// 2. If our proxy expires and is then renewed.

			// Here, we assume there isn't a jobmanager running until we
			// learn otherwise.
			myResource->JMComplete( this );

			errorString = "";
			if ( jobContact == NULL ) {
				if ( condorState == COMPLETED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEANUP_RESTART;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					submitLogged = true;
				}
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					executeLogged = true;
				}

				gmState = GM_REGISTER;
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_callback_register( jobContact,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &status,
										&error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				globusError = GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER;
				gmState = GM_RESTART;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				myResource->JMAlreadyRunning( this );
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_PROTOCOL_FAILED ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ) {
				// The jobmanager appears to not be running.
				// The port appears to be in use as follows:
				// CONTACTING_JOB_MANAGER: port not in use
				// PROTOCOL_FAILED: port in use by another protocol
				// AUTHORIZATION: port in use by another user's jobmanager
				// JOB_CONTACT_NOT_FOUND: port in use for another job
				gmState = GM_JOBMANAGER_ASLEEP;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				myResource->JMAlreadyRunning( this );
				LOG_GLOBUS_ERROR( "globus_gram_client_job_callback_register()", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			myResource->JMAlreadyRunning( this );
			callbackRegistered = true;
			UpdateGlobusState( status, error );
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				// The jobmanager is waiting for the submit commit and won't
				// respond to a stdio update signal until after it receives
				// the commit signal. But if we commit without a stdio
				// update, file stage-in will fail. So we cancel the submit
				// and start a fresh one.
				gmState = GM_CLEANUP_CANCEL;
			} else {
				gmState = GM_STDIO_UPDATE;
			}
			} break;
		case GM_STDIO_UPDATE: {
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->c_str(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
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
				// JEF Handle GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_UPDATE)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
			gmState = GM_SUBMITTED;
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
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
			std::string job_contact;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				myResource->JMComplete(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				// Don't set HOLD_REASON here --- that way, the reason will get set
				// to the globus error that caused the submission failure.
				// jobAd->Assign( ATTR_HOLD_REASON,"Attempts to submit failed" );

				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// call CancelSubmit() when there's no job left on the
				// remote host and you don't plan to submit one.
				// Once RequestJM() is called, you must call JMComplete()
				// when the jobmanager is no longer running and you don't
				// plan to start one.
				if ( myResource->RequestSubmit(this) == false ||
					 myResource->RequestJM(this, true) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->globus_gram_client_job_request( 
										resourceManagerString,
										RSL->c_str(),
										param_boolean( "DELEGATE_FULL_JOB_GSI_CREDENTIALS",
													   false ) ? 0 : 1,
										gramCallbackContact, job_contact,
										false);
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				jmShouldBeStoppingTime = 0;
				lastSubmitAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					myResource->JMComplete( this );
					connect_failure_gatekeeper = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					myResource->JMComplete( this );
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				numSubmitAttempts++;
				jmProxyExpireTime = jobProxy->expiration_time;
				jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
							   (int)jmProxyExpireTime );
				if ( rc == GLOBUS_SUCCESS ) {
					// Previously this supported GRAM 1.0
					dprintf(D_ALWAYS, "(%d.%d) Unexpected remote response.  GRAM 1.6 is now required.\n", procID.cluster, procID.proc);
					formatstr(errorString,"Unexpected remote response.  Remote server must speak GRAM 1.6");
					myResource->JMComplete( this );
					gmState = GM_HOLD;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					callbackRegistered = true;
					GlobusSetRemoteJobId( job_contact.c_str(), false );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					myResource->JMComplete( this );
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->c_str());
					globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( jobAd,
														   rc,
														   gahp->globus_gram_client_error_string(rc) );
					// See if the user wants to rematch. We evaluate the
					// expressions here because GM_CLEAR_REQUEST may
					// decide to hold the job before it evaluates it.
					jobAd->LookupBool(ATTR_REMATCH_CHECK,wantRematch);

					gmState = GM_CLEAR_REQUEST;
				}
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
				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
					requestScheddUpdate( this, true );
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
				if ( GetCallbacks() ) {
					if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
						 (globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT ||
						  globusStateErrorCode == 0) ) {
						dprintf(D_FULLDEBUG,"(%d.%d) jobmanager timed out on commit, clearing request\n",procID.cluster, procID.proc);
						doResubmit = 1;
						gmState = GM_CLEAR_REQUEST;
						break;
					}
				}
				GOTO_RESTART_IF_JM_DOWN;
				CHECK_PROXY;
				rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure_jobmanager = true;
					break;
				}
				// JEF Handle GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
					globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( jobAd, globusError,
														   gahp->globus_gram_client_error_string(globusError) );
					gmState = GM_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_CHECK_OUTPUT;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( GetCallbacks() ) {
				reevaluate_state = true;
				break;
			} else if ( JmShouldSleep() ) {
				gmState = GM_PUT_TO_SLEEP;
			} else {
					// The jobmanager doesn't accept proxy refresh commands
					// once it hits stage-out state
				if ( jmProxyExpireTime < jobProxy->expiration_time &&
					 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ) {
					gmState = GM_REFRESH_PROXY;
					break;
				}
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				int probe_interval = myResource->GetJobPollInterval();
				if ( now >= lastProbeTime + probe_interval ) {
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probe_interval) > now ) {
					delay = (lastProbeTime + probe_interval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				GOTO_RESTART_IF_JM_DOWN;
				CHECK_PROXY;
				rc = gahp->globus_gram_client_job_refresh_credentials(
										jobContact,
										param_boolean( "DELEGATE_FULL_JOB_GSI_CREDENTIALS",
													   false ) ? 0 : 1 );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure_jobmanager = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_RESOURCES &&
					 !proxyRefreshRefused ) {
						// The jobmanager is probably in stage-out state
						// and refusing proxy refresh commands. Do a poll
						// now and see. If the jobmanager isn't in
						// stage-out, then we'll end up back in this state.
						// If we get the same error, then try a restart
						// of the jobmanager.

						// This is caused by a bug in the gram client code.
						// After sending the 'renew' command and receiving
						// the response from the jobmanager, it tries to
						// perform a delegation without looking to the
						// jobmanager's response to see if the jobmanager
						// is refusing to accept it. When the delegation
						// fails (due to the jobmanager closing the
						// connection), the client returns this error.
					proxyRefreshRefused = true;
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				proxyRefreshRefused = false;
					// JEF handled JOB_CONTACT_NOT_FOUND here
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					break;
				}
				jmProxyExpireTime = jobProxy->expiration_time;
				jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
							   (int)jmProxyExpireTime );
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				GOTO_RESTART_IF_JM_DOWN;
				CHECK_PROXY;
				rc = gahp->globus_gram_client_job_status( jobContact,
														  &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure_jobmanager = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
					globusError = rc;
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
						gmState = GM_RESTART;
					} else {
						gmState = GM_STOP_AND_RESTART;
					}
					break;
				}
				UpdateGlobusState( status, error );
				ClearCallbacks();
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_CHECK_OUTPUT: {
			// The job has completed. Make sure we got all the output.
			// If we haven't, stop and restart the jobmanager to prompt
			// sending of the rest.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DONE_COMMIT;
			} else {
				char size_str[128];
				if ( streamOutput == false && streamError == false &&
					 stageOutput == false && stageError == false ) {
					gmState = GM_DONE_SAVE;
					break;
				}
				GOTO_RESTART_IF_JM_DOWN;
				CHECK_PROXY;
				int output_size = -1;
				int error_size = -1;
				GetOutputSize( output_size, error_size );
				sprintf( size_str, "%d %d", output_size, error_size );
				rc = gahp->globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE,
									size_str, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure_jobmanager = true;
					break;
				}
				if ( rc == GLOBUS_SUCCESS ) {
					// HACK!
					retryStdioSize = true;
					gmState = GM_DONE_SAVE;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_STILL_STREAMING ) {
					dprintf( D_FULLDEBUG, "(%d.%d) ERROR_STILL_STREAMING, will wait and retry\n", procID.cluster, procID.proc);
					gmState = GM_OUTPUT_WAIT;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE ) {
					// HACK!
					if ( retryStdioSize ) {
						dprintf( D_FULLDEBUG, "(%d.%d) ERROR_STDIO_SIZE, will wait and retry\n", procID.cluster, procID.proc);
						retryStdioSize = false;
						gmState = GM_OUTPUT_WAIT;
					} else {
					// HACK!
					retryStdioSize = true;
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					dprintf( D_FULLDEBUG, "(%d.%d) Requesting jobmanager restart because of GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE\n",
							 procID.cluster, procID.proc);
					dprintf( D_FULLDEBUG, "(%d.%d) output_size = %d, error_size = %d\n",
							 procID.cluster, procID.proc,output_size, error_size );
					}
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_SIZE)", rc );
					// HACK!
					retryStdioSize = true;
					globusError = rc;
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
						gmState = GM_RESTART;
					} else {
						gmState = GM_STOP_AND_RESTART;
					}
					dprintf( D_FULLDEBUG, "(%d.%d) Requesting jobmanager restart because of unknown error\n",
							 procID.cluster, procID.proc);
				}
			}
			} break;
		case GM_OUTPUT_WAIT: {
			// We haven't received all the output from the job, but we
			// think the jobmanager is still trying to send it. Wait until
			// we think it's all here, then check again with the jobmanager.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DONE_COMMIT;
				break;
			}
			if ( outputWaitLastGrowth == 0 ) {
				outputWaitLastGrowth = now;
				outputWaitOutputSize = 0;
				outputWaitErrorSize = 0;
				GetOutputSize( outputWaitOutputSize, outputWaitErrorSize );
				ClearCallbacks();
			} else {
				int new_output_size, new_error_size;

				GetOutputSize( new_output_size, new_error_size );

				if ( new_output_size > outputWaitOutputSize ) {
					dprintf(D_FULLDEBUG,"(%d.%d) saw new output size %d\n",procID.cluster,procID.proc,new_output_size);
					outputWaitOutputSize = new_output_size;
					outputWaitLastGrowth = now;
				}

				if ( new_error_size > outputWaitErrorSize ) {
					dprintf(D_FULLDEBUG,"(%d.%d) saw new error size %d\n",procID.cluster,procID.proc,new_error_size);
					outputWaitErrorSize = new_error_size;
					outputWaitLastGrowth = now;
				}
			}
			if ( now > outputWaitLastGrowth + outputWaitGrowthTimeout ) {
				dprintf(D_FULLDEBUG,"(%d.%d) no new output/error for %d seconds, retrying STDIO_SIZE\n",procID.cluster,procID.proc,outputWaitGrowthTimeout);
				outputWaitLastGrowth = 0;
				gmState = GM_CHECK_OUTPUT;
			} else if ( GetCallbacks() ) {
				dprintf(D_FULLDEBUG,"(%d.%d) got a callback, retrying STDIO_SIZE\n",procID.cluster,procID.proc);
				outputWaitLastGrowth = 0;
				gmState = GM_CHECK_OUTPUT;
			} else {
				daemonCore->Reset_Timer( evaluateStateTid,
										 OUTPUT_WAIT_POLL_INTERVAL );
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.

			if(useGridShell && !mergedGridShellOutClassad) {
				if( ! merge_file_into_classad(outputClassadFilename.c_str(), jobAd) ) {
					/* TODO: put job on hold or otherwise don't let it
					   quietly pass into the great beyond? */
					dprintf(D_ALWAYS,"(%d.%d) Failed to add job result attributes to job's classad.  Job's history will lack run information.\n",procID.cluster,procID.proc);
				} else {
					mergedGridShellOutClassad = true;
				}
			}

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
			// Tell the jobmanager it can clean up and exit.
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
				globusError = rc;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
					gmState = GM_RESTART;
				} else {
					gmState = GM_STOP_AND_RESTART;
				}
				break;
			}
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				// And even if we decide to go to GM_DELETE, that may
				// not actually remove the job from the queue if 
				// leave_job_in_queue attribute evals to TRUE --- and then
				// the job will errantly go on hold when the user
				// subsequently does a condor_rm.
			if ( jobContact != NULL ) {
				myResource->JMComplete( this );
				myResource->CancelSubmit( this );
				jmDown = false;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				GlobusSetRemoteJobId( NULL, false );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_STOP_AND_RESTART: {
			// Something has wrong with the jobmanager and we want to stop
			// it and restart it to clear up the problem.
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS &&
				 rc != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STOP_MANAGER)", rc );
				globusError = rc;
				gmState = GM_CANCEL;
			} else {
				if ( !jmShouldBeStoppingTime ) {
					jmShouldBeStoppingTime = now;
				}
				myResource->JMComplete( this );
				gmState = GM_RESTART;
			}
			} break;
		case GM_RESTART: {
			// Something has gone wrong with the jobmanager and we need to
			// restart it.
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( globusError != 0 && globusError == lastRestartReason ) {
				dprintf( D_FULLDEBUG, "(%d.%d) Restarting jobmanager for same reason "
						 "two times in a row: %d, aborting request\n",
						 procID.cluster, procID.proc,globusError );
				gmState = GM_CLEAR_REQUEST;
			} else if ( now < lastRestartAttempt + restartInterval ) {
				// After a restart, wait at least restartInterval before
				// trying another one.
				daemonCore->Reset_Timer( evaluateStateTid,
								(lastRestartAttempt + restartInterval) - now );
			} else {
				if ( jmShouldBeStoppingTime ) {
					// If the jobmanager is shutting down (because either
					// we told it to stop or we got a callback saying it
					// was stopping), give it some time to finish up and
					// exit. A bug in older jobmanagers causes a loss of
					// data if a restarted jobmanager finds an old one
					// still alive.
					// See Globus Bugzilla Bug #5467
					int jm_gone_timeout = param_integer( "GRIDMANAGER_JM_EXIT_LIMIT", 30 );
					if ( jmShouldBeStoppingTime + jm_gone_timeout > now ) {
						unsigned int delay = 0;
						delay = (jmShouldBeStoppingTime + jm_gone_timeout) - now;
						daemonCore->Reset_Timer( evaluateStateTid, delay );
dprintf(D_FULLDEBUG,"(%d.%d) JEF: delaying restart for %d seconds\n",procID.cluster,procID.proc,delay);
						break;
					}
else{dprintf(D_FULLDEBUG,"(%d.%d) JEF: proceeding immediately with restart\n",procID.cluster,procID.proc);}
				}

				std::string job_contact;

				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// call CancelSubmit() when there's no job left on the
				// remote host and you don't plan to submit one.
				// Once RequestJM() is called, you must call JMComplete()
				// when the jobmanager is no longer running and you don't
				// plan to start one.
				if ( myResource->RequestSubmit(this) == false ||
					 myResource->RequestJM(this, false) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildRestartRSL();
				}
				rc = gahp->globus_gram_client_job_request(
										resourceManagerString,
										RSL->c_str(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, job_contact,
										true );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastRestartAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					myResource->JMComplete( this );
					connect_failure_gatekeeper = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					myResource->JMComplete( this );
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE &&
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) 
				{
						// Here we tried to restart, but the jobmanager claimed
						// there was no state file.  
						// If we still think we are in UNSUBMITTED state, the
						// odds are overwhelming that the jobmanager bailed
						// out and deleted the state file before we sent 
						// a commit signal.  So resubmit.
						// HOWEVER ---- this is evil and wrong!!!  What
						// should really happen is the Globus jobmanager
						// should *not* delete the state file if it fails
						// to receive the commit within 5 minutes.  
						// Don't understand why?  
						// Ask Todd T <tannenba@cs.wisc.edu>
					myResource->JMComplete( this );
					gmState = GM_CLEAR_REQUEST;
					doResubmit = 1;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE &&
					 condorState == COMPLETED ) {
					// Our restart attempt failed because the jobmanager
					// couldn't find the state file. If the job is marked
					// COMPLETED, then it's almost certain that we told the
					// jobmanager to clean up but died before we could
					// remove the job from the queue. So let's just remove
					// it now.
					myResource->JMComplete( this );
					gmState = GM_DELETE;
					break;
				}
				// TODO: What should be counted as a restart attempt and
				// what shouldn't?
				numRestartAttempts++;
				numRestartAttemptsThisSubmit++;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE ) {
					// If the jobmanager should be stopped now (because
					// either we told it to stop or we got a callback saying
					// it was stopping), give it a little time to finish
					// exiting. If it's still around then, assume it's
					// hosed.
					//
					// NOTE: A bug in older jobmanagers causes a loss of
					// data if a restarted jobmanager finds an old one
					// still alive.
					// See Globus Bugzilla Bug #5467
					//
					// TODO: need to avoid an endless loop of old jm not
					// responding, start new jm, new jm says old one still
					// running, try to contact old jm again. How likely is
					// this to happen?
					myResource->JMComplete( this );
					jmDown = false;
					if ( !job_contact.empty() ) {
						GlobusSetRemoteJobId( job_contact.c_str(), false );
						requestScheddUpdate( this, false );
					}
					gmState = GM_START;
					break;
				}
				jmShouldBeStoppingTime = 0;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
					myResource->JMComplete( this );
					gmState = GM_CLEAR_REQUEST;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					callbackRegistered = true;
					jmProxyExpireTime = jobProxy->expiration_time;
					jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
								   (int)jmProxyExpireTime );
					jmDown = false;
					GlobusSetRemoteJobId( job_contact.c_str(), false );
					if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
						globusState = globusStateBeforeFailure;
					}
					globusStateErrorCode = 0;
					lastRestartReason = globusError;
					ClearCallbacks();
					gmState = GM_RESTART_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					myResource->JMComplete( this );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_RESTART_SAVE: {
			// Save the restarted jobmanager's contact string on the schedd.
			if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
				requestScheddUpdate( this, true );
				break;
			}
			gmState = GM_RESTART_COMMIT;
			} break;
		case GM_RESTART_COMMIT: {
			// Tell the jobmanager it can proceed with the restart.
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
					 ( status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					   status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ) ) {
					// TODO: should this get sent to the schedd in
					//   GM_RESTART_SAVE??
					dprintf(D_FULLDEBUG,"(%d.%d) jobmanager's job state went from DONE to %s across a restart, do the same here\n",
							procID.cluster, procID.proc, GlobusJobStatusName(status) );
					globusState = status;
					jobAd->Assign( ATTR_GLOBUS_STATUS, status );
					SetRemoteJobStatus( GlobusJobStatusName( status ) );
					enteredCurrentGlobusState = time(NULL);
					requestScheddUpdate( this, false );
				}
				// Do an active status call after the restart.
				// This is part of a workaround for Globus bug 3411
				probeNow = true;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				GOTO_RESTART_IF_JM_DOWN;
				CHECK_PROXY;
				if ( now < lastRestartAttempt + CANCEL_AFTER_RESTART_DELAY ) {
					// After a restart, wait a bit before attempting a
					// cancel. If the job is done but the jobmanager
					// hasn't noticed yet, then we'll get GRAM error
					// JOB_CANCEL_FAILED.
					daemonCore->Reset_Timer( evaluateStateTid,
								(lastRestartAttempt + CANCEL_AFTER_RESTART_DELAY) - now );
					break;
				}
				rc = gahp->globus_gram_client_job_cancel( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure_jobmanager = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
						// The jobmanager is already in either FAILED or
						// DONE state and ready to shut down. Go to
						// GM_CANCEL_WAIT, where we'll deal with it
						// appropriately.
					gmState = GM_CANCEL_WAIT;
					break;
				} else if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_cancel()", rc );
					globusError = rc;
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
						gmState = GM_RESTART;
					} else {
						gmState = GM_CLEAR_REQUEST;
					}
					break;
				}
			}
			if ( callbackRegistered ) {
				gmState = GM_CANCEL_WAIT;
			} else {
				// This can happen if we're restarting and fail to
				// reattach to a running jobmanager
				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_CANCEL_WAIT: {
			// A cancel has been successfully issued. Wait for the
			// accompanying FAILED callback. Probe the jobmanager
			// occassionally to make sure it hasn't died on us.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_DONE_COMMIT;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else {
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				int probe_interval = myResource->GetJobPollInterval();
				if ( now >= lastProbeTime + probe_interval ) {
					GOTO_RESTART_IF_JM_DOWN;
					CHECK_PROXY;
					rc = gahp->globus_gram_client_job_status( jobContact,
														&status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure_jobmanager = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
						globusError = rc;
						gmState = GM_CLEAR_REQUEST;
					}
					UpdateGlobusState( status, error );
					ClearCallbacks();
					lastProbeTime = now;
				} else {
					GetCallbacks();
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probe_interval) > now ) {
					delay = (lastProbeTime + probe_interval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.
			if ( globusStateErrorCode ==
				 GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {

				myResource->JMComplete( this );
				gmState = GM_PROXY_EXPIRED;

			//} else if ( FailureIsRestartable( globusStateErrorCode ) ) {
			} else if ( condorState != HELD && condorState != REMOVED &&
						( RetryFailureAlways( globusStateErrorCode ) ||
						  ( RetryFailureOnce( globusStateErrorCode ) &&
							globusStateErrorCode != lastRestartReason ) ) ) {

				// The job may still be submitted and/or recoverable,
				// so stop the jobmanager and restart it.
				if ( FailureNeedsCommit( globusStateErrorCode ) ) {
					globusError = globusStateErrorCode;
					gmState = GM_STOP_AND_RESTART;
				} else {
					myResource->JMComplete( this );
					gmState = GM_RESTART;
				}

			} else {

				if ( FailureNeedsCommit( globusStateErrorCode ) ) {

					// Sending a COMMIT_END here means we no longer care
					// about this job submission. Either we know the job
					// isn't pending/running or the user has told us to
					// forget lost job submissions.
					GOTO_RESTART_IF_JM_DOWN;
					CHECK_PROXY;
					rc = gahp->globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure_jobmanager = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
						globusError = rc;
						if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
							gmState = GM_RESTART;
						} else {
							gmState = GM_STOP_AND_RESTART;
						}
						break;
					}

				}

				myResource->CancelSubmit( this );
				myResource->JMComplete( this );
				jmDown = false;
				requestScheddUpdate( this, false );

				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					GlobusSetRemoteJobId( NULL, false );
					gmState = GM_CLEAR_REQUEST;
				}

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
			if ( (jobContact != NULL || (globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED && globusStateErrorCode != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED)) 
				     // && condorState != REMOVED 
					 && wantRematch == false
					 && wantResubmit == false 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				jobAd->LookupBool(ATTR_REMATCH_CHECK,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = false;
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
				// Save the globus error code that led to this submission
				// failing, if any
			if ( previousGlobusError == 0 ) {
				previousGlobusError = globusStateErrorCode ?
					globusStateErrorCode : globusError;
			}
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
			}
			SetRemoteJobStatus( NULL );
			globusStateErrorCode = 0;
			globusError = 0;
			lastRestartReason = 0;
			numRestartAttemptsThisSubmit = 0;
			errorString = "";
			ClearCallbacks();
			useGridJobMonitor = true;
			if ( jmProxyExpireTime != 0 ) {
				jmProxyExpireTime = 0;
				jobAd->AssignExpr( ATTR_DELEGATED_PROXY_EXPIRATION, "Undefined" );
			}
			// HACK!
			retryStdioSize = true;
			myResource->CancelSubmit( this );
			myResource->JMComplete( this );
			if ( jobContact != NULL ) {
				GlobusSetRemoteJobId( NULL, false );
				jmDown = false;
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
				bool dummy;
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
			if ( jobAd->dirtyBegin() != jobAd->dirtyEnd() ) {
				requestScheddUpdate( this, true );
				break;
			}
			DeleteOutput();
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			jmShouldBeStoppingTime = 0;
			gmState = GM_UNSUBMITTED;
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?
			if ( jobContact &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
				//SetRemoteJobStatus( GlobusJobStatusName( globusState ) );
				//UpdateGlobusState( GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN, 0 );
			}
			// If the condor state is already HELD, then someone already
			// HELD it, so don't update anything else.
			if ( condorState != HELD ) {

				// Set the hold reason as best we can
				// TODO: set the hold reason in a more robust way.
				char holdReason[1024];
				int holdCode = 0;
				int holdSubCode = 0;
				holdReason[0] = '\0';
				holdReason[sizeof(holdReason)-1] = '\0';
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason,
									 sizeof(holdReason) );
				jobAd->LookupInteger(ATTR_HOLD_REASON_CODE,holdCode);
				jobAd->LookupInteger(ATTR_HOLD_REASON_SUBCODE,holdSubCode);
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.c_str(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' && globusStateErrorCode == 0 &&
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					snprintf( holdReason, 1024, "Job failed, no reason given by GRAM server" );
					holdCode = CONDOR_HOLD_CODE_GlobusGramError;
					holdSubCode = 0;
				}
				if ( holdReason[0] == '\0' && globusStateErrorCode != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s",
							  globusStateErrorCode,
							  gahp->globus_gram_client_error_string( globusStateErrorCode ) );
					holdCode = CONDOR_HOLD_CODE_GlobusGramError;
					holdSubCode = globusStateErrorCode;
				}
				if ( holdReason[0] == '\0' && globusError != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s", globusError,
							  gahp->globus_gram_client_error_string( globusError ) );
					holdCode = CONDOR_HOLD_CODE_GlobusGramError;
					holdSubCode = globusError;
				}
				if ( holdReason[0] == '\0' && previousGlobusError != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s",
							  previousGlobusError,
							  gahp->globus_gram_client_error_string( previousGlobusError ) );
					holdCode = CONDOR_HOLD_CODE_GlobusGramError;
					holdSubCode = previousGlobusError;
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason, holdCode, holdSubCode );
			}
			gmState = GM_DELETE;
			} break;
		case GM_PROXY_EXPIRED: {
			// The proxy for this job is either expired or about to expire.
			// If requested, put the job on hold. Otherwise, wait for the
			// proxy to be refreshed, then resume handling the job.
			bool hold_if_credential_expired = 
				param_boolean("HOLD_JOB_IF_CREDENTIAL_EXPIRES",true);
			if ( hold_if_credential_expired ) {
					// set hold reason via Globus cred expired error code
				globusStateErrorCode =
					GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED;
				gmState = GM_HOLD;
				break;
			}
			// If our proxy is expired, then the jobmanager's is as well,
			// and it will exit.
			myResource->JMComplete( this );

			// if ( jobProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
			if ( jobProxy->expiration_time > (minProxy_time + 60) + now ) {
				// resume handling the job.
				gmState = GM_START;
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		case GM_PUT_TO_SLEEP: {
			GOTO_RESTART_IF_JM_DOWN;
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 //rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STOP_MANAGER)", rc );
				globusError = rc;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
					gmState = GM_JOBMANAGER_ASLEEP;
				} else {
					gmState = GM_CANCEL;
				}
			} else {
				if ( !jmShouldBeStoppingTime ) {
					jmShouldBeStoppingTime = now;
				}
				myResource->JMComplete( this );
				gmState = GM_JOBMANAGER_ASLEEP;
			}
			} break;
		case GM_JOBMANAGER_ASLEEP: {
			if ( callbackGlobusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
				 callbackGlobusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ) {
				// Small hack to ignore the callback caused by the STOP
				// signal we sent to GM_PUT_TO_SLEEP
				ClearCallbacks();
			} else {
				GetCallbacks();
			}
			CHECK_PROXY;
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				// The GRAM poll script returned FAILED, which the
				// jobmanager considers a permanent, fatal error.
				// Disable the grid monitor for this job submission so
				// that we don't put the jobmanager right back to sleep
				// after we restart it. We want it to do a poll itself
				// and enter the FAILED state.
				useGridJobMonitor = false;
			}
			globusError = 0;
			if ( JmShouldSleep() == false ) {
				gmState = GM_RESTART;
			}
			} break;
		case GM_CLEANUP_RESTART: {
			// We want cancel a job submission, but first we need to restart
			// the jobmanager.
			// This is best effort. If anything goes wrong, we ignore it.
			//   (except for proxy expiration)
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				std::string job_contact = NULL;

				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// call CancelSubmit() when the job is gone from the
				// remote host.
				if ( myResource->RequestSubmit(this) == false ||
					 myResource->RequestJM(this, false) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildRestartRSL();
				}
				rc = gahp->globus_gram_client_job_request(
										resourceManagerString,
										RSL->c_str(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, job_contact,
										true);
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					myResource->JMComplete( this );
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE &&
					 condorState == COMPLETED ) {
					// Our restart attempt failed because the jobmanager
					// couldn't find the state file. If the job is marked
					// COMPLETED, then it's almost certain that we told the
					// jobmanager to clean up but died before we could
					// remove the job from the queue. So let's just remove
					// it now.
					gmState = GM_DELETE;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE ) {
					jmDown = false;
					gmState = GM_CLEANUP_CANCEL;
					break;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					callbackRegistered = true;
					jmProxyExpireTime = jobProxy->expiration_time;
					jobAd->Assign( ATTR_DELEGATED_PROXY_EXPIRATION,
								   (int)jmProxyExpireTime );
					jmDown = false;
					GlobusSetRemoteJobId( job_contact.c_str(), false );
					gmState = GM_CLEANUP_COMMIT;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					// Clear out the job id so that the job won't be held
					// in GM_CLEAR_REQUEST
					GlobusSetRemoteJobId( NULL, false );
					globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
					jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
					SetRemoteJobStatus( NULL );
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_CLEANUP_COMMIT: {
			// We are canceling a job submission.
			// Tell the jobmanager it can proceed with the restart.
			// This is best-effort. If anything goes wrong, we ignore it.
			//   (except for proxy expiration)
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
			}
			gmState = GM_CLEANUP_CANCEL;
			} break;
		case GM_CLEANUP_CANCEL: {
			// We are canceling a job submission.
			// This is best-effort. If anything goes wrong, we ignore it.
			//   (except for proxy expiration)
			CHECK_PROXY;
			rc = gahp->globus_gram_client_job_cancel( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			gmState = GM_CLEANUP_WAIT;
			} break;
		case GM_CLEANUP_WAIT: {
			// A cancel has been successfully issued. Wait for the
			// accompanying FAILED callback. Probe the jobmanager
			// occassionally to make sure it hasn't died on us.
			// This is best-effort. If anything goes wrong, we ignore it.
			//   (except for proxy expiration)
 
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
				 now < enteredCurrentGmState + 30 ) {

				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( now >= lastProbeTime + 5 ) {
					CHECK_PROXY;
					rc = gahp->globus_gram_client_job_status( jobContact,
														&status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
						// Clear out the job id so that the job won't be held
						// in GM_CLEAR_REQUEST
						GlobusSetRemoteJobId( NULL, false );
						globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
						jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
						SetRemoteJobStatus( NULL );
						gmState = GM_CLEAR_REQUEST;
					}
					UpdateGlobusState( status, error );
					ClearCallbacks();
					lastProbeTime = now;
				} else {
					GetCallbacks();
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + 5) > now ) {
					delay = (lastProbeTime + 5) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			} else {
				rc = gahp->globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
				// Clear out the job id so that the job won't be held
				// in GM_CLEAR_REQUEST
				GlobusSetRemoteJobId( NULL, false );
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
				SetRemoteJobStatus( NULL );
				gmState = GM_CLEAR_REQUEST;
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
			/*
			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
					procID.cluster, procID.proc,
					GlobusJobStatusName(old_globus_state),
					GlobusJobStatusName(globusState));
			*/
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

	if ( ( connect_failure_jobmanager || connect_failure_gatekeeper ) && 
		 !resourceDown ) {
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
			if ( connect_failure_jobmanager ) {
				jmUnreachable = true;
			}
			RequestPing();
		}
	}
}

void GlobusJob::CommunicationTimeout()
{
	// This function is called by a daemonCore timer if the resource
	// object has been waiting too long for a gatekeeper ping to 
	// succeed.
	// For now, put the job on hold.

	globusStateErrorCode = GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED;
	gmState = GM_HOLD;
	communicationTimeoutTid = -1;
	SetEvaluateState();
}

void GlobusJob::NotifyResourceDown()
{
	BaseJob::NotifyResourceDown();

	jmUnreachable = false;
	// set a timeout timer, so we don't wait forever for this
	// resource to reappear.
	if ( communicationTimeoutTid != -1 ) {
		// timer already set, our work is done
		return;
	}
	int timeout = param_integer("GLOBUS_GATEKEEPER_TIMEOUT",60*60*24*5);
	int time_of_death = 0;
	unsigned int now = time(NULL);
	jobAd->LookupInteger( ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME, time_of_death );
	if ( time_of_death ) {
		timeout = timeout - (now - time_of_death);
	}

	if ( timeout > 0 ) {
		communicationTimeoutTid = daemonCore->Register_Timer( timeout,
							(TimerHandlercpp)&GlobusJob::CommunicationTimeout,
							"CommunicationTimeout", (Service*) this );
	} else {
		// timeout must have occurred while the gridmanager was down
		CommunicationTimeout();
	}

	if (!time_of_death) {
		jobAd->Assign(ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME,(int)now);
		requestScheddUpdate( this, false );
	}
}

void GlobusJob::NotifyResourceUp()
{
	BaseJob::NotifyResourceUp();

	if ( communicationTimeoutTid != -1 ) {
		daemonCore->Cancel_Timer(communicationTimeoutTid);
		communicationTimeoutTid = -1;
	}
	if ( jmUnreachable ) {
		jmDown = true;
	}
	jmUnreachable = false;
	int time_of_death = 0;
	jobAd->LookupInteger( ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME, time_of_death );
	if ( time_of_death ) {
		jobAd->AssignExpr(ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME,"Undefined");
		requestScheddUpdate( this, false );
	}
}

bool GlobusJob::AllowTransition( int new_state, int old_state )
{

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
    if ( new_state == old_state ||
		 new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED) ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN) ||
		 ( old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) ) {
		return false;
	}

	return true;
}


void GlobusJob::UpdateGlobusState( int new_state, int new_error_code )
{
	bool allow_transition;

	lastRemoteStatusUpdate = time(NULL);

	allow_transition = AllowTransition( new_state, globusState );

		// We want to call SetRemoteJobStatus() even if the status
		// hasn't changed, so that BaseJob will know that we got a
		// status update.
	if ( ( allow_transition || new_state == globusState ) &&
		 new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {

		SetRemoteJobStatus( GlobusJobStatusName( new_state ) );
	}

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				GlobusJobStatusName(globusState),
				GlobusJobStatusName(new_state));

		if ( ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			   new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				if ( !submitFailedLogged ) {
					WriteGlobusSubmitFailedEventToUserLog( jobAd,
														   new_error_code,
														   gahp->globus_gram_client_error_string(new_error_code) );
					submitFailedLogged = true;
				}
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				if ( !submitLogged ) {
						// The GlobusSubmit event is now deprecated
					WriteGlobusSubmitEventToUserLog( jobAd );
					WriteGridSubmitEventToUserLog( jobAd );
					submitLogged = true;
				}

				jobAd->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
									  num_globus_submits );
				num_globus_submits++;
				jobAd->Assign( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE)
			 && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
			globusStateBeforeFailure = globusState;
		} else {
			jobAd->Assign( ATTR_GLOBUS_STATUS, new_state );
		}

		globusState = new_state;
		globusStateErrorCode = new_error_code;
		enteredCurrentGlobusState = time(NULL);

		requestScheddUpdate( this, false );

		SetEvaluateState();
	}
}

void GlobusJob::GramCallback( int new_state, int new_error_code )
{
	lastRemoteStatusUpdate = time(NULL);

	if ( AllowTransition(new_state,
						 callbackGlobusState ?
						 callbackGlobusState :
						 globusState ) ) {

		callbackGlobusState = new_state;
		callbackGlobusStateErrorCode = new_error_code;

		SetEvaluateState();
	} else if ( new_state == globusState ) {
		SetRemoteJobStatus( GlobusJobStatusName( globusState ) );
	}
}

bool GlobusJob::GetCallbacks()
{
	if ( callbackGlobusState != 0 ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateErrorCode );

		callbackGlobusState = 0;
		callbackGlobusStateErrorCode = 0;

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
			 globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED &&
			 !jmShouldBeStoppingTime ) {

			jmShouldBeStoppingTime = time(NULL);
		}

		return true;
	} else {
		return false;
	}
}

void GlobusJob::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
}

BaseResource *GlobusJob::GetResource()
{
	return (BaseResource *)myResource;
}

void GlobusJob::GlobusSetRemoteJobId( const char *job_id, bool is_gt5 )
{
		// We need to maintain a hashtable based on job contact strings with
		// the port number stripped. This is because the port number in the
		// jobmanager contact string changes as jobmanagers are restarted.
		// We need to keep the port number of the current jobmanager so that
		// we can contact it, but job status messages can arrive using either
		// the current port (from the running jobmanager) or the original
		// port (from the Grid Monitor).
	if ( jobContact ) {
		JobsByContact.remove(globusJobId(jobContact));
	}
	if ( job_id ) {
		ASSERT( JobsByContact.insert(globusJobId(job_id), this) == 0 );
	}

	free( jobContact );
	if ( job_id ) {
		jobContact = strdup( job_id );
	} else {
		jobContact = NULL;
	}

	if ( myResource ) {
		is_gt5 = myResource->IsGt5();
	}
	std::string full_job_id;
	if ( job_id ) {
		formatstr( full_job_id, "%s %s %s", is_gt5 ? "gt5" : "gt2",
							 resourceManagerString, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

bool GlobusJob::IsExitStatusValid()
{
	/* Using gridshell?  They're valid.  No gridshell?  Not available. */
	return useGridShell;
}

std::string *GlobusJob::buildSubmitRSL()
{
	bool transfer;
	std::string *rsl = new std::string;
	std::string iwd = "";
	std::string riwd = "";
	std::string buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;

	if(useGridShell) {
		dprintf(D_FULLDEBUG, "(%d.%d) Using gridshell\n",
			procID.cluster, procID.proc );
	}

	if ( jobAd->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( jobAd->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
		iwd = attr_value;
		int len = strlen(attr_value);
		if ( len > 1 && attr_value[len - 1] != '/' ) {
			iwd += '/';
		}
	} else {
		iwd = "/";
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	//Start off the RSL
	formatstr( *rsl, "&(rsl_substitution=(GRIDMANAGER_GASS_URL %s))",
				  gassServerUrl );

	std::string executable_path;
	GetJobExecutable( jobAd, executable_path );
	if ( executable_path.empty() ) {
			// didn't find any executable in the spool directory,
			// so use what is explicitly stated in the job ad
		if( ! jobAd->LookupString( ATTR_JOB_CMD, &attr_value ) ) {
			attr_value = (char *)malloc(1);
			attr_value[0] = 0;
		}
		executable_path = attr_value;
		free(attr_value);
		attr_value = NULL;
	}
	*rsl += "(executable=";

	bool transfer_executable = false;
		// TODO JEF this looks very fishy
	if( ! jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) ) {
		transfer_executable = true;
	}

	std::string input_classad_filename;
	std::string output_classad_filename;
	std::string gridshell_log_filename;
	formatstr( gridshell_log_filename, "condor_gridshell.log.%d.%d",
			 procID.cluster, procID.proc );

	if( useGridShell ) {
		// We always transfer the gridshell executable.

		/* TODO XXX adesmet: I'm probably stomping all over the GRAM 1.4
		   detection and work around.  For example, I assume I can stick the
		   real executable into the transfer_input_files, but 1.4 doesn't
		   support that (1.6 does).  Make sure this is worked out.  */

		/* TODO XXX adesmet: This assumes that the local gridshell can run on
		 * the 
		   remote side.  For cross-architecture/OS jobs, this is false.  We'll
		   need to more intelligently select a gridshell binary, perhaps
		   autodetectings (surprisingly hard), or forcing user to specify
		   CPU/OS (and defaulting to local one if not specified).*/

		buff = "$(GRIDMANAGER_GASS_URL)";
		char * tmp = param("GRIDSHELL");
		if( tmp ) {
			buff += tmp;
			free(tmp);
		} else {

			/* TODO XXX adesmet: Put job on hold, then bail.  Also add test to
			   condor_submit.  If job.gridshell == TRUE, then condor_config_val
			   GRIDSHELL must be defined. */

		}

		bool bsuccess = write_classad_input_file( jobAd, executable_path, input_classad_filename );
		if( ! bsuccess ) {
			/* TODO XXX adesmet: Writing to file failed?  Bail. */
			dprintf(D_ALWAYS, "(%d.%d) Attempt to write gridshell file %s failed.\n", 
				procID.cluster, procID.proc, input_classad_filename.c_str() );
		}

		formatstr(output_classad_filename, "%s.OUT", input_classad_filename.c_str());
		outputClassadFilename = output_classad_filename;


	} else if ( transfer_executable ) {
		buff = "$(GRIDMANAGER_GASS_URL)";
		if ( executable_path[0] != '/' ) {
			buff += iwd;
		}
		buff += executable_path;

	} else {
		buff = executable_path;
	}

	*rsl += rsl_stringify( buff );

	if ( jobAd->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
		*rsl += ")(directory=";
		*rsl += rsl_stringify( attr_value );

		riwd = attr_value;
	} else {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.
		*rsl += ")(scratchdir='')(directory=$(SCRATCH_DIRECTORY)";

		riwd = "$(SCRATCH_DIRECTORY)";
	}
	if ( riwd[riwd.length()-1] != '/' ) {
		riwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if(useGridShell) {
		/* for gridshell, pass the gridshell the filename of the input
		   classad.  The real arguments will be in the classad, so we
		   don't need to pass them. */
		*rsl += ")(arguments=-gridshell -job-input-ad ";
		*rsl += input_classad_filename;
		*rsl += " -job-output-ad ";
		*rsl += output_classad_filename;
		*rsl += " -job-stdin - -job-stdout - -job-stderr -";
	} else {
		ArgList args;
		std::string arg_errors;
		if(!args.AppendArgsFromClassAd(jobAd, arg_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
					procID.cluster, procID.proc, arg_errors.c_str());
			formatstr(errorString, "Failed to read job arguments: %s\n",
					arg_errors.c_str());
			delete rsl;
			return NULL;
		}
		if(args.Count() != 0) {
			*rsl += ")(arguments=";
			for(int i=0;i<args.Count();i++) {
				if(i) {
					*rsl += ' ';
				}
				*rsl += rsl_stringify(args.GetArg(i));
			}
		}
	}

	if ( jobAd->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {
		*rsl += ")(stdin=";
		if ( !jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer ) || transfer ) {
			buff = "$(GRIDMANAGER_GASS_URL)";
			if ( attr_value[0] != '/' ) {
				buff += iwd;
			}
			buff += attr_value;
		} else {
			buff = attr_value;
		}
		*rsl += rsl_stringify( buff );
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( streamOutput ) {
		*rsl += ")(stdout=";
		formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localOutput );
		*rsl += rsl_stringify( buff );
	} else {
		if ( stageOutput ) {
			*rsl += ")(stdout=$(GLOBUS_CACHED_STDOUT)";
		} else {
			if ( jobAd->LookupString(ATTR_JOB_OUTPUT, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				*rsl += ")(stdout=";
				*rsl += rsl_stringify( attr_value );
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
	}

	if ( streamError ) {
		*rsl += ")(stderr=";
		formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localError );
		*rsl += rsl_stringify( buff );
	} else {
		if ( stageError ) {
			*rsl += ")(stderr=$(GLOBUS_CACHED_STDERR)";
		} else {
			if ( jobAd->LookupString(ATTR_JOB_ERROR, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				*rsl += ")(stderr=";
				*rsl += rsl_stringify( attr_value );
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
	}

	bool has_input_files = jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) && *attr_value;

	if ( ( useGridShell && transfer_executable ) || has_input_files) {
		StringList filelist( NULL, "," );
		if( attr_value ) {
			filelist.initializeFromString( attr_value );
		}
		if( useGridShell ) {
			filelist.append(input_classad_filename.c_str());
			if(transfer_executable) {
				filelist.append(executable_path.c_str());
			}
		}
		if ( !filelist.isEmpty() ) {
			char *filename;
			*rsl += ")(file_stage_in=";
			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {
				// append file pairs to rsl
				*rsl += '(';
				buff = "$(GRIDMANAGER_GASS_URL)";
				if ( filename[0] != '/' ) {
					buff += iwd;
				}
				buff += filename;
				*rsl += rsl_stringify( buff );
				*rsl += ' ';
				buff = riwd;
				buff += condor_basename( filename );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError || useGridShell) {
		StringList filelist( NULL, "," );
		if( attr_value ) {
			filelist.initializeFromString( attr_value );
		}
		if( useGridShell ) {
				// If we're the grid shell, we want to append some
				// files to  be transfered back: the final status
				// ClassAd from the gridshell

			ASSERT( output_classad_filename.c_str() );
			filelist.append( output_classad_filename.c_str() );
			filelist.append( gridshell_log_filename.c_str() );
		}
		if ( !filelist.isEmpty() || stageOutput || stageError ) {
			char *filename;
			*rsl += ")(file_stage_out=";

			if ( stageOutput ) {
				*rsl += "($(GLOBUS_CACHED_STDOUT) ";
				formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localOutput );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}

			if ( stageError ) {
				*rsl += "($(GLOBUS_CACHED_STDERR) ";
				formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localError );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}

			char *remaps = NULL;
			std::string new_name;
			jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {
				// append file pairs to rsl
				*rsl += '(';
				buff = "";
				if ( filename[0] != '/' ) {
					buff = riwd;
				}
				buff += filename;
				*rsl += rsl_stringify( buff );
				*rsl += ' ';
				buff = "$(GRIDMANAGER_GASS_URL)";
				if ( remaps && filename_remap_find( remaps,
												condor_basename( filename ),
												new_name ) ) {
					if ( new_name[0] != '/' ) {
						buff += iwd;
					}
					buff += new_name;
				} else {
					buff += iwd;
					buff += condor_basename( filename );
				}
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}
			free( remaps );
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( useGridShell ) {
		*rsl += ")(environment=";
		*rsl += "(CONDOR_CONFIG 'only_env')";
		*rsl += "(_CONDOR_GRIDSHELL_DEBUG 'D_FULLDEBUG')";
		*rsl += "(_CONDOR_GRIDSHELL_LOG '";
		*rsl += gridshell_log_filename.c_str();
		*rsl += "')";
	} else {
		Env envobj;
		std::string env_errors;
		if(!envobj.MergeFrom(jobAd, env_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job environment: %s\n",
					procID.cluster, procID.proc, env_errors.c_str());
			formatstr(errorString, "Failed to read job environment: %s\n",
					env_errors.c_str());
			delete rsl;
			if (rsl_suffix) free(rsl_suffix);
			return NULL;
		}
		char **env_vec = envobj.getStringArray();
		int i = 0;
		if ( env_vec[0] ) {
			*rsl += ")(environment=";
		}
		for ( i = 0; env_vec[i]; i++ ) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			*equals = '\0';
			formatstr( buff, "(%s %s)", env_vec[i],
							 rsl_stringify(equals + 1) );
			*rsl += buff;
		}
		deleteStringArray(env_vec);
	}

	formatstr( buff, ")(proxy_timeout=%d", JM_MIN_PROXY_TIME );
	*rsl += buff;

	int default_timeout = JM_COMMIT_TIMEOUT;
	int probe_interval = myResource->GetJobPollInterval();
	if ( default_timeout < 2 * probe_interval ) {
		default_timeout = 2 * probe_interval;
	}
	int commit_timeout = param_integer("GRIDMANAGER_GLOBUS_COMMIT_TIMEOUT", default_timeout);

	formatstr( buff, ")(save_state=yes)(two_phase=%d)"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))",
				  commit_timeout);
	*rsl += buff;

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	dprintf( D_FULLDEBUG, "Final RSL: %s\n", rsl->c_str() );
	return rsl;
}

std::string *GlobusJob::buildRestartRSL()
{
	std::string *rsl = new std::string;
	std::string buff;

	DeleteOutput();

	formatstr( *rsl, "&(rsl_substitution=(GRIDMANAGER_GASS_URL %s))(restart=%s)"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))", gassServerUrl,
				  jobContact );
	if ( streamOutput ) {
		*rsl += "(stdout=";
		formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localOutput );
		*rsl += rsl_stringify( buff );
		*rsl += ")(stdout_position=0)";
	}
	if ( streamError ) {
		*rsl += "(stderr=";
		formatstr( buff, "$(GRIDMANAGER_GASS_URL)%s", localError );
		*rsl += rsl_stringify( buff );
		*rsl += ")(stderr_position=0)";
	}

	formatstr( buff, "(proxy_timeout=%d)", JM_MIN_PROXY_TIME );
	*rsl += buff;

	return rsl;
}

std::string *GlobusJob::buildStdioUpdateRSL()
{
	std::string *rsl = new std::string;
	std::string buff;
	char *attr_value = NULL; /* in case the classad lookups fail */

	DeleteOutput();

	formatstr( *rsl, "&(remote_io_url=%s)", gassServerUrl );
	if ( streamOutput ) {
		*rsl += "(stdout=";
		formatstr( buff, "%s%s", gassServerUrl, localOutput );
		*rsl += rsl_stringify( buff );
		*rsl += ")(stdout_position=0)";
	}
	if ( streamError ) {
		*rsl += "(stderr=";
		formatstr( buff, "%s%s", gassServerUrl, localError );
		*rsl += rsl_stringify( buff );
		*rsl += ")(stderr_position=0)";
	}

	if ( jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		// GRAM 1.6 won't let you change file transfer info in a
		// stdio-update, so force it to fail, resulting in a stop-and-
		// restart
		*rsl += "(invalid=bad)";
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError ) {
		// GRAM 1.6 won't let you change file transfer info in a
		// stdio-update, so force it to fail, resulting in a stop-and-
		// restart
		*rsl += "(invalid=bad)";
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	return rsl;
}

bool GlobusJob::GetOutputSize( int& output_size, int& error_size )
{
	int rc;
	struct stat file_status;
	bool retval = true;

	if ( streamOutput || stageOutput ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
					 "(%d.%d) stat failed for output file %s (errno=%d)\n",
					 procID.cluster, procID.proc, localOutput, errno );
			output_size = 0;
			retval = false;
		} else {
			output_size = file_status.st_size;
		}
	}

	if ( streamError || stageError ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
					 "(%d.%d) stat failed for error file %s (errno=%d)\n",
					 procID.cluster, procID.proc, localError, errno );
			error_size = 0;
			retval = false;
		} else {
			error_size = file_status.st_size;
		}
	}

	return retval;
}

void GlobusJob::DeleteOutput()
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

bool GlobusJob::RetryFailureOnce( int error_code )
{
	switch ( error_code ) {
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGE_OUT_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE:
		return true;
	default:
		return false;
	}
}

bool GlobusJob::RetryFailureAlways( int error_code )
{
	switch ( error_code ) {
	case GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED:
		return true;
	default:
		return false;
	}
}

/*
bool GlobusJob::FailureIsRestartable( int error_code )
{
	switch( error_code ) {
		// Normally, 0 isn't a valid error_code, but it can be returned
		// when a poll of the job by the jobmanager fails, which is not
		// restartable.
	case 0:
	case GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_NOT_FOUND:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STDIN_NOT_FOUND:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_EXECUTABLE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_STDIN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_PERMISSIONS:
	case GLOBUS_GRAM_PROTOCOL_ERROR_BAD_DIRECTORY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_CANCELLED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_SUBMIT_UNKNOWN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JOBTYPE_NOT_SUPPORTED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_TEMP_SCRIPT_FILE_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JOB_EXECUTION_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_USER_PROXY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC:
		// STAGE_OUT_FAILED can be a temporary error that a restart can
		// fix, but it's often caused by having a file in the stage out
		// list that the job didn't produce, which is a permanent error.
		// Given our current limited handling of errors that may or may
		// not be temporary, treat it as a permanent error.
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGE_OUT_FAILED:
		return false;
	case GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED:
	default:
		return true;
	}
}
*/

bool GlobusJob::FailureNeedsCommit( int error_code )
{
	switch( error_code ) {
	case GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED:
		return false;
	default:
		return true;
	}
}

bool
GlobusJob::JmShouldSleep()
{
	// Don't put the jobmanager to sleep if we want to do a status call.
	// This is part of a workaround for Globus bug 3411.
	if ( probeNow == true ) {
		return false;
	}

	if (!jobProxy) {
		return false;
	}

	if ( jmProxyExpireTime < jobProxy->expiration_time ) {
		// Don't forward the refreshed proxy if the remote proxy has more
		// than GRIDMANAGER_PROXY_RENEW_LIMIT time left.
		int renew_min = param_integer( "GRIDMANAGER_PROXY_REFRESH_TIME", 6*3600 );
		if ( time(NULL) >= jmProxyExpireTime - renew_min ) {
			return false;
		} else {
			daemonCore->Reset_Timer( evaluateStateTid,
								 ( jmProxyExpireTime - renew_min ) - time(NULL) );
		}
	}
	if ( condorState != IDLE && condorState != RUNNING ) {
		return false;
	}
	if ( myResource->IsGt5() ) {
		return false;
	}
	if ( GlobusResource::GridMonitorEnabled() == false ) {
		return false;
	}
	if ( useGridJobMonitor == false ) {
		return false;
	}

	// If our resource object is making its first attempt to start the
	// grid monitor and our jobmanager is asleep, wait for the grid
	// monitor to succeed or fail before considering restarting the
	// jobmanager.
	// This is meant to avoid unnecessary jobmanager restarts when the
	// gridmanager starts up after a failure.
	if ( !myResource->GridMonitorFirstStartup() ||
		 gmState != GM_JOBMANAGER_ASLEEP ) {

		int limit = param_integer( "GRID_MONITOR_NO_STATUS_TIMEOUT", 15*60 );
		if ( myResource->LastGridJobMonitorUpdate() >
			 lastRemoteStatusUpdate + limit ) {
			return false;
		}
		if ( myResource->GridJobMonitorActive() == false ) {
			return false;
		}
	}

	switch ( globusState ) {
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING:
		return true;
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE:
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED:
		if ( ( !streamOutput && !streamError ) ||
			 myResource->GetJMLimit(true) != GM_RESOURCE_UNLIMITED ) {

			return true;
		} else {
			return false;
		}
	default:
		return false;
	}
}
