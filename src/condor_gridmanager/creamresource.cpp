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

#include "creamresource.h"
#include "creamjob.h"
#include "gridmanager.h"

#ifdef WIN32
	#include <sys/types.h> 
	#include <sys/timeb.h>
#endif

// Enable more expensive debugging and testing code
#define DEBUG_CREAM 1


#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100


int CreamResource::gahpCallTimeout = 300;	// default value

std::map <std::string, CreamResource *>
    CreamResource::ResourcesByName;

#define CHECK_DELEGATION_INTERVAL	300
#define LIFETIME_EXTEND_INTERVAL	300
#define PROXY_REFRESH_INTERVAL		300

#define CREAM_JOB_STATE_UNSET			""
#define CREAM_JOB_STATE_REGISTERED		"REGISTERED"
#define CREAM_JOB_STATE_PENDING			"PENDING"
#define CREAM_JOB_STATE_IDLE			"IDLE"
#define CREAM_JOB_STATE_RUNNING			"RUNNING"
#define CREAM_JOB_STATE_REALLY_RUNNING	"REALLY-RUNNING"
#define CREAM_JOB_STATE_CANCELLED		"CANCELLED"
#define CREAM_JOB_STATE_HELD			"HELD"
#define CREAM_JOB_STATE_ABORTED			"ABORTED"
#define CREAM_JOB_STATE_DONE_OK			"DONE-OK"
#define CREAM_JOB_STATE_DONE_FAILED		"DONE-FAILED"
#define CREAM_JOB_STATE_UNKNOWN			"UNKNOWN"
#define CREAM_JOB_STATE_PURGED			"PURGED"

struct CreamProxyDelegation {
	char *deleg_uri;
	time_t lifetime;
	time_t last_lifetime_extend;
	time_t proxy_expire;
	time_t last_proxy_refresh;
	Proxy *proxy;
	std::string error_message;
};

CreamResource *CreamResource::FindOrCreateResource( const char *resource_name,
													const Proxy *proxy )
{
	CreamResource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	std::string &key = HashName( canonical_name, proxy->subject->subject_name, proxy->subject->first_fqan );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new CreamResource( canonical_name, proxy );
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

CreamResource::CreamResource( const char *resource_name,
							  const Proxy *proxy )
	: BaseResource( resource_name )
{
	initialized = false;
	proxySubject = strdup( proxy->subject->subject_name );
	proxyFQAN = strdup( proxy->subject->fqan );
	proxyFirstFQAN = strdup( proxy->subject->first_fqan );
	delegationTimerId = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&CreamResource::checkDelegation,
							"CreamResource::checkDelegation", (Service*)this );
	activeDelegationCmd = NULL;
	delegationServiceUri = NULL;
	gahp = NULL;
	deleg_gahp = NULL;
	status_gahp = NULL;
	m_leaseGahp = NULL;

	hasLeases = true;
	m_hasSharedLeases = true;
	m_defaultLeaseDuration = 6 * 60 * 60;

	const char delegservice_name[] = "/ce-cream/services/gridsite-delegation";
	const char *name_ptr;
	int name_len;
	if ( strncmp( "https://", resource_name, 8 ) == 0 ) {
		name_ptr = &resource_name[8];
	} else {
		name_ptr = resource_name;
	}
	name_len = strcspn( name_ptr, "/" );
	delegationServiceUri = (char *)malloc( 8 +         // "https://"
										   name_len +  // host/port
										   sizeof( delegservice_name ) + 
										   1 );        // terminating \0
	ASSERT( delegationServiceUri != NULL );
	strcpy( delegationServiceUri, "https://" );
	snprintf( delegationServiceUri + 8, name_len + 1, "%s", name_ptr );
	strcat( delegationServiceUri, delegservice_name );

	const char service_name[] = "/ce-cream/services/CREAM";

	serviceUri = (char *)malloc( 8 +         // "https://"
								 name_len +  // host/port
								 sizeof( service_name ) + 
								 1 );        // terminating \0
	ASSERT( serviceUri != NULL );
	strcpy( serviceUri, "https://" );
	snprintf( serviceUri + 8, name_len + 1, "%s", name_ptr );
	strcat( serviceUri, service_name );

}

