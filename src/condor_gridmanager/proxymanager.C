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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "condor_config.h"
#include "classad_hashtable.h"
#include "util_lib_proto.h"

#include "proxymanager.h"
#include "gahp-client.h"

#define HASH_TABLE_SIZE			500

#define TIMER_UNSET				-1

template class HashTable<HashKey, Proxy *>;
template class HashBucket<HashKey, Proxy *>;

HashTable <HashKey, Proxy *> ProxiesByPath( HASH_TABLE_SIZE,
											hashFunction );

static bool proxymanager_initialized = false;
static bool gahp_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;
char *MasterProxyPath;
Proxy MasterProxy;
static bool use_single_proxy = false;
static bool(*InitGahpFunc)(const char *) = NULL;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value

static GahpClient my_gahp;
static int next_gahp_proxy_id = 1;

int CheckProxies();

static bool
InitializeProxyManager()
{
	// proxy_filename will be filled in by UseSingleProxy() or
	// UseMultipleProxies() as appropriate
	MasterProxy.proxy_filename = NULL;
	MasterProxy.num_references = 0;
	MasterProxy.expiration_time = 0;
	MasterProxy.gahp_proxy_id = 0;
	MasterProxy.near_expired = true;

	CheckProxies_tid = daemonCore->Register_Timer( 1, CheckProxies_interval,
												   (TimerHandler)&CheckProxies,
												   "CheckProxies", NULL );

	if ( my_gahp.Startup() == false ) {
		dprintf( D_ALWAYS, "Error starting GAHP\n" );
		EXCEPT( "GAHP startup command failed!" );
	}

	proxymanager_initialized = true;

	return true;
}

static bool
SetMasterProxy( const Proxy *new_master )
{
	int rc;
	MyString tmp_file;

	if ( use_single_proxy == false ) {

		tmp_file.sprintf( "%s.tmp", MasterProxy.proxy_filename );

		rc = copy_file( new_master->proxy_filename, tmp_file.Value() );
		if ( rc != 0 ) {
			return false;
		}

		rc = rotate_file( tmp_file.Value(), MasterProxy.proxy_filename );
		if ( rc != 0 ) {
			unlink( tmp_file.Value() );
			return false;
		}

	}

	MasterProxy.expiration_time = new_master->expiration_time;
	MasterProxy.near_expired = new_master->near_expired;

	return true;
}

// Initialize the ProxyManager module in multi-proxy mode. This is its
// normal mode of operation. proxy_dir is the directory in which the
// module should place the "master" proxy file. init_gahp_func will be
// called once a valid proxy has been given to the ProxyManager. That
// function can then intialize the GAHP as it sees fit. The parameter
// proxy should be used in the INITIALIZE_FROM_FILE command. Note that
// ProxyManager will start the GAHP and cache proxies before the given
// function is called.
bool UseMultipleProxies( const char *proxy_dir,
						 bool(*init_gahp_func)(const char *proxy) )
{
	if ( proxymanager_initialized == true ) {
		return false;
	}

	use_single_proxy = false;
	InitGahpFunc = init_gahp_func;

	if ( InitializeProxyManager() == false ) {
		return false;
	}

	MyString buf;
	buf.sprintf( "%s/master_proxy", proxy_dir );
	MasterProxy.proxy_filename = strdup( buf.Value() );

	return true;
}	

// Initialize the ProxyManager module in single-proxy mode. This is for
// use with old (i.e. 6.4.x) schedds. proxy_path is the path to the single
// proxy that is to be used. init_gahp_func will be
// called once a valid proxy has been given to the ProxyManager. That
// function can then intialize the GAHP as it sees fit. The parameter
// proxy should be used in the INITIALIZE_FROM_FILE command. Note that
// ProxyManager will start the GAHP and cache proxies before the given
// function is called.
bool
UseSingleProxy( const char *proxy_path,
				bool(*init_gahp_func)(const char *proxy) )
{
	if ( proxymanager_initialized == true ) {
		return false;
	}

	use_single_proxy = true;
	InitGahpFunc = init_gahp_func;

	if ( InitializeProxyManager() == false ) {
		return false;
	}

	MasterProxy.proxy_filename = strdup( proxy_path );

	return true;
}

