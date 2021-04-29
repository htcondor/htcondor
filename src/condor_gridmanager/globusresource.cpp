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
#include "condor_config.h"
#include "string_list.h"
#include "directory.h"

#include "globusresource.h"
#include "globusjob.h"
#include "gridmanager.h"

#define DEFAULT_MAX_JOBMANAGERS_PER_RESOURCE		10

// If the grid_monitor appears hosed, how long do we
// disable grid_monitoring that site for?
// (We actually just set the timer for that site to this)
int GlobusResource::monitorDisableLength = DEFAULT_GM_DISABLE_LENGTH;

int GlobusResource::gahpCallTimeout = 300;	// default value
bool GlobusResource::enableGridMonitor = false;

std::map <std::string, GlobusResource *>
    GlobusResource::ResourcesByName;

static unsigned int g_MonitorUID = 0;

GlobusResource *GlobusResource::FindOrCreateResource( const char *resource_name,
													  const Proxy *proxy,
													  bool is_gt5 )
{
	GlobusResource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	std::string &key = HashName( canonical_name, proxy->subject->fqan );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new GlobusResource( canonical_name, proxy, is_gt5 );
		ASSERT(resource);
		if ( resource->Init() == false ) {
			delete resource;
			resource = NULL;
		} else {
			ResourcesByName[key] = resource;
		}
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}

GlobusResource::GlobusResource( const char *resource_name,
								const Proxy *proxy, bool is_gt5 )
	: BaseResource( resource_name )
{
	initialized = false;
	proxySubject = strdup( proxy->subject->subject_name );
	proxyFQAN = strdup( proxy->subject->fqan );

	if ( param_boolean( "GRAM_VERSION_DETECTION", true ) ) {
		m_isGt5 = false;
		m_versionKnown = false;
	} else {
		m_isGt5 = is_gt5;
		m_versionKnown = true;
	}

	submitJMLimit = DEFAULT_MAX_JOBMANAGERS_PER_RESOURCE / 2;
	restartJMLimit = DEFAULT_MAX_JOBMANAGERS_PER_RESOURCE - submitJMLimit;

	checkMonitorTid = daemonCore->Register_Timer( TIMER_NEVER,
							(TimerHandlercpp)&GlobusResource::CheckMonitor,
							"GlobusResource::CheckMonitor", (Service*)this );
	monitorActive = false;
	monitorSubmitActive = false;
	monitorStarting = false;

	monitorDirectory = NULL;
	monitorJobStatusFile = NULL;
	monitorLogFile = NULL;
	jobStatusFileLastReadTime = 0;
	logFileLastReadTime = 0;
	jobStatusFileLastUpdate = 0;
	monitorGramJobId = NULL;
	gahp = NULL;
	monitorGahp = NULL;
	monitorRetryTime = 0;
	monitorFirstStartup = true;
	monitorGramJobStatus = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
	monitorGramErrorCode = 0;
}

GlobusResource::~GlobusResource()
{
	ResourcesByName.erase( HashName( resourceName, proxyFQAN ) );
	if ( checkMonitorTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( checkMonitorTid );
	}
	CleanupMonitorJob();
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( monitorGahp != NULL ) {
		delete monitorGahp;
	}
	if ( proxySubject ) {
		free( proxySubject );
	}
	free( proxyFQAN );
}

