/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "globusresource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

// If the grid_monitor appears hosed, how long do we
// disable grid_monitoring that site for?
// (We actually just set the timer for that site to this)
#define GM_DISABLE_LENGTH (60*60)

template class List<GlobusJob>;
template class Item<GlobusJob>;

int GlobusResource::probeInterval = 300;	// default value
int GlobusResource::probeDelay = 15;		// default value
int GlobusResource::gahpCallTimeout = 300;	// default value
bool GlobusResource::enableGridMonitor = false;

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, GlobusResource *>;
template class HashBucket<HashKey, GlobusResource *>;

HashTable <HashKey, GlobusResource *> ResourcesByName( HASH_TABLE_SIZE,
													   hashFunction );
static unsigned int g_MonitorUID = 0;

GlobusResource::GlobusResource( const char *resource_name )
	: BaseResource( resource_name )
{
	resourceDown = false;
	firstPingDone = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GlobusResource::DoPing,
								"GlobusResource::DoPing", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;
	gahp = new GahpClient();
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );
	submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	checkMonitorTid = TIMER_UNSET;
	monitorActive = false;

	monitorJobStatusFile = 0;
	monitorLogFile = 0;
	logFileTimeoutLastReadTime = 0;
	initialMonitorStart = true;

	Reconfig();
}

GlobusResource::~GlobusResource()
{
	daemonCore->Cancel_Timer( pingTimerId );
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( checkMonitorTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( checkMonitorTid );
	}
	CleanupMonitorJob();
}

void GlobusResource::Reconfig()
{
	char *param_value;

	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );

	submitLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS_PER_RESOURCE" );
	if ( param_value == NULL ) {
		// Check old parameter name
		param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS" );
	}
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			submitLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					submitLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( submitLimit <= 0 ) {
		submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	}

	jobLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			jobLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					jobLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( jobLimit <= 0 ) {
		jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;
	}

	// If the jobLimit was widened, move jobs from Wanted to Allowed and
	// add them to Queued
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		GlobusJob *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
//		submitsQueued.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		GlobusJob *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
		queued_job->SetEvaluateState();
	}

	if ( enableGridMonitor && checkMonitorTid == TIMER_UNSET ) {
		// start grid monitor
		checkMonitorTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&GlobusResource::CheckMonitor,
							"GlobusResource::CheckMonitor", (Service*)this );
	} else if ( !enableGridMonitor && checkMonitorTid != TIMER_UNSET ) {
		// stop grid monitor
		if ( monitorActive ) {
			StopMonitor();
		}

		daemonCore->Cancel_Timer( checkMonitorTid );
		checkMonitorTid = TIMER_UNSET;
	}
}

const char *GlobusResource::CanonicalName( const char *name )
{
	static MyString canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	canonical.sprintf( "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.Value();
}

void GlobusResource::RegisterJob( GlobusJob *job, bool already_submitted )
{
	registeredJobs.Append( job );

	if ( already_submitted ) {
		submitsAllowed.Append( job );
	}

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}
}

void GlobusResource::UnregisterJob( GlobusJob *job )
{
	CancelSubmit( job );
	registeredJobs.Delete( job );
	pingRequesters.Delete( job );
}