// An entity (e.g. GlobusJob, GlobusResource object) should call this
// function when it wants to use a proxy managed by ProxyManager. proxy_path
// is the path to the proxy it wants to use. notify_tid is a timer id that
// will be signalled when something interesting happens with the proxy
// (it's about to expire or has been refreshed). A Proxy struct will be
// returned. When the Proxy is no longer needed, ReleaseProxy() should be
// called with it. No blocking operations are performed, and the proxy will
// not be cached in the GAHP server when AcquireProxy() returns.
// If Proxy.gahp_proxy_id is set to a negative value, it's not ready for
// use yet with any GAHP commands. Once it is ready, Proxy.gahp_proxy_id
// will be set to set to the GAHP cache id (a non-negative number) and
// notify_tid will be signalled. If no notifications are desired, give a
// negative number for notify_tid or omit it. Note the the Proxy returned
// is a shared data-structure and shouldn't be delete'd or modified by
// the caller. If NULL is given for proxy_path, a refernce to the "master"
// proxy is returned.
Proxy *
AcquireProxy( const char *proxy_path, int notify_tid )
{
	if ( proxymanager_initialized == false ) {
		return NULL;
	}
	if ( use_single_proxy && proxy_path != NULL &&
		 strcmp( proxy_path, MasterProxy.proxy_filename ) ) {
		EXCEPT( "Different proxies used in single-proxy mode!" );
	}

	if ( proxy_path == NULL ) {
		MasterProxy.num_references++;
		if ( notify_tid > 0 &&
			 MasterProxy.notification_tids.IsMember( notify_tid ) == false ) {
			MasterProxy.notification_tids.Append( notify_tid );
		}
		return &MasterProxy;
	}

	int expire_time;
	Proxy *proxy;

	if ( ProxiesByPath.lookup( HashKey(proxy_path), proxy ) == 0 ) {
		// We already know about this proxy,
		// return the existing Proxy struct
		proxy->num_references++;
		if ( notify_tid > 0 &&
			 proxy->notification_tids.IsMember( notify_tid ) == false ) {
			proxy->notification_tids.Append( notify_tid );
		}
		return proxy;
	}

	// We don't know about this proxy yet,
	// create a new Proxy struct and signal CheckProxies
	expire_time = x509_proxy_expiration_time( proxy_path );
	if ( expire_time < 0 ) {
		dprintf( D_ALWAYS, "Something wrong with proxy %s\n", proxy_path );
		return NULL;
	}

	proxy = new Proxy;
	proxy->proxy_filename = strdup( proxy_path);
	proxy->num_references = 1;
	proxy->expiration_time = expire_time;
	proxy->near_expired = (expire_time - time(NULL)) <= minProxy_time;
	proxy->gahp_proxy_id = -1;
	if ( notify_tid > 0 &&
		 proxy->notification_tids.IsMember( notify_tid ) == false ) {
		proxy->notification_tids.Append( notify_tid );
	}

	ProxiesByPath.insert(HashKey(proxy_path), proxy);

	daemonCore->Reset_Timer( CheckProxies_tid, 0 );

	return proxy;
}

// Call this function to indicate that you are done with a Proxy previously
// acquired with AcquireProxy(). Do not delete the Proxy yourself. The
// ProxyManager code will take care of that for you. If you provided a
// notify_tid to AcquireProxy(), provide it again here.
void
ReleaseProxy( Proxy *proxy, int notify_tid )
{
	if ( proxymanager_initialized == false || proxy == NULL ) {
		return;
	}

	if ( proxy == &MasterProxy ) {
		MasterProxy.num_references--;
		MasterProxy.notification_tids.Delete( notify_tid );
		return;
	}

	Proxy *hashed_proxy;

	if ( ProxiesByPath.lookup( HashKey(proxy->proxy_filename),
							   hashed_proxy ) == 0 ) {
		if ( hashed_proxy != proxy ) {
			EXCEPT( "Different Proxy objects with same filename!" );
		}

		proxy->num_references--;
		proxy->notification_tids.Delete( notify_tid );

		if ( proxy->num_references == 0 ) {
			daemonCore->Reset_Timer( CheckProxies_tid, 0 );
		}
	} else {
		EXCEPT( "Released Proxy object isn't in hashtable!" );
	}
}

void doCheckProxies()
{
	if ( CheckProxies_tid != TIMER_UNSET ) {
		daemonCore->Reset_Timer( CheckProxies_tid, 0 );
	}
}

