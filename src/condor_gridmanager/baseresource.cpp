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

#include "baseresource.h"
#include "basejob.h"
#include "gridmanager.h"
#include "gahp-client.h"

#include <algorithm>
#include <limits>

#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		1000
#define DEFAULT_JOB_POLL_RATE 5
#define DEFAULT_JOB_POLL_INTERVAL 60

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
	m_pingErrCode = GRU_PING_FAILED;

	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	hasLeases = false;
	updateLeasesTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&BaseResource::UpdateLeases,
								"BaseResource::UpdateLeases", (Service*)this );
	lastUpdateLeases = 0;
	updateLeasesActive = false;
	updateLeasesCmdActive = false;
	m_hasSharedLeases = false;
	m_defaultLeaseDuration = -1;
	m_sharedLeaseExpiration = 0;

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
	m_batchPollTid = TIMER_UNSET;

	m_paramJobPollRate = -1;
	m_paramJobPollInterval = -1;
	m_jobPollInterval = 0;
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
	if ( m_batchPollTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer ( m_batchPollTid );
	}
	if ( resourceName != NULL ) {
		free( resourceName );
	}
}

void BaseResource::Reconfig()
{
	int tmp_int;
	char *param_value;
	std::string param_name;

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	setProbeInterval( tmp_int );

	tmp_int = -1;
	formatstr( param_name, "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_%s",
						ResourceType() );
	param_value = param( param_name.c_str() );
	if ( param_value == NULL ) {
		param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
	}
	if ( param_value != NULL ) {
		const char* tmp;
		StringTokenIterator limits(param_value);
		if ( (tmp = limits.next()) ) {
			tmp_int = atoi(tmp);
			while ( (tmp = limits.next()) ) {
				if (strstr(resourceName, tmp) != 0 && (tmp = limits.next())) {
					tmp_int = atoi(tmp);
				}
			}
		}
		free( param_value );
	}
	if ( tmp_int <= 0 ) {
		jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;
	} else {
		jobLimit = tmp_int;
	}

	// If the jobLimit was widened, move jobs from Wanted to Allowed and
	// signal them
	while ( submitsAllowed.size() < jobLimit &&
	        submitsWanted.size() > 0 ) {
		BaseJob *wanted_job = submitsWanted.front();
		submitsWanted.pop_front();
		submitsAllowed.insert(wanted_job);
		wanted_job->SetEvaluateState();
	}

	formatstr( param_name, "GRIDMANAGER_JOB_PROBE_RATE_%s", ResourceType() );
	m_paramJobPollRate = param_integer( param_name.c_str(), -1 );
	if ( m_paramJobPollRate <= 0 ) {
		m_paramJobPollRate = param_integer( "GRIDMANAGER_JOB_PROBE_RATE",
											DEFAULT_JOB_POLL_RATE );
	}
	if ( m_paramJobPollRate <= 0 ) {
		m_paramJobPollRate = DEFAULT_JOB_POLL_RATE;
	}

	const char *legacy_job_poll_param = NULL;
	const char *type = ResourceType();
	if ( strcmp( type, "condor" ) == 0 ) {
		legacy_job_poll_param = "CONDOR_JOB_POLL_INTERVAL";
	} else if ( strcmp( type, "batch" ) == 0 ||
				strcmp( type, "pbs" ) == 0 ||
				strcmp( type, "lsf" ) == 0 ||
				strcmp( type, "nqs" ) == 0 ||
				strcmp( type, "sge" ) == 0 ||
				strcmp( type, "naregi" ) == 0 ) {
		legacy_job_poll_param = "INFN_JOB_POLL_INTERVAL";
	}

	formatstr( param_name, "GRIDMANAGER_JOB_PROBE_INTERVAL_%s", ResourceType() );
	m_paramJobPollInterval = param_integer( param_name.c_str(), -1 );
	if ( m_paramJobPollInterval <= 0 ) {
		m_paramJobPollInterval = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", -1 );
	}
	if ( m_paramJobPollInterval <= 0 && legacy_job_poll_param ) {
		m_paramJobPollInterval = param_integer( legacy_job_poll_param, -1 );
	}
	if ( m_paramJobPollInterval <= 0 ) {
		m_paramJobPollInterval = DEFAULT_JOB_POLL_INTERVAL;
	}

	SetJobPollInterval();

	_collectorUpdateInterval = param_integer ( 
		"GRIDMANAGER_COLLECTOR_UPDATE_INTERVAL", 5*60 );

}

