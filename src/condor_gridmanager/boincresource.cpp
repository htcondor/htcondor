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
#include "string_list.h"

#include "boincresource.h"
#include "boincjob.h"
#include "gridmanager.h"

#ifdef WIN32
	#include <sys/types.h> 
	#include <sys/timeb.h>
#endif

using std::list;
using std::vector;
using std::set;

#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100
#define DEFAULT_LEASE_DURATION		(6 * 60 * 60)
#define SUBMIT_DELAY				2
#define LEASE_RETRY_INTERVAL		(5 * 60)


int BoincResource::gahpCallTimeout = 300;	// default value

std::map <std::string, BoincResource *>
    BoincResource::ResourcesByName;

enum BatchSubmitStatus {
	BatchUnsubmitted,
	BatchMaybeSubmitted,
	BatchSubmitting,
	BatchSubmitted,
	BatchFailed,
};

struct BoincBatch {
	std::string m_batch_name;
	std::string m_app_name;
	BatchSubmitStatus m_submit_status;
	bool m_need_full_query;
	time_t m_lease_time;
	time_t m_last_lease_attempt;
	time_t m_last_insert;
	std::string m_error_message;
	std::set<BoincJob *> m_jobs;
	std::set<BoincJob *> m_jobs_ready;
	std::set<BoincJob *> m_jobs_done;
};

BoincResource *BoincResource::FindOrCreateResource( const char *resource_name,
													const char *authenticator )
{
	BoincResource *resource = NULL;
	std::string &key = HashName( resource_name, authenticator );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new BoincResource( resource_name, authenticator );
		ASSERT(resource);
		if ( resource->Init() == false ) {
			delete resource;
			resource = NULL;
		} else {
			ResourcesByName[key] = resource;
		}
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}

BoincResource::BoincResource( const char *resource_name,
							  const char *authenticator )
	: BaseResource( resource_name )
{
	initialized = false;
	gahp = NULL;
	m_statusGahp = NULL;
	m_leaseGahp = NULL;
	m_activeLeaseBatch = NULL;
	m_submitGahp = NULL;
	m_activeSubmitBatch = NULL;

	m_activeLeaseTime = 0;

//	hasLeases = true;
//	m_hasSharedLeases = true;
//	m_defaultLeaseDuration = 6 * 60 * 60;

	m_needFullQuery = false;
	m_doingFullQuery = false;
	m_lastQueryTime = "0";

	m_serviceUri = strdup( resource_name );
	m_authenticator = strdup( authenticator );

	m_leaseTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&BoincResource::UpdateBoincLeases,
							"BoincResource::UpdateBoincLeases", (Service*)this );

	m_submitTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&BoincResource::DoBatchSubmits,
							"BoincResource::DoBatchSubmits", (Service*)this );
}

BoincResource::~BoincResource()
{
	daemonCore->Cancel_Timer( m_leaseTid );
	daemonCore->Cancel_Timer( m_submitTid );

	ResourcesByName.erase( HashName( resourceName, m_authenticator ) );

	free( m_serviceUri );
	free( m_authenticator );

	delete gahp;
	delete m_statusGahp;
	delete m_leaseGahp;
	delete m_submitGahp;

	while ( !m_batches.empty() ) {
		delete m_batches.front();
		m_batches.pop_front();
	}
}

