

#include "condor_common.h"

#include "globusresource.h"


// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1
#define TIME_NEVER		10000000

template class List<GlobusJob>;
template class Item<GlobusJob>;


GlobusResource::probeInterval = 300;	// default value

GlobusResource::GlobusResource( char *resource_name )
{
	resourceDown = false;
	pingTimerId = daemonCore->Register_Timer( TIME_NEVER,
								(TimerHandlercpp)&GlobusResouce::DoPing,
								"GlobusResource::DoPing", (Service*)this );
	gahp.resetTimerOnResults( pingTimerId );
	gahp.setMode( normal );
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
}

void GlobusResource::UnregisterJob( GlobusJob *job )
{
	registeredJobs.Delete( job );
	pingRequesters.Delete( job );

	if ( registerdJobs.IsEmpty() ) {
		DeleteResource( this );
	}
}

void GlobusResource::RequestPing( GlobusJob *job )
{
	pingRequesters.Append( job );
	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool GlobusResource::IsEmpty()
{
	return registerdJobs.IsEmpty();
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

	daemonCore->Reset_Timer( pingTimerId, TIME_NEVER );

	rc = gahp.globus_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return 0;
	}

	if ( rc == GLOBUS_GRAM_CLIENT_ERROR_CONNECTION_FAILED ) {
		ping_failed = true;
	}

	if ( ping_failed == resourceDown ) {
		// State of resource hasn't changed. Notify ping requesters only.
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
		resourceDown = ping_failed;

		registeredJobs.Rewind();
		while ( registeredJobs.Next( job ) ) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}

		pingRequesters.Rewind();
		while ( pingRequesters.DeleteCurrent() ) {
			pingRequesters.DeleteCurrent();
		}
	}

	if ( resourceDown ) {
		daemonCore->Reset_Timer( pingTimerId, probeInterval );
	}

	return 0;
}
