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
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "spooled_job_files.h"
#include "condor_config.h"
#include "HashTable.h"
#include "util_lib_proto.h"
#include "env.h"
#include "directory.h"
#include "daemon.h"
#include "internet.h"
#include "simplelist.h"
#include "my_username.h"
#include "globus_utils.h"

#include "proxymanager.h"
#include "gridmanager.h"

#include <sstream>


HashTable <std::string, Proxy *> ProxiesByFilename( hashFunction );
HashTable <std::string, ProxySubject *> SubjectsByName( hashFunction );

static bool proxymanager_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;
static char *masterProxyDirectory = NULL;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value
int myproxyGetDelegationReaperId = 0;

static int next_proxy_id = 1;

void CheckProxies();

static bool
SetMasterProxy( Proxy *master, const Proxy *copy_src )
{
	int rc;
	std::string tmp_file;

	formatstr( tmp_file, "%s.tmp", master->proxy_filename );

	rc = copy_file( copy_src->proxy_filename, tmp_file.c_str() );
	if ( rc != 0 ) {
		return false;
	}

	rc = rotate_file( tmp_file.c_str(), master->proxy_filename );
	if ( rc != 0 ) {
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink( tmp_file.c_str() );
		return false;
	}

	master->expiration_time = copy_src->expiration_time;
	master->near_expired = copy_src->near_expired;

	Callback cb;
	master->m_callbacks.Rewind();
	while ( master->m_callbacks.Next( cb ) ) {
		((cb.m_data)->*(cb.m_func_ptr))();
	}

	return true;
}

// Initialize the ProxyManager module. proxy_dir is the directory in
// which the module should place the "master" proxy files.
bool InitializeProxyManager( const char *proxy_dir )
{
	if ( proxymanager_initialized == true ) {
		return false;
	}

	CheckProxies_tid = daemonCore->Register_Timer( 1, CheckProxies_interval,
												   CheckProxies,
												   "CheckProxies" );

	masterProxyDirectory = strdup( proxy_dir );

	ReconfigProxyManager();

	proxymanager_initialized = true;

	return true;
}	