bool BoincResource::Init()
{
	if ( initialized ) {
		return true;
	}

		// TODO This assumes that at least one BoincJob has already
		// initialized the gahp server. Need a better solution.
	std::string gahp_name;
	formatstr( gahp_name, "BOINC" );

	gahp = new GahpClient( gahp_name.c_str() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );
	gahp->setBoincResource( this );

	m_statusGahp = new GahpClient( gahp_name.c_str() );

	StartBatchStatusTimer();

	m_statusGahp->setNotificationTimerId( BatchPollTid() );
	m_statusGahp->setMode( GahpClient::normal );
	m_statusGahp->setTimeout( gahpCallTimeout );
	m_statusGahp->setBoincResource( this );

	m_leaseGahp = new GahpClient( gahp_name.c_str() );

	m_leaseGahp->setNotificationTimerId( m_leaseTid );
	m_leaseGahp->setMode( GahpClient::normal );
	m_leaseGahp->setTimeout( gahpCallTimeout );
	m_leaseGahp->setBoincResource( this );

	m_submitGahp = new GahpClient( gahp_name.c_str() );

	m_submitGahp->setNotificationTimerId( m_submitTid );
	m_submitGahp->setMode( GahpClient::normal );
	m_submitGahp->setTimeout( 1800 );
	m_submitGahp->setBoincResource( this );

	char* pool_name = param( "COLLECTOR_HOST" );
	if ( pool_name ) {
		StringList collectors( pool_name );
		free( pool_name );
		pool_name = collectors.print_to_string();
	}
	if ( !pool_name ) {
		pool_name = strdup( "NoPool" );
	}

	free( pool_name );

	initialized = true;

	Reconfig();

	return true;
}

void BoincResource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );
	m_statusGahp->setTimeout( gahpCallTimeout );
	m_leaseGahp->setTimeout( gahpCallTimeout );
	// TODO need longer timeout for submission
	m_submitGahp->setTimeout( gahpCallTimeout );
}

const char *BoincResource::ResourceType()
{
	return "boinc";
}

std::string & BoincResource::HashName( const char *resource_name,
									 const char *authenticator )
{
	static std::string hash_name;

	formatstr( hash_name, "boinc %s %s", resource_name, authenticator );

	return hash_name;
}

void BoincResource::RegisterJob( BaseJob *base_job )
{
	BoincJob* job = dynamic_cast<BoincJob*>( base_job );
	ASSERT( job );

	int job_lease;
	if ( m_sharedLeaseExpiration == 0 ) {
		if ( job->jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, job_lease ) ) {
			m_sharedLeaseExpiration = job_lease;
		}
	} else {
		if ( job->jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, job_lease ) ) {
			job->UpdateJobLeaseSent( m_sharedLeaseExpiration );
		}
	}

	// TODO should we also reset the timer if this job has a shorter
	//   lease duration than all existing jobs?
	if ( m_sharedLeaseExpiration == 0 ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, 0 );
	}

	BaseResource::RegisterJob( job );
}

void BoincResource::UnregisterJob( BaseJob *base_job )
{
	BoincJob *job = dynamic_cast<BoincJob*>( base_job );
	ASSERT( job );

	BaseResource::UnregisterJob( job );

	for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
		  batch_itr != m_batches.end(); batch_itr++ ) {
		for ( set<BoincJob *>::iterator job_itr = (*batch_itr)->m_jobs.begin();
			  job_itr != (*batch_itr)->m_jobs.end(); job_itr++ ) {

			if ( (*job_itr) == job ) {
				(*batch_itr)->m_jobs.erase( (*job_itr) );
				(*batch_itr)->m_jobs_ready.erase( (*job_itr) );
				(*batch_itr)->m_jobs_done.erase( (*job_itr) );
				break;
			}
		}
		if ( (*batch_itr)->m_jobs.empty() ) {
			// This batch is empty, clean it up and delete it
			if ( m_activeLeaseBatch == (*batch_itr) ) {
				// We have a lease command running on this batch
				// Purge it and check other batches
				m_leaseGahp->purgePendingRequests();
				m_activeLeaseBatch = NULL;
				daemonCore->Reset_Timer( m_leaseTid, 0 );
			}
			if ( m_activeSubmitBatch == (*batch_itr) ) {
				// We have a submit command running on this batch
				// Purge it and check other batches
				m_submitGahp->purgePendingRequests();
				m_activeSubmitBatch = NULL;
				daemonCore->Reset_Timer( m_submitTid, 0 );
			}
			delete (*batch_itr);
			m_batches.erase( batch_itr );
			break;
		}
	}
}

