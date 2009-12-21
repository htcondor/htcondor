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
#include "job_lease.h"

#include "baseresource.h"
#include "basejob.h"
#include "gridmanager.h"
#include "gahp-client.h"

#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		1000

int BaseResource::probeInterval = 300;	// default value
int BaseResource::probeDelay = 15;		// default value

BaseResource::BaseResource( const char *resource_name )
{
	resourceName = strdup( resource_name );
	deleteMeTid = TIMER_UNSET;

	resourceDown = false;
	firstPingDone = false;
	pingActive = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&BaseResource::Ping,
								"BaseResource::Ping", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;

	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	hasLeases = false;
	updateLeasesTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&BaseResource::UpdateLeases,
								"BaseResource::UpdateLeases", (Service*)this );
	lastUpdateLeases = 0;
	updateLeasesActive = false;
	leaseAttrsSynched = false;
	updateLeasesCmdActive = false;

	_updateCollectorTimerId = daemonCore->Register_Timer ( 
		0,
		(TimerHandlercpp)&BaseResource::UpdateCollector,
		"BaseResource::UpdateCollector",
		(Service*)this );
	_lastCollectorUpdate = 0;
	_firstCollectorUpdate = true;
	_collectorUpdateInterval = param_integer ( 
		"GRIDMANAGER_COLLECTOR_UPDATE_INTERVAL", 5*60 );

	m_batchStatusActive = false;
	m_batchPollTid = 0;
}

BaseResource::~BaseResource()
{
	if ( deleteMeTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( deleteMeTid );
	}
 	daemonCore->Cancel_Timer( pingTimerId );
	if ( updateLeasesTimerId != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( updateLeasesTimerId );
	}
	if ( _updateCollectorTimerId != TIMER_UNSET ) {
		daemonCore->Cancel_Timer ( _updateCollectorTimerId );
	}
	if ( resourceName != NULL ) {
		free( resourceName );
	}
}

void BaseResource::Reconfig()
{
	int tmp_int;
	char *param_value;
	MyString param_name;

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	setProbeInterval( tmp_int );

	jobLimit = -1;
	param_name.sprintf( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_%s",
						ResourceType() );
	param_value = param( param_name.Value() );
	if ( param_value == NULL ) {
		param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
	}
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
	// signal them
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		BaseJob *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}

	_collectorUpdateInterval = param_integer ( 
		"GRIDMANAGER_COLLECTOR_UPDATE_INTERVAL", 5*60 );

}

char *BaseResource::ResourceName()
{
	return resourceName;
}

void BaseResource::DeleteMe()
{
	deleteMeTid = TIMER_UNSET;

	if ( IsEmpty() ) {
        
        /* Tell the Collector that we're gone and self destruct */
        Invalidate ();

		delete this;
		// DO NOT REFERENCE ANY MEMBER VARIABLES BELOW HERE!!!!!!!
	}
}

bool BaseResource::Invalidate () {

    ClassAd ad;
    
    /* Set the correct types */
    ad.SetMyTypeName ( GRID_ADTYPE );

    /* We only want to invalidate this resource. Using the tuple
       (HashName,SchedName,Owner) as unique id. */
    MyString line;
    line.sprintf ( 
        "((%s =?= \"%s\") && (%s =?= \"%s\") && "
		 "(%s =?= \"%s\") && (%s =?= \"%s\"))",
        "HashName", GetHashName (),
        ATTR_SCHEDD_NAME, ScheddObj->name (),
		ATTR_SCHEDD_IP_ADDR, ScheddObj->addr (),
        ATTR_OWNER, myUserName );
    ad.Assign ( ATTR_REQUIREMENTS, line );

    dprintf (
        D_FULLDEBUG,
        "BaseResource::InvalidateResource: \n%s\n",
        line.Value() );
    
	return daemonCore->sendUpdates( INVALIDATE_GRID_ADS, &ad, NULL, true ) > 0;
}

bool BaseResource::SendUpdate () {

    ClassAd ad;

	/* Set the correct types */
    ad.SetMyTypeName ( GRID_ADTYPE );

    /* populate class ad with the relevant resource information */
    PublishResourceAd ( &ad );

	daemonCore->publish( &ad );

    MyString tmp;
    ad.sPrint ( tmp );
    dprintf (
        D_FULLDEBUG,
        "BaseResource::UpdateResource: \n%s\n",
        tmp.Value() );

	return daemonCore->sendUpdates( UPDATE_GRID_AD, &ad, NULL, true ) > 0;
}

