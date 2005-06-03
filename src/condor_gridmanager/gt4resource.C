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

template class List<GT4Job>;
template class Item<GT4Job>;
template class List<ProxyDelegation>;
template class Item<ProxyDelegation>;

int GT4Resource::probeInterval = 300;	// default value
int GT4Resource::probeDelay = 15;		// default value
int GT4Resource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, GT4Resource *>;
template class HashBucket<HashKey, GT4Resource *>;

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
	resourceDown = false;
	firstPingDone = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GT4Resource::DoPing,
								"GT4Resource::DoPing", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;
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

	daemonCore->Cancel_Timer( pingTimerId );
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
	char *param_value;

	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );

	submitLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS_PER_RESOURCE" );
	if ( param_value == NULL ) {
		// Check old parameter name
		param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS" );
	}
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			submitLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					submitLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( submitLimit <= 0 ) {
		submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	}

	jobLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
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
	// add them to Queued
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		GT4Job *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
//		submitsQueued.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		GT4Job *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
		queued_job->SetEvaluateState();
	}
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

void GT4Resource::RegisterJob( GT4Job *job, bool already_submitted )
{
	registeredJobs.Append( job );

	if ( already_submitted ) {
		submitsAllowed.Append( job );
	}

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}
}

void GT4Resource::UnregisterJob( GT4Job *job )
{
	CancelSubmit( job );
	registeredJobs.Delete( job );
	pingRequesters.Delete( job );

	if ( job->delegatedCredentialURI != NULL ) {
		bool delete_deleg = true;
		GT4Job *next_job;
		registeredJobs.Rewind();
		while ( (next_job = registeredJobs.Next()) != NULL ) {
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

	if ( IsEmpty() ) {
		ResourcesByName.remove( HashKey( HashName( resourceName, proxySubject ) ) );
		delete this;
	}
}

void GT4Resource::RequestPing( GT4Job *job )
{
	pingRequesters.Append( job );

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool GT4Resource::RequestSubmit( GT4Job *job )
{
	bool already_allowed = false;
	GT4Job *jobptr;

	submitsQueued.Rewind();
	while ( submitsQueued.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsAllowed.Rewind();
	while ( submitsAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			already_allowed = true;
			break;
		}
	}

	if ( already_allowed == false ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			EXCEPT("In GT4Resource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full\n",resourceName);
		}
		if ( submitsAllowed.Length() < jobLimit ) {
			submitsAllowed.Append( job );
			// proceed to see if submitLimit applies
		} else {
			submitsWanted.Append( job );
			return false;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In GT4Resource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n",resourceName);
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
		return true;
	} else {
		submitsQueued.Append( job );
		return false;
	}
}

void GT4Resource::SubmitComplete( GT4Job *job )
{
	if ( submitsInProgress.Delete( job ) ) {
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			GT4Job *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsQueued if the job wasn't in
		// submitsInProgress.
		submitsQueued.Delete( job );
	}

	return;
}

void GT4Resource::CancelSubmit( GT4Job *job )
{
	if ( submitsAllowed.Delete( job ) ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			GT4Job *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
//			submitsQueued.Append( wanted_job );
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		submitsWanted.Delete( job );
	}

	SubmitComplete( job );

	return;
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
			while ( (next_job = registeredJobs.Next()) != NULL ) {
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

bool GT4Resource::IsEmpty()
{
	return registeredJobs.IsEmpty();
}

bool GT4Resource::IsDown()
{
	return resourceDown;
}

int GT4Resource::DoPing()
{
	int rc;
	bool ping_failed = false;
	GT4Job *job;

	// Don't perform a ping if we have no requesters and the resource is up
	if ( pingRequesters.IsEmpty() && resourceDown == false &&
		 firstPingDone == true ) {
		daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );
		return TRUE;
	}

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
		return TRUE;
	}

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		daemonCore->Reset_Timer( pingTimerId, 5 );
		return TRUE;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_NEAR_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		return TRUE;
	}

	rc = gahp->gt4_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		return 0;
	}

	lastPing = time(NULL);

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
		ping_failed = true;
	}

	if ( ping_failed == resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
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
		dprintf(D_ALWAYS,"resource %s is now %s\n",resourceName,
				ping_failed?"down":"up");

		resourceDown = ping_failed;
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

	return 0;
}