const char *BoincResource::GetHashName()
{
	return HashName( resourceName, m_authenticator ).c_str();
}

void BoincResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	gahp->PublishStats( resource_ad );
}

bool BoincResource::JoinBatch( BoincJob *job, std::string &batch_name,
							   std::string & /*error_str*/ )
{
	if ( !batch_name.empty() ) {
		BoincBatch *batch = NULL;
		for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
			  batch_itr != m_batches.end(); batch_itr++ ) {
			if ( (*batch_itr)->m_batch_name == batch_name ) {
				batch = *batch_itr;
				break;
			}
		}

		if ( batch == NULL ) {
			batch = new BoincBatch();
			batch->m_batch_name = batch_name;
			batch->m_app_name = job->GetAppName();
			batch->m_lease_time = 0;
			batch->m_last_lease_attempt = 0;
			batch->m_submit_status = BatchMaybeSubmitted;
			m_batches.push_back( batch );
		}

		// TODO update batch->m_lease_time based in job's lease expiration
		//   time, if appropriate
		if ( batch->m_submit_status == BatchUnsubmitted ) {
			batch->m_submit_status = BatchMaybeSubmitted;
		}
		if ( batch->m_submit_status == BatchMaybeSubmitted && !job->remoteState.empty() ) {
			batch->m_submit_status = BatchSubmitted;
		}
		batch->m_last_insert = time(NULL);
		batch->m_jobs.insert( job );
		batch->m_need_full_query = true;
		m_needFullQuery = true;
		return true;
	} else {

		BoincBatch *batch = NULL;
		for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
			  batch_itr != m_batches.end(); batch_itr++ ) {
			// Assume all jobs with the same application can go into the
			// same boinc batch.
			// But we can't add this job to a batch that's already been
			// submitted.
			if ( (*batch_itr)->m_app_name == job->GetAppName() && (*batch_itr)->m_submit_status == BatchUnsubmitted ) {
				batch = *batch_itr;
				break;
			}
		}

		if ( batch == NULL ) {
			batch = new BoincBatch();
			batch->m_app_name = job->GetAppName();
			// This batch naming scheme assumes all jobs with the same
			// application can go into the same boinc batch.
			formatstr( batch->m_batch_name, "condor#%s#%s#%d", ScheddName,
					   batch->m_app_name.c_str(), (int)time(NULL) );
			batch->m_lease_time = 0;
			batch->m_last_lease_attempt = 0;
			batch->m_submit_status = BatchUnsubmitted;
			m_batches.push_back( batch );
		}

		batch->m_last_insert = time(NULL);
		batch->m_jobs.insert( job );
		batch_name = batch->m_batch_name;
		batch->m_need_full_query = true;
		m_needFullQuery = true;
		return true;
	}
	// TODO For an unsubmitted batch, may need to update state to indicate
	//   it's not ready to submit yet.
	// TODO Need to check error recovery
	//   Ensure that all jobs claiming to be in a batch were included in
	//   submission
	//   Could query BOINC server to enforce
}

BoincSubmitResponse BoincResource::Submit( BoincJob *job,
										   std::string &error_str )
{
	if ( job->remoteBatchName == NULL ) {
		error_str = "Job has no batch name";
		return BoincSubmitFailure;
	}

	BoincBatch *batch = NULL;
	for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
		  batch_itr != m_batches.end(); batch_itr++ ) {
		if ( (*batch_itr)->m_batch_name == job->remoteBatchName ) {
			batch = *batch_itr;
			break;
		}
	}
	if ( batch == NULL ) {
		error_str = "BoincBatch not found";
		return BoincSubmitFailure;
	}

	if ( batch->m_submit_status == BatchFailed ) {
		error_str = batch->m_error_message;
		return BoincSubmitFailure;
	}
	if ( batch->m_submit_status == BatchSubmitted ) {
		return BoincSubmitSuccess;
	}
	if ( batch->m_submit_status == BatchSubmitting ) {
		// If we've started submitting, avoid any checks below
		return BoincSubmitWait;
	}
	// TODO any other cases where we're not waiting for submission?

	batch->m_jobs_ready.insert( job );
	// If we in BatchMaybeSubmitted and m_error_message is set, then
	// the batch query failed and we should return the error message
	// to the job. We don't return BoincSubmitFailure, because that
	// indicates the batch has not been submitted and no cancelation
	// is required.
	if ( batch->m_submit_status == BatchMaybeSubmitted ) {
		error_str = batch->m_error_message;
	}
	if ( BatchReadyToSubmit( batch ) ) {
		daemonCore->Reset_Timer( m_submitTid, 0 );
	}
	return BoincSubmitWait;
}

