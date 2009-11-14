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

#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100


int CreamResource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

HashTable <HashKey, CreamResource *>
    CreamResource::ResourcesByName( HASH_TABLE_SIZE,
									hashFunction );

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
	MyString error_message;
};

CreamResource *CreamResource::FindOrCreateResource( const char *resource_name,
													const char *proxy_subject )
{
	int rc;
	CreamResource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	const char *hash_name = HashName( canonical_name, proxy_subject );
	ASSERT(hash_name);

	rc = ResourcesByName.lookup( HashKey( hash_name ), resource );
	if ( rc != 0 ) {
		resource = new CreamResource( canonical_name, proxy_subject );
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

CreamResource::CreamResource( const char *resource_name,
							  const char *proxy_subject )
	: BaseResource( resource_name )
{
	initialized = false;
	proxySubject = strdup( proxy_subject );
	delegationTimerId = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&CreamResource::checkDelegation,
							"CreamResource::checkDelegation", (Service*)this );
	activeDelegationCmd = NULL;
	delegationServiceUri = NULL;
	gahp = NULL;
	deleg_gahp = NULL;

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
	strcpy( delegationServiceUri, "https://" );
	snprintf( delegationServiceUri + 8, name_len + 1, "%s", name_ptr );
	strcat( delegationServiceUri, delegservice_name );

	const char service_name[] = "/ce-cream/services/CREAM";

	serviceUri = (char *)malloc( 8 +         // "https://"
								 name_len +  // host/port
								 sizeof( service_name ) + 
								 1 );        // terminating \0
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
dprintf(D_FULLDEBUG,"    deleting %s\n",next_deleg->deleg_uri);
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

	ResourcesByName.remove( HashKey( HashName( resourceName, proxySubject ) ) );

	daemonCore->Cancel_Timer( delegationTimerId );
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( deleg_gahp != NULL ) {
		delete deleg_gahp;
	}
	if ( proxySubject ) {
		free( proxySubject );
	}
}

bool CreamResource::Init()
{
	if ( initialized ) {
		return true;
	}

		// TODO This assumes that at least one CreamJob has already
		// initialized the gahp server. Need a better solution.
	MyString gahp_name;
	gahp_name.sprintf( "CREAM/%s", proxySubject );

	gahp = new GahpClient( gahp_name.Value() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	deleg_gahp = new GahpClient( gahp_name.Value() );

	deleg_gahp->setNotificationTimerId( delegationTimerId );
	deleg_gahp->setMode( GahpClient::normal );
	deleg_gahp->setTimeout( gahpCallTimeout );

	initialized = true;

	Reconfig();

	return true;
}

void CreamResource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );
}

const char *CreamResource::ResourceType()
{
	return "cream";
}

const char *CreamResource::CanonicalName( const char *name )
{
/*
	static MyString canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	canonical.sprintf( "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.Value();
*/
	return name;
}

const char *CreamResource::HashName( const char *resource_name,
									 const char *proxy_subject )
{
	static MyString hash_name;

	hash_name.sprintf( "cream %s#%s", resource_name, proxy_subject );

	return hash_name.Value();
}

void CreamResource::UnregisterJob( CreamJob *job )
{
	if ( job->delegatedCredentialURI != NULL ) {
		bool delete_deleg = true;
		CreamJob *next_job;
		registeredJobs.Rewind();
		while ( (next_job = (CreamJob*)registeredJobs.Next()) != NULL ) {
			if ( strcmp( job->delegatedCredentialURI,
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
				if ( strcmp( job->delegatedCredentialURI,
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
}

const char *CreamResource::GetHashName()
{
	return HashName( resourceName, proxySubject );
}

void CreamResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
}

void CreamResource::registerDelegationURI( const char *deleg_uri,
										 Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** registerDelegationURI(%s,%s)\n",deleg_uri,job_proxy->proxy_filename);
	CreamProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( strcmp( deleg_uri, next_deleg->deleg_uri ) == 0 ) {
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
			if ( next_deleg->error_message.IsEmpty() ) {
				return NULL;
			} else {
				return next_deleg->error_message.Value();
			}
		}
	}

	dprintf( D_FULLDEBUG, "getDelegationError(): failed to find "
			 "CreamProxyDelegation for proxy %s\n", job_proxy->proxy_filename );
	return NULL;
}

int CreamResource::ProxyCallback()
{
	daemonCore->Reset_Timer( delegationTimerId, 0 );
	return 0;
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
				struct timeval tv;
				gettimeofday( &tv, NULL );
				delegation_uri.sprintf( "%d.%d", tv.tv_sec, tv.tv_usec );
			}

			deleg_gahp->setDelegProxy( next_deleg->proxy );

			rc = deleg_gahp->cream_delegate(delegationServiceUri,
											delegation_uri.Value() );
			
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return;
			}
			if ( rc != 0 ) {
					// Failure, what to do?
				dprintf( D_ALWAYS, "delegate_credentials(%s) failed!\n",
						 delegationServiceUri );
				activeDelegationCmd = NULL;
				next_deleg->error_message = "Failed to create proxy delegation";
				signal_jobs = true;
			} else {
dprintf(D_FULLDEBUG,"      %s\n",delegation_uri.Value());
				activeDelegationCmd = NULL;
					// we are assuming responsibility to free this
				next_deleg->deleg_uri = strdup( delegation_uri.Value() );
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
			CreamJob *next_job;
			deleg_gahp->setDelegProxy( next_deleg->proxy );
			
				//create a list of jobIds using the proxy
			StringList job_ids;
			registeredJobs.Rewind();
			while ( (next_job = (CreamJob *)registeredJobs.Next()) != NULL) {
				if(next_job->remoteJobId != NULL) {
						//Make sure deleg URI matches w/ job's before adding to list
						//And make sure the job has been started and is not in terminal state.
					if ( strcmp( next_job->delegatedCredentialURI, next_deleg->deleg_uri ) == 0 &&
						 next_job->remoteState != CREAM_JOB_STATE_REGISTERED &&
						 next_job->remoteState != CREAM_JOB_STATE_CANCELLED &&
						 next_job->remoteState != CREAM_JOB_STATE_DONE_OK &&
						 next_job->remoteState != CREAM_JOB_STATE_DONE_FAILED &&
						 next_job->remoteState != CREAM_JOB_STATE_ABORTED) {
						job_ids.append(next_job->remoteJobId);
					}
				}
			}
			
			rc = deleg_gahp->cream_proxy_renew(serviceUri,
											   delegationServiceUri,
											   next_deleg->deleg_uri,
											   job_ids);
			
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return;
			}
			next_deleg->last_proxy_refresh = now;
			if ( rc != 0 ) {
					/* CREAM will return an error if a single job in the list fails to be
					   renewed. However, the rest of the jobs will be renewed successfully.
					*/
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
						next_job->SetEvaluateState();
					}
				} else if ( next_job->jobProxy == next_deleg->proxy ) {
					next_job->SetEvaluateState();
				}
			}
		}
	}
}

void CreamResource::DoPing( time_t& ping_delay, bool& ping_complete,
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