char *BaseResource::ResourceName()
{
	return resourceName;
}

void BaseResource::DeleteMe( int /* timerID */ )
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
    SetMyTypeName ( ad, GRID_ADTYPE );

    /* We only want to invalidate this resource. Using the tuple
       (HashName,SchedName,Owner) as unique id. */
	std::string line;
	formatstr( line,
        "((TARGET.%s =?= \"%s\") && (TARGET.%s =?= \"%s\") && "
		 "(TARGET.%s =?= \"%s\") && (TARGET.%s =?= \"%s\"))",
        ATTR_HASH_NAME, GetHashName (),
        ATTR_SCHEDD_NAME, ScheddObj->name (),
		ATTR_SCHEDD_IP_ADDR, ScheddObj->addr (),
		ATTR_OWNER, myUserName );
    ad.AssignExpr ( ATTR_REQUIREMENTS, line.c_str() );

	ad.Assign( ATTR_HASH_NAME, GetHashName() );
	ad.Assign( ATTR_SCHEDD_NAME, ScheddObj->name() );
	ad.Assign( ATTR_SCHEDD_IP_ADDR, ScheddObj->addr() );
	ad.Assign( ATTR_OWNER, myUserName );

    dprintf (
        D_FULLDEBUG,
        "BaseResource::InvalidateResource: \n%s\n",
        line.c_str() );
    
	return daemonCore->sendUpdates( INVALIDATE_GRID_ADS, &ad, NULL, true ) > 0;
}

bool BaseResource::SendUpdate () {

    ClassAd ad;

	/* Set the correct types */
    SetMyTypeName ( ad, GRID_ADTYPE );

    /* populate class ad with the relevant resource information */
    PublishResourceAd ( &ad );

	daemonCore->publish( &ad );

	std::string tmp;
    sPrintAd ( tmp, ad );
    dprintf (
        D_FULLDEBUG,
        "BaseResource::UpdateResource: \n%s\n",
        tmp.c_str() );

	return daemonCore->sendUpdates( UPDATE_GRID_AD, &ad, NULL, true ) > 0;
}

void BaseResource::UpdateCollector( int /* timerID */ ) {

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
    if ( !SendUpdate () && !daemonCore->getCollectorList()->getList().empty() ) {
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
	std::string buff;

	formatstr( buff, "%s %s", ResourceType(), resourceName );
	resource_ad->Assign( ATTR_NAME, buff.c_str() );
	resource_ad->Assign( "HashName", GetHashName() );
	resource_ad->Assign( ATTR_SCHEDD_NAME, ScheddName );
    resource_ad->Assign( ATTR_SCHEDD_IP_ADDR, ScheddObj->addr() );
	resource_ad->Assign( ATTR_OWNER, myUserName );
	if ( SelectionValue ) {
		resource_ad->Assign( ATTR_GRIDMANAGER_SELECTION_VALUE, SelectionValue );
	}
	resource_ad->Assign( "NumJobs", registeredJobs.size() );
	resource_ad->Assign( "JobLimit", jobLimit );
	resource_ad->Assign( "SubmitsAllowed", submitsAllowed.size() );
	resource_ad->Assign( "SubmitsWanted", submitsWanted.size() );
	if ( resourceDown ) {
		resource_ad->Assign( ATTR_GRID_RESOURCE_UNAVAILABLE_TIME,
							 lastStatusChange );
		if (!m_pingErrMsg.empty()) {
			resource_ad->Assign(ATTR_GRID_RESOURCE_UNAVAILABLE_REASON,
			                    m_pingErrMsg);
		}
		resource_ad->Assign(ATTR_GRID_RESOURCE_UNAVAILABLE_REASON_CODE,
		                    m_pingErrCode);
	}

	int num_idle = 0;
	int num_running = 0;
	for (auto job: registeredJobs) {
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
	registeredJobs.insert(job);

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}

	int lease_expiration = -1;
	job->jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, lease_expiration );
	if ( lease_expiration > 0 ) {
		RequestUpdateLeases();
	}

	if ( deleteMeTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( deleteMeTid );
		deleteMeTid = TIMER_UNSET;
	}
}

void BaseResource::UnregisterJob( BaseJob *job )
{
	CancelSubmit( job );

	pingRequesters.erase(job);
	registeredJobs.erase(job);

	std::erase(leaseUpdates, job);

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
	return registeredJobs.empty();
}