bool BoincResource::BatchReadyToSubmit( BoincBatch *batch, unsigned *delay )
{
	if ( time(NULL) < batch->m_last_insert + SUBMIT_DELAY ) {
		if ( delay ) {
			*delay = (batch->m_last_insert + SUBMIT_DELAY) - time(NULL);
		}
		return false;
	}
	if ( batch->m_jobs != batch->m_jobs_ready ) {
		if ( delay ) {
			*delay = TIMER_NEVER;
		}
		return false;
	}
	return true;
}

bool BoincResource::JobDone( BoincJob *job )
{
	for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
		  batch_itr != m_batches.end(); batch_itr++ ) {

		if ( (*batch_itr)->m_jobs.find( job ) == (*batch_itr)->m_jobs.end() ) {
			continue;
		}
		(*batch_itr)->m_jobs_done.insert( job );
		if ( (*batch_itr)->m_jobs_done == (*batch_itr)->m_jobs ) {
			return true;
		} else {
			return false;
		}
	}
	return false;
}

void BoincResource::DoPing( unsigned& ping_delay, bool& ping_complete,
							bool& ping_succeeded )
{
	int rc;

	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	rc = gahp->boinc_ping();
	
	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}


BoincResource::BatchStatusResult BoincResource::StartBatchStatus()
{
	m_statusBatches.clearAll();
	for ( std::list<BoincBatch*>::iterator itr = m_batches.begin();
		  itr != m_batches.end(); itr++ ) {
		if ( (*itr)->m_submit_status == BatchSubmitted ||
			 (*itr)->m_submit_status == BatchMaybeSubmitted ) {
			if ( m_needFullQuery && !(*itr)->m_need_full_query ) {
				continue;
			}
			m_statusBatches.append( (*itr)->m_batch_name.c_str() );
		}
	}
	m_doingFullQuery = m_needFullQuery;
	m_needFullQuery = false;

	return FinishBatchStatus();
}

