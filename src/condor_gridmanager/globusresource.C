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
#include "condor_config.h"
#include "string_list.h"

#include "globusresource.h"
#include "gridmanager.h"

// timer id value that indicates the timer is not registered
#define TIMER_UNSET		-1

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

#define LOG_FILE_TIMEOUT		300

template class List<GlobusJob>;
template class Item<GlobusJob>;

int GlobusResource::probeInterval = 300;	// default value
int GlobusResource::probeDelay = 15;		// default value
int GlobusResource::gahpCallTimeout = 300;	// default value
bool GlobusResource::enableGridMonitor = false;

GlobusResource::GlobusResource( const char *resource_name )
{
	resourceDown = false;
	firstPingDone = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GlobusResource::DoPing,
								"GlobusResource::DoPing", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;
	gahp.setNotificationTimerId( pingTimerId );
	gahp.setMode( GahpClient::normal );
	gahp.setTimeout( gahpCallTimeout );
	resourceName = strdup( resource_name );
	submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	myProxy = AcquireProxy( NULL, pingTimerId );

	checkMonitorTid = TIMER_UNSET;
	monitorActive = false;

	MyString buff;

	buff.sprintf( "%s/grid-monitor-job-status.%s.%d", GridmanagerScratchDir,
				  resourceName, getpid() );
	monitorJobStatusFile = strdup( buff.Value() );

	buff.sprintf( "%s/grid-monitor-log.%s.%d", GridmanagerScratchDir,
				  resourceName, getpid() );
	monitorLogFile = strdup( buff.Value() );

	Reconfig();
}

GlobusResource::~GlobusResource()
{
	ReleaseProxy( NULL, pingTimerId );
	daemonCore->Cancel_Timer( pingTimerId );
	if ( checkMonitorTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( checkMonitorTid );
	}
	if ( resourceName != NULL ) {
		free( resourceName );
	}
	if ( monitorJobStatusFile != NULL ) {
		unlink( monitorJobStatusFile );
		free( monitorJobStatusFile );
	}
	if ( monitorLogFile != NULL ) {
		unlink( monitorLogFile );
		free( monitorLogFile );
	}
}