bool GlobusResource::Init()
{
	if ( initialized ) {
		return true;
	}

	std::string gahp_name;
	formatstr( gahp_name, "GT2/%s", proxyFQAN );

	gahp = new GahpClient( gahp_name.c_str() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	monitorGahp = new GahpClient( gahp_name.c_str() );

	monitorGahp->setNotificationTimerId( checkMonitorTid );
	monitorGahp->setMode( GahpClient::normal );
	monitorGahp->setTimeout( gahpCallTimeout );

	initialized = true;

	Reconfig();

	return true;
}

void GlobusResource::Reconfig()
{
	int tmp_int;

	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );

	tmp_int = param_integer( "GRIDMANAGER_MAX_JOBMANAGERS_PER_RESOURCE",
							 DEFAULT_MAX_JOBMANAGERS_PER_RESOURCE );
	if ( tmp_int == 0 ) {
		submitJMLimit = GM_RESOURCE_UNLIMITED;
		restartJMLimit = GM_RESOURCE_UNLIMITED;
	} else {
		if ( tmp_int < 2 ) {
			tmp_int = 2;
		}
		submitJMLimit = tmp_int / 2;
		restartJMLimit = tmp_int - submitJMLimit;
	}

	// If the jobmanager limits were widened, move jobs from Wanted to
	// Allowed and signal them
	while ( ( submitJMsAllowed.Length() + restartJMsAllowed.Length() <
			  submitJMLimit + restartJMLimit ) &&
			( submitJMsWanted.Length() != 0 ||
			  restartJMsWanted.Length() != 0 ) ) {
		JMComplete( NULL );
	}

	if ( enableGridMonitor ) {
		// start grid monitor
		daemonCore->Reset_Timer( checkMonitorTid, 0 );
	} else {
		// stop grid monitor
		if ( monitorActive || monitorStarting ) {
			StopMonitor();
		}

		daemonCore->Reset_Timer( checkMonitorTid, TIMER_NEVER );
	}
}

const char *GlobusResource::ResourceType()
{
	return "gt2";
}

const char *GlobusResource::CanonicalName( const char *name )
{
	static std::string canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	formatstr( canonical, "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.c_str();
}

std::string & GlobusResource::HashName( const char *resource_name,
									  const char *proxy_subject )
{
	static std::string hash_name;

	formatstr( hash_name, "gt2 %s#%s", resource_name, proxy_subject );

	return hash_name;
}

const char *GlobusResource::GetHashName()
{
	return HashName( resourceName, proxyFQAN ).c_str();
}

void GlobusResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
	resource_ad->Assign( ATTR_X509_USER_PROXY_FQAN, proxyFQAN );
	resource_ad->Assign( "SubmitJobmanagerLimit", submitJMLimit );
	resource_ad->Assign( "SubmitJobmanagersAllowed", submitJMsAllowed.Number() );
	resource_ad->Assign( "SubmitJobmanagersWanted", submitJMsWanted.Number() );
	resource_ad->Assign( "RestartJobmanagerLimit", restartJMLimit );
	resource_ad->Assign( "RestartJobmanagersAllowed", restartJMsAllowed.Number() );
	resource_ad->Assign( "RestartJobmanagersWanted", restartJMsWanted.Number() );

	gahp->PublishStats( resource_ad );
}

void GlobusResource::UnregisterJob( BaseJob *base_job )
{
	GlobusJob* job = dynamic_cast<GlobusJob*>( base_job );

	JMComplete( job );

	BaseResource::UnregisterJob( job );
		// This object may be deleted now. Don't do anything below here!
}

void GlobusResource::SetJobPollInterval()
{
	if ( m_isGt5 ) {
		BaseResource::SetJobPollInterval();
	} else {
		m_jobPollInterval = (submitJMsAllowed.Length() + restartJMsAllowed.Length()) / m_paramJobPollRate;
		if ( m_jobPollInterval < m_paramJobPollInterval ) {
			m_jobPollInterval = m_paramJobPollInterval;
		}
	}
}