BoincResource::BatchStatusResult BoincResource::FinishBatchStatus()
{
	if ( m_statusBatches.isEmpty() ) {
		return BSR_DONE;
	}

	std::string old_query_time = m_doingFullQuery ? "0" : m_lastQueryTime;
	std::string new_query_time;
	GahpClient::BoincQueryResults results;
	int rc = m_statusGahp->boinc_query_batches( m_statusBatches, old_query_time,
												new_query_time, results );
	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return BSR_PENDING;
	}
	if ( rc != 0 ) {
		// TODO Save error string for use in hold messages?
		dprintf( D_ALWAYS, "Error getting BOINC status: %s\n",
				 m_statusGahp->getErrorString() );

		// Check if one of the batches we queried for doesn't exist on
		// the server.
		const char *ptr = strstr( m_statusGahp->getErrorString(),
								  "no batch named " );
		if ( ptr ) {
			// Advance pointer to name of missing batch
			ptr += 15;

			BoincBatch *batch = NULL;
			for ( std::list<BoincBatch*>::iterator batch_itr = m_batches.begin();
				  batch_itr != m_batches.end(); batch_itr++ ) {
				if ( (*batch_itr)->m_batch_name == ptr ) {
					batch = *batch_itr;
					break;
				}
			}
			m_statusBatches.remove( ptr );
			if ( batch == NULL ) {
				dprintf( D_ALWAYS, "Failed to find batch %s!\n", ptr );
			} else if ( batch->m_submit_status == BatchMaybeSubmitted ) {
				batch->m_submit_status = BatchUnsubmitted;
				daemonCore->Reset_Timer( m_submitTid, 0 );
			} else {
				dprintf( D_ALWAYS, "Submitted batch %s not found on server!\n",
						 ptr );
				// TODO How do we react?
				//   Notify the jobs that they don't exist?
				//   At the very least, we should avoid querying this batch
				//   in the future.
			}
			// Careful about recusrion here!
			// TODO Avoid recursing?
			return FinishBatchStatus();
		}

		// TODO Check for error "not owner of <batch name>"

		// If this error looks like it would affect a submit command,
		// notify all jobs in BatchMaybeSubmitted state.
		for ( std::list<BoincBatch*>::iterator batch_itr = m_batches.begin();
			  batch_itr != m_batches.end(); batch_itr++ ) {
			if ( (*batch_itr)->m_submit_status != BatchMaybeSubmitted ||
				 !(*batch_itr)->m_error_message.empty() ) {
				continue;
			}
			(*batch_itr)->m_error_message = m_statusGahp->getErrorString();
			for ( set<BoincJob *>::iterator job_itr = (*batch_itr)->m_jobs.begin();
				  job_itr != (*batch_itr)->m_jobs.end(); job_itr++ ) {
				(*job_itr)->SetEvaluateState();
			}
		}

		m_statusBatches.clearAll();
		return BSR_ERROR;
	}

	// TODO If we knew that we were querying all batches, we'd want to
	// update m_lastQueryTime.
	if ( !m_doingFullQuery ) {
		m_lastQueryTime = new_query_time;
	}

	// TODO We're not detecting missing jobs from batch results
	//   Do we need to?
	m_statusBatches.rewind();
	const char *batch_name;

	// Iterate over the batches in the response
	for ( GahpClient::BoincQueryResults::iterator i = results.begin(); i != results.end(); i++ ) {
		batch_name = m_statusBatches.next();
		ASSERT( batch_name );

		BoincBatch *batch = NULL;
		for ( list<BoincBatch *>::iterator batch_itr = m_batches.begin();
			  batch_itr != m_batches.end(); batch_itr++ ) {
			if ( (*batch_itr)->m_batch_name == batch_name ) {
				batch = *batch_itr;
				break;
			}
		}

		// If we're in recovery, we may not know whether this batch has
		// been submitted.
		if ( batch && batch->m_submit_status == BatchMaybeSubmitted ) {

			if ( i->empty() ) {
				// Batch doesn't exist on sever
				// Consider it for submission
				batch->m_submit_status = BatchUnsubmitted;
				daemonCore->Reset_Timer( m_submitTid, 0 );
			} else {
				// Batch exists on server
				// Signal the jobs
				batch->m_submit_status = BatchSubmitted;
				for ( set<BoincJob *>::iterator job_itr = batch->m_jobs.begin();
					  job_itr != batch->m_jobs.end(); job_itr++ ) {
					(*job_itr)->SetEvaluateState();
				}
				daemonCore->Reset_Timer( m_leaseTid, 0 );
			}
			break;
		}

		// Iterate over the jobs for this batch
		for ( GahpClient::BoincBatchResults::iterator j = i->begin(); j != i->end(); j++ ) {
			const char *job_id = strrchr( j->first.c_str(), '#' );
			if ( job_id == NULL ) {
				dprintf( D_ALWAYS, "Failed to find job id in '%s'\n", j->first.c_str() );
				continue;
			}
			job_id++;
			PROC_ID proc_id;
			if ( sscanf( job_id, "%d.%d", &proc_id.cluster, &proc_id.proc ) != 2 ) {
				dprintf( D_ALWAYS, "Failed to parse job id '%s'\n", j->first.c_str() );
				continue;
			}
			BaseJob *base_job;
			if ( BaseJob::JobsByProcId.lookup( proc_id, base_job ) == 0 ) {
				BoincJob *boinc_job = dynamic_cast<BoincJob*>( base_job );
				ASSERT( boinc_job != NULL );
				boinc_job->NewBoincState( j->second.c_str() );
			}
		}
		// If not doing a full query, update all jobs in the batch that
		// their status is unchanged.
		if ( batch && !m_doingFullQuery ) {
			for ( std::set<BoincJob*>::iterator job_itr = batch->m_jobs.begin(); job_itr != batch->m_jobs.end(); job_itr++ ) {
				(*job_itr)->NewBoincState( (*job_itr)->remoteState.c_str() );
			}
		}
	}

	m_statusBatches.clearAll();
	return BSR_DONE;
}

