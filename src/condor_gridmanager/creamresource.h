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


#ifndef CREAMRESOURCE_H
#define CREAMRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"

#include "proxymanager.h"
#include "baseresource.h"
#include "gahp-client.h"


class CreamJob;
class CreamResource;
struct CreamProxyDelegation;

class CreamResource : public BaseResource
{
 protected:
	CreamResource( const char *resource_name, const char *proxy_subject );
	~CreamResource();

 public:
	bool Init();
	const char *ResourceType();
	void Reconfig();
	void UnregisterJob( CreamJob *job );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	void registerDelegationURI( const char *deleg_uri, Proxy *job_proxy );
	const char *getDelegationURI( Proxy *job_proxy );
	const char *getDelegationError( Proxy *job_proxy );
	const char *getDelegationService() { return delegationServiceUri; };

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static CreamResource *FindOrCreateResource( const char *resource_name,
												const char *proxy_subject );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	int ProxyCallback();

	// This should be private, but CreamJob references it directly for now
	static HashTable <HashKey, CreamResource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );
	void checkDelegation();

	bool initialized;

	char *proxySubject;
	int delegationTimerId;
	CreamProxyDelegation *activeDelegationCmd;
	char *serviceUri;
	char *delegationServiceUri;
	List<CreamProxyDelegation> delegatedProxies;
	static int gahpCallTimeout;
	MyString delegation_uri;
	GahpClient *gahp;
	GahpClient *deleg_gahp;
};

#endif