bool GlobusResource::RequestJM( GlobusJob *job, bool is_submit )
{
	GlobusJob *jobptr;

	if ( m_isGt5 ) {
		return true;
	}

	if ( is_submit ) {
		submitJMsWanted.Rewind();
		while ( submitJMsWanted.Next( jobptr ) ) {
			if ( jobptr == job ) {
				return false;
			}
		}

		submitJMsAllowed.Rewind();
		while ( submitJMsAllowed.Next( jobptr ) ) {
			if ( jobptr == job ) {
				return true;
			}
		}

		if ( submitJMsAllowed.Length() + restartJMsAllowed.Length() <
			 submitJMLimit + restartJMLimit ) {
			submitJMsAllowed.Append( job );
			SetJobPollInterval();
			return true;
		} else {
			submitJMsWanted.Append( job );
			return false;
		}
	} else {
		restartJMsWanted.Rewind();
		while ( restartJMsWanted.Next( jobptr ) ) {
			if ( jobptr == job ) {
				return false;
			}
		}

		restartJMsAllowed.Rewind();
		while ( restartJMsAllowed.Next( jobptr ) ) {
			if ( jobptr == job ) {
				return true;
			}
		}

		if ( submitJMsAllowed.Length() + restartJMsAllowed.Length() <
			 submitJMLimit + restartJMLimit ) {
			restartJMsAllowed.Append( job );
			SetJobPollInterval();
			return true;
		} else {
			restartJMsWanted.Append( job );
			return false;
		}
	}
}

void GlobusResource::JMComplete( GlobusJob *job )
{
	if ( m_isGt5 ) {
		return;
	}

	// If the job was in one of the Allowed queues, check whether we
	// should promote a job from the Wanted queues to take its place.
	// Also perform the check if NULL is passed.
	if ( job == NULL || submitJMsAllowed.Delete( job ) ||
		 restartJMsAllowed.Delete( job ) ) {

		// Check if there's room in the Allowed queues for a job and
		// if there are any jobs waiting in the Wanted queues.
		if ( ( submitJMsAllowed.Length() + restartJMsAllowed.Length() <
			   submitJMLimit + restartJMLimit ) &&
			 ( submitJMsWanted.Length() != 0 ||
			   restartJMsWanted.Length() != 0 ) ) {

			// Which Wanted queue should we take a job from? If either
			// queue is empty, take a job from the other queue.
			// If neither queue is empty, take a job from the queue
			// that's under its limit.
			if ( submitJMsWanted.Length() == 0 ||
				 ( restartJMsWanted.Length() != 0 &&
				   restartJMsAllowed.Length() < restartJMLimit ) ) {

				GlobusJob *queued_job = restartJMsWanted.Head();
				restartJMsWanted.Delete( queued_job );
				restartJMsAllowed.Append( queued_job );
				queued_job->SetEvaluateState();

			} else {

				GlobusJob *queued_job = submitJMsWanted.Head();
				submitJMsWanted.Delete( queued_job );
				submitJMsAllowed.Append( queued_job );
				queued_job->SetEvaluateState();
			}
		}

		SetJobPollInterval();
	} else {
		// We only have to check the Wanted queues if the job wasn't in
		// the Allowed queues
		submitJMsWanted.Delete( job );
		restartJMsWanted.Delete( job );
	}
}

void GlobusResource::JMAlreadyRunning( GlobusJob *job )
{
	if ( !m_isGt5 ) {
		restartJMsAllowed.Append( job );
		SetJobPollInterval();
	}
}

void GlobusResource::DoPing( unsigned& ping_delay, bool& ping_complete,
							 bool& ping_succeeded )
{
	int rc;

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_IS_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		ping_delay = TIMER_NEVER;
		return;
	}

	ping_delay = 0;

	if ( m_versionKnown ) {
		rc = gahp->globus_gram_client_ping( resourceName );
	} else {
		rc = gahp->globus_gram_client_get_jobmanager_version( resourceName );
	}

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) 
	{
		ping_complete = true;
		ping_succeeded = false;
	} else {
		if ( !m_versionKnown ) {
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_UNPACK_FAILED ) {
				m_isGt5 = false;
				m_versionKnown = true;
			} else if ( rc == GLOBUS_SUCCESS ) {
				m_isGt5 = true;
				m_versionKnown = true;

				// Any jobs queued up under gt2 throttles need to signaled.
				GlobusJob *job;
				submitJMsWanted.Rewind();
				while ( submitJMsWanted.Next( job ) ) {
					job->SetEvaluateState();
				}

				restartJMsWanted.Rewind();
				while ( restartJMsWanted.Next( job ) ) {
					job->SetEvaluateState();
				}
			}
		}

		ping_complete = true;
		ping_succeeded = true;
	}
}