GahpClient * BoincResource::BatchGahp() { return m_statusGahp; }

void BoincResource::DoBatchSubmits()
{
dprintf(D_FULLDEBUG,"*** DoBatchSubmits()\n");
	unsigned delay = TIMER_NEVER;

	if ( m_submitGahp == NULL || !m_submitGahp->isStarted() ) {
		dprintf( D_FULLDEBUG, "gahp server not up yet, delaying DoBatchSubmits\n" );
		daemonCore->Reset_Timer( m_submitTid, 5 );
		return;
	}

	for ( list<BoincBatch*>::iterator batch = m_batches.begin();
		  batch != m_batches.end(); batch++ ) {

		if ( (*batch)->m_submit_status == BatchMaybeSubmitted ||
			 (*batch)->m_submit_status == BatchSubmitted ||
			 (*batch)->m_submit_status == BatchFailed ) {
			continue;
		}

		// If we have an active submit command, skip to that batch
		unsigned this_delay = TIMER_NEVER;
		if ( m_activeSubmitBatch != NULL ) {
			if ( *batch != m_activeSubmitBatch ) {
				continue;
			}
		} else if ( !BatchReadyToSubmit( *batch, &this_delay ) ) {
			if ( this_delay < delay ) {
				delay = this_delay;
			}
			continue;
		}

		if ( m_activeSubmitBatch == NULL ) {

			// Let's start submitting this batch
			int rc = m_submitGahp->boinc_submit( (*batch)->m_batch_name.c_str(),
												 (*batch)->m_jobs );
			if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
				dprintf( D_ALWAYS, "New boinc_submit() didn't return PENDING!?: %s\n", m_submitGahp->getErrorString() );
				m_submitGahp->purgePendingRequests();
				// TODO What else should we do?
			} else {
				(*batch)->m_submit_status = BatchSubmitting;
				m_activeSubmitBatch = (*batch);
				delay = TIMER_NEVER;
				break; // or reset timer and return?
			}

		} else {
			// active submit command, check if it's done
			// TODO avoid overhead of recreating arguments for 
			//   already-submitted command
			int rc = m_submitGahp->boinc_submit( (*batch)->m_batch_name.c_str(),
												 (*batch)->m_jobs );
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				// do nothing, wait for command to complete
				delay = TIMER_NEVER;
				break;
			}
			m_activeSubmitBatch = NULL;

			if ( rc == 0 ) {
				// success
				(*batch)->m_submit_status = BatchSubmitted;
				daemonCore->Reset_Timer( m_leaseTid, 0 );
			} else {
				dprintf( D_ALWAYS, "Failed to submit batch %s: %s\n",
						 (*batch)->m_batch_name.c_str(),
						 m_submitGahp->getErrorString() );
				(*batch)->m_submit_status = BatchFailed;
				(*batch)->m_error_message = m_submitGahp->getErrorString();
			}

			for ( set<BoincJob *>::iterator job = (*batch)->m_jobs.begin();
				  job != (*batch)->m_jobs.end(); job++ ) {

				(*job)->SetEvaluateState();
			}
			// Re-call this method immediately to check for other batches
			// to submit.
			delay = 0;
		}
	}

	daemonCore->Reset_Timer( m_submitTid, delay );
}

