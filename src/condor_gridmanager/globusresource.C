

#include "condor_common.h"

#include "globusresource.h"
#include "gridmanager.h"

template class List<GlobusJob>;
template class Item<GlobusJob>;


int GlobusResource::probeInterval = 300;	// default value

GlobusResource::GlobusResource( char *resource_name )
{
	resourceDown = false;
	pingTimerId = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&GlobusResource::DoPing,
								"GlobusResource::DoPing", (Service*)this );
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
}

void GlobusResource::UnregisterJob( GlobusJob *job )
{
	registeredJobs.Delete( job );
	pingRequesters.Delete( job );

	if ( registeredJobs.IsEmpty() ) {
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

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {
		ping_failed = true;
	}

	if ( ping_failed == resourceDown ) {
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