void
GlobusResource::CheckMonitor()
{
	BaseJob *base_job = NULL;
	GlobusJob *job;
	// TODO should we require our jobs to request the grid monitor before
	//   we'll start it up?
	// TODO what if we're in the middle of a ping when we get here? Either
	//   delay until ping is done or have seperate GahpClient
	// TODO if resource is down, should we delay any actions?
	daemonCore->Reset_Timer( checkMonitorTid, TIMER_NEVER );
	dprintf(D_FULLDEBUG, "grid_monitor for %s entering CheckMonitor\n",
		resourceName);

	if ( m_versionKnown && m_isGt5 ) {
		dprintf( D_FULLDEBUG, "Disabling grid_monitor for GRAM5 server %s\n",
				 resourceName );
		return;
	}

	if ( monitorGahp->isInitialized() == false ) {
		dprintf( D_ALWAYS, "GAHP server not initialized yet, not submitting "
				 "grid_monitor now\n" );
		daemonCore->Reset_Timer( checkMonitorTid, 5 );
		return;
	}

	if ( !enableGridMonitor ) {
		return;
	}

	if ( time(NULL) < monitorRetryTime ) {
		daemonCore->Reset_Timer( checkMonitorTid,
								 monitorRetryTime - time(NULL) );
		return;
	}

	if ( firstPingDone == false ) {
		dprintf(D_FULLDEBUG,"grid_monitor for %s: first ping not done yet, "
			"will retry later\n", resourceName);
		daemonCore->Reset_Timer( checkMonitorTid, 5 );
		return;
	}

	if ( monitorSubmitActive ) {
		int rc;
		std::string job_contact;
		monitorGahp->setMode( GahpClient::results_only );
		rc = monitorGahp->globus_gram_client_job_request( NULL, NULL, 0, NULL,
														  job_contact, false );
		if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
			 rc == GAHPCLIENT_COMMAND_PENDING ) {
				// do nothing
		} else if ( rc == 0 ) {
				// successful submit
			monitorGramJobId = strdup( job_contact.c_str() );
			monitorSubmitActive = false;
		} else {
				// submit failed
			dprintf(D_ALWAYS, "grid_monitor job submit failed for resource %s, gram error %d (%s)\n",
					resourceName, rc,
					monitorGahp->globus_gram_client_error_string(rc));
			monitorSubmitActive = false;
			AbandonMonitor();
			return;
		}
	}

	if ( monitorActive == false && monitorStarting == false ) {
		monitorStarting = true;
		if ( SubmitMonitorJob() == true ) {
			daemonCore->Reset_Timer( checkMonitorTid, 30 );
		} else {
			dprintf(D_ALWAYS, "Unable to start grid_monitor for resource %s\n",
				resourceName);
			// TODO: Do nice retry?
			AbandonMonitor();
		}
	} else {
		int rc;
		struct stat file_status;
		int job_status_mod_time;
		int log_mod_time;

		if(monitorJobStatusFile == NULL) {
			EXCEPT("Consistency problem for GlobusResource %s, null job status file name", resourceName);
		}

		if(monitorLogFile == NULL) {
			EXCEPT("Consistency problem for GlobusResource %s, null monitor log file name", resourceName);
		}

		rc = stat( monitorJobStatusFile, &file_status );
		if ( rc < 0 ) {
			EXCEPT( "stat(%s) failed, errno=%d", monitorJobStatusFile, errno );
		}
		job_status_mod_time = file_status.st_mtime;

		rc = stat( monitorLogFile, &file_status );
		if ( rc < 0 ) {
			EXCEPT( "stat(%s) failed, errno=%d", monitorLogFile, errno );
		}
		log_mod_time = file_status.st_mtime;

		if ( job_status_mod_time > jobStatusFileLastReadTime ) {
			dprintf(D_FULLDEBUG, "grid_monitor job status for %s file has been refreshed.\n", resourceName);

			ReadFileStatus status = ReadMonitorJobStatusFile();
			if(status == RFS_OK) {
				dprintf(D_FULLDEBUG, "Read grid_monitor status file for %s successfully\n", resourceName);
				jobStatusFileLastReadTime = time(NULL);
				jobStatusFileLastUpdate = time(NULL);
				daemonCore->Reset_Timer( checkMonitorTid, 30 );

			} else if(status == RFS_PARTIAL) {
				const int RETRY_TIME = 10;
				dprintf(D_FULLDEBUG,"*** status file is partial, "
					"will try again in %d seconds\n", RETRY_TIME);
				daemonCore->Reset_Timer( checkMonitorTid, RETRY_TIME );

			} else if(status == RFS_ERROR) {
				dprintf(D_ALWAYS,"grid_monitor: error reading job status "
					"file for %s, stopping grid monitor\n", resourceName);
				// TODO: Try to restart monitor?
				AbandonMonitor();
				return;

			} else {
				EXCEPT("ReadMonitorJobStatusFile returned unexpected %d "
					"(for %s)", (int)status, resourceName);
			}

		}

		int log_file_timeout = param_integer("GRID_MONITOR_HEARTBEAT_TIMEOUT", 300);
		int monitor_retry_duration = param_integer("GRID_MONITOR_RETRY_DURATION", 900);

		if ( log_mod_time > logFileLastReadTime ) {

			dprintf(D_FULLDEBUG, "grid_monitor log file for %s updated.\n",
				resourceName);
			rc = ReadMonitorLogFile();

			switch( rc ) {
			case 0: // Normal / OK
				dprintf(D_FULLDEBUG,
					"grid_monitor log file for %s looks normal\n",
					resourceName);
				if ( monitorStarting ) {
					dprintf(D_ALWAYS, "Successfully started grid_monitor "
							"for %s\n", resourceName);
					monitorStarting = false;
					monitorFirstStartup = false;
					monitorActive = true;
					registeredJobs.Rewind();
					while ( registeredJobs.Next( base_job ) ) {
						job = static_cast<GlobusJob*>( base_job );
						job->SetEvaluateState();
					}
				}
				logFileLastReadTime = time(NULL);
				daemonCore->Reset_Timer( checkMonitorTid, 30 );
				break;

			case 1: // Exitted normally (should restart)
				dprintf(D_FULLDEBUG,
					"grid_monitor for %s reached maximum lifetime, "
					"restarting...\n", resourceName);
				if ( SubmitMonitorJob() == true ) {
					dprintf(D_FULLDEBUG,
						"grid_monitor for %s restarted.\n", resourceName);
					daemonCore->Reset_Timer( checkMonitorTid, 30 );
				} else {
					dprintf(D_ALWAYS, 
						"Unable to restart grid_monitor for resource %s\n",
						resourceName);
					// TODO: Try to restart monitor?
					AbandonMonitor();
				}
				break;

			case 2: // Exitted with error
				dprintf(D_ALWAYS,"Error with grid_monitor for %s, stopping.\n",
					resourceName);
				// TODO: Try to restart monitor?
				AbandonMonitor();
				break;

			default:
				EXCEPT( "Unknown return value %d from ReadLogFile", rc );

			}

		} else if ( time(NULL) > logFileLastReadTime + log_file_timeout &&
					!monitorStarting ) {
			dprintf( D_ALWAYS, "Haven't heard from running grid_monitor "
					 "at %s for %d seconds, trying new job submission\n",
					 resourceName, log_file_timeout );
			if( ! SubmitMonitorJob() ) {
				dprintf(D_ALWAYS, "Failed to restart grid_monitor.  Giving up on grid_monitor for site %s\n", resourceName);
				AbandonMonitor();
			}
			daemonCore->Reset_Timer( checkMonitorTid, 30);

		} else if ( monitorStarting &&
					time(NULL) > logFileLastReadTime + monitor_retry_duration) {
			dprintf( D_ALWAYS, "Haven't heard from new grid_monitor "
					 "at %s for %d seconds, giving up\n",
					 resourceName, monitor_retry_duration );
			AbandonMonitor();

		} else {
			daemonCore->Reset_Timer( checkMonitorTid, 30 );
		}
	}

	return;
}