CreamResource::~CreamResource()
{
dprintf(D_FULLDEBUG,"*** ~CreamResource\n");
	CreamProxyDelegation *next_deleg;
	delegatedProxies.Rewind();
	while ( (next_deleg = delegatedProxies.Next()) != NULL ) {
dprintf(D_FULLDEBUG,"    deleting %s\n",next_deleg->deleg_uri?next_deleg->deleg_uri:"(undelegated)");
		delegatedProxies.DeleteCurrent();
		free( next_deleg->deleg_uri );
		ReleaseProxy( next_deleg->proxy,
					  (TimerHandlercpp)&CreamResource::ProxyCallback, this );
		delete next_deleg;
	}
	if ( delegationServiceUri != NULL ) {
		free( delegationServiceUri );
	}

	if ( serviceUri != NULL ) {
		free( serviceUri );
	}

	ResourcesByName.erase( HashName( resourceName, proxySubject, proxyFirstFQAN ) );

	daemonCore->Cancel_Timer( delegationTimerId );
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( deleg_gahp != NULL ) {
		delete deleg_gahp;
	}
	delete status_gahp;
	delete m_leaseGahp;
	if ( proxySubject ) {
		free( proxySubject );
	}
	free( proxyFQAN );
	free( proxyFirstFQAN );
}

bool CreamResource::Init()
{
	if ( initialized ) {
		return true;
	}

		// TODO This assumes that at least one CreamJob has already
		// initialized the gahp server. Need a better solution.
	std::string gahp_name;
	formatstr( gahp_name, "CREAM/%s/%s", proxySubject, proxyFirstFQAN );

	gahp = new GahpClient( gahp_name.c_str() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	deleg_gahp = new GahpClient( gahp_name.c_str() );

	deleg_gahp->setNotificationTimerId( delegationTimerId );
	deleg_gahp->setMode( GahpClient::normal );
	deleg_gahp->setTimeout( gahpCallTimeout );

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

	formatstr( m_leaseId, "Condor#%s#%s#%s", myUserName, ScheddName, pool_name );

	free( pool_name );

	initialized = true;

	Reconfig();

	return true;
}

void CreamResource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );
	deleg_gahp->setTimeout( gahpCallTimeout );
	status_gahp->setTimeout( gahpCallTimeout );
	m_leaseGahp->setTimeout( gahpCallTimeout );
}

const char *CreamResource::ResourceType()
{
	return "cream";
}

const char *CreamResource::CanonicalName( const char *name )
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

std::string & CreamResource::HashName( const char *resource_name,
									 const char *proxy_subject,
									 const char *proxy_first_fqan )
{
	static std::string hash_name;

	formatstr( hash_name, "cream %s#%s#%s", resource_name, proxy_subject,
			 proxy_first_fqan );

	return hash_name;
}

void CreamResource::RegisterJob( BaseJob *base_job )
{
	CreamJob* job = dynamic_cast<CreamJob*>( base_job );
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

void CreamResource::UnregisterJob( BaseJob *base_job )
{
	CreamJob *job = dynamic_cast<CreamJob*>( base_job );
	ASSERT( job );

	if ( job->delegatedCredentialURI != NULL ) {
		bool delete_deleg = true;
		CreamJob *next_job;
		registeredJobs.Rewind();
		while ( (next_job = (CreamJob*)registeredJobs.Next()) != NULL ) {
			if ( next_job->delegatedCredentialURI != NULL &&
				 strcmp( job->delegatedCredentialURI,
						 next_job->delegatedCredentialURI ) == 0 ) {
				delete_deleg = false;
				break;
			}
		}
		if ( delete_deleg ) {
dprintf(D_FULLDEBUG,"*** deleting delegation %s\n",job->delegatedCredentialURI);
			CreamProxyDelegation *next_deleg;
			delegatedProxies.Rewind();
			while ( (next_deleg = delegatedProxies.Next()) != NULL ) {
				if ( next_deleg->deleg_uri != NULL &&
					 strcmp( job->delegatedCredentialURI,
							 next_deleg->deleg_uri ) == 0 ) {
					delegatedProxies.DeleteCurrent();
					if ( activeDelegationCmd == next_deleg ) {
						deleg_gahp->purgePendingRequests();
						activeDelegationCmd = NULL;
					}
					free( next_deleg->deleg_uri );
					ReleaseProxy( next_deleg->proxy,
								  (TimerHandlercpp)&CreamResource::ProxyCallback,
								  this );
					delete next_deleg;
				}
			}
		}
	}

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

const char *CreamResource::GetHashName()
{
	return HashName( resourceName, proxySubject, proxyFirstFQAN ).c_str();
}

void CreamResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
	resource_ad->Assign( ATTR_X509_USER_PROXY_FQAN, proxyFQAN );
	resource_ad->Assign( ATTR_X509_USER_PROXY_FIRST_FQAN, proxyFirstFQAN );

	gahp->PublishStats( resource_ad );
}

void CreamResource::registerDelegationURI( const char *deleg_uri,
										 Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** registerDelegationURI(%s,%s)\n",deleg_uri,job_proxy->proxy_filename);
	CreamProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( next_deleg->deleg_uri != NULL &&
			 strcmp( deleg_uri, next_deleg->deleg_uri ) == 0 ) {
dprintf(D_FULLDEBUG,"    found CreamProxyDelegation\n");
			return;
		}
	}

dprintf(D_FULLDEBUG,"    creating new CreamProxyDelegation\n");
	next_deleg = new CreamProxyDelegation;
	next_deleg->deleg_uri = strdup( deleg_uri );
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->last_lifetime_extend = 0;
	next_deleg->last_proxy_refresh = 0;
	next_deleg->proxy = job_proxy;
	AcquireProxy( job_proxy, (TimerHandlercpp)&CreamResource::ProxyCallback, this );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );
}

