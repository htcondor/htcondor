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

#include "gt4resource.h"
#include "gt4job.h"
#include "gridmanager.h"

#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100
#define DEFAULT_MAX_WS_DESTROYS_PER_RESOURCE		5


int GT4Resource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

HashTable <HashKey, GT4Resource *>
    GT4Resource::ResourcesByName( HASH_TABLE_SIZE,
								  hashFunction );

#define CHECK_DELEGATION_INTERVAL	300
#define LIFETIME_EXTEND_INTERVAL	300
#define PROXY_REFRESH_INTERVAL		300

struct GT4ProxyDelegation {
	char *deleg_uri;
	time_t lifetime;
	time_t last_lifetime_extend;
	time_t proxy_expire;
	time_t last_proxy_refresh;
	Proxy *proxy;
	MyString error_message;
};

GT4Resource *GT4Resource::FindOrCreateResource( const char *resource_name,
												const char *proxy_subject )
{
	int rc;
	GT4Resource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	const char *hash_name = HashName( canonical_name, proxy_subject );
	ASSERT(hash_name);

	rc = ResourcesByName.lookup( HashKey( hash_name ), resource );
	if ( rc != 0 ) {
		resource = new GT4Resource( canonical_name, proxy_subject );
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

GT4Resource::GT4Resource( const char *resource_name,
						  const char *proxy_subject )
	: BaseResource( resource_name )
{
	initialized = false;
	proxySubject = strdup( proxy_subject );
	delegationTimerId = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&GT4Resource::checkDelegation,
							"GT4Resource::checkDelegation", (Service*)this );
	activeDelegationCmd = NULL;
	delegationServiceUri = NULL;
	gahp = NULL;
	deleg_gahp = NULL;
	m_isGram42 = false;

	const char service_name[] = "/wsrf/services/DelegationFactoryService";
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
										   sizeof( service_name ) + 
										   1 );        // terminating \0
	strcpy( delegationServiceUri, "https://" );
	snprintf( delegationServiceUri + 8, name_len + 1, "%s", name_ptr );
	strcat( delegationServiceUri, service_name );
}

GT4Resource::~GT4Resource()
{
dprintf(D_FULLDEBUG,"*** ~GT4Resource\n");
	GT4ProxyDelegation *next_deleg;
	delegatedProxies.Rewind();
	while ( (next_deleg = delegatedProxies.Next()) != NULL ) {
dprintf(D_FULLDEBUG,"    deleting %s\n",next_deleg->deleg_uri);
		delegatedProxies.DeleteCurrent();
		free( next_deleg->deleg_uri );
		ReleaseProxy( next_deleg->proxy,
					  (TimerHandlercpp)&GT4Resource::ProxyCallback, this );
		delete next_deleg;
	}
	if ( delegationServiceUri != NULL ) {
		free( delegationServiceUri );
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

bool GT4Resource::Init()
{
	if ( initialized ) {
		return true;
	}

		// TODO This assumes that at least one GT4Job has already
		// instantiated a GahpClient. Need a better solution.
/*
	MyString gahp_name;
	gahp_name.sprintf( "GT4/%s", proxySubject );

	gahp = new GahpClient( gahp_name.Value() );

	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	deleg_gahp = new GahpClient( gahp_name.Value() );

	deleg_gahp->setNotificationTimerId( delegationTimerId );
	deleg_gahp->setMode( GahpClient::normal );
	deleg_gahp->setTimeout( gahpCallTimeout );
*/
	if ( ConfigureGahp() == false ) {
		return false;
	}

	initialized = true;

	Reconfig();

	return true;
}

void GT4Resource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );

	destroyLimit = param_integer( "GRIDMANAGER_MAX_WS_DESTROYS_PER_RESOURCE",
								  DEFAULT_MAX_WS_DESTROYS_PER_RESOURCE );
	if ( destroyLimit == 0 ) {
		destroyLimit = GM_RESOURCE_UNLIMITED;
	}

	// If the destroyLimit was widened, move jobs from Wanted to Allowed and
	// signal them
	while ( destroysAllowed.Length() < destroyLimit &&
			destroysWanted.Length() > 0 ) {
		GT4Job *wanted_job = destroysWanted.Head();
		destroysWanted.Delete( wanted_job );
		destroysAllowed.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}
}

