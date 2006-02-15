/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef GT4RESOURCE_H
#define GT4RESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "gt4job.h"
#include "baseresource.h"
#include "gahp-client.h"


class GT4Job;
class GT4Resource;
struct ProxyDelegation;

class GT4Resource : public BaseResource
{
 protected:
	GT4Resource( const char *resource_name, const char *proxy_subject );
	~GT4Resource();

 public:
	bool Init();
	void Reconfig();
	void UnregisterJob( GT4Job *job );

	void registerDelegationURI( const char *deleg_uri, Proxy *job_proxy );
	const char *getDelegationURI( Proxy *job_proxy );

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GT4Resource *FindOrCreateResource( const char *resource_name,
											  const char *proxy_subject );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// This should be private, but GT4Job references it directly for now
	static HashTable <HashKey, GT4Resource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );
	int checkDelegation();

	bool initialized;

	char *proxySubject;
	int delegationTimerId;
	ProxyDelegation *activeDelegationCmd;
	char *delegationServiceUri;
	List<ProxyDelegation> delegatedProxies;
	static int gahpCallTimeout;

	GahpClient *gahp;
	GahpClient *deleg_gahp;
};

#endif