void BaseResource::SetJobPollInterval()
{
	m_jobPollInterval = submitsAllowed.size() / m_paramJobPollRate;
	if ( m_jobPollInterval < m_paramJobPollInterval ) {
		m_jobPollInterval = m_paramJobPollInterval;
	}
}

void BaseResource::RequestPing( BaseJob *job )
{
		// A child resource object may request a ping in response to a
		// failed collective job status command. They will pass a NULL
		// for the job pointer.
	if ( job ) {
		pingRequesters.insert(job);
	}

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool BaseResource::RequestSubmit( BaseJob *job )
{
	for (auto jobptr: submitsWanted) {
		if ( jobptr == job ) {
			return false;
		}
	}

	if (submitsAllowed.count(job) > 0) {
		return true;
	}

	if ( submitsAllowed.size() < jobLimit &&
		 submitsWanted.size() > 0 ) {
		EXCEPT("In BaseResource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full",resourceName);
	}
	if ( submitsAllowed.size() < jobLimit ) {
		submitsAllowed.insert(job);
		SetJobPollInterval();
		return true;
	} else {
		submitsWanted.push_back(job);
		return false;
	}
}

void BaseResource::CancelSubmit( BaseJob *job )
{
	if (submitsAllowed.erase(job)) {
		if ( submitsAllowed.size() < jobLimit &&
		     submitsWanted.size() > 0 ) {
			BaseJob *wanted_job = submitsWanted.front();
			submitsWanted.pop_front();
			submitsAllowed.insert(wanted_job);
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		auto itr = submitsWanted.begin();
		while (itr != submitsWanted.end()) {
			if (*itr == job) {
				itr = submitsWanted.erase(itr);
			} else {
				itr++;
			}
		}
	}

	std::erase(leaseUpdates, job);

	SetJobPollInterval();

	return;
}

void BaseResource::AlreadySubmitted( BaseJob *job )
{
	submitsAllowed.insert(job);
	SetJobPollInterval();
}

void BaseResource::Ping( int /* timerID */ )
{
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

	unsigned ping_delay;
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
	if (ping_succeeded) {
		m_pingErrMsg.clear();
	}

	if ( ping_succeeded != resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
				ping_succeeded?"up":"down");

		for (auto job: pingRequesters) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}
		pingRequesters.clear();
	} else {
		// State of resource has changed. Notify every job.
		dprintf(D_ALWAYS,"resource %s is now %s\n",resourceName,
				ping_succeeded?"up":"down");

		resourceDown = !ping_succeeded;
		lastStatusChange = lastPing;

		firstPingDone = true;

		for (auto job: registeredJobs) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}

		pingRequesters.clear();
		// TODO trigger ad update here
		//   UpdateCollector() doesn't allow more frequent updates currently
	}

	if ( resourceDown ) {
		daemonCore->Reset_Timer( pingTimerId, probeInterval );
	}
}

void BaseResource::DoPing( unsigned& ping_delay, bool& ping_complete,
						   bool& ping_succeeded )
{
	ping_delay = 0;
	ping_complete = true;
	ping_succeeded = true;
}

time_t BaseResource::GetLeaseExpiration( const BaseJob *job ) const
{
	// Without a job to consider, just return the shared lease
	// expiration, if we have one.
	if ( job == NULL ) {
		return m_sharedLeaseExpiration;
	}
	// What lease expiration should be used for this job?
	// Some jobs may have no lease (default for grid-type condor).
	// Otherwise, if we haven't established a shared lease yet,
	// calculate one for this job only.
	time_t new_expiration = 0;
	int job_lease_duration = m_defaultLeaseDuration;
	job->jobAd->LookupInteger( ATTR_JOB_LEASE_DURATION, job_lease_duration );
	if ( job_lease_duration > 0 ) {
		new_expiration = m_sharedLeaseExpiration > 0 ?
			m_sharedLeaseExpiration : time(NULL) + job_lease_duration;
	}
	return new_expiration;
}

void BaseResource::RequestUpdateLeases() const
{
	if ( updateLeasesTimerId != TIMER_UNSET ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, 0 );
	}
}