const char *GT4Resource::ResourceType()
{
	return "gt4";
}

const char *GT4Resource::CanonicalName( const char *name )
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

const char *GT4Resource::HashName( const char *resource_name,
								   const char *proxy_subject )
{
	static MyString hash_name;

	hash_name.sprintf( "gt4 %s#%s", resource_name, proxy_subject );

	return hash_name.Value();
}

const char *GT4Resource::GetHashName()
{
	return HashName( resourceName, proxySubject );
}

void GT4Resource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
}

void GT4Resource::UnregisterJob( GT4Job *job )
{
	if ( job->delegatedCredentialURI != NULL ) {
		bool delete_deleg = true;
		GT4Job *next_job;
		registeredJobs.Rewind();
		while ( (next_job = (GT4Job*)registeredJobs.Next()) != NULL ) {
			if ( strcmp( job->delegatedCredentialURI,
						 next_job->delegatedCredentialURI ) == 0 ) {
				delete_deleg = false;
				break;
			}
		}
		if ( delete_deleg ) {
dprintf(D_FULLDEBUG,"*** deleting delegation %s\n",job->delegatedCredentialURI);
			GT4ProxyDelegation *next_deleg;
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
								  (TimerHandlercpp)&GT4Resource::ProxyCallback, this );
					delete next_deleg;
				}
			}
		}
	}

	BaseResource::UnregisterJob( job );
		// This object may be deleted now. Don't do anything below here!
}

void GT4Resource::registerDelegationURI( const char *deleg_uri,
										 Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** registerDelegationURI(%s,%s)\n",deleg_uri,job_proxy->proxy_filename);
	GT4ProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( strcmp( deleg_uri, next_deleg->deleg_uri ) == 0 ) {
dprintf(D_FULLDEBUG,"    found GT4ProxyDelegation\n");
			return;
		}
	}

dprintf(D_FULLDEBUG,"    creating new GT4ProxyDelegation\n");
	next_deleg = new GT4ProxyDelegation;
	next_deleg->deleg_uri = strdup( deleg_uri );
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->last_lifetime_extend = 0;
	next_deleg->last_proxy_refresh = 0;
	next_deleg->proxy = job_proxy;
	AcquireProxy( job_proxy, (TimerHandlercpp)&GT4Resource::ProxyCallback, this );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );
}

const char *GT4Resource::getDelegationURI( Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** getDelegationURI(%s)\n",job_proxy->proxy_filename);
	GT4ProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( next_deleg->proxy == job_proxy ) {
				// If the delegation hasn't happened yet, this will return
				// NULL, which tells the caller to continue to wait.
dprintf(D_FULLDEBUG,"    found GT4ProxyDelegation\n");
			return next_deleg->deleg_uri;
		}
	}

dprintf(D_FULLDEBUG,"    creating new GT4ProxyDelegation\n");
	next_deleg = new GT4ProxyDelegation;
	next_deleg->deleg_uri = NULL;
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->last_lifetime_extend = 0;
	next_deleg->last_proxy_refresh = 0;
	next_deleg->proxy = job_proxy;
	AcquireProxy( job_proxy, (TimerHandlercpp)&GT4Resource::ProxyCallback, this );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );

	return NULL;
}

const char *GT4Resource::getDelegationError( Proxy *job_proxy )
{
	GT4ProxyDelegation *next_deleg;

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
			 "GT4ProxyDelegation for proxy %s\n", job_proxy->proxy_filename );
	return NULL;
}

