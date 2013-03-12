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


#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100


int BoincResource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

HashTable <HashKey, BoincResource *>
    BoincResource::ResourcesByName( HASH_TABLE_SIZE,
									hashFunction );

#define LIFETIME_EXTEND_INTERVAL	300


struct BoincBatch {
	char *m_batch_name;
	bool m_submitted;
	time_t m_lease_time;
	std::string m_error_message;
	std::vector<BoincJob *> m_jobs;
};

BoincResource *BoincResource::FindOrCreateResource( const char *resource_name )
{
	int rc;
	BoincResource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	const char *hash_name = HashName( canonical_name );
	ASSERT(hash_name);

	rc = ResourcesByName.lookup( HashKey( hash_name ), resource );
	if ( rc != 0 ) {
		resource = new BoincResource( canonical_name );
		ASSERT(resource);
		if ( resource->Init() == false ) {
			delete resource;
			resource = NULL;
		} else {
			ResourcesByName.insert( HashKey( hash_name ), resource );
		}
	} else {
		ASSERT(resource);
	}

	return resource;
}

BoincResource::BoincResource( const char *resource_name )
	: BaseResource( resource_name )
{
	initialized = false;
	gahp = NULL;
	status_gahp = NULL;
	m_leaseGahp = NULL;

	hasLeases = true;
	m_hasSharedLeases = true;
	m_defaultLeaseDuration = 6 * 60 * 60;

}

BoincResource::~BoincResource()
{
	if ( serviceUri != NULL ) {
		free( serviceUri );
	}

	ResourcesByName.remove( HashKey( HashName( resourceName ) ) );

	delete gahp;
	delete status_gahp;
	delete m_leaseGahp;
}

bool BoincResource::Init()
{
	if ( initialized ) {
		return true;
	}

		// TODO This assumes that at least one CreamJob has already
		// initialized the gahp server. Need a better solution.
	std::string gahp_name;
	formatstr( gahp_name, "BOINC" );

	gahp = new GahpClient( gahp_name.c_str() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	status_gahp = new GahpClient( gahp_name.c_str() );

	StartBatchStatusTimer();

	status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( gahpCallTimeout );

	m_leaseGahp = new GahpClient( gahp_name.c_str() );

	m_leaseGahp->setNotificationTimerId( updateLeasesTimerId );
	m_leaseGahp->setMode( GahpClient::normal );
	m_leaseGahp->setTimeout( gahpCallTimeout );

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
	status_gahp->setTimeout( gahpCallTimeout );
	m_leaseGahp->setTimeout( gahpCallTimeout );
}

const char *BoincResource::ResourceType()
{
	return "boinc";
}

const char *BoincResource::CanonicalName( const char *name )
{
/*
	static std::string canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	sprintf( canonical, "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.c_str();
*/
	return name;
}

const char *BoincResource::HashName( const char *resource_name )
{
	static std::string hash_name;

	formatstr( hash_name, "boinc %s", resource_name );

	return hash_name.c_str();
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

		// We have trouble maintaining the shared lease with the cream
		// server while we have no jobs for this resource object.
		// This can lead to trouble if a new job shows up before we're
		// deleted. So delete immediately until we can overhaul the
		// lease-update code.
	if ( IsEmpty() ) {
		daemonCore->Reset_Timer( deleteMeTid, 0 );
	}
}

const char *BoincResource::GetHashName()
{
	return HashName( resourceName );
}

void BoincResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );
}

bool BoincResource::JoinBatch( std::string &batch_name, std::string &error_str )
{
	// if batch_name == ""
	//   search for compatible BoincBatch
	//   if not found, create one
	//   add job to BoincBatch
	//   set batch_name
	//   return true
	// else
	//   search for named BoincBatch
	//   if not found, create it
	//   add job to BoincBatch
	//   return true
	// NOTE Need to check error recovery
	//   Ensure that all jobs claiming to be in a batch were included in
	//   submission
	//   Could query BOINC server to enforce
}

int BoincResource::Submit( /* ... */ )
{
	// a job is indicating it's ready for its batch to be submitted
	// possible responses:
	//   * ok, please wait
	//   * submission successful
	//   * submission failed, error msg
	// If we return 'ok, please wait', we promise to trigger job's state
	//   evaluation when answer changes (success or failure)
	// Actual submission will wait until all jobs in batch call Submit(),
	//   plus a possible delay for additional jobs to join batch
	// Submission is done in another method.
}

void BoincResource::DoPing( time_t& ping_delay, bool& ping_complete,
							bool& ping_succeeded )
{
	int rc;

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}

	ping_delay = 0;

	rc = gahp->boinc_ping( resourceName );
	
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
	ASSERT(status_gahp);

	// TODO perform batch query gahp command
	//   when done, iterate over results
	//   find each BoincJob and call NewBoincState()
}

BoincResource::BatchStatusResult BoincResource::FinishBatchStatus()
{
	// As it happens, we can use the same code path
	// for starting and finishing a batch status for
	// CREAM jobs.  Indeed, doing so is simpler.
	return StartBatchStatus();
}

GahpClient * BoincResource::BatchGahp() { return status_gahp; }

void BoincResource::DoUpdateSharedLease( time_t& update_delay,
										 bool& update_complete,
										 bool& update_succeeded )
{
	// TODO Can we use BaseResource's existing code for lease updates?
	//  It assumes a separate lease per job or one lease for all jobs.
	//  We have a lease for each batch.
}