void
GlobusResource::AbandonMonitor()
{
	dprintf(D_ALWAYS, "Giving up on grid_monitor for site %s.  "
		"Will retry in %d seconds (%d minutes)\n",
		resourceName, monitorDisableLength, monitorDisableLength / 60);
	StopMonitor();
	monitorRetryTime = time(NULL) + monitorDisableLength;
	daemonCore->Reset_Timer( checkMonitorTid, monitorDisableLength );
}

void
GlobusResource::StopMonitor()
{
	BaseJob *base_job = NULL;
	GlobusJob *job;

	dprintf(D_ALWAYS, "Stopping grid_monitor for resource %s\n", resourceName);

		// Do we want to notify jobs when the grid monitor fails?
	monitorActive = false;
	monitorFirstStartup = false;
	monitorStarting = false;
	if ( monitorActive || monitorFirstStartup ) {
		registeredJobs.Rewind();
		while ( registeredJobs.Next( base_job ) ) {
			job = dynamic_cast<GlobusJob*>( base_job );
			job->SetEvaluateState();
		}
	}
	StopMonitorJob();
}

void
GlobusResource::StopMonitorJob()
{
	/* Not much to do, we just fire and forget the grid_monitor currently.
	   In the future we might want to try cancelling the job itself.
	*/
	monitorSubmitActive = false;
	monitorGahp->purgePendingRequests();
	CleanupMonitorJob();
}