void BaseResource::UpdateLeases( int /* timerID */ )
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
	delay = (lastUpdateLeases + UPDATE_LEASE_DELAY) - time(NULL);
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, delay );
dprintf(D_FULLDEBUG,"    UpdateLeases: last update too recent, delaying %d secs\n",delay);
		return;
	}

	daemonCore->Reset_Timer( updateLeasesTimerId, TIMER_NEVER );

    if ( updateLeasesActive == false ) {
		time_t new_lease_duration = std::numeric_limits<time_t>::max();
		dprintf(D_FULLDEBUG,"    UpdateLeases: calc'ing new leases\n");
		for (auto curr_job: registeredJobs) {
			time_t job_lease_duration = m_defaultLeaseDuration;
			curr_job->jobAd->LookupInteger( ATTR_JOB_LEASE_DURATION, job_lease_duration );
			if ( job_lease_duration > 0 && job_lease_duration < new_lease_duration ) {
				new_lease_duration = job_lease_duration;
			}
		}
dprintf(D_FULLDEBUG,"    UpdateLeases: new shared lease duration: %ld\n", new_lease_duration);
		// This is how close to the lease expiration time we want to be
		// when we try a renewal.
		int renew_threshold = ( new_lease_duration * 2 / 3 ) + 10;
		if ( new_lease_duration == INT_MAX ||
			 m_sharedLeaseExpiration > time(NULL) + renew_threshold ) {
			// Lease doesn't need renewal, yet.
			time_t next_renew_time = m_sharedLeaseExpiration - renew_threshold;
			if ( new_lease_duration == INT_MAX ||
				 next_renew_time > time(NULL) + 3600 ) {
				next_renew_time = time(NULL) + 3600;
			}
dprintf(D_FULLDEBUG,"    UpdateLeases: nothing to renew, resetting timer for %ld secs\n",next_renew_time - time(NULL));
			lastUpdateLeases = time(NULL);
			daemonCore->Reset_Timer( updateLeasesTimerId,
									 next_renew_time - time(NULL) );
			return;
		} else {
			// Time to renew the lease
			m_sharedLeaseExpiration = time(NULL) + new_lease_duration;
			if ( !m_hasSharedLeases ) {
				for (auto curr_job: registeredJobs) {
					std::string job_id;
					int tmp;
					if ( curr_job->jobAd->LookupString( ATTR_GRID_JOB_ID, job_id ) &&
						 curr_job->jobAd->LookupInteger( ATTR_JOB_LEASE_DURATION, tmp )
					) {
						leaseUpdates.emplace_back(curr_job);
					}
				}
			}
dprintf(D_FULLDEBUG,"    new shared lease expiration at %ld, performing renewal...\n",m_sharedLeaseExpiration);
			requestScheddUpdateNotification( updateLeasesTimerId );
			updateLeasesActive = true;
		}
	}

	unsigned update_delay = 0;
	bool update_complete;
	std::vector<PROC_ID> update_succeeded;
	bool update_success;
dprintf(D_FULLDEBUG,"    UpdateLeases: calling DoUpdateLeases\n");
	if ( m_hasSharedLeases ) {
		DoUpdateSharedLease( update_delay, update_complete, update_success );
	} else {
		DoUpdateLeases( update_delay, update_complete, update_succeeded );
	}

	if ( update_delay ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, update_delay );
dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases wants delay of %u secs\n",update_delay);
		return;
	}

	if ( !update_complete ) {
		updateLeasesCmdActive = true;
dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases in progress\n");
		return;
	}

dprintf(D_FULLDEBUG,"    UpdateLeases: DoUpdateLeases complete, processing results\n");
	bool first_update = lastUpdateLeases == 0;

	updateLeasesCmdActive = false;
	lastUpdateLeases = time(NULL);

	if ( m_hasSharedLeases ) {
		std::string tmp;
		for (auto curr_job: registeredJobs) {
			if ( first_update ) {
				// New jobs may be waiting for the lease be to established
				// before they proceed with submission.
				curr_job->SetEvaluateState();
			}
			if ( curr_job->jobAd->LookupString( ATTR_GRID_JOB_ID, tmp ) ) {
				curr_job->UpdateJobLeaseSent( m_sharedLeaseExpiration );
			}
		}
	} else {
std::string msg = "    update_succeeded:";
		for (auto& curr_id: update_succeeded) {
formatstr_cat(msg, " %d.%d", curr_id.cluster, curr_id.proc);
			auto itr = BaseJob::JobsByProcId.find(curr_id);
			if (itr != BaseJob::JobsByProcId.end()) {
				itr->second->UpdateJobLeaseSent(m_sharedLeaseExpiration);
			}
		}
dprintf(D_FULLDEBUG,"%s\n",msg.c_str());
		leaseUpdates.clear();
	}

	updateLeasesActive = false;

dprintf(D_FULLDEBUG,"    UpdateLeases: lease update complete, resetting timer for 30 secs\n");
	daemonCore->Reset_Timer( updateLeasesTimerId, UPDATE_LEASE_DELAY );
}