void GlobusResource::Reconfig()
{
	char *param_value;

	gahp.setTimeout( gahpCallTimeout );

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

char *GlobusResource::ResourceName()
{
	return resourceName;
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

	if ( PROXY_IS_EXPIRED( myProxy ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		return TRUE;
	}

	rc = gahp.globus_gram_client_ping( resourceName );

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
dprintf(D_FULLDEBUG,"*** entering CheckMonitor\n");

	if ( firstPingDone == false ) {
dprintf(D_FULLDEBUG,"*** first ping not done yet, retry later\n");
		daemonCore->Reset_Timer( checkMonitorTid, 5 );
		return TRUE;
	}

	if ( monitorActive == false ) {
		if ( SubmitMonitorJob() == true ) {
			monitorActive = true;
			// signal all jobs
			registeredJobs.Rewind();
			while ( registeredJobs.Next( job ) ) {
				job->SetEvaluateState();
			}
			daemonCore->Reset_Timer( checkMonitorTid, 30 );
		} else {
dprintf(D_FULLDEBUG,"*** grid monitor disabled, stopping\n");
			daemonCore->Reset_Timer( checkMonitorTid, 60*60 );
		}
	} else {
		int rc;
		struct stat file_status;
		int job_status_mod_time;
		int log_mod_time;

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

dprintf(D_FULLDEBUG,"*** job status file has been refreshed!\n");
			if ( ReadMonitorJobStatusFile() == true ) {
dprintf(D_FULLDEBUG,"*** read status file successfully\n");
				jobStatusFileLastReadTime = time(NULL);
				daemonCore->Reset_Timer( checkMonitorTid, 30 );
			} else {
dprintf(D_FULLDEBUG,"*** error reading job status file, stopping grid monitor\n");
				StopMonitor();
				daemonCore->Reset_Timer( checkMonitorTid, 60*60 );
				return TRUE;
			}

		}

		if ( log_mod_time > logFileLastReadTime ) {

dprintf(D_FULLDEBUG,"*** log file has been refreshed!\n");
			rc = ReadMonitorLogFile();

			switch( rc ) {
			case 0:
dprintf(D_FULLDEBUG,"*** log file looks normal\n");
				logFileLastReadTime = time(NULL);
				daemonCore->Reset_Timer( checkMonitorTid, 30 );
				break;
			case 1:
dprintf(D_FULLDEBUG,"*** grid monitor reached max lifetime, restarting...\n");
				if ( SubmitMonitorJob() == true ) {
dprintf(D_FULLDEBUG,"***    success!\n");
					daemonCore->Reset_Timer( checkMonitorTid, 30 );
				} else {
dprintf(D_FULLDEBUG,"***    failure\n");
					StopMonitor();
					daemonCore->Reset_Timer( checkMonitorTid, 60*60 );
				}
				break;
			case 2:
dprintf(D_FULLDEBUG,"*** error in grid monitor, stopping\n");
				StopMonitor();
				daemonCore->Reset_Timer( checkMonitorTid, 60*60 );
				break;
			default:
				EXCEPT( "Unknown return value %d from ReadLogFile", rc );
			}

		} else if ( time(NULL) > logFileLastReadTime + LOG_FILE_TIMEOUT ) {

dprintf(D_FULLDEBUG,"*** log file too old, stopping monitor\n");
			StopMonitor();
			daemonCore->Reset_Timer( checkMonitorTid, 60*60 );

		} else {
			daemonCore->Reset_Timer( checkMonitorTid, 30 );
		}
	}

	return TRUE;
}

void
GlobusResource::StopMonitor()
{
	GlobusJob *job;

	monitorActive = false;
	registeredJobs.Rewind();
	while ( registeredJobs.Next( job ) ) {
		job->SetEvaluateState();
	}
	// try to cancel monitor job?
	unlink( monitorJobStatusFile );
	unlink( monitorLogFile );
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

	rc = unlink( monitorJobStatusFile );
	if ( rc < 0 && errno != ENOENT ) {
		dprintf( D_ALWAYS, "unlink(%s) failed, errno=%d\n",
				 monitorJobStatusFile, errno );
		return false;
	}
	rc = creat( monitorJobStatusFile, S_IRUSR|S_IWUSR );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "creat(%s,%d) failed, errno=%d\n",
				 monitorJobStatusFile, S_IRUSR|S_IWUSR, errno );
		return false;
	} else {
		close( rc );
	}

	rc = unlink( monitorLogFile );
	if ( rc < 0 && errno != ENOENT ) {
		dprintf( D_ALWAYS, "unlink(%s) failed, errno=%d\n",
				 monitorLogFile, errno );
		return false;
	}
	rc = creat( monitorLogFile, S_IRUSR|S_IWUSR );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "creat(%s,%d) failed, errno=%d\n",
				 monitorLogFile, S_IRUSR|S_IWUSR, errno );
		return false;
	} else {
		close( rc );
	}

	jobStatusFileLastReadTime = now;
	logFileLastReadTime = now;

	tmp_gahp.setMode( GahpClient::normal );

	monitor_executable = param( "GRID_MONITOR" );
	if ( monitor_executable == NULL ) {
		dprintf( D_ALWAYS, "GRID_MONITOR not defined!\n" );
		return false;
	}

	RSL.sprintf( "&(executable=%s%s)(stdout=%s%s)(arguments='--dest-url=%s%s')",
				 gassServerUrl, monitor_executable, gassServerUrl,
				 monitorLogFile, gassServerUrl, monitorJobStatusFile );

	free( monitor_executable );

	contact.sprintf( "%s/jobmanager-fork", resourceName );

	rc = tmp_gahp.globus_gram_client_job_request( contact.Value(), RSL.Value(),
												  0, NULL, NULL );

	if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
		dprintf( D_ALWAYS, "globus_gram_client_job_request() returned %d!\n",
				 rc );
		return false;
	}

	return true;
}

bool
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

	fp = fopen( monitorJobStatusFile, "r" );
	if ( fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open GridMonitor job status file %s\n",
				 monitorJobStatusFile );
		return false;
	}

	if ( fgets( buff, sizeof(buff), fp ) == NULL ) {
		dprintf( D_ALWAYS, "Can't read GridMonitor job status file %s\n",
				 monitorJobStatusFile );
		fclose( fp );
		return false;
	}

	while ( fgets( buff, sizeof(buff), fp ) != NULL ) {
		contact[0] = '\0';
		status = 0;

		if ( sscanf( buff, "%s %d", contact, &status ) == 2 &&
			 *contact != '\0' && status > 0 ) {
			int rc;
			GlobusJob *job;

			rc = JobsByContact.lookup( HashKey( globusJobId(contact) ), job );
			if ( rc == 0 & job != NULL ) {
				if ( status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					status=GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT;
				}
dprintf(D_FULLDEBUG,"*** sent callback of %d to %d.%d\n",status,job->procID.cluster,job->procID.proc);
				job->GramCallback( status, 0 );
			}
		}
	}

	fclose( fp );

	return true;
}

int
GlobusResource::ReadMonitorLogFile()
{
	// return 0 on nothing of interest, 1 on normal exit, 2 on error exit
	int retval = 0;
	FILE *fp;
	char buff[1024];

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
dprintf(D_ALWAYS,"*** log file has error code %d\n",error_code);
				retval = 2;
			}

		}
	}

	fclose( fp );

	return retval;
}