const char *CreamResource::getDelegationURI( Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** getDelegationURI(%s)\n",job_proxy->proxy_filename);
	CreamProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( next_deleg->proxy == job_proxy ) {
				// If the delegation hasn't happened yet, this will return
				// NULL, which tells the caller to continue to wait.
dprintf(D_FULLDEBUG,"    found CreamProxyDelegation\n");
			return next_deleg->deleg_uri;
		}
	}

dprintf(D_FULLDEBUG,"    creating new CreamProxyDelegation\n");
	next_deleg = new CreamProxyDelegation;
	next_deleg->deleg_uri = NULL;
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->last_lifetime_extend = 0;
	next_deleg->last_proxy_refresh = 0;
	next_deleg->proxy = job_proxy;
	AcquireProxy( job_proxy, (TimerHandlercpp)&CreamResource::ProxyCallback, this );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );

	return NULL;
}

const char *CreamResource::getDelegationError( Proxy *job_proxy )
{
	CreamProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( next_deleg->proxy == job_proxy ) {
			if ( next_deleg->error_message.empty() ) {
				return NULL;
			} else {
				return next_deleg->error_message.c_str();
			}
		}
	}

	dprintf( D_FULLDEBUG, "getDelegationError(): failed to find "
			 "CreamProxyDelegation for proxy %s\n", job_proxy->proxy_filename );
	return NULL;
}

void CreamResource::ProxyCallback() const
{
	daemonCore->Reset_Timer( delegationTimerId, 0 );
}

