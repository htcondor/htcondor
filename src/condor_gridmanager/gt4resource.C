/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "gt4resource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

#define LOG_FILE_TIMEOUT		300

template class List<ProxyDelegation>;
template class Item<ProxyDelegation>;

int GT4Resource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

HashTable <HashKey, GT4Resource *>
    GT4Resource::ResourcesByName( HASH_TABLE_SIZE,
								  hashFunction );

struct ProxyDelegation {
	char *deleg_uri;
	time_t lifetime;
	time_t proxy_expire;
	Proxy *proxy;
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
							"GT4REsource::checkDelegation", (Service*)this );
	activeDelegationCmd = NULL;
	delegationServiceUri = NULL;
	gahp = NULL;
	deleg_gahp = NULL;
	submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

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
	ProxyDelegation *next_deleg;
	delegatedProxies.Rewind();
	while ( (next_deleg = delegatedProxies.Next()) != NULL ) {
dprintf(D_FULLDEBUG,"    deleting %s\n",next_deleg->deleg_uri);
		delegatedProxies.DeleteCurrent();
		free( next_deleg->deleg_uri );
			// unacquire proxy?
		ReleaseProxy( next_deleg->proxy );
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
		// initialized the gahp server. Need a better solution.
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

	initialized = true;

	Reconfig();

	return true;
}

void GT4Resource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );
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

	hash_name.sprintf( "%s#%s", resource_name, proxy_subject );

	return hash_name.Value();
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
			bool reacquire_proxy = false;
			Proxy *proxy_to_reacquire = job->jobProxy;
			ProxyDelegation *next_deleg;
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
						// unacquire proxy?
					ReleaseProxy( next_deleg->proxy );
					delete next_deleg;
				} else if ( next_deleg->proxy == proxy_to_reacquire ) {
					reacquire_proxy = true;
				}
			}
			if ( reacquire_proxy ) {
				AcquireProxy( proxy_to_reacquire, delegationTimerId );
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
	ProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( strcmp( deleg_uri, next_deleg->deleg_uri ) == 0 ) {
dprintf(D_FULLDEBUG,"    found ProxyDelegation\n");
			return;
		}
	}

dprintf(D_FULLDEBUG,"    creating new ProxyDelegation\n");
	next_deleg = new ProxyDelegation;
	next_deleg->deleg_uri = strdup( deleg_uri );
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->proxy = job_proxy;
		// acquire proxy?
	AcquireProxy( job_proxy, delegationTimerId );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );
}

const char *GT4Resource::getDelegationURI( Proxy *job_proxy )
{
dprintf(D_FULLDEBUG,"*** getDelegationURI(%s)\n",job_proxy->proxy_filename);
	ProxyDelegation *next_deleg;

	delegatedProxies.Rewind();

	while ( ( next_deleg = delegatedProxies.Next() ) != NULL ) {
		if ( next_deleg->proxy == job_proxy ) {
				// If the delegation hasn't happened yet, this will return
				// NULL, which tells the caller to continue to wait.
dprintf(D_FULLDEBUG,"    found ProxyDelegation\n");
			return next_deleg->deleg_uri;
		}
	}

dprintf(D_FULLDEBUG,"    creating new ProxyDelegation\n");
	next_deleg = new ProxyDelegation;
	next_deleg->deleg_uri = NULL;
	next_deleg->proxy_expire = 0;
	next_deleg->lifetime = 0;
	next_deleg->proxy = job_proxy;
		// acquire proxy?
	AcquireProxy( job_proxy, delegationTimerId );
	delegatedProxies.Append( next_deleg );

		// TODO add smarter timer that delays a few seconds
	daemonCore->Reset_Timer( delegationTimerId, 0 );

	return NULL;
}

int GT4Resource::checkDelegation()
{
dprintf(D_FULLDEBUG,"*** checkDelegation()\n");
	bool signal_jobs;
	ProxyDelegation *next_deleg;

	if ( deleg_gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying checkDelegation\n" );
		daemonCore->Reset_Timer( delegationTimerId, 5 );
		return 0;
	}

	daemonCore->Reset_Timer( delegationTimerId, 60*60 );

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
				return 0;
			}
			if ( rc != 0 ) {
					// Failure, what to do?
				dprintf( D_ALWAYS, "delegate_credentials(%s) failed!\n",
						 delegationServiceUri );
				activeDelegationCmd = NULL;
			} else {
dprintf(D_FULLDEBUG,"      %s\n",delegation_uri);
				activeDelegationCmd = NULL;
					// we are assuming responsibility to free this
				next_deleg->deleg_uri = delegation_uri;
				next_deleg->proxy_expire = next_deleg->proxy->expiration_time;
				next_deleg->lifetime = time(NULL) + 12*60*60;
				signal_jobs = true;
			}
		}

		if ( next_deleg->deleg_uri &&
			 next_deleg->proxy_expire < next_deleg->proxy->expiration_time ) {
dprintf(D_FULLDEBUG,"    refreshing %s\n",next_deleg->deleg_uri);
			int rc;
			deleg_gahp->setDelegProxy( next_deleg->proxy );
			rc = deleg_gahp->gt4_gram_client_refresh_credentials(
													next_deleg->deleg_uri );
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return 0;
			}
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

		if ( next_deleg->deleg_uri &&
			 next_deleg->lifetime < time(NULL) + 11*60*60 ) {
dprintf(D_FULLDEBUG,"    extending %s\n",next_deleg->deleg_uri);
			int rc;
			time_t new_lifetime = 12*60*60;
			rc = deleg_gahp->gt4_set_termination_time( next_deleg->deleg_uri,
													   new_lifetime );
			if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
				activeDelegationCmd = next_deleg;
				return 0;
			}
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
dprintf(D_FULLDEBUG,"    signalling jobs for %s\n",next_deleg->deleg_uri);
			GT4Job *next_job;
			registeredJobs.Rewind();
			while ( (next_job = (GT4Job*)registeredJobs.Next()) != NULL ) {
				if ( next_job->delegatedCredentialURI != NULL ) {
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
	return 0;
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
	} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}