void
GlobusResource::CleanupMonitorJob()
{
	if ( monitorGramJobId ) {
		monitorGahp->globus_gram_client_job_cancel( monitorGramJobId );

		free( monitorGramJobId );
		monitorGramJobId = NULL;
		monitorGramJobStatus = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
		monitorGramErrorCode = 0;
	}
	if ( monitorDirectory ) {
		std::string tmp_dir;

		formatstr( tmp_dir, "%s.remove", monitorDirectory );

		if (0 != rename( monitorDirectory, tmp_dir.c_str())) {
			dprintf(D_ALWAYS, "Cannot rename %s to %s\n", monitorDirectory, tmp_dir.c_str());
		}
		free( monitorDirectory );
		monitorDirectory = NULL;

		Directory tmp( tmp_dir.c_str() );
		tmp.Remove_Entire_Directory();

		if (0 != rmdir(tmp_dir.c_str())){
			dprintf(D_ALWAYS, "Cannot remove %s\n", tmp_dir.c_str());
		}
	}
	if(monitorJobStatusFile)
	{
		free(monitorJobStatusFile);
		monitorJobStatusFile = NULL;
	}
	if(monitorLogFile)
	{
		free(monitorLogFile);
		monitorLogFile = NULL;
	}
}

bool
GlobusResource::SubmitMonitorJob()
{
	// return true if job submitted, else return false
	int now = time(NULL);
	int rc;
	char *monitor_executable;
	std::string contact;
	std::string RSL;

	StopMonitorJob();
	
	/* Create monitor directory and files */
	g_MonitorUID++;
	std::string buff;

	formatstr( buff, "%s/grid-monitor.%s.%d", GridmanagerScratchDir,
				  resourceName, g_MonitorUID );
	monitorDirectory = strdup( buff.c_str() );

	if ( mkdir( monitorDirectory, 0700 ) < 0 ) {
		dprintf( D_ALWAYS, "SubmitMonitorJob: mkdir(%s,0700) failed, "
				 "errno=%d (%s)\n", monitorDirectory, errno,
				 strerror( errno ) );
		free( monitorDirectory );
		monitorDirectory = NULL;
		return false;
	}

	formatstr( buff, "%s/grid-monitor-job-status", monitorDirectory );
	monitorJobStatusFile = strdup( buff.c_str() );

	formatstr( buff, "%s/grid-monitor-log", monitorDirectory );
	monitorLogFile = strdup( buff.c_str() );


	rc = creat( monitorJobStatusFile, S_IREAD|S_IWRITE );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"creat(%s,%d) failed, errno=%d (%s)\n",
			resourceName, monitorJobStatusFile, S_IREAD|S_IWRITE, errno,
			strerror( errno ) );
		return false;
	} else {
		close( rc );
	}

	rc = creat( monitorLogFile, S_IREAD|S_IWRITE );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"creat(%s,%d) failed, errno=%d (%s)\n",
			 resourceName, monitorLogFile, S_IREAD|S_IWRITE, errno,
			strerror( errno ) );
		return false;
	} else {
		close( rc );
	}

	jobStatusFileLastReadTime = now;
	logFileLastReadTime = now;

	monitor_executable = param( "GRID_MONITOR" );
	if ( monitor_executable == NULL ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"GRID_MONITOR not defined!\n", resourceName );
		return false;
	}

	monitorGahp->setMode( GahpClient::normal );

	const char *gassServerUrl = monitorGahp->getGlobusGassServerUrl();
	formatstr( RSL, "&(executable=%s%s)(stdout=%s%s)(arguments='--dest-url=%s%s')",
				 gassServerUrl, monitor_executable, gassServerUrl,
				 monitorLogFile, gassServerUrl, monitorJobStatusFile );

	free( monitor_executable );

	formatstr( contact, "%s/jobmanager-fork", resourceName );

	std::string job_contact;
	rc = monitorGahp->globus_gram_client_job_request( contact.c_str(),
													  RSL.c_str(), 1,
													  monitorGahp->getGt2CallbackContact(),
													  job_contact, false );

	if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"globus_gram_client_job_request() returned %d!\n",
			 resourceName, rc );
		return false;
	}

	monitorSubmitActive = true;
	return true;
}

