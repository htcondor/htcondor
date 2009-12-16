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


#ifndef GT4RESOURCE_H
#define GT4RESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"

#include "proxymanager.h"
#include "baseresource.h"
#include "gahp-client.h"


class GT4Job;
class GT4Resource;
struct GT4ProxyDelegation;

class GT4Resource : public BaseResource
{
 protected:
	GT4Resource( const char *resource_name, const Proxy *proxy );
	~GT4Resource();

 public:
	bool Init();
	const char *ResourceType();
	void Reconfig();
	const char *GetHashName();
	void UnregisterJob( GT4Job *job );

	void PublishResourceAd( ClassAd *resource_ad );

	void registerDelegationURI( const char *deleg_uri, Proxy *job_proxy );
	const char *getDelegationURI( Proxy *job_proxy );
	const char *getDelegationError( Proxy *job_proxy );

	bool RequestDestroy( GT4Job *job );
	void DestroyComplete( GT4Job *job );

	bool IsGram42() { return m_isGram42; }

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GT4Resource *FindOrCreateResource( const char *resource_name,
											  const Proxy *proxy );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	int ProxyCallback();

	// This should be private, but GT4Job references it directly for now
	static HashTable <HashKey, GT4Resource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );
	void checkDelegation();
	bool ConfigureGahp();

	bool initialized;

	char *proxySubject;
	char *proxyFQAN;
	int delegationTimerId;
	GT4ProxyDelegation *activeDelegationCmd;
	char *delegationServiceUri;
	List<GT4ProxyDelegation> delegatedProxies;
	static int gahpCallTimeout;

		// We need to throttle the number of destroy requests going to each
		// resource.
	List<GT4Job> destroysWanted;
	List<GT4Job> destroysAllowed;
	int destroyLimit;

	GahpClient *gahp;
	GahpClient *deleg_gahp;

	bool m_isGram42;
};

#endif