// Most of the heavy lifting occurs here. All interaction with the GAHP
// server (aside from startup) happens here. This function is called
// periodically to check for updated proxies. It can be called earlier
// if a new proxy shows up, an old proxy can be removed, or a proxy is
// about to expire.
int CheckProxies()
{
	Proxy *next_proxy;
	Proxy *new_master;
	int new_max_expire;
	int now = time(NULL);
	int next_check = CheckProxies_interval + now;
	int rc;
dprintf(D_FULLDEBUG,"CheckProxies called\n");

	if ( proxymanager_initialized == false ) {
		daemonCore->Reset_Timer( CheckProxies_tid, CheckProxies_interval );
		return TRUE;
	}

	// As we check our proxies, keep an eye out for a new master proxy.
	// The new master needs to be valid and have an expiration time at
	// 60 seconds longer than the current master proxy.
	new_master = NULL;
	new_max_expire = MasterProxy.expiration_time + 60;
	if ( new_max_expire < now + 180 ) {
		new_max_expire = now + 180;
	}

	ProxiesByPath.startIterations();

	while ( ProxiesByPath.iterate( next_proxy ) != 0 ) {

		// Remove any proxies that are no longer being used by anyone
		if ( next_proxy->num_references == 0 ) {
dprintf(D_FULLDEBUG,"  removing old proxy %d\n",next_proxy->gahp_proxy_id);
			ProxiesByPath.remove( HashKey(next_proxy->proxy_filename) );
			if ( my_gahp.uncacheProxy( next_proxy->gahp_proxy_id ) == false ) {
				EXCEPT( "GAHP uncache command failed!" );
			}
			free( next_proxy->proxy_filename );
			delete next_proxy;
			continue;
		}

		int new_expiration = x509_proxy_expiration_time( next_proxy->proxy_filename );
		// If the proxy is valid, and either it hasn't been cached in the
		// gahp_server yet or it's been updated, (re)cache it in the
		// gahp_server and notify everyone who cares.
		if ( new_expiration > now + 180 &&
			 ( next_proxy->gahp_proxy_id == -1 ||
			   new_expiration > next_proxy->expiration_time ) ) {

			if ( next_proxy->gahp_proxy_id == -1 ) {
				next_proxy->gahp_proxy_id = next_gahp_proxy_id++;
			}

			next_proxy->expiration_time = new_expiration;
			next_proxy->near_expired = false;

dprintf(D_FULLDEBUG,"  (re)caching proxy %d\n",next_proxy->gahp_proxy_id);
			if ( my_gahp.cacheProxyFromFile( next_proxy->gahp_proxy_id,
											 next_proxy->proxy_filename ) == false ) {
				// TODO is there a better way to react?
				EXCEPT( "GAHP cache command failed!" );
			}

			int tid;
			next_proxy->notification_tids.Rewind();
			while ( next_proxy->notification_tids.Next( tid ) ) {
				daemonCore->Reset_Timer( tid, 0 );
			}

		} 
		
		if ( new_expiration <= now + minProxy_time ) {
			// This proxy has expired or is about to expire. Mark it
			// as such and notify everyone who cares.
			if ( next_proxy->near_expired == false ) {
dprintf(D_FULLDEBUG,"  marking proxy %d as about to expire\n",next_proxy->gahp_proxy_id);
				next_proxy->near_expired = true;
				int tid;
				next_proxy->notification_tids.Rewind();
				while ( next_proxy->notification_tids.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}
				my_gahp.useCachedProxy( -1 );
			}
		}

		// Check if this proxy should become the new master proxy
		if ( next_proxy->expiration_time > new_max_expire ) {
			new_master = next_proxy;
			new_max_expire = next_proxy->expiration_time;
		}

		// If this proxy will expire before the next scheduled check,
		// reschedule the check for when this proxy is about to expire
		if ( next_proxy->expiration_time - minProxy_time < next_check &&
			 !next_proxy->near_expired ) {
			next_check = next_proxy->expiration_time - minProxy_time;
		}

	}

	// If we found a new master proxy, copy it to the master proxy location,
	// update the master Proxy struct, update the GAHP server, and notify
	// everyone who cares
	if ( new_master != NULL && SetMasterProxy( new_master ) == true ) {

dprintf(D_FULLDEBUG,"  proxy %d is now the master proxy\n",new_master->gahp_proxy_id);
		if ( gahp_initialized == false ) {
			// This is our first master proxy, perform the callback so that
			// the GAHP can be intialized with it
dprintf(D_FULLDEBUG,"  first master found, calling gahp init function\n");
			if ( (*InitGahpFunc)( MasterProxy.proxy_filename ) == false ) {
				EXCEPT( "GAHP initalization failed!" );
			}

			if ( my_gahp.cacheProxyFromFile( MasterProxy.gahp_proxy_id,
											 MasterProxy.proxy_filename ) == false ) {
				EXCEPT( "GAHP cache command failed!" );
			}

			my_gahp.setMode( GahpClient::blocking );

			gahp_initialized = true;
		} else {
			// Refresh the master proxy credentials in the GAHP
dprintf(D_FULLDEBUG,"  refreshing master proxy in gahp\n");
			rc = my_gahp.globus_gram_client_set_credentials( MasterProxy.proxy_filename );
			// TODO if set-credentials fails, what to do?
			if ( rc != 0 ) {
				dprintf( D_ALWAYS, "GAHP set credentails failed! rc=%d\n", rc);
			}
			if ( my_gahp.cacheProxyFromFile( MasterProxy.gahp_proxy_id,
											 MasterProxy.proxy_filename ) == false ) {
				EXCEPT( "GAHP cache command failed!" );
			}
		}

		int tid;
		MasterProxy.notification_tids.Rewind();
		while ( MasterProxy.notification_tids.Next( tid ) ) {
			daemonCore->Reset_Timer( tid, 0 );
		}
	}

	// Check if the master proxy is about to expire
	if ( MasterProxy.expiration_time <= now + minProxy_time &&
		 MasterProxy.near_expired == false ) {
		MasterProxy.near_expired = true;
		int tid;
		MasterProxy.notification_tids.Rewind();
		while ( MasterProxy.notification_tids.Next( tid ) ) {
			daemonCore->Reset_Timer( tid, 0 );
		}
	}

	// next_check is the absolute time of the next check, convert it to
	// a relative time (from now)
dprintf(D_FULLDEBUG,"  will call CheckProxies again in %d seconds\n",next_check-now);
	daemonCore->Reset_Timer( CheckProxies_tid, next_check - now );

	return TRUE;
}