GlobusResource::ReadFileStatus
GlobusResource::ReadMonitorJobStatusFile()
{
	// return true if file successfully processed and jobs notified,
	// else return false. 
	// TODO should distinguish between temporary and permanent problems.
	//   e.g. if file is incomplete, retry after short delay
	FILE *fp;
	char buff[1024];
	char contact[1024];
	int status;
	int scan_start = 0;
	int scan_finish = 0;
	int job_count = 0;

	if(monitorJobStatusFile == NULL) {
		EXCEPT("Consistency problem for GlobusResource::ReadMonitorJobStatusFile %s, null job status file name", resourceName);
	}

	fp = safe_fopen_wrapper_follow( monitorJobStatusFile, "r" );
	if ( fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open grid_monitor job status file %s\n",
				 monitorJobStatusFile );
		return RFS_ERROR;
	}

	if ( fgets( buff, sizeof(buff), fp ) == NULL ) {
		if( feof(fp) ) {
			dprintf( D_FULLDEBUG, "grid_monitor job status file empty (%s), treating as partial.\n",
					 monitorJobStatusFile );
			fclose( fp );
			return RFS_PARTIAL;
		}
		dprintf( D_ALWAYS, "Can't read grid_monitor job status file %s\n",
				 monitorJobStatusFile );
		fclose( fp );
		return RFS_ERROR;
	}
	if ( sscanf( buff, "%d %d", &scan_start, &scan_finish ) != 2 ) {
		dprintf( D_ALWAYS, "Failed to read scan times from grid_monitor "
				 "status file %s\n", monitorJobStatusFile );
		fclose( fp );
		return RFS_ERROR;
	}

	bool found_eof = false;
	while ( fgets( buff, sizeof(buff), fp ) != NULL ) {
		contact[0] = '\0';
		status = 0;

		const char * MAGIC_EOF = "GRIDMONEOF";
		if(strncmp(buff, MAGIC_EOF, strlen(MAGIC_EOF)) == 0) {
			found_eof = true;
			break;
		}

		if ( sscanf( buff, "%s %d", contact, &status ) == 2 &&
			 *contact != '\0' && status > 0 ) {
			int rc;
			GlobusJob *job = NULL;

			job_count++;

			rc = JobsByContact.lookup( globusJobId(contact), job );
			if ( rc == 0 && job != NULL ) {
				if ( status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					status=GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT;
				}
					// Don't flood the log file
					// with a long stream of identical job status updates.
					// We do need to send identical job status updates to
					// the job so that it can track the last time we
					// received an update on its status.
				if ( status != job->globusState ) {
					dprintf(D_FULLDEBUG,"Sending callback of %d to %d.%d (%s)\n",status,job->procID.cluster,job->procID.proc, resourceName);
				}
				job->GramCallback( status, 0 );
			}
		}
	}

	fclose( fp );

	int limit = param_integer( "GRID_MONITOR_NO_STATUS_TIMEOUT", 15*60 );
	int now = time(NULL);
	GlobusJob *next_job;
	registeredJobs.Rewind();
	while ( (next_job = (GlobusJob *)registeredJobs.Next()) != NULL ) {
		if ( next_job->jobContact &&
			 now > next_job->lastRemoteStatusUpdate + limit ) {
			next_job->SetEvaluateState();
		}
	}

	dprintf( D_FULLDEBUG, "Read %s grid_monitor status file for %s: "
			 "scan start=%d, scan finish=%d, job count=%d\n",
			 found_eof ? "full" : "partial",
			 resourceName, scan_start, scan_finish, job_count );

	if(found_eof)
		return RFS_OK;
	return RFS_PARTIAL;
}