void GlobusResource::RequestPing( GlobusJob *job )
{
	pingRequesters.Append( job );

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool GlobusResource::RequestSubmit( GlobusJob *job )
{
	bool already_allowed = false;
	GlobusJob *jobptr;

	submitsQueued.Rewind();
	while ( submitsQueued.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsAllowed.Rewind();
	while ( submitsAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			already_allowed = true;
			break;
		}
	}

	if ( already_allowed == false ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			EXCEPT("In GlobusResource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full\n",resourceName);
		}
		if ( submitsAllowed.Length() < jobLimit ) {
			submitsAllowed.Append( job );
			// proceed to see if submitLimit applies
		} else {
			submitsWanted.Append( job );
			return false;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In GlobusResource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n",resourceName);
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
		return true;
	} else {
		submitsQueued.Append( job );
		return false;
	}
}

void GlobusResource::SubmitComplete( GlobusJob *job )
{
	if ( submitsInProgress.Delete( job ) ) {
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			GlobusJob *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsQueued if the job wasn't in
		// submitsInProgress.
		submitsQueued.Delete( job );
	}

	return;
}

void GlobusResource::CancelSubmit( GlobusJob *job )
{
	if ( submitsAllowed.Delete( job ) ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			GlobusJob *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
//			submitsQueued.Append( wanted_job );
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		submitsWanted.Delete( job );
	}

	SubmitComplete( job );

	return;
}

bool GlobusResource::IsEmpty()
{
	return registeredJobs.IsEmpty();
}

bool GlobusResource::IsDown()
{
	return resourceDown;
}

int GlobusResource::DoPing()
{
	int rc;
	bool ping_failed = false;
	GlobusJob *job;

	// Don't perform a ping if we have no requesters and the resource is up
	if ( pingRequesters.IsEmpty() && resourceDown == false &&
		 firstPingDone == true ) {
		daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );
		return TRUE;
	}

	// Don't start a new ping too soon after the previous one. If the
	// resource is up, the minimum time between pings is probeDelay. If the
	// resource is down, the minimum time between pings is probeInterval.
	int delay;
	if ( resourceDown == false ) {
		delay = (lastPing + probeDelay) - time(NULL);
	} else {
		delay = (lastPing + probeInterval) - time(NULL);
	}
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( pingTimerId, delay );
		return TRUE;
	}

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		daemonCore->Reset_Timer( pingTimerId, 5 );
		return TRUE;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_IS_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		return TRUE;
	}

	rc = gahp->globus_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return 0;
	}

	lastPing = time(NULL);

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
		 rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) 
	{
		ping_failed = true;
	}

	if ( ping_failed == resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
				ping_failed?"down":"up");

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}
	} else {
		// State of resource has changed. Notify every job.
		dprintf(D_ALWAYS,"resource %s is now %s\n",resourceName,
				ping_failed?"down":"up");

		resourceDown = ping_failed;
		lastStatusChange = lastPing;

		firstPingDone = true;

		registeredJobs.Rewind();
		while ( registeredJobs.Next( job ) ) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
		}
	}

	if ( resourceDown ) {
		daemonCore->Reset_Timer( pingTimerId, probeInterval );
	}

	return 0;
}

int
GlobusResource::CheckMonitor()
{
	GlobusJob *job;
	// TODO should we require our jobs to request the grid monitor before
	//   we'll start it up?
	// TODO what if we're in the middle of a ping when we get here? Either
	//   delay until ping is done or have seperate GahpClient
	// TODO if resource is down, should we delay any actions?
	daemonCore->Reset_Timer( checkMonitorTid, TIMER_NEVER );
	dprintf(D_FULLDEBUG, "grid_monitor for %s entering CheckMonitor\n",
		resourceName);

	if ( firstPingDone == false ) {
		dprintf(D_FULLDEBUG,"grid_monitor for %s: first ping not done yet, "
			"will retry later\n", resourceName);
		daemonCore->Reset_Timer( checkMonitorTid, 5 );
		return TRUE;
	}

	if ( monitorActive == false ) {
		if ( SubmitMonitorJob() == true ) {
			// signal all jobs
			registeredJobs.Rewind();
			while ( registeredJobs.Next( job ) ) {
				job->SetEvaluateState();
			}
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
			EXCEPT("Consistency problem for GlobusResource %s, null job status file name\n", resourceName);
		}

		if(monitorLogFile == NULL) {
			EXCEPT("Consistency problem for GlobusResource %s, null monitor log file name\n", resourceName);
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
				daemonCore->Reset_Timer( checkMonitorTid, 30 );

			} else if(status == RFS_PARTIAL) {
				const int RETRY_TIME = 10;
				dprintf(D_FULLDEBUG,"*** status file is partial, "
					"will try again in %d seconds\n", RETRY_TIME);
				daemonCore->Reset_Timer( checkMonitorTid, RETRY_TIME );

			} else if(status == RFS_ERROR) {
				dprintf(D_ALWAYS,"grid_monitor: error reading job status "
					"file, stopping grid monitor\n");
				// TODO: Try to restart monitor?
				AbandonMonitor();
				return TRUE;

			} else {
				EXCEPT("ReadMonitorJobStatusFile returned unexpected %d "
					"(for %s)\n", (int)status, resourceName);
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
				logFileLastReadTime = time(NULL);
				logFileTimeoutLastReadTime = time(NULL);
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

		} else if ( time(NULL) > logFileLastReadTime + log_file_timeout ) {
			if( ! SubmitMonitorJob() ) {
				dprintf(D_ALWAYS, "Failed to restart grid_monitor.  Giving up on grid_monitor for site %s\n", resourceName);
				AbandonMonitor();
			}
			daemonCore->Reset_Timer( checkMonitorTid, 30);

		} else if ( time(NULL) > logFileTimeoutLastReadTime + monitor_retry_duration) {
			AbandonMonitor();
			dprintf(D_ALWAYS, "grid_monitor log file for %s is too old.\n",
				resourceName);
			AbandonMonitor();

		} else {
			daemonCore->Reset_Timer( checkMonitorTid, 30 );
		}
	}

	return TRUE;
}

