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