int
GlobusResource::ReadMonitorLogFile()
{
	// return 0 on nothing of interest, 1 on normal exit, 2 on error exit
	int retval = 0;
	FILE *fp;
	char buff[1024];

	if( monitorLogFile == NULL)
	{
			EXCEPT("Consistency problem for GlobusResource::ReadMonitorLogFile %s, null monitor log file name", resourceName);
	}

	fp = safe_fopen_wrapper_follow( monitorLogFile, "r" );
	if ( fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open grid_monitor log file %s\n",
				 monitorLogFile );
		return 2;
	}

	while ( fgets( buff, sizeof(buff), fp ) != NULL ) {
		int error_code;

		if ( sscanf( buff, "%*d-%*d-%*d %*d:%*d:%*d ERROR: %d",
					 &error_code ) == 1 ) {

			if ( error_code == 0 ) {
				retval = 1;
			} else {
				dprintf(D_ALWAYS,
					"grid_monitor log file for %s has error code %d\n", 
					resourceName, error_code);
				retval = 2;
			}

		}
	}

	fclose( fp );

	return retval;
}

void
GlobusResource::gridMonitorCallback( int state, int errorcode )
{
	if ( state != monitorGramJobStatus ) {
		dprintf( D_FULLDEBUG, "grid_monitor for %s: gram callback "
				 "status=%d errorcode=%d\n", resourceName, state, errorcode );
		monitorGramJobStatus = state;
		monitorGramErrorCode = errorcode;
	}
}
