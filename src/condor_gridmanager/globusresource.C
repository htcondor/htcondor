

#include "condor_common.h"

#include "globusresource.h"
#include "gridmanager.h"


int GlobusResource::probeInterval = 300;	// default value
int GlobusResource::probeDelay = 15;		// default value
int GlobusResource::submitLimit = 5;		// default value
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
	gahp.setTimeout( gahpCallTimeout );

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		GlobusJob *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
		queued_job->SetEvaluateState();
	}
}

void GlobusResource::RegisterJob( GlobusJob *job )
{
	registeredJobs.Append( job );

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

bool GlobusResource::CancelSubmit( GlobusJob *job )
{
	if ( submitsQueued.Delete( job ) ) {
		return true;
	}

	if ( submitsInProgress.Delete( job ) ) {
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			GlobusJob *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
		}
		return true;
	}

	return false;
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