int GT4Resource::ProxyCallback()
{
	daemonCore->Reset_Timer( delegationTimerId, 0 );
	return 0;
}

void GT4Resource::checkDelegation()
{
dprintf(D_FULLDEBUG,"*** checkDelegation()\n");
	bool signal_jobs;
	GT4ProxyDelegation *next_deleg;
	time_t now = time( NULL );

	if ( deleg_gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying checkDelegation\n" );
		daemonCore->Reset_Timer( delegationTimerId, 5 );
		return;
	}

	daemonCore->Reset_Timer( delegationTimerId, CHECK_DELEGATION_INTERVAL );

	if ( resourceDown || !firstPingDone ) {
		return;
	}

	delegatedProxies.Rewind();

	while ( (next_deleg = delegatedProxies.Next()) != NULL ) {

		if ( activeDelegationCmd != NULL && next_deleg != activeDelegationCmd ) {
			continue;
		}
		signal_jobs = false;

		if ( next_deleg->deleg_uri == NULL ) {
dprintf(D_FULLDEBUG,"    new delegation\n");
			int rc;
			char *delegation_uri = NULL;
			deleg_gahp->setDelegProxy( next_deleg->proxy );
			rc = deleg_gahp->gt4_gram_client_delegate_credentials(
														delegationServiceUri,
														&delegation_uri );
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
dprintf(D_FULLDEBUG,"      %s\n",delegation_uri);
				activeDelegationCmd = NULL;
					// we are assuming responsibility to free this
				next_deleg->deleg_uri = delegation_uri;
					// In the delgation service, the delegated proxy's
					// expiration is pegged to the resource's lifetime.
					// Right now, the gahp sets that to 12 hours. At some
					// point, it should be changed to let it default to
					// the full life of the proxy or allow the invoker
					// to set it as part of the resource creation.
				next_deleg->proxy_expire = next_deleg->proxy->expiration_time;
				if ( next_deleg->proxy_expire > now + 12*60*60 ) {
					next_deleg->proxy_expire = now + 12*60*60;
				}
				next_deleg->lifetime = now + 12*60*60;
				next_deleg->error_message = "";
				signal_jobs = true;
			}
		}

			// At the moment, the gahp always delegates a credential with
			// a lifetime no longer than 12 hours. So don't try refreshing
			// a delegated proxy with an expiration greater than 11 hours
			// from now.
		if ( next_deleg->deleg_uri &&
			 next_deleg->proxy_expire < next_deleg->proxy->expiration_time &&
			 next_deleg->proxy_expire < now + 11*60*60 &&
			 now >= next_deleg->last_proxy_refresh + PROXY_REFRESH_INTERVAL ) {
dprintf(D_FULLDEBUG,"    refreshing %s\n",next_deleg->deleg_uri);
			int rc;
			deleg_gahp->setDelegProxy( next_deleg->proxy );
			rc = deleg_gahp->gt4_gram_client_refresh_credentials(
													next_deleg->deleg_uri );
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
				if ( next_deleg->proxy_expire > now + 12*60*60 ) {
					next_deleg->proxy_expire = now + 12*60*60;
				}
					// ??? do we want to signal jobs in this case?
				signal_jobs = true;
			}
		}

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
				return;
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

		activeDelegationCmd = NULL;

		if ( signal_jobs ) {
dprintf(D_FULLDEBUG,"    signalling jobs for %s\n",next_deleg->deleg_uri?next_deleg->deleg_uri:"(undelegated)");
			GT4Job *next_job;
			registeredJobs.Rewind();
			while ( (next_job = (GT4Job*)registeredJobs.Next()) != NULL ) {
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

bool GT4Resource::RequestDestroy( GT4Job *job )
{
	if ( m_isGram42 ) {
		return true;
	}

	GT4Job *jobptr;

	destroysWanted.Rewind();
	while ( destroysWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	destroysAllowed.Rewind();
	while ( destroysAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	if ( destroysAllowed.Length() < destroyLimit &&
		 destroysWanted.Length() > 0 ) {
		EXCEPT("In GT4Resource for %s, destroysWanted is not empty and "
			   "destroysAllowed is not full\n",resourceName);
	}

	if ( destroysAllowed.Length() < destroyLimit ) {
		destroysAllowed.Append( job );
		return true;
	} else {
		destroysWanted.Append( job );
		return false;
	}
}

void GT4Resource::DestroyComplete( GT4Job *job )
{
	if ( m_isGram42 ) {
		return;
	}

	if ( destroysAllowed.Delete( job ) ) {
		if ( destroysAllowed.Length() < destroyLimit &&
			 destroysWanted.Length() > 0 ) {

			GT4Job *queued_job = destroysWanted.Head();
			destroysWanted.Delete( queued_job );
			destroysAllowed.Append( queued_job );
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check destroysWanted if the job wasn't in
		// destroysAllowed.
		destroysWanted.Delete( job );
	}

	return;
}

void GT4Resource::DoPing( time_t& ping_delay, bool& ping_complete,
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

	rc = gahp->gt4_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc == 0 ) {
		ping_complete = true;
		ping_succeeded = true;
	} else {
		if ( m_isGram42 == false &&
			 strncmp( gahp->getErrorString(), "Addressing header is a "
					  "draft version of WS Addressing:", 54 ) == 0 ) {

			m_isGram42 = true;
			if ( ConfigureGahp() ) {
				ping_delay = 0;
				ping_complete = false;
				daemonCore->Reset_Timer( pingTimerId, 0 );
			} else {
				m_isGram42 = false;
				ping_complete = true;
				ping_succeeded = true;
			}
		} else if ( strncmp( gahp->getErrorString(),
							 "java.net.ConnectException:", 26 ) == 0 ) {
			ping_complete = true;
			ping_succeeded = false;
		} else {
			ping_complete = true;
			ping_succeeded = true;
		}
	}
	if ( ping_complete && ping_succeeded ) {
			// The proxy delegation code may be waiting for a successful
			// ping. Wake it up.
		daemonCore->Reset_Timer( delegationTimerId, 0 );
	}
}

bool
GT4Resource::ConfigureGahp()
{
	GahpClient *new_gahp = NULL;
	GahpClient *new_deleg_gahp = NULL;

dprintf(D_ALWAYS,"JEF: ConfigureGahp()\n");
	if ( m_isGram42 && !registeredJobs.IsEmpty() ) {
dprintf(D_ALWAYS,"JEF: Telling jobs to switch to Gram 42\n");
		bool failed = true;
		GT4Job *next_job;
		registeredJobs.Rewind();
		while ( (next_job = (GT4Job*)registeredJobs.Next()) != NULL ) {
			bool result = next_job->SwitchToGram42();
			if ( result ) {
				failed = false;
			}
		}
		if ( failed ) {
			return false;
		}
	}

	MyString gahp_name;
	if ( m_isGram42 ) {
		gahp_name.sprintf( "GT42/%s", proxySubject );
	} else {
		gahp_name.sprintf( "GT4/%s", proxySubject );
	}

	new_gahp = new GahpClient( gahp_name.Value() );

	new_gahp->setNotificationTimerId( pingTimerId );
	new_gahp->setMode( GahpClient::normal );
	new_gahp->setTimeout( gahpCallTimeout );

	new_deleg_gahp = new GahpClient( gahp_name.Value() );

	new_deleg_gahp->setNotificationTimerId( delegationTimerId );
	new_deleg_gahp->setMode( GahpClient::normal );
	new_deleg_gahp->setTimeout( gahpCallTimeout );

	delete gahp;
	gahp = new_gahp;
	delete deleg_gahp;
	deleg_gahp = new_deleg_gahp;

	return true;
}