void BaseResource::UpdateCollector () {

	/* avoid updating the collector too often, except on the 
	first update */
	if ( !_firstCollectorUpdate ) {
		int delay = ( _lastCollectorUpdate + 
			_collectorUpdateInterval ) - time ( NULL );
		if ( delay > 0 ) {
			daemonCore->Reset_Timer ( _updateCollectorTimerId, delay );
			return;
		}
	} else {
		_firstCollectorUpdate = false;
	}

	/* Update the the Collector as to this resource's state */
    if ( !SendUpdate () && !daemonCore->getCollectorList()->IsEmpty() ) {
		dprintf (
			D_FULLDEBUG,
			"BaseResource::UpdateCollector: Updating Collector(s) "
			"failed.\n" );
	}

	/* reset the timer to fire again at the defined interval */
	daemonCore->Reset_Timer ( 
		_updateCollectorTimerId, 
		_collectorUpdateInterval );
	_lastCollectorUpdate = time ( NULL );
}

void BaseResource::PublishResourceAd( ClassAd *resource_ad )
{
	MyString buff;

	buff.sprintf( "%s %s", ResourceType(), resourceName );
	resource_ad->Assign( ATTR_NAME, buff.Value() );
	resource_ad->Assign( "HashName", GetHashName() );
	resource_ad->Assign( ATTR_SCHEDD_NAME, ScheddName );
    resource_ad->Assign( ATTR_SCHEDD_IP_ADDR, ScheddObj->addr() );
	resource_ad->Assign( ATTR_OWNER, myUserName );
	resource_ad->Assign( "NumJobs", registeredJobs.Number() );
	resource_ad->Assign( "JobLimit", jobLimit );
	resource_ad->Assign( "SubmitsAllowed", submitsAllowed.Number() );
	resource_ad->Assign( "SubmitsWanted", submitsWanted.Number() );
	if ( resourceDown ) {
		resource_ad->Assign( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME,
							 (int)lastStatusChange );
	}

	int num_idle = 0;
	int num_running = 0;
	BaseJob *job;
	registeredJobs.Rewind();
	while ( registeredJobs.Next( job ) ) {
		switch ( job->condorState ) {
		case IDLE:
			num_idle++;
			break;
		case RUNNING:
			num_running++;
			break;
		default:
			break;
		}
	}

	resource_ad->Assign( ATTR_RUNNING_JOBS, num_running );
	resource_ad->Assign( ATTR_IDLE_JOBS, num_idle );
}

void BaseResource::RegisterJob( BaseJob *job )
{
	registeredJobs.Append( job );

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}

	if ( deleteMeTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( deleteMeTid );
		deleteMeTid = TIMER_UNSET;
	}
}

void BaseResource::UnregisterJob( BaseJob *job )
{
	CancelSubmit( job );

	pingRequesters.Delete( job );
	registeredJobs.Delete( job );
	leaseUpdates.Delete( job );

	if ( IsEmpty() ) {
		int delay = param_integer( "GRIDMANAGER_EMPTY_RESOURCE_DELAY", 5*60 );
		if ( delay < 0 ) {
			delay = 0;
		}
		deleteMeTid = daemonCore->Register_Timer( delay,
								(TimerHandlercpp)&BaseResource::DeleteMe,
								"BaseResource::DeleteMe", (Service*)this );
	}
}

bool BaseResource::IsEmpty()
{
	return registeredJobs.IsEmpty();
}

void BaseResource::RequestPing( BaseJob *job )
{
		// A child resource object may request a ping in response to a
		// failed collective job status command. They will pass a NULL
		// for the job pointer.
	if ( job ) {
		pingRequesters.Append( job );
	}

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool BaseResource::RequestSubmit( BaseJob *job )
{
	BaseJob *jobptr;

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsAllowed.Rewind();
	while ( submitsAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	if ( submitsAllowed.Length() < jobLimit &&
		 submitsWanted.Length() > 0 ) {
		EXCEPT("In BaseResource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full\n",resourceName);
	}
	if ( submitsAllowed.Length() < jobLimit ) {
		submitsAllowed.Append( job );
		return true;
	} else {
		submitsWanted.Append( job );
		return false;
	}
}

void BaseResource::CancelSubmit( BaseJob *job )
{
	if ( submitsAllowed.Delete( job ) ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			BaseJob *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		submitsWanted.Delete( job );
	}

	leaseUpdates.Delete( job );

	return;
}

void BaseResource::AlreadySubmitted( BaseJob *job )
{
	submitsAllowed.Append( job );
}

void BaseResource::Ping()
{
	BaseJob *job;

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
		return;
	}

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	time_t ping_delay;
	bool ping_complete;
	bool ping_succeeded;
	DoPing( ping_delay, ping_complete, ping_succeeded );

	if ( ping_delay ) {
		daemonCore->Reset_Timer( pingTimerId, ping_delay );
		return;
	}

	if ( !ping_complete ) {
		pingActive = true;
		return;
	}

	pingActive = false;
	lastPing = time(NULL);

	if ( ping_succeeded != resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
				ping_succeeded?"up":"down");

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
				ping_succeeded?"up":"down");

		resourceDown = !ping_succeeded;
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
}

