

#include "condor_common.h"

#include "globusresource.h"
#include "gridmanager.h"


int GlobusResource::probeInterval = 300;	// default value
int GlobusResource::probeDelay = 15;		// default value
int GlobusResource::submitLimit = 5;		// default value

GlobusResource::GlobusResource( char *resource_name )
{
	resourceDown = false;
	firstPingDone = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GlobusResource::DoPing,
								"GlobusResource::DoPing", (Service*)this );
	lastPing = 0;
	gahp.setNotificationTimerId( pingTimerId );
	gahp.setMode( GahpClient::normal );
	resourceName = strdup( resource_name );
}

GlobusResource::~GlobusResource()
{
	daemonCore->Cancel_Timer( pingTimerId );
	if ( resourceName != NULL ) {
		free( resourceName );
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
dprintf(D_FULLDEBUG,"*** Job %d.%d already in submitsQueued\n",job->procID.cluster,job->procID.proc);
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
dprintf(D_FULLDEBUG,"*** Job %d.%d already in submitsInProgress\n",job->procID.cluster,job->procID.proc);
			return true;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In GlobusResource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n");
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
dprintf(D_FULLDEBUG,"*** Job %d.%d appended to submitsInProgress\n",job->procID.cluster,job->procID.proc);
		return true;
	} else {
		submitsQueued.Append( job );
dprintf(D_FULLDEBUG,"*** Job %d.%d appended to submitsQueued\n",job->procID.cluster,job->procID.proc);
		return false;
	}
}

bool GlobusResource::CancelSubmit( GlobusJob *job )
{
	if ( submitsQueued.Delete( job ) ) {
dprintf(D_FULLDEBUG,"*** Job %d.%d removed from submitsQueued\n",job->procID.cluster,job->procID.proc);
		return true;
	}

	if ( submitsInProgress.Delete( job ) ) {
dprintf(D_FULLDEBUG,"*** Job %d.%d removed from submitsInProgress\n",job->procID.cluster,job->procID.proc);
		if ( submitsQueued.Length() > 0 ) {
			GlobusJob *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
dprintf(D_FULLDEBUG,"*** Job %d.%d moved to submitsInProgress\n",queued_job->procID.cluster,queued_job->procID.proc);
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

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {
		ping_failed = true;
	}

	if ( ping_failed == resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_FULLDEBUG,"resource %s is still %s\n",resourceName,
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
		dprintf(D_FULLDEBUG,"resource %s is now %s\n",resourceName,
				ping_failed?"down":"up");

		resourceDown = ping_failed;

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