void BoincResource::UpdateBoincLeases()
{
dprintf(D_FULLDEBUG,"*** UpdateBoincLeases()\n");
	unsigned delay = TIMER_NEVER;
	time_t now = time(NULL);

	if ( m_leaseGahp == NULL || !m_leaseGahp->isStarted() ) {
		dprintf( D_FULLDEBUG, "gahp server not up yet, delaying UpdateBoincLeases\n" );
		daemonCore->Reset_Timer( m_leaseTid, 5 );
		return;
	}

	for ( list<BoincBatch*>::iterator batch = m_batches.begin();
		  batch != m_batches.end(); batch++ ) {

		if ( (*batch)->m_submit_status != BatchSubmitted ) {
			continue;
		}

		// If we have an active lease update command, skip to that batch
		if ( m_activeLeaseBatch != NULL && *batch != m_activeLeaseBatch ) {
			continue;
		}

		if ( m_activeLeaseBatch == NULL ) {
			// Calculate how long until this batch's lease needs to be updated
			time_t renew_time = (*batch)->m_lease_time - ((2 * DEFAULT_LEASE_DURATION) / 3);
			if ( renew_time <= now ) {

				if ( ( (*batch)->m_last_lease_attempt + LEASE_RETRY_INTERVAL ) > now ) {
					unsigned this_delay = ( (*batch)->m_last_lease_attempt + LEASE_RETRY_INTERVAL ) - now;
					if ( this_delay < delay ) {
						delay = this_delay;
					}
					dprintf(D_FULLDEBUG,"    skipping recently attempted lease renewal\n");

					continue;
				}

				// lease needs to be updated
				time_t new_lease_time = time(NULL) + DEFAULT_LEASE_DURATION;
				int rc = m_leaseGahp->boinc_set_lease( (*batch)->m_batch_name.c_str(),
													   new_lease_time );
				if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
					dprintf( D_ALWAYS, "New boinc_set_lease() didn't return PENDING!?: %s\n", m_leaseGahp->getErrorString() );
					m_leaseGahp->purgePendingRequests();
					// TODO What else should we do?
				} else {
					m_activeLeaseBatch = (*batch);
					m_activeLeaseTime = new_lease_time;
					delay = TIMER_NEVER;
					break; // or reset timer and return?
				}
			} else {
				unsigned this_delay = renew_time - now;
				if ( this_delay < delay ) {
					delay = this_delay;
				}
			}
		} else {
			// active lease command, check if it's done
			int rc = m_leaseGahp->boinc_set_lease( (*batch)->m_batch_name.c_str(),
												   m_activeLeaseTime );
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				// do nothing, wait for command to complete
				break;
			}
			m_activeLeaseBatch = NULL;
			(*batch)->m_last_lease_attempt = time(NULL);
			if ( rc == 0 ) {
				// success
				(*batch)->m_lease_time = m_activeLeaseTime;
				for ( set<BoincJob *>::iterator job = (*batch)->m_jobs.begin();
					  job != (*batch)->m_jobs.end(); job++ ) {

					(*job)->UpdateJobLeaseSent( m_activeLeaseTime );
				}
			} else {
				dprintf( D_ALWAYS, "Failed to set lease for batch %s: %s\n",
						 (*batch)->m_batch_name.c_str(),
						 m_leaseGahp->getErrorString() );
				// TODO What else do we do?
			}
			// Now that this lease command is done, re-examine all
			// batches.
			delay = 0;
			break;

		}
	}

	daemonCore->Reset_Timer( m_leaseTid, delay );
}

/*
void BoincResource::DoUpdateSharedLease( unsigned& update_delay,
										 bool& update_complete,
										 bool& update_succeeded )
{
	// TODO Can we use BaseResource's existing code for lease updates?
	//  It assumes a separate lease per job or one lease for all jobs.
	//  We have a lease for each batch.
}
*/
