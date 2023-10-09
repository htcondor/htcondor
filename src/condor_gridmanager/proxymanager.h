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



#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <vector>

typedef void (Service::*CallbackType)();

struct Callback {
	CallbackType m_func_ptr;
	Service *m_data;
	bool operator==(const Callback &rhs) const {
		if ( this->m_func_ptr == rhs.m_func_ptr && this->m_data == rhs.m_data ) {
			return true;
		}
		return false;
	}
};

struct ProxySubject;

struct Proxy {
	char *proxy_filename;
	time_t expiration_time;
	bool near_expired;
	int id;
	int num_references;
	std::vector<Callback> m_callbacks;
	
	ProxySubject *subject;
};

struct ProxySubject {
	char *fqan;
	char *first_fqan;
	char *subject_name;
	char *email;
	bool has_voms_attrs;
	Proxy *master_proxy;
	std::vector<Proxy*> proxies;
};

#define PROXY_NEAR_EXPIRED( p ) \
    ((p)->expiration_time - time(NULL) <= minProxy_time)
#define PROXY_IS_EXPIRED( p ) \
    ((p)->expiration_time - time(NULL) <= 180)
#define PROXY_IS_VALID( p ) \
    ((p)->expiration_time >= 0)

extern int CheckProxies_interval;
extern int minProxy_time;

bool InitializeProxyManager( const char *proxy_dir );
void ReconfigProxyManager();

Proxy *AcquireProxy( const ClassAd *job_ad, std::string &error,
					 CallbackType func_ptr = NULL, Service *data = NULL );
Proxy *AcquireProxy( Proxy *proxy, CallbackType func_ptr = NULL, Service *data = NULL );
void ReleaseProxy( Proxy *proxy, CallbackType func_ptr = NULL, Service *data = NULL );
void DeleteProxy (Proxy *& proxy);

void doCheckProxies();

#endif // defined PROXYMANAGER_H