void BaseResource::DoPing( time_t& ping_delay, bool& ping_complete,
						   bool& ping_succeeded )
{
	ping_delay = 0;
	ping_complete = true;
	ping_succeeded = true;
}

void BaseResource::UpdateLeases()
{
dprintf(D_FULLDEBUG,"*** UpdateLeases called\n");
	if ( hasLeases == false ) {
dprintf(D_FULLDEBUG,"    Leases not supported, cancelling timer\n" );
		daemonCore->Cancel_Timer( updateLeasesTimerId );
		updateLeasesTimerId = TIMER_UNSET;
		return;
	}

	// Don't start a new lease update too soon after the previous one.
	int delay;
	delay = (lastUpdateLeases + 30) - time(NULL);
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, delay );
dprintf(D_FULLDEBUG,"    UpdateLeases: last update too recent, delaying\n");
		return;
	}

	daemonCore->Reset_Timer( updateLeasesTimerId, TIMER_NEVER );

    if ( updateLeasesActive == false ) {
		BaseJob *curr_job;
dprintf(D_FULLDEBUG,"    UpdateLeases: calc'ing new leases\n");
		registeredJobs.Rewind();
		while ( registeredJobs.Next( curr_job ) ) {
			int new_expire;
			MyString  job_id;
				// Don't update the lease for a job that isn't submitted
				// anywhere. The Job object will start the lease when it
				// submits the job.
			if ( curr_job->jobAd->LookupString( ATTR_GRID_JOB_ID, job_id ) &&
				 CalculateJobLease( curr_job->jobAd, new_expire ) ) {

				curr_job->UpdateJobLeaseSent( new_expire );
				leaseUpdates.Append( curr_job );
			}
		}
		if ( leaseUpdates.IsEmpty() ) {
			lastUpdateLeases = time(NULL);
			daemonCore->Reset_Timer( updateLeasesTimerId, 30 );
		} else {
			requestScheddUpdateNotification( updateLeasesTimerId );
			updateLeasesActive = true;
			leaseAttrsSynched = false;
		}
		return;
	}

	if ( leaseAttrsSynched == false ) {
		bool still_dirty = false;
		BaseJob *curr_job;
		leaseUpdates.Rewind();
		while ( leaseUpdates.Next( curr_job ) ) {
			bool exists, dirty;
			curr_job->jobAd->GetDirtyFlag( ATTR_JOB_LEASE_EXPIRATION,
										   &exists, &dirty );
			if ( !exists ) {
					// What!? The attribute disappeared? Forget about renewing
					// the lease then
				dprintf( D_ALWAYS, "Lease attribute disappeared for job %d.%d, ignoring it\n",
						 curr_job->procID.cluster, curr_job->procID.proc );
				leaseUpdates.DeleteCurrent();
			}
			if ( dirty ) {
				still_dirty = true;
				requestScheddUpdate( curr_job, false );
			}
		}
		if ( still_dirty ) {
			requestScheddUpdateNotification( updateLeasesTimerId );
dprintf(D_FULLDEBUG,"    UpdateLeases: waiting for schedd synch\n");
			return;
		}
else dprintf(D_FULLDEBUG,"    UpdateLeases: leases synched\n");
	}

	leaseAttrsSynched = true;

	time_t update_delay;
	bool update_complete;
	SimpleList<PROC_ID> update_succeeded;
dprintf(D_FULLDEBUG,"    UpdateLeases: calling DoUpdateLeases\n");
	DoUpdateLeases( update_delay, update_complete, update_succeeded );

	if ( update_delay ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, update_delay );
dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases wants delay\n");
		return;
	}

	if ( !update_complete ) {
		updateLeasesCmdActive = true;
dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases in progress\n");
		return;
	}

dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases complete, processing results\n");
	updateLeasesCmdActive = false;
	lastUpdateLeases = time(NULL);

update_succeeded.Rewind();
PROC_ID id;
MyString msg = "    update_succeeded:";
while(update_succeeded.Next(id)) msg.sprintf_cat(" %d.%d", id.cluster, id.proc);
dprintf(D_FULLDEBUG,"%s\n",msg.Value());
	BaseJob *curr_job;
	leaseUpdates.Rewind();
	while ( leaseUpdates.Next( curr_job ) ) {
		bool curr_renewal_failed;
		bool last_renewal_failed = false;
		if ( update_succeeded.IsMember( curr_job->procID ) ) {
dprintf(D_FULLDEBUG,"    %d.%d is in succeeded list\n",curr_job->procID.cluster,curr_job->procID.proc);
			curr_renewal_failed = false;
		} else {
dprintf(D_FULLDEBUG,"    %d.%d is not in succeeded list\n",curr_job->procID.cluster,curr_job->procID.proc);
			curr_renewal_failed = true;
		}
		curr_job->jobAd->LookupBool( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
									 last_renewal_failed );
		if ( curr_renewal_failed != last_renewal_failed ) {
			curr_job->jobAd->Assign( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
									 curr_renewal_failed );
			requestScheddUpdate( curr_job, false );
		}
		leaseUpdates.DeleteCurrent();
	}

	updateLeasesActive = false;

	daemonCore->Reset_Timer( updateLeasesTimerId, 30 );
}