void BaseResource::DoUpdateLeases( unsigned& update_delay,
                                   bool& update_complete,
                                   std::vector<PROC_ID>& /* update_succeeded */ )
{
dprintf(D_FULLDEBUG,"*** BaseResource::DoUpdateLeases called\n");
	update_delay = 0;
	update_complete = true;
}

void BaseResource::DoUpdateSharedLease( unsigned& update_delay,
										bool& update_complete,
										bool& update_succeeded )
{
dprintf(D_FULLDEBUG,"*** BaseResource::DoUpdateSharedLease called\n");
	update_delay = 0;
	update_complete = true;
	update_succeeded = false;
}

void BaseResource::StartBatchStatusTimer()
{
	if(m_batchPollTid != TIMER_UNSET) {
		EXCEPT("BaseResource::StartBatchStatusTimer called more than once!");
	}
	dprintf(D_FULLDEBUG, "Grid type for %s will use batch status requests (DoBatchStatus).\n", ResourceName());
	m_batchPollTid = daemonCore->Register_Timer( 0,
		(TimerHandlercpp)&BaseResource::DoBatchStatus,
		"BaseResource::DoBatchStatus", (Service*)this );
}

int BaseResource::BatchStatusInterval() const
{
	return m_paramJobPollInterval;
}

BaseResource::BatchStatusResult BaseResource::StartBatchStatus()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::StartBatchStatus() called.");
	return BSR_ERROR; // Required by Visual C++ compiler
}

BaseResource::BatchStatusResult BaseResource::FinishBatchStatus()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::FinishBatchStatus() called.");
	return BSR_ERROR; // Required by Visual C++ compiler
}

GahpClient * BaseResource::BatchGahp()
{
	/*
		Likely cause: someone called StartBatchStatusTimer
		but failed to reimplement this.
	*/
	EXCEPT("Internal consistency error: BaseResource::BatchGahp() called.");
	return 0;
}

void BaseResource::DoBatchStatus( int /* timerID */ )
{
	dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus for %s.\n", ResourceName());

	if ( ( registeredJobs.empty() || resourceDown ) &&
		 m_batchStatusActive == false ) {
			// No jobs or we can't talk to the schedd, so no point
			// in polling
		daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
		dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus for %s skipped for %d seconds because %s.\n", ResourceName(), BatchStatusInterval(), resourceDown ? "the resource is down":"there are no jobs registered");
		return;
	}

	GahpClient * gahp = BatchGahp();
	if ( gahp && gahp->isStarted() == false ) {
		int GAHP_INIT_DELAY = 5;
		dprintf( D_ALWAYS,"BaseResource::DoBatchStatus: gahp server not up yet, delaying %d seconds\n", GAHP_INIT_DELAY );
		daemonCore->Reset_Timer( m_batchPollTid, GAHP_INIT_DELAY );
		return;
	}

	daemonCore->Reset_Timer( m_batchPollTid, TIMER_NEVER );

	if(m_batchStatusActive == false) {
		dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Starting bulk job poll of %s\n", ResourceName());
		BatchStatusResult bsr = StartBatchStatus();
		switch(bsr) {
			case BSR_DONE:
				dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Finished bulk job poll of %s\n", ResourceName());
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return;

			case BSR_ERROR:
				dprintf(D_ALWAYS, "BaseResource::DoBatchStatus: An error occurred trying to start a bulk poll of %s\n", ResourceName());
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return;

			case BSR_PENDING:
				m_batchStatusActive = true;
				return;

			default:
				EXCEPT("BaseResource::DoBatchStatus: Unknown BatchStatusResult %d", (int)bsr);
		}

	} else {
		BatchStatusResult bsr = FinishBatchStatus();
		switch(bsr) {
			case BSR_DONE:
				dprintf(D_FULLDEBUG, "BaseResource::DoBatchStatus: Finished bulk job poll of %s\n", ResourceName());
				m_batchStatusActive = false;
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return;

			case BSR_ERROR:
				dprintf(D_ALWAYS, "BaseResource::DoBatchStatus: An error occurred trying to finish a bulk poll of %s\n", ResourceName());
				m_batchStatusActive = false;
				daemonCore->Reset_Timer( m_batchPollTid, BatchStatusInterval() );
				return;

			case BSR_PENDING:
				return;

			default:
				EXCEPT("BaseResource::DoBatchStatus: Unknown BatchStatusResult %d", (int)bsr);
		}
	}
}
