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
#include "util_lib_proto.h"
#include "env.h"
#include "directory.h"
#include "daemon.h"
#include "internet.h"
#include "my_username.h"
#include "globus_utils.h"

#include "proxymanager.h"
#include "gridmanager.h"

#include <algorithm>
#include <map>


std::map<std::string, Proxy *> ProxiesByFilename;
std::map<std::string, ProxySubject *> SubjectsByName;

static bool proxymanager_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;
static char *masterProxyDirectory = NULL;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value

static int next_proxy_id = 1;

void CheckProxies(int tid);

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

	for (Callback cb: master->m_callbacks) {
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
			  CallbackType func_ptr, Service *data  )
{
	if ( proxymanager_initialized == false ) {
		error = "Internal Error: ProxyManager not initialized";
		return NULL;
	}

	time_t expire_time;
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
			std::string s = iwd + DIR_DELIM_CHAR + proxy_path;
			proxy_path = s;
		}
	}

	auto it = ProxiesByFilename.find(proxy_path);
	if (it != ProxiesByFilename.end()) {
		proxy = it->second;
		// We already know about this proxy,
		// use the existing Proxy struct
		proxy->num_references++;
		if ( func_ptr ) {
			Callback cb;
			cb.m_func_ptr = func_ptr;
			cb.m_data = data;
			if (proxy->m_callbacks.end() == 
					std::find(proxy->m_callbacks.begin(),
						proxy->m_callbacks.end(), cb)) {
				proxy->m_callbacks.push_back( cb );
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
			if (proxy->m_callbacks.end() == 
					std::find(proxy->m_callbacks.begin(),
						proxy->m_callbacks.end(), cb)) {
				proxy->m_callbacks.push_back( cb );
			}
		}

		ProxiesByFilename[proxy_path] = proxy;

		auto it = SubjectsByName.find(fqan);
		if (it != SubjectsByName.end()) {
			proxy_subject = it->second;
		} else {
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
			ProxiesByFilename[new_master->proxy_filename] = new_master;

			proxy_subject->master_proxy = new_master;

			SubjectsByName[proxy_subject->fqan] = proxy_subject;
		}

		proxy_subject->proxies.push_back(proxy);

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

	return proxy;
}

Proxy *
AcquireProxy( Proxy *proxy, CallbackType func_ptr, Service *data )
{
	proxy->num_references++;
	if ( func_ptr ) {
		Callback cb;
		cb.m_func_ptr = func_ptr;
		cb.m_data = data;
		if (proxy->m_callbacks.end() == 
				std::find(proxy->m_callbacks.begin(),
					proxy->m_callbacks.end(), cb)) {
			proxy->m_callbacks.push_back( cb );
		}
	}
	return proxy;
}

// Call this function to indicate that you are done with a Proxy previously
// acquired with AcquireProxy(). Do not delete the Proxy yourself. The
// ProxyManager code will take care of that for you. If you provided a
// notify_tid to AcquireProxy(), provide it again here.
void
ReleaseProxy( Proxy *proxy, CallbackType func_ptr, Service *data )
{
	if ( proxymanager_initialized == false || proxy == NULL ) {
		return;
	}

	proxy->num_references--;
	if ( func_ptr ) {
		Callback cb;
		cb.m_func_ptr = func_ptr;
		cb.m_data = data;
		auto it = std::find(proxy->m_callbacks.begin(), proxy->m_callbacks.end(), cb);
		if (it != proxy->m_callbacks.end()) {
			proxy->m_callbacks.erase(it);
		}
	}

	if ( proxy->num_references < 0 ) {
		dprintf( D_ALWAYS, "Reference count for proxy %s is negative!\n",
				 proxy->proxy_filename );
	}

	if ( proxy->num_references <= 0 ) {

		ProxySubject *proxy_subject = proxy->subject;

		if ( proxy != proxy_subject->master_proxy ) {
			DeleteProxy( proxy );
		}

			// TODO should this be moved into DeleteProxy()?
		if ( proxy_subject->proxies.empty() &&
			 proxy_subject->master_proxy->num_references <= 0 ) {

			// TODO shouldn't we be deleting the physical file for the
			//   master proxy, since we created it?
			ProxiesByFilename.erase(proxy_subject->master_proxy->proxy_filename);
			free( proxy_subject->master_proxy->proxy_filename );
			delete proxy_subject->master_proxy;

			SubjectsByName.erase(proxy_subject->fqan);
			free( proxy_subject->subject_name );
			if ( proxy_subject->email )
				free( proxy_subject->email );
			free( proxy_subject->fqan );
			free( proxy_subject->first_fqan );
			delete proxy_subject;
		}

	}

}

// Utility function to deep-delete the Proxy data structure
void DeleteProxy (Proxy *& proxy)
{
	ProxiesByFilename.erase(proxy->proxy_filename);

	std::erase(proxy->subject->proxies, proxy);

	if (proxy->proxy_filename) {
		free( proxy->proxy_filename );
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
void CheckProxies(int /* tid */)
{
	time_t now = time(NULL);
	time_t next_check = CheckProxies_interval + now;

	dprintf( D_FULLDEBUG, "Checking proxies\n" );

	for (auto& [key, curr_subject]: SubjectsByName) {

		Proxy *new_master = curr_subject->master_proxy;

		for (auto curr_proxy: curr_subject->proxies) {

			time_t new_expiration =
				x509_proxy_expiration_time( curr_proxy->proxy_filename );

			curr_proxy->near_expired =
				(curr_proxy->expiration_time - now) <= minProxy_time;

			if ( new_expiration > curr_proxy->expiration_time ) {

				curr_proxy->expiration_time = new_expiration;

				curr_proxy->near_expired =
					(curr_proxy->expiration_time - now) <= minProxy_time;

				for (Callback cb: curr_proxy->m_callbacks) {
					((cb.m_data)->*(cb.m_func_ptr))();
				}

				if ( curr_proxy->expiration_time > new_master->expiration_time ) {
					new_master = curr_proxy;
				}

			} else if ( curr_proxy->near_expired ) {

				for (Callback cb: curr_proxy->m_callbacks) {
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