void
GlobusResource::AbandonMonitor()
{
	dprintf(D_ALWAYS, "Giving up on grid_monitor for site %s.  "
		"Will retry in %d seconds (%d minutes)\n",
		resourceName, GM_DISABLE_LENGTH, GM_DISABLE_LENGTH / 60);
	StopMonitor();
	daemonCore->Reset_Timer( checkMonitorTid, GM_DISABLE_LENGTH);
	initialMonitorStart = true;
}

void
GlobusResource::StopMonitor()
{
	GlobusJob *job;

	dprintf(D_ALWAYS, "Stopping grid_monitor for resource %s\n", resourceName);

	monitorActive = false;
	registeredJobs.Rewind();
	while ( registeredJobs.Next( job ) ) {
		job->SetEvaluateState();
	}
	StopMonitorJob();
}

void
GlobusResource::StopMonitorJob()
{
	/* Not much to do, we just fire and forget the grid_monitor currently.
	   In the future we might want to try cancelling the job itself.
	*/
	monitorActive = false;
	CleanupMonitorJob();
}

void
GlobusResource::CleanupMonitorJob()
{
	if(monitorJobStatusFile)
	{
		unlink( monitorJobStatusFile );
		free(monitorJobStatusFile);
		monitorJobStatusFile = 0;
	}
	if(monitorLogFile)
	{
		unlink( monitorLogFile );
		free(monitorLogFile);
		monitorLogFile = 0;
	}
}

