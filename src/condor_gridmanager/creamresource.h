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

#ifndef CREAMRESOURCE_H
#define CREAMRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "creamjob.h"
#include "baseresource.h"
#include "gahp-client.h"


class CreamJob;
class CreamResource;
struct ProxyDelegation;

class CreamResource : public BaseResource
{
 protected:
	CreamResource( const char *resource_name, const char *proxy_subject );
	~CreamResource();

 public:
	bool Init();
	void Reconfig();
	void UnregisterJob( CreamJob *job );

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

	// This should be private, but CreamJob references it directly for now
	static HashTable <HashKey, CreamResource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );
	int checkDelegation();

	bool initialized;

	char *proxySubject;
	int delegationTimerId;
	ProxyDelegation *activeDelegationCmd;
	char *serviceUri;
	char *delegationServiceUri;
	List<ProxyDelegation> delegatedProxies;
	static int gahpCallTimeout;
	MyString delegation_uri;
	GahpClient *gahp;
	GahpClient *deleg_gahp;
};

#endif
