/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "condor_config.h"
#include "classad_hashtable.h"
#include "util_lib_proto.h"
#include "env.h"
#include "directory.h"
#include "daemon.h"
#include "internet.h"
#include "simplelist.h"
#include "my_username.h"


#include "proxymanager.h"
//#include "myproxy_manager.h"
#include "gahp-client.h"

#define HASH_TABLE_SIZE			500

#define TIMER_UNSET				-1

template class HashTable<HashKey, Proxy *>;
template class HashBucket<HashKey, Proxy *>;


template class SimpleList<MyProxyEntry*>;


//template class HashTable<HashKey, MyProxyManager *>;
//template class HashBucket<HashKey, MyProxyManager *>;

HashTable <HashKey, Proxy *> ProxiesByPath( HASH_TABLE_SIZE,
											hashFunction );
//HashTable <HashKey, MyProxyManager *> MyProxyManagersByPath ( HASH_TABLE_SIZE, hashFunction );

//MyProxyManager myProxyManager;

static bool proxymanager_initialized = false;
static bool gahp_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;
char *MasterProxyPath;
Proxy MasterProxy;
static bool use_single_proxy = false;
static bool(*InitGahpFunc)(const char *) = NULL;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value
int myproxyGetDelegationReaperId = 0;


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

// Set the host:port of the MyProxy server for a given proxy
// Proxymanager will talk to the given server when the proxy is about to expire
/*int SetMyProxyHostForProxy ( const char* hostport, const char* server_dn, const char* myproxy_pwd, const Proxy * proxy ) {
	if (hostport == NULL || proxy == NULL) {
		return FALSE;
	}

	if (myProxyManager.AddMyProxyEntry (hostport, server_dn, myproxy_pwd, proxy->proxy_filename)) {

		dprintf (D_ALWAYS, "Created MyProxy manager (%s) for %s\n", hostport, proxy->proxy_filename);

		return TRUE;
	}

	return FALSE;
}*/

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

void DeleteMyProxyEntry (MyProxyEntry *& myproxy_entry) {
	if (myproxy_entry->get_delegation_pid != FALSE) {
		// Kill the process
		//daemonCore->Shutdown_Graceful (proxy->myproxy_entry->get_delegation_pid);
		myproxy_entry->get_delegation_pid=FALSE;
	}

	if (myproxy_entry->myproxy_host) {
		free (myproxy_entry->myproxy_host);
	}

	if (myproxy_entry->myproxy_server_dn) {
		free (myproxy_entry->myproxy_server_dn);
	}

	if (myproxy_entry->myproxy_password) {
		free (myproxy_entry->myproxy_password);
	}

	if (myproxy_entry->myproxy_credential_name) {
		free (myproxy_entry->myproxy_credential_name);
	}

	if (myproxy_entry->get_delegation_err_filename) {
		free (myproxy_entry->get_delegation_err_filename);
	}

	if (myproxy_entry->get_delegation_password_pipe[0] > -1) {
		close (myproxy_entry->get_delegation_password_pipe[0]);
	}

	if (myproxy_entry->get_delegation_password_pipe[1] > -1) {
		close (myproxy_entry->get_delegation_password_pipe[1]);
	}


}