void CreamResource::checkDelegation()
{
dprintf(D_FULLDEBUG,"*** checkDelegation()\n");
	bool signal_jobs;
	CreamProxyDelegation *next_deleg;
	time_t now = time( NULL );
	
	if ( deleg_gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying checkDelegation\n" );
		daemonCore->Reset_Timer( delegationTimerId, 5 );
		return;
	}

	daemonCore->Reset_Timer( delegationTimerId, CHECK_DELEGATION_INTERVAL );

	delegatedProxies.Rewind();

	while ( (next_deleg = delegatedProxies.Next()) != NULL ) {
		if ( activeDelegationCmd != NULL && next_deleg != activeDelegationCmd ) {
			continue;
		}
		signal_jobs = false;

		if ( next_deleg->deleg_uri == NULL ) {
dprintf(D_FULLDEBUG,"    new delegation\n");
			int rc;
		
				/* TODO generate better id */
			if ( delegation_uri == "" ) {
#ifdef WIN32
				struct _timeb timebuffer;
				_ftime( &timebuffer );
				formatstr( delegation_uri, "%d.%d", timebuffer.time, timebuffer.millitm );
#else
				struct timeval tv;
				gettimeofday( &tv, NULL );
				formatstr( delegation_uri, "%d.%d", (int)tv.tv_sec, (int)tv.tv_usec );
#endif
			}

			deleg_gahp->setDelegProxy( next_deleg->proxy );

			rc = deleg_gahp->cream_delegate(delegationServiceUri,
											delegation_uri.c_str() );
			
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return;
			}
			if ( rc != 0 ) {
					// Failure, what to do?
				dprintf( D_ALWAYS, "delegate_credentials(%s) failed!\n",
						 delegationServiceUri );
				activeDelegationCmd = NULL;
				const char *err = deleg_gahp->getErrorString();
				if ( err ) {
					next_deleg->error_message = err;
				} else {
					next_deleg->error_message = "Failed to create proxy delegation";
				}
				signal_jobs = true;
			} else {
dprintf(D_FULLDEBUG,"      %s\n",delegation_uri.c_str());
				activeDelegationCmd = NULL;
					// we are assuming responsibility to free this
				next_deleg->deleg_uri = strdup( delegation_uri.c_str() );
				next_deleg->proxy_expire = next_deleg->proxy->expiration_time;
				next_deleg->lifetime = now + 12*60*60;
				next_deleg->error_message = "";
				signal_jobs = true;
			}
			delegation_uri = "";
		}

		if ( next_deleg->deleg_uri &&
			 next_deleg->proxy_expire < next_deleg->proxy->expiration_time &&
			 now >= next_deleg->last_proxy_refresh + PROXY_REFRESH_INTERVAL ) {
dprintf(D_FULLDEBUG,"    refreshing %s\n",next_deleg->deleg_uri);
			int rc;
			deleg_gahp->setDelegProxy( next_deleg->proxy );
			
			rc = deleg_gahp->cream_proxy_renew(delegationServiceUri,
											   next_deleg->deleg_uri);
			
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return;
			}
			next_deleg->last_proxy_refresh = now;
			if ( rc != 0 ) {
					// Failure, what to do?
				dprintf( D_ALWAYS, "refresh_credentials(%s) failed!\n",
						 next_deleg->deleg_uri );
				activeDelegationCmd = NULL;
			} else {
				dprintf(D_FULLDEBUG,"      done\n");
				activeDelegationCmd = NULL;
				next_deleg->proxy_expire = next_deleg->proxy->expiration_time;
					// ??? do we want to signal jobs in this case?
				signal_jobs = true;
			}
		}
		
/*
		if ( next_deleg->deleg_uri &&
			 next_deleg->lifetime < now + 11*60*60 &&
			 now >= next_deleg->last_lifetime_extend + LIFETIME_EXTEND_INTERVAL ) {
dprintf(D_FULLDEBUG,"    extending %s\n",next_deleg->deleg_uri);
			int rc;
			time_t new_lifetime = 12*60*60;
			rc = deleg_gahp->gt4_set_termination_time( next_deleg->deleg_uri,
													   new_lifetime );
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return 0;
			}
			next_deleg->last_lifetime_extend = now;
			if ( rc != 0 ) {
					// Failure, what to do?
				dprintf( D_ALWAYS, "refresh_credentials(%s) failed!\n",
						 next_deleg->deleg_uri );
				activeDelegationCmd = NULL;
			} else {
dprintf(D_FULLDEBUG,"      done\n");
				activeDelegationCmd = NULL;
				next_deleg->lifetime = new_lifetime;
					// ??? do we want to signal jobs in this case?
				signal_jobs = false;
			}
		}
*/

		activeDelegationCmd = NULL;

		if ( signal_jobs ) {
dprintf(D_FULLDEBUG,"    signalling jobs for %s\n",next_deleg->deleg_uri?next_deleg->deleg_uri:"(undelegated)");
			CreamJob *next_job;
			registeredJobs.Rewind();
			while ( (next_job = (CreamJob*)registeredJobs.Next()) != NULL ) {
				if ( next_job->delegatedCredentialURI != NULL &&
					 next_deleg->deleg_uri != NULL ) {
					if ( strcmp( next_job->delegatedCredentialURI,
								 next_deleg->deleg_uri ) == 0 ) {
						next_job->ProxyCallback();
					}
				} else if ( next_job->jobProxy == next_deleg->proxy ) {
					next_job->ProxyCallback();
				}
			}
		}
	}
}

void CreamResource::DoPing( unsigned& ping_delay, bool& ping_complete,
							bool& ping_succeeded )
{
	int rc;

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_NEAR_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		ping_delay = TIMER_NEVER;
		return;
	}

	ping_delay = 0;

	rc = gahp->cream_ping( resourceName );
	
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