bool
GlobusResource::SubmitMonitorJob()
{
	// return true if job submitted, else return false
	GahpClient tmp_gahp;
	int now = time(NULL);
	int rc;
	char *monitor_executable;
	MyString contact;
	MyString RSL;

	StopMonitorJob();
	
	/* Create monitor file names */
	{
		g_MonitorUID++;
		MyString buff;

		buff.sprintf( "%s/grid-monitor-job-status.%s.%d.%d",
		              GridmanagerScratchDir,
		              resourceName, getpid(), g_MonitorUID );
		monitorJobStatusFile = strdup( buff.Value() );

		buff.sprintf( "%s/grid-monitor-log.%s.%d.%u", GridmanagerScratchDir,
					  resourceName, getpid(), g_MonitorUID );
		monitorLogFile = strdup( buff.Value() );
	}


	rc = unlink( monitorJobStatusFile );
	if ( rc < 0 && errno != ENOENT ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"unlink(%s) failed, errno=%d\n",
			 resourceName, monitorJobStatusFile, errno );
		return false;
	}
	rc = creat( monitorJobStatusFile, S_IREAD|S_IWRITE );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"creat(%s,%d) failed, errno=%d\n",
			 resourceName, monitorJobStatusFile, S_IREAD|S_IWRITE, errno );
		return false;
	} else {
		close( rc );
	}

	rc = unlink( monitorLogFile );
	if ( rc < 0 && errno != ENOENT ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"unlink(%s) failed, errno=%d\n",
			 resourceName, monitorLogFile, errno );
		return false;
	}
	rc = creat( monitorLogFile, S_IREAD|S_IWRITE );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"creat(%s,%d) failed, errno=%d\n",
			 resourceName, monitorLogFile, S_IREAD|S_IWRITE, errno );
		return false;
	} else {
		close( rc );
	}

	jobStatusFileLastReadTime = now;
	logFileLastReadTime = now;

	if( initialMonitorStart) {
		// Anything special to do on a cold start?
		// (It's possible for this to get called repeatedly
		// if someone wants to force a cold restart (say, after
		// AbandonMonitor())
		logFileTimeoutLastReadTime = now;
		initialMonitorStart = false;
	}

	tmp_gahp.setMode( GahpClient::normal );

	monitor_executable = param( "GRID_MONITOR" );
	if ( monitor_executable == NULL ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"GRID_MONITOR not defined!\n", resourceName );
		return false;
	}

	const char *gassServerUrl = tmp_gahp.getGlobusGassServerUrl();
	RSL.sprintf( "&(executable=%s%s)(stdout=%s%s)(arguments='--dest-url=%s%s')",
				 gassServerUrl, monitor_executable, gassServerUrl,
				 monitorLogFile, gassServerUrl, monitorJobStatusFile );

	free( monitor_executable );

	contact.sprintf( "%s/jobmanager-fork", resourceName );

	rc = tmp_gahp.globus_gram_client_job_request( contact.Value(), RSL.Value(),
												  0, NULL, NULL );

	if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
		dprintf( D_ALWAYS, "Failed to submit grid_monitor to %s: "
			"globus_gram_client_job_request() returned %d!\n",
			 resourceName, rc );
		return false;
	}

	dprintf(D_ALWAYS, "Successfully started grid_monitor for %s\n", resourceName);
	monitorActive = true;
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

	if(monitorJobStatusFile == NULL) {
		EXCEPT("Consistency problem for GlobusResource::ReadMonitorJobStatusFile %s, null job status file name\n", resourceName);
	}

	fp = fopen( monitorJobStatusFile, "r" );
	if ( fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open GridMonitor job status file %s\n",
				 monitorJobStatusFile );
		return RFS_ERROR;
	}

	if ( fgets( buff, sizeof(buff), fp ) == NULL ) {
		if( feof(fp) ) {
			dprintf( D_FULLDEBUG, "GridMonitor job status file empty (%s), treating as partial.\n",
					 monitorJobStatusFile );
			return RFS_PARTIAL;
		}
		dprintf( D_ALWAYS, "Can't read GridMonitor job status file %s\n",
				 monitorJobStatusFile );
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
		}

		if ( sscanf( buff, "%s %d", contact, &status ) == 2 &&
			 *contact != '\0' && status > 0 ) {
			int rc;
			GlobusJob *job = NULL;

			rc = JobsByContact.lookup( HashKey( globusJobId(contact) ), job );
			if ( rc == 0 && job != NULL ) {
				if ( status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					status=GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT;
				}
				dprintf(D_FULLDEBUG,"Sending callback of %d to %d.%d (%s)\n",status,job->procID.cluster,job->procID.proc, resourceName);
				job->GramCallback( status, 0 );
			}
		}
	}

	fclose( fp );

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
			EXCEPT("Consistency problem for GlobusResource::ReadMonitorLogFile %s, null monitor log file name\n", resourceName);
	}

	fp = fopen( monitorLogFile, "r" );
	if ( fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open GridMonitor log file %s\n",
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