// Utility function to deep-delete the Proxy data structure
void DeleteProxy (Proxy *& proxy) {

	if (proxy->proxy_filename) {
		free( proxy->proxy_filename );
	}

	MyProxyEntry * myproxy_entry;
	proxy->myproxy_entries.Rewind();
	proxy->myproxy_entries.Next(myproxy_entry);

	while (proxy->myproxy_entries.Current(myproxy_entry) == true) {
		DeleteMyProxyEntry (myproxy_entry);
		delete myproxy_entry;
	}

	delete proxy;
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

			// If myproxy-get-delegation is still running, don't delete this just yet
			next_proxy->myproxy_entries.Rewind();
			MyProxyEntry * mpe;
			int keep = FALSE;
			while (next_proxy->myproxy_entries.Next(mpe)) {
				if (mpe->get_delegation_pid != FALSE) {
					keep = TRUE;
					break;
				}
			}
			if (keep) {
				continue;
			}


dprintf(D_FULLDEBUG,"  removing old proxy %d\n",next_proxy->gahp_proxy_id);
			ProxiesByPath.remove( HashKey(next_proxy->proxy_filename) );
			if ( my_gahp.uncacheProxy( next_proxy->gahp_proxy_id ) == false ) {
				EXCEPT( "GAHP uncache command failed!" );
			}
			/*free( next_proxy->proxy_filename );
			delete next_proxy;*/
			DeleteProxy (next_proxy);
			continue;
		}

		/*

		This is moved to RefreshProxy

		// If this proxy is renewable via myproxy
		// and we don't have the password for it yet,
		// ask the Schedd

		next_proxy->myproxy_entries.Rewind();
		MyProxyEntry * mpe = NULL;
		while (next_proxy->myproxy_entries.Next(mpe)) {
			if (!mpe->myproxy_password) {
				if (!GetMyProxyPasswordFromSchedD (mpe->cluster_id,
													mpe->proc_id,
													&(mpe->myproxy_password))) {
					dprintf (D_ALWAYS,
						"Unable to retrieve MyProxy password from SchedD for proxy (job: %d.%d) %s\n",
						mpe->cluster_id,
						mpe->proc_id,
						next_proxy->proxy_filename);
					// This is unfortunate, let's hope the proxy doesn't expire before we finish the job
				}
			}
		}*/

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


		// Check whether to renew the proxy (renew 4 hrs beforehand)
		if ( (new_expiration <= now + 4*60*60) && (!next_proxy->myproxy_entries.IsEmpty()) ) {
			dprintf (D_FULLDEBUG, "About to RefreshProxyThruMyProxy() for %s\n", next_proxy->proxy_filename);
			RefreshProxyThruMyProxy (next_proxy);
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

int RefreshProxyThruMyProxy(Proxy * proxy) {

	char * proxy_filename = proxy->proxy_filename;
	MyProxyEntry * myProxyEntry = NULL;


	// Starting from the most recent myproxy entry
	// Find an entry with a password
	int found = FALSE;
	proxy->myproxy_entries.Rewind();
	while (proxy->myproxy_entries.Next (myProxyEntry)) {
		if (myProxyEntry->myproxy_password ||
			GetMyProxyPasswordFromSchedD (myProxyEntry->cluster_id,
												myProxyEntry->proc_id,
												&(myProxyEntry->myproxy_password))) {
			found=TRUE;

			//. Now move it to the front of the list
			proxy->myproxy_entries.DeleteCurrent();
			proxy->myproxy_entries.Prepend(myProxyEntry);
			break;
		}
	}

	if (!found) {
		// We're screwed - can't get MyProxy passwords for any entry
		return FALSE;
	}

  // Make sure we're not called more often than necessary and if
  time_t now=time(NULL);
  if ((myProxyEntry->get_delegation_pid != FALSE) || (now - myProxyEntry->last_invoked_time < 30)) {
  	dprintf (D_ALWAYS, "proxy %s too soon or myproxy-get-delegation already started\n",proxy_filename);
	return FALSE;
  }
  myProxyEntry->last_invoked_time=now;


  // If you don't have a myproxy password, ask SchedD for it
  if (!myProxyEntry->myproxy_password) {
  	// Will there ever be a case when there is no MyProxy password needed at all?
  	return FALSE;
  }

	// Initialize reaper, if needed
	if (myproxyGetDelegationReaperId == 0 ) {
		myproxyGetDelegationReaperId = daemonCore->Register_Reaper(
					   "GetDelegationReaper",
					   (ReaperHandler) &MyProxyGetDelegationReaper,
					   "GetDelegation Reaper");
  	}

	// Set up environnment for myproxy-get-delegation
	Env myEnv;
	char buff[_POSIX_PATH_MAX];

	if (myProxyEntry->myproxy_server_dn) {
		sprintf (buff, "MYPROXY_SERVER_DN=%s", myProxyEntry->myproxy_server_dn);
		myEnv.Put (buff);
		dprintf (D_FULLDEBUG, "%s\n", buff);
	}


	sprintf (buff,"X509_USER_PROXY=%s", proxy_filename);
	myEnv.Put (buff);
	dprintf (D_FULLDEBUG, "%s\n", buff);


    // Print password (this will end up in stdin for myproxy-get-delegation)
	pipe (myProxyEntry->get_delegation_password_pipe);
	write (myProxyEntry->get_delegation_password_pipe[1],
		myProxyEntry->myproxy_password,
		strlen (myProxyEntry->myproxy_password));
	write (myProxyEntry->get_delegation_password_pipe[1], "\n", 1);


	// Figure out user name;
	char * username = my_username(0);	


	// Figure out myproxy host and port
	char * myproxy_host = getHostFromAddr (myProxyEntry->myproxy_host);
	int myproxy_port = getPortFromAddr (myProxyEntry->myproxy_host);

	// args
	char args[1000];
	sprintf(args, "-v -o %s -s %s -d -t %d -S -l %s",
		proxy_filename,
		myproxy_host,
		12,
		username);


	// Optional port argument
	if (myproxy_port) {
		sprintf (buff, " -p %d ", myproxy_port);
		strcat (args, buff);
	}

	// Optional credential name argument
	if (myProxyEntry->myproxy_credential_name) {
		sprintf (buff, " -k %s ", myProxyEntry->myproxy_credential_name);
		strcat (args, buff);
	}

	free (username);
	free (myproxy_host);

	// Create temporary file to store myproxy-get-delegation's stderr
	myProxyEntry->get_delegation_err_filename = create_temp_file();
	chmod (myProxyEntry->get_delegation_err_filename, 0600);
	myProxyEntry->get_delegation_err_fd = open (myProxyEntry->get_delegation_err_filename,O_RDWR);
	if (myProxyEntry->get_delegation_err_fd == -1) {
		dprintf (D_ALWAYS, "Error opening file %s\n",myProxyEntry->get_delegation_err_filename);
	}


	int arrIO[3];
	arrIO[0]=myProxyEntry->get_delegation_password_pipe[0]; //stdin
	arrIO[1]=-1; //myProxyEntry->get_delegation_err_fd;
	arrIO[2]=myProxyEntry->get_delegation_err_fd; // stderr

	char * myproxy_get_delegation_pgm = param ("MYPROXY_GET_DELEGATION");
	if (!myproxy_get_delegation_pgm) {
		dprintf (D_ALWAYS, "MYPROXY_GET_DELEGATION not defined in config file\n");
		return FALSE;
	}


	dprintf (D_ALWAYS, "Calling %s %s\n", myproxy_get_delegation_pgm, args);

	int pid = daemonCore->Create_Process (
					myproxy_get_delegation_pgm,
					args,
					PRIV_USER_FINAL,
					myproxyGetDelegationReaperId,
					FALSE,
					myEnv.getDelimitedString(),
					NULL,	// cwd
					FALSE, // new proc group
					NULL,  // socket inherit
					arrIO); // in/out/err streams

	free (myproxy_get_delegation_pgm);

  	if (pid == FALSE) {
		dprintf (D_ALWAYS, "Failed to run myproxy-get-delegation\n");

		myProxyEntry->get_delegation_err_fd=-1;
		myProxyEntry->get_delegation_pid=FALSE;
		close (myProxyEntry->get_delegation_err_fd);
		unlink (myProxyEntry->get_delegation_err_filename);// Remove the temporary file
		free (myProxyEntry->get_delegation_err_filename);

		close (myProxyEntry->get_delegation_password_pipe[0]);
		close (myProxyEntry->get_delegation_password_pipe[1]);
		myProxyEntry->get_delegation_password_pipe[0]=-1;
		myProxyEntry->get_delegation_password_pipe[1]=-1;

		return FALSE;
	}

  myProxyEntry->get_delegation_pid = pid;

  return TRUE;

}


int MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus) {

	// Find the right MyProxyEntry
	Proxy *proxy=NULL;
	MyProxyEntry *matched_entry=NULL;
	int found = FALSE;

	// Iterate through each proxy
	ProxiesByPath.startIterations();
	while ( ProxiesByPath.iterate( proxy ) != 0 ) {
		// Iterate through all myproxy entries for the proxy
		proxy->myproxy_entries.Rewind();
		while (proxy->myproxy_entries.Next(matched_entry)) {
			if (matched_entry->get_delegation_pid == exitPid) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			break;
		}
	}

 	if (!found) {
		dprintf (D_ALWAYS, "WEIRD! MyProxyManager::GetDelegationReaper unable to find entry for pid %d", exitPid);
		return FALSE;
	}

	if (exitStatus == 0) {
		dprintf (D_ALWAYS, "myproxy-get-delegation for proxy %s exited successfully\n", proxy->proxy_filename);
		close (matched_entry->get_delegation_err_fd);
	} else {
		// This myproxyEntry is no good, move it to the back of the list
		MyProxyEntry * myProxyEntry = NULL;
		proxy->myproxy_entries.Rewind();
		if (proxy->myproxy_entries.Next (myProxyEntry)) {
			proxy->myproxy_entries.DeleteCurrent();
			proxy->myproxy_entries.Append (myProxyEntry);
		}

		// In the case of an error, append the stderr stream of myproxy-get-delegation to log
		close (matched_entry->get_delegation_err_fd);

		char buff[500];
		buff[0]='\0';
		int fd = open (matched_entry->get_delegation_err_filename, O_RDONLY);
		if (fd != -1) {
			int bytes_read = read (fd, buff, 499);
			close (fd);
			buff[bytes_read]='\0';
		} else {
			dprintf (D_ALWAYS, "WEIRD! Cannot read err file %s\n", matched_entry->get_delegation_err_filename);
		}

		dprintf (D_ALWAYS, "myproxy-get-delegation for proxy %s, for job (%d,%d) exited with code %d, output (top):\n%s\n\n",
			proxy->proxy_filename,
			matched_entry->cluster_id,
			matched_entry->proc_id,
			WEXITSTATUS(exitStatus),
			buff);

	}


	// Clean up
	close (matched_entry->get_delegation_password_pipe[0]);
	close (matched_entry->get_delegation_password_pipe[1]);
	matched_entry->get_delegation_password_pipe[0]=-1;
	matched_entry->get_delegation_password_pipe[1]=-1;

	matched_entry->get_delegation_err_fd=-1;
	matched_entry->get_delegation_pid=FALSE;
	unlink (matched_entry->get_delegation_err_filename);// Remove the temporary file
	free (matched_entry->get_delegation_err_filename);
	matched_entry->get_delegation_err_filename=NULL;

   return TRUE;
}

int GetMyProxyPasswordFromSchedD (int cluster_id, int proc_id, char ** password) {

	// This might seem not necessary, but it IS
	// For some reason you can't just pass cluster_id to sock->code() directly!!!!
	int cluster, proc;
	cluster = cluster_id;
	proc = proc_id;

	dprintf ( D_FULLDEBUG, " GetMyProxyPasswordFromSchedD %d, %d\n", cluster_id, proc_id);

	// Get At Schedd
	Daemon	schedd( DT_SCHEDD );
	if( ! schedd.locate() ) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Can't find address of local schedd\n" );
		return FALSE;
	}

	// Start command
	Sock* sock;
	if (!(sock = schedd.startCommand( GET_MYPROXY_PASSWORD, Stream::reli_sock, 0))) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Could not connect to local schedd\n" );
		return FALSE;
	}

	sock->encode();

	if (!sock->code (cluster) || !sock->code(proc)) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Could not encode clusterId, procId\n" );
		return FALSE;
	}

	sock->eom();
	sock->decode();

	if (!sock->code (*password)) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Can't retrieve password\n" );
		return FALSE;

	}

	sock->eom();
	sock->close();
	delete sock;
	return TRUE;
}