// Read config values from the config file. Call this function when the
// ProxyManager is first initialized and whenever a condor reconfig signal
// is received.
void ReconfigProxyManager()
{
	CheckProxies_interval = param_integer( "GRIDMANAGER_CHECKPROXY_INTERVAL",
										   10 * 60 );

	minProxy_time = param_integer( "GRIDMANAGER_MINIMUM_PROXY_TIME", 3 * 60 );

	// Always check the proxies on a reconfig.
	doCheckProxies();
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
// function when it wants to use a proxy and have it managed by
// ProxyManager. job_ad contains attributes related to the proxy to be
// used. error allows the function to return an error message if proxy
// acquisition fails. notify_tid is a timer id that
// will be signalled when something interesting happens with the proxy
// (it's about to expire or has been refreshed). A Proxy struct will be
// returned. When the Proxy is no longer needed, ReleaseProxy() should be
// called with it. If no notifications are desired, give a
// negative number for notify_tid or omit it. Note the the Proxy returned
// is a shared data-structure and shouldn't be delete'd or modified by
// the caller.
// If AcquireProxy() encounters an error, it will store an error message
// in the error parameter and return NULL. If the job ad doesn't contain
// any proxy-related attributes, AcquireProxy() will store the empty
// string in the error parameter and return NULL.
Proxy *
AcquireProxy( const ClassAd *job_ad, std::string &error,
			  TimerHandlercpp func_ptr, Service *data  )
{
	if ( proxymanager_initialized == false ) {
		error = "Internal Error: ProxyManager not initialized";
		return NULL;
	}

	int expire_time;
	Proxy *proxy = NULL;
	ProxySubject *proxy_subject = NULL;
	char *subject_name = NULL;
	char *fqan = NULL;
	char *email = NULL;
	char *first_fqan = NULL;
	std::string proxy_path;
	bool has_voms_attrs = false;

	if ( job_ad->LookupString( ATTR_X509_USER_PROXY, proxy_path ) == 0 ) {
		//sprintf( error, "%s is not set in the job ad", ATTR_X509_USER_PROXY );
		error = "";
		return NULL;
	}

	// If Condor-C submitted the job, the proxy_path is relative to the
	// spool directory.  For the purposes of this function, extend the
	// proxy path with the ATTR_JOB_IWD
	if (proxy_path[0] != DIR_DELIM_CHAR) {
		std::string iwd;
		job_ad->LookupString(ATTR_JOB_IWD, iwd);
		if (!iwd.empty()) {
			std::stringstream ss;
			ss << iwd << DIR_DELIM_CHAR << proxy_path;
			proxy_path = ss.str();
		}
	}

	if ( ProxiesByFilename.lookup( proxy_path, proxy ) == 0 ) {
		// We already know about this proxy,
		// use the existing Proxy struct
		proxy->num_references++;
		if ( func_ptr ) {
			Callback cb;
			cb.m_func_ptr = func_ptr;
			cb.m_data = data;
			if ( proxy->m_callbacks.IsMember( cb ) == false ) {
				proxy->m_callbacks.Append( cb );
			}
		}
		return proxy;

	} else {

		// We don't know about this proxy yet,
		// find the proxy's expiration time and subject name
		expire_time = x509_proxy_expiration_time( proxy_path.c_str() );
		if ( expire_time < 0 ) {
			dprintf( D_ALWAYS, "Failed to get expiration time of proxy %s: %s\n",
					 proxy_path.c_str(), x509_error_string() );
			formatstr( error, "Failed to get expiration time of proxy: %s",
					   x509_error_string() );
			return NULL;
		}
		subject_name = x509_proxy_identity_name( proxy_path.c_str() );
		if ( subject_name == NULL ) {
			dprintf( D_ALWAYS, "Failed to get identity of proxy %s: %s\n",
					 proxy_path.c_str(), x509_error_string() );
			formatstr( error, "Failed to get identity of proxy: %s",
					   x509_error_string() );
			return NULL;
		}

		email = x509_proxy_email( proxy_path.c_str() );

		fqan = NULL;
#if defined(HAVE_EXT_GLOBUS)
		int rc = extract_VOMS_info_from_file( proxy_path.c_str(), 0, NULL,
											  &first_fqan, &fqan );
		if ( rc != 0 && rc != 1 ) {
			dprintf( D_ALWAYS, "Failed to get voms info of proxy %s\n",
					 proxy_path.c_str() );
			error = "Failed to get voms info of proxy";
			free( subject_name );
			free( email );
			return NULL;
		}
#endif
		if ( fqan ) {
			has_voms_attrs = true;
		} else {
			fqan = strdup( subject_name );
		}

		// Create a Proxy struct for our new proxy and populate it
		proxy = new Proxy;
		proxy->proxy_filename = strdup(proxy_path.c_str());
		proxy->num_references = 1;
		proxy->expiration_time = expire_time;
		proxy->near_expired = (expire_time - time(NULL)) <= minProxy_time;
		proxy->id = next_proxy_id++;
		if ( func_ptr ) {
			Callback cb;
			cb.m_func_ptr = func_ptr;
			cb.m_data = data;
			if ( proxy->m_callbacks.IsMember( cb ) == false ) {
				proxy->m_callbacks.Append( cb );
			}
		}

		ProxiesByFilename.insert(proxy_path, proxy);

		if ( SubjectsByName.lookup( fqan, proxy_subject ) != 0 ) {
			// We don't know about this proxy subject yet,
			// create a new ProxySubject and fill it out
			std::string tmp;
			proxy_subject = new ProxySubject;
			proxy_subject->subject_name = strdup( subject_name );
			proxy_subject->email = email ? strdup( email ) : NULL;
			proxy_subject->fqan = strdup( fqan );
			proxy_subject->first_fqan = first_fqan ? strdup( first_fqan ) : NULL;
			proxy_subject->has_voms_attrs = has_voms_attrs;

			// Create a master proxy for our new ProxySubject
			Proxy *new_master = new Proxy;
			new_master->id = next_proxy_id++;
			formatstr( tmp, "%s/master_proxy.%d", masterProxyDirectory,
						 new_master->id );
			new_master->proxy_filename = strdup( tmp.c_str() );
			new_master->num_references = 0;
			new_master->subject = proxy_subject;
			SetMasterProxy( new_master, proxy );
			ASSERT( ProxiesByFilename.insert( new_master->proxy_filename,
			                                  new_master ) == 0 );

			proxy_subject->master_proxy = new_master;

			SubjectsByName.insert(proxy_subject->fqan,
								  proxy_subject);
		}

		proxy_subject->proxies.Append( proxy );

		proxy->subject = proxy_subject;

		// If the new Proxy is longer-lived than the current master proxy for
		// this subject, copy it for the new master.
		if ( proxy->expiration_time > proxy_subject->master_proxy->expiration_time ) {
			SetMasterProxy( proxy_subject->master_proxy, proxy );
		}

		free( subject_name );
		free( email );
		free( fqan );
		free( first_fqan );
	}

		// MyProxy crap
	std::string buff;
	if ( job_ad->LookupString( ATTR_MYPROXY_HOST_NAME, buff ) ) {

		int cluster;
		int proc;

		ASSERT( job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster ) );
		ASSERT( job_ad->LookupInteger( ATTR_PROC_ID, proc ) );

		MyProxyEntry * myProxyEntry =new MyProxyEntry();

		myProxyEntry->last_invoked_time=0;
		myProxyEntry->get_delegation_pid=FALSE;
		myProxyEntry->get_delegation_err_fd=-1;
		myProxyEntry->get_delegation_err_filename=NULL;
		myProxyEntry->myproxy_server_dn=NULL;
		myProxyEntry->myproxy_password=NULL;
		myProxyEntry->myproxy_credential_name=NULL;

		myProxyEntry->myproxy_host=strdup(buff.c_str());
		myProxyEntry->cluster_id = cluster;
		myProxyEntry->proc_id = proc;

		// Get optional MYPROXY_SERVER_DN attribute
		if (job_ad->LookupString (ATTR_MYPROXY_SERVER_DN, buff)) {
			myProxyEntry->myproxy_server_dn=strdup(buff.c_str());
		}

		if (job_ad->LookupString (ATTR_MYPROXY_CRED_NAME, buff)) {
			myProxyEntry->myproxy_credential_name=strdup(buff.c_str());
		}

		if (job_ad->LookupInteger (ATTR_MYPROXY_REFRESH_THRESHOLD, myProxyEntry->refresh_threshold)) {
			//myProxyEntry->refresh_threshold=atoi(buff);	// In minutes
			dprintf (D_FULLDEBUG, "MyProxy Refresh Threshold %d\n",myProxyEntry->refresh_threshold);
		} else {
			myProxyEntry->refresh_threshold = 4*60;	// default 4 hrs
			dprintf (D_FULLDEBUG, "MyProxy Refresh Threshold %d (default)\n",myProxyEntry->refresh_threshold);
		}

		if (job_ad->LookupInteger (ATTR_MYPROXY_NEW_PROXY_LIFETIME, myProxyEntry->new_proxy_lifetime)) {
			//myProxyEntry->new_proxy_lifetime=atoi(buff); // In hours
			dprintf (D_FULLDEBUG, "MyProxy New Proxy Lifetime %d\n",myProxyEntry->new_proxy_lifetime);
		} else {
			myProxyEntry->new_proxy_lifetime = 12; // default 12 hrs
			dprintf (D_FULLDEBUG, "MyProxy New Proxy Lifetime %d (default)\n",myProxyEntry->new_proxy_lifetime);
		}

		dprintf (D_FULLDEBUG,
				 "Adding new MyProxy entry for proxy %s : host=%s, cred name=%s\n",
				 proxy->proxy_filename,
				 myProxyEntry->myproxy_host,
				 (myProxyEntry->myproxy_credential_name!=NULL)?(myProxyEntry->myproxy_credential_name):"<default>");
		proxy->myproxy_entries.Prepend (myProxyEntry); // Add at the top of the list, so it'll be used first

		// See if we already have a MyProxy entry for the given host/credential name
		/*int found = FALSE;
		MyProxyEntry * currentMyProxyEntry = NULL;
		jobProxy->myproxy_entries.Rewind();
			while (jobProxy->myproxy_entries.Next (currentMyProxyEntry)) {
			if (strcmp (currentMyProxyEntry->myproxy_host, myProxyEntry->myproxy_host)) {
				continue;
			}
				if (myProxyEntry->myproxy_credential_name == NULL || myProxyEntry->myproxy_credential_name == NULL) {
				if (myProxyEntry->myproxy_credential_name != NULL || myProxyEntry->myproxy_credential_name != NULL) {
					// One credential name is NULL, the other is not
					continue;
				}
			} else {
				if (strcmp (currentMyProxyEntry->myproxy_credential_name, myProxyEntry->myproxy_credential_name))  {
					// credential names non-null and not-equal
					continue;
				}
			}

			// If we've gotten this far, we've got a match
			found = TRUE;
			break;
		}

		//... If we don't, insert it
		if (!found) {
			dprintf (D_FULLDEBUG,
					 "Adding new MyProxy entry for proxy %s : host=%s, cred name=%s\n",
					 jobProxy->proxy_filename,
					 myProxyEntry->myproxy_host,
					 (myProxyEntry->myproxy_credential_name!=NULL)?(myProxyEntry->myproxy_credential_name):"<default>");
			jobProxy->myproxy_entries.Append (myProxyEntry);
		} else {
			// No need to insert this
			delete myProxyEntry;
		}*/
	}

	return proxy;
}