CreamResource::BatchStatusResult CreamResource::StartBatchStatus()
{
	ASSERT(status_gahp);

	GahpClient::CreamJobStatusMap results;
	int rc = status_gahp->cream_job_status_all(ResourceName(), results);
	if(rc == GAHPCLIENT_COMMAND_PENDING) { 
		return BSR_PENDING;
	}
	if(rc != 0) {
		dprintf(D_ALWAYS, "Error attempting a Cream batch status query: %d\n", rc);
		return BSR_ERROR;
	}
	for(GahpClient::CreamJobStatusMap::const_iterator it = results.begin();
		it != results.end(); it++) {

		const GahpClient::CreamJobStatus & status = it->second;

		std::string full_job_id = CreamJob::getFullJobId(ResourceName(), status.job_id.c_str());
		BaseJob * bjob = NULL;
		int rc2 = BaseJob::JobsByRemoteId.lookup(full_job_id, bjob);
		if(rc2 != 0) {
			// Job not found. Probably okay; we might see jobs
			// submitted via other means, or jobs we've abandoned.
			dprintf(D_FULLDEBUG, "Job %s on remote host is unknown. Skipping.\n", status.job_id.c_str());
			continue;
		}
		CreamJob * job = dynamic_cast<CreamJob *>(bjob);
		ASSERT(job);
		job->NewCreamState(status.job_status.c_str(), status.exit_code, 
			status.failure_reason.c_str());
		dprintf(D_FULLDEBUG, "%d.%d %s new status: %s, %d, %s\n", 
			job->procID.cluster, job->procID.proc,
			status.job_id.c_str(),
			status.job_status.c_str(), 
			status.exit_code, 
			status.failure_reason.c_str());
	}

#if DEBUG_CREAM
	{
		CreamJob *job;
		registeredJobs.Rewind();
		while ( (job = dynamic_cast<CreamJob*>(registeredJobs.Next())) != NULL ) {
			if(job->remoteJobId == NULL) { continue; }
			if(results.find(job->remoteJobId) == results.end()) {
				dprintf(D_FULLDEBUG, "%d.%d %s NOT updated\n",
					job->procID.cluster, job->procID.proc,
					job->remoteJobId);
			}
		}
	}
#endif
	return BSR_DONE;
}

CreamResource::BatchStatusResult CreamResource::FinishBatchStatus()
{
	// As it happens, we can use the same code path
	// for starting and finishing a batch status for
	// CREAM jobs.  Indeed, doing so is simpler.
	return StartBatchStatus();
}

GahpClient * CreamResource::BatchGahp() { return status_gahp; }

const char *CreamResource::getLeaseId()
{
	// TODO trigger a DoUpdateLeases() if we don't have a lease set yet
	if ( lastUpdateLeases ) {
		return m_leaseId.c_str();
	} else {
		return NULL;
	}
}

const char *CreamResource::getLeaseError()
{
	// TODO
	return NULL;
}

void CreamResource::DoUpdateSharedLease( unsigned& update_delay,
										 bool& update_complete,
										 bool& update_succeeded )
{
	int rc;
	time_t our_expiration;
	time_t server_expiration;
	BaseJob *curr_job;

	// TODO Should we worry about jobs having different lease ids?
	if ( m_leaseGahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying lease update\n" );
		update_delay = 5;
		return;
	}

	update_delay = 0;

	our_expiration = m_sharedLeaseExpiration;
	server_expiration = m_sharedLeaseExpiration;

	rc = m_leaseGahp->cream_set_lease( resourceName, m_leaseId.c_str(),
									   server_expiration );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		update_complete = false;
	} else if ( rc != 0 ) {
		dprintf( D_FULLDEBUG, "*** Lease update failed!\n" );
		const char *err = m_leaseGahp->getErrorString();
		if ( err ) {
			m_leaseErrorMsg = err;
		} else {
			m_leaseErrorMsg = "Failed to set lease";
		}
		update_complete = true;
		update_succeeded = false;
	} else {
		m_leaseErrorMsg = "";
		update_complete = true;
		update_succeeded = true;

		m_sharedLeaseExpiration = server_expiration;

		registeredJobs.Rewind();
		while ( registeredJobs.Next( curr_job ) ) {
			std::string tmp;
			if ( !curr_job->jobAd->LookupString( ATTR_GRID_JOB_ID, tmp ) ) {
				continue;
			}
			if ( our_expiration != server_expiration ) {
				curr_job->UpdateJobLeaseSent( server_expiration );
			}
		}
	}
}
