

#include "condor_common.h"

#include "globusresource.h"


// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1

template class List<GlobusJob>;
template class Item<GlobusJob>;


GlobusResource::GlobusResource( char *resource_name )
{
	resourceDown = false;
	pingTimerId = TIMER_UNSET;
	resourceName = strdup( resource_name );
}

GlobusResource::~GlobusResource()
{
	if ( pingTimerId != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( pingTimerId );
	}
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
	if ( pingTimerId != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( pingTimerId );
	}
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GlobusResouce::DoPing,
								"GlobusResource::DoPing", (Service*)this );

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

	pingTimerId = TIMER_UNSET;

	rc = globus_gram_client_ping( resourceName )

	if ( rc == GLOBUS_CALL_PENDING ) {
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
		// TODO: We should param for the ping interval
		pingTimerId = daemonCode->Register_Timer( 300,
								(TimerHandlercpp)&GlobusResouce::DoPing,
								"GlobusResource::DoPing", (Service*)this );
	}

	return 0;
}