Proxy *
AcquireProxy( Proxy *proxy, TimerHandlercpp func_ptr, Service *data )
{
	proxy->num_references++;
	if ( func_ptr ) {
		Callback cb;
		cb.m_func_ptr = func_ptr;
		cb.m_data = data;
		if ( proxy->m_callbacks.IsMember( cb ) == false ) {
			proxy->m_callbacks.Append( cb );
		}
	}
	return proxy;
}

// Call this function to indicate that you are done with a Proxy previously
// acquired with AcquireProxy(). Do not delete the Proxy yourself. The
// ProxyManager code will take care of that for you. If you provided a
// notify_tid to AcquireProxy(), provide it again here.
void
ReleaseProxy( Proxy *proxy, TimerHandlercpp func_ptr, Service *data )
{
	if ( proxymanager_initialized == false || proxy == NULL ) {
		return;
	}

	proxy->num_references--;
	if ( func_ptr ) {
		Callback cb;
		cb.m_func_ptr = func_ptr;
		cb.m_data = data;
		proxy->m_callbacks.Delete( cb );
	}

	if ( proxy->num_references < 0 ) {
		dprintf( D_ALWAYS, "Reference count for proxy %s is negative!\n",
				 proxy->proxy_filename );
	}

	if ( proxy->num_references <= 0 ) {

/* TODO If we're going to keep proxies around while myproxy delegation is
 *  still running, we need a way to clean them up later (either in
 *  CheckProxies() or MyProxyGetDelegationReaper()).
		// If myproxy-get-delegation is still running, don't delete this just yet
		proxy->myproxy_entries.Rewind();
		MyProxyEntry * mpe;
		int keep = FALSE;
		while (proxy->myproxy_entries.Next(mpe)) {
			if (mpe->get_delegation_pid != FALSE) {
				keep = TRUE;
				break;
			}
		}
*/

		ProxySubject *proxy_subject = proxy->subject;

		if ( proxy != proxy_subject->master_proxy ) {
			DeleteProxy( proxy );
		}

			// TODO should this be moved into DeleteProxy()?
		if ( proxy_subject->proxies.IsEmpty() &&
			 proxy_subject->master_proxy->num_references <= 0 ) {

			// TODO shouldn't we be deleting the physical file for the
			//   master proxy, since we created it?
			ProxiesByFilename.remove( proxy_subject->master_proxy->proxy_filename );
			free( proxy_subject->master_proxy->proxy_filename );
			delete proxy_subject->master_proxy;

			SubjectsByName.remove( proxy_subject->fqan );
			free( proxy_subject->subject_name );
			if ( proxy_subject->email )
				free( proxy_subject->email );
			free( proxy_subject->fqan );
			free( proxy_subject->first_fqan );
			delete proxy_subject;
		}

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
void DeleteProxy (Proxy *& proxy)
{
	ProxiesByFilename.remove( proxy->proxy_filename );

	proxy->subject->proxies.Delete( proxy );

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

// This function is called
// periodically to check for updated proxies. It can be called earlier
// if a proxy is about to expire.
void CheckProxies()
{
	int now = time(NULL);
	int next_check = CheckProxies_interval + now;
	ProxySubject *curr_subject;

	dprintf( D_FULLDEBUG, "Checking proxies\n" );

	SubjectsByName.startIterations();

	while ( SubjectsByName.iterate( curr_subject ) != 0 ) {

		Proxy *curr_proxy;
		Proxy *new_master = curr_subject->master_proxy;

		curr_subject->proxies.Rewind();

		while ( curr_subject->proxies.Next( curr_proxy ) != false ) {

			int new_expiration =
				x509_proxy_expiration_time( curr_proxy->proxy_filename );

			// Check whether to renew the proxy (need to check all myproxy entries)
			if (!curr_proxy->myproxy_entries.IsEmpty()) {
				curr_proxy->myproxy_entries.Rewind();
				MyProxyEntry * myProxyEntry=NULL;

				while (curr_proxy->myproxy_entries.Next (myProxyEntry) != false ) {
					if (new_expiration <= now + (myProxyEntry->refresh_threshold*60)) {
						dprintf (D_FULLDEBUG,
								"About to RefreshProxyThruMyProxy() for %s\n",
								curr_proxy->proxy_filename);
						RefreshProxyThruMyProxy (curr_proxy);
						break;
					}
				}
			}

			curr_proxy->near_expired =
				(curr_proxy->expiration_time - now) <= minProxy_time;

			if ( new_expiration > curr_proxy->expiration_time ) {

				curr_proxy->expiration_time = new_expiration;

				curr_proxy->near_expired =
					(curr_proxy->expiration_time - now) <= minProxy_time;

				Callback cb;
				curr_proxy->m_callbacks.Rewind();
				while ( curr_proxy->m_callbacks.Next( cb ) ) {
					((cb.m_data)->*(cb.m_func_ptr))();
				}

				if ( curr_proxy->expiration_time > new_master->expiration_time ) {
					new_master = curr_proxy;
				}

			} else if ( curr_proxy->near_expired ) {

				Callback cb;
				curr_proxy->m_callbacks.Rewind();
				while ( curr_proxy->m_callbacks.Next( cb ) ) {
					((cb.m_data)->*(cb.m_func_ptr))();
				}
			}

			if ( curr_proxy->expiration_time - minProxy_time < next_check &&
				 !curr_proxy->near_expired ) {
				next_check = curr_proxy->expiration_time - minProxy_time;
			}

		}

		if ( new_master != curr_subject->master_proxy ) {
			
			SetMasterProxy( curr_subject->master_proxy, new_master );

		}

	}

	// next_check is the absolute time of the next check, convert it to
	// a relative time (from now)
	if ( next_check < now ) {
		next_check = now;
	}
	daemonCore->Reset_Timer( CheckProxies_tid, next_check - now );
}

int RefreshProxyThruMyProxy(Proxy * proxy)
{
	char * proxy_filename = proxy->proxy_filename;
	MyProxyEntry * myProxyEntry = NULL;
	std::string args_string;
	int pid;

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
	if ((myProxyEntry->get_delegation_pid != FALSE) ||
		(now - myProxyEntry->last_invoked_time < 30)) {

		dprintf (D_ALWAYS,
			 "proxy %s too soon or myproxy-get-delegation already started\n",
				 proxy_filename);
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
					    &MyProxyGetDelegationReaper,
					   "GetDelegation Reaper");
 	}

	// Set up environnment for myproxy-get-delegation
	Env myEnv;
	std::string buff;

	if (myProxyEntry->myproxy_server_dn) {
		formatstr( buff, "MYPROXY_SERVER_DN=%s",
				 myProxyEntry->myproxy_server_dn);
		myEnv.SetEnv(buff.c_str());
		dprintf (D_FULLDEBUG, "%s\n", buff.c_str());
	}


	formatstr(buff, "X509_USER_PROXY=%s", proxy_filename);
	myEnv.SetEnv (buff.c_str());
	dprintf (D_FULLDEBUG, "%s\n", buff.c_str());


	// Print password (this will end up in stdin for myproxy-get-delegation)
	if (pipe (myProxyEntry->get_delegation_password_pipe)) {
		dprintf(D_ALWAYS,
				"Failed to pipe(2) in RefreshProxyThruMyProxy "
				"for writing password, aborting\n");
		return FALSE;
	}

	// Just to be sure
	if ((myProxyEntry->get_delegation_password_pipe[0] < 0) ||
		(myProxyEntry->get_delegation_password_pipe[1] < 0)) {

		dprintf(D_ALWAYS, "pipe(2) returned negative fds in RefreshProxyThruMyProxy, aborting\n");
	   return FALSE;
	}	   

	int written = write (myProxyEntry->get_delegation_password_pipe[1],
		   myProxyEntry->myproxy_password,
		   strlen (myProxyEntry->myproxy_password));
	if (written < (int) strlen (myProxyEntry->myproxy_password)) {
		dprintf(D_ALWAYS, "Failed to write to pipe in RefreshProxyThruMyProxy %d\n", errno);
		return FALSE;
    }
	written = write (myProxyEntry->get_delegation_password_pipe[1], "\n", 1);
	if (written < 1) {
		dprintf(D_ALWAYS, "Failed to write to pipe in RefreshProxyThruMyProxy %d\n", errno);
		return FALSE;
	}


	// Figure out user name;
	char * username = my_username(0);	


	// Figure out myproxy host and port
	char * myproxy_host = getHostFromAddr (myProxyEntry->myproxy_host);
	int myproxy_port = getPortFromAddr (myProxyEntry->myproxy_host);

	// args
	ArgList args;
	args.AppendArg(proxy_filename);
	args.AppendArg("-v");
	args.AppendArg("-o");
	args.AppendArg(proxy_filename);
	args.AppendArg("-s");
	args.AppendArg(myproxy_host);
	args.AppendArg("-d");
	args.AppendArg("-t");
	args.AppendArg(myProxyEntry->new_proxy_lifetime);
	args.AppendArg("-S");
	args.AppendArg("-l");
	args.AppendArg(username);


	// Optional port argument
	if (myproxy_port) {
		args.AppendArg("-p");
		args.AppendArg(myproxy_port);
	}

	// Optional credential name argument
	if (myProxyEntry->myproxy_credential_name) {
		args.AppendArg("-k");
		args.AppendArg(myProxyEntry->myproxy_credential_name);
	}

	free (username);
	free (myproxy_host);

	// Create temporary file to store myproxy-get-delegation's stderr
	myProxyEntry->get_delegation_err_filename = create_temp_file();
	if(!myProxyEntry->get_delegation_err_filename) {
		dprintf( D_ALWAYS, "Failed to create temp file\n");
	} else {
		int r = chmod (myProxyEntry->get_delegation_err_filename, 0600);
		if (r < 0) {
			dprintf( D_ALWAYS, "Failed to chmod %s to 0600, continuing anyway\n", myProxyEntry->get_delegation_err_filename);
		}
		myProxyEntry->get_delegation_err_fd = safe_open_wrapper_follow(myProxyEntry->get_delegation_err_filename,O_RDWR);
		if (myProxyEntry->get_delegation_err_fd == -1) {
			dprintf (D_ALWAYS, "Error opening file %s\n",
					 myProxyEntry->get_delegation_err_filename);
		}
	}


	int arrIO[3];
	arrIO[0]=myProxyEntry->get_delegation_password_pipe[0]; //stdin
	arrIO[1]=myProxyEntry->get_delegation_err_fd;
	arrIO[2]=myProxyEntry->get_delegation_err_fd; // stderr

	char * myproxy_get_delegation_pgm = param ("MYPROXY_GET_DELEGATION");
	if (!myproxy_get_delegation_pgm) {
		dprintf (D_ALWAYS, "MYPROXY_GET_DELEGATION not defined in config file\n");
		goto error_exit;
	}


	args.GetArgsStringForDisplay(args_string);
	dprintf (D_ALWAYS, "Calling %s %s\n", myproxy_get_delegation_pgm, args_string.c_str());

	pid = daemonCore->Create_Process (
					myproxy_get_delegation_pgm,
					args,
					PRIV_USER_FINAL,
					myproxyGetDelegationReaperId,
					FALSE,
					FALSE,
					&myEnv,
					NULL,	// cwd
					NULL,  // process family info
					NULL,  // socket inherit
					arrIO); // in/out/err streams

	free (myproxy_get_delegation_pgm);

	if (pid == FALSE) {
		dprintf (D_ALWAYS, "Failed to run myproxy-get-delegation\n");
		goto error_exit;
	}

	myProxyEntry->get_delegation_pid = pid;

	return TRUE;

 error_exit:
	myProxyEntry->get_delegation_pid=FALSE;

	if (myProxyEntry->get_delegation_err_fd >= 0) {
		close (myProxyEntry->get_delegation_err_fd);
		myProxyEntry->get_delegation_err_fd=-1;
	}

	if (myProxyEntry->get_delegation_err_filename) {
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink (myProxyEntry->get_delegation_err_filename);// Remove the tempora
		free (myProxyEntry->get_delegation_err_filename);
		myProxyEntry->get_delegation_err_filename=NULL;
	}

	if (myProxyEntry->get_delegation_password_pipe[0] >= 0) {
		close (myProxyEntry->get_delegation_password_pipe[0]);
		myProxyEntry->get_delegation_password_pipe[0]=-1;
	}
	if (myProxyEntry->get_delegation_password_pipe[1] >= 0 ) {
		close (myProxyEntry->get_delegation_password_pipe[1]);
		myProxyEntry->get_delegation_password_pipe[1]=-1;
	}

	return FALSE;
}


int MyProxyGetDelegationReaper(int exitPid, int exitStatus)
{
	// Find the right MyProxyEntry
	Proxy *proxy=NULL;
	MyProxyEntry *matched_entry=NULL;
	int found = FALSE;

	// Iterate through each proxy
	ProxiesByFilename.startIterations();
	while ( ProxiesByFilename.iterate( proxy ) != 0 ) {
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
		std::string output;
		int fd = safe_open_wrapper_follow(matched_entry->get_delegation_err_filename, O_RDONLY);
		if (fd != -1) {
			int bytes_read;
			do {
				bytes_read = read( fd, buff, 499 );
				if ( bytes_read > 0 ) {
					buff[bytes_read] = '\0';
					output += buff;
				} else if ( bytes_read < 0 ) {
					dprintf( D_ALWAYS, "WEIRD! Cannot read err file %s, "
							 "errno=%d (%s)\n",
							 matched_entry->get_delegation_err_filename,
							 errno, strerror( errno ) );
				}
			} while ( bytes_read > 0 );
			close (fd);
		} else {
			dprintf( D_ALWAYS, "WEIRD! Cannot open err file %s, "
					 "errno=%d (%s)\n",
					 matched_entry->get_delegation_err_filename,
					 errno, strerror( errno ) );
		}

		dprintf (D_ALWAYS, "myproxy-get-delegation for proxy %s, for job (%d.%d) exited with code %d, output (top):\n%s\n",
			proxy->proxy_filename,
			matched_entry->cluster_id,
			matched_entry->proc_id,
			WEXITSTATUS(exitStatus),
			output.c_str());

	}


	// Clean up
	close (matched_entry->get_delegation_password_pipe[0]);
	close (matched_entry->get_delegation_password_pipe[1]);
	matched_entry->get_delegation_password_pipe[0]=-1;
	matched_entry->get_delegation_password_pipe[1]=-1;

	matched_entry->get_delegation_err_fd=-1;
	matched_entry->get_delegation_pid=FALSE;
	MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
	unlink (matched_entry->get_delegation_err_filename);// Remove the temporary file
	free (matched_entry->get_delegation_err_filename);
	matched_entry->get_delegation_err_filename=NULL;

   return TRUE;
}

int GetMyProxyPasswordFromSchedD (int cluster_id, int proc_id,
								  char ** password)
{
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

	sock->end_of_message();
	sock->decode();

	if (!sock->code (*password)) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Can't retrieve password\n" );
		return FALSE;

	}

	sock->end_of_message();
	sock->close();
	delete sock;
	return TRUE;
}