void BaseResource::DoUpdateLeases( time_t& update_delay,
								   bool& update_complete,
								   SimpleList<PROC_ID>& /* update_succeeded */ )
{
dprintf(D_FULLDEBUG,"*** BaseResource::DoUpdateLeases called\n");
	update_delay = 0;
	update_complete = true;
}

void BaseResource::StartBatchStatusTimer()
{
	if(m_batchPollTid) {
		EXCEPT("BaseResource::StartBatchStatusTimer called more than once!");
	}
	dprintf(D_FULLDEBUG, "Grid type for %s will use batch status requests (DoBatchStatus).\n", ResourceName());
	m_batchPollTid = daemonCore->Register_Timer( 0,
		(TimerHandlercpp)&BaseResource::DoBatchStatus,
		"BaseResource::DoBatchStatus", (Service*)this );
}

int BaseResource::BatchStatusInterval() const
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::BatchStatusInterval() called.");
	return 0; // Required by Visual C++ compiler
}

BaseResource::BatchStatusResult BaseResource::StartBatchStatus()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::BatchStatusInterval() called.");
	return BSR_ERROR; // Required by Visual C++ compiler
}

BaseResource::BatchStatusResult BaseResource::FinishBatchStatus()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::BatchStatusInterval() called.");
	return BSR_ERROR; // Required by Visual C++ compiler
}

GahpClient * BaseResource::BatchGahp()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::BatchStatusInterval() called.");
	return 0;
}

int BaseResource::DoBatchStatus()
{
	dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus for %s.\n", ResourceName());

	if ( ( registeredJobs.IsEmpty() || resourceDown ) &&
		 m_batchStatusActive == false ) {
			// No jobs or we can't talk to the schedd, so no point
			// in polling
		daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
		dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus for %s skipped for %d seconds because %s.\n", ResourceName(), BatchStatusInterval(), resourceDown ? "the resource is down":"there are no jobs registered");
		return 0;
	}

	GahpClient * gahp = BatchGahp();
	if(gahp && (gahp->isStarted() == false) ) {
		dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus for %s is trying to start the GAHP.\n", ResourceName());
		if ( gahp->Startup() == false ) {
				// Failed to start the gahp server. Don't do anything
				// about it. The job objects will also fail on this call
				// and should go on hold as a result.
			daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
			dprintf(D_ALWAYS, "BaseResource::DoBatchStatus for %s failed to start the GAHP.\n", ResourceName());
			return 0;
		}
	}

	if ( gahp->isInitialized() == false ) {
		int GAHP_INIT_DELAY = 5;
		dprintf( D_ALWAYS,"BaseResource::DoBatchStatus: gahp server not up yet, delaying %d seconds\n", GAHP_INIT_DELAY );
		daemonCore->Reset_Timer( m_batchPollTid, GAHP_INIT_DELAY );
		return 0;
	}

	daemonCore->Reset_Timer( m_batchPollTid, TIMER_NEVER );

	if(m_batchStatusActive == false) {
		dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Starting bulk job poll of %s\n", ResourceName());
		BatchStatusResult bsr = StartBatchStatus();
		switch(bsr) {
			case BSR_DONE:
				dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Finished bulk job poll of %s\n", ResourceName());
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return 0;

			case BSR_ERROR:
				dprintf(D_ALWAYS, "BaseResource::DoBatchStatus: An error occurred trying to start a bulk poll of %s\n", ResourceName());
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return 0;

			case BSR_PENDING:
				m_batchStatusActive = true;
				return 0;

			default:
				EXCEPT("BaseResource::DoBatchStatus: Unknown BatchStatusResult %d\n", (int)bsr);
		}

	} else {
		BatchStatusResult bsr = FinishBatchStatus();
		switch(bsr) {
			case BSR_DONE:
				dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Finished bulk job poll of %s\n", ResourceName());
				m_batchStatusActive = false;
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return 0;

			case BSR_ERROR:
				dprintf(D_ALWAYS, "BaseResource::DoBatchStatus: An error occurred trying to finish a bulk poll of %s\n", ResourceName());
				m_batchStatusActive = false;
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return 0;

			case BSR_PENDING:
				return 0;

			default:
				EXCEPT("BaseResource::DoBatchStatus: Unknown BatchStatusResult %d\n", (int)bsr);
		}
	}
	return 0;
}
