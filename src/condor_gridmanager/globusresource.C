

#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "globusresource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

int GlobusResource::probeInterval = 300;	// default value
int GlobusResource::probeDelay = 15;		// default value
int GlobusResource::gahpCallTimeout = 300;	// default value

GlobusResource::GlobusResource( char *resource_name )
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

	Reconfig();
}

GlobusResource::~GlobusResource()
{
	daemonCore->Cancel_Timer( pingTimerId );
	if ( resourceName != NULL ) {
		free( resourceName );
	}
}

void GlobusResource::Reconfig()
{
	char *param_value;

	gahp.setTimeout( gahpCallTimeout );

//	submitLimit = param_integer( "GRIDMANAGER_MAX_PENDING_SUBMITS", 5 );
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

//	jobLimit = param_integer("GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE",
//							 100 );
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

dprintf(D_FULLDEBUG,"*** enterting Reconfig\n");
	// If the jobLimit was widened, move jobs from Wanted to Allowed and
	// add them to Queued
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		GlobusJob *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
//		submitsQueued.Append( wanted_job );
dprintf(D_FULLDEBUG,"***   job %d.%d moved from submitsWanted to submitsAllowed\n",wanted_job->procID.cluster,wanted_job->procID.proc);
//dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsQueued\n",wanted_job->procID.cluster,wanted_job->procID.proc);
		wanted_job->SetEvaluateState();
	}

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		GlobusJob *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
dprintf(D_FULLDEBUG,"***   job %d.%d moved from submitsQueued to submitsInProgress\n",queued_job->procID.cluster,queued_job->procID.proc);
		queued_job->SetEvaluateState();
	}
}

void GlobusResource::RegisterJob( GlobusJob *job, bool already_submitted )
{
	registeredJobs.Append( job );

	if ( already_submitted ) {
dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsAllowed, returning false\n",job->procID.cluster,job->procID.proc);
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

	int delay = (lastPing + probeDelay) - time(NULL);
	if ( delay < 0 ) {
		delay = 0;
	}
	daemonCore->Reset_Timer( pingTimerId, delay );
}

bool GlobusResource::RequestSubmit( GlobusJob *job )
{
	bool already_allowed = false;
	GlobusJob *jobptr;
dprintf(D_FULLDEBUG,"*** enterting RequestSubmit() for job %d.%d\n",job->procID.cluster,job->procID.proc);

	submitsQueued.Rewind();
	while ( submitsQueued.Next( jobptr ) ) {
		if ( jobptr == job ) {
dprintf(D_FULLDEBUG,"***   job %d.%d already in submitsQueued, returning false\n",job->procID.cluster,job->procID.proc);
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
dprintf(D_FULLDEBUG,"***   job %d.%d already in submitsInProgress, returning true\n",job->procID.cluster,job->procID.proc);
			return true;
		}
	}

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
dprintf(D_FULLDEBUG,"***   job %d.%d already in submitsWanted, returning false\n",job->procID.cluster,job->procID.proc);
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
dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsAllowed\n",job->procID.cluster,job->procID.proc);
			// proceed to see if submitLimit applies
		} else {
			submitsWanted.Append( job );
dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsWanted, returning false\n",job->procID.cluster,job->procID.proc);
			return false;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In GlobusResource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n",resourceName);
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsInProgress, returning true\n",job->procID.cluster,job->procID.proc);
		return true;
	} else {
		submitsQueued.Append( job );
dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsQueued, returning false\n",job->procID.cluster,job->procID.proc);
		return false;
	}
}

void GlobusResource::SubmitComplete( GlobusJob *job )
{
dprintf(D_FULLDEBUG,"*** enterting SubmitComplete() for job %d.%d\n",job->procID.cluster,job->procID.proc);
	if ( submitsInProgress.Delete( job ) ) {
dprintf(D_FULLDEBUG,"***   job %d.%d removed from submitsInProgress\n",job->procID.cluster,job->procID.proc);
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			GlobusJob *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
dprintf(D_FULLDEBUG,"***   job %d.%d moved from submitsQueued to submitsInProgress\n",queued_job->procID.cluster,queued_job->procID.proc);
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsQueued if the job wasn't in
		// submitsInProgress.
bool foo=
		submitsQueued.Delete( job );
if(foo)dprintf(D_FULLDEBUG,"***   job %d.%d removed from submitsQueued\n",job->procID.cluster,job->procID.proc);
	}

	return;
}

void GlobusResource::CancelSubmit( GlobusJob *job )
{
dprintf(D_FULLDEBUG,"*** enterting CancelSubmit() for job %d.%d\n",job->procID.cluster,job->procID.proc);
	if ( submitsAllowed.Delete( job ) ) {
dprintf(D_FULLDEBUG,"***   job %d.%d removed from submitsAllowed\n",job->procID.cluster,job->procID.proc);
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			GlobusJob *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
//			submitsQueued.Append( wanted_job );
dprintf(D_FULLDEBUG,"***   job %d.%d moved from submitsWanted to submitsAllowed\n",wanted_job->procID.cluster,wanted_job->procID.proc);
//dprintf(D_FULLDEBUG,"***   job %d.%d appended to submitsQueued\n",wanted_job->procID.cluster,wanted_job->procID.proc);
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
bool foo=
		submitsWanted.Delete( job );
if(foo)dprintf(D_FULLDEBUG,"***   job %d.%d removed from submitsWanted\n",job->procID.cluster,job->procID.proc);
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

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	rc = gahp.globus_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return 0;
	}

	lastPing = time(NULL);

	if ( rc != GLOBUS_SUCCESS ) {
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
