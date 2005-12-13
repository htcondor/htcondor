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


#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "simplelist.h"

struct ProxySubject;

struct MyProxyEntry {
	char * myproxy_host;
	//int myproxy_port;
	char * myproxy_server_dn;
	char * myproxy_credential_name;
	
	int refresh_threshold; // in minutes
	int new_proxy_lifetime; // in hours

	char * myproxy_password;

	// For requesting MyProxy password
	int cluster_id;
	int proc_id;

	// get-delegation-invocation details
	time_t last_invoked_time;
	int get_delegation_pid;
	int get_delegation_err_fd;
	char  * get_delegation_err_filename ;
	int get_delegation_password_pipe[2]; // to send pwd via stdin
};


struct Proxy {
	char *proxy_filename;
	int expiration_time;
	bool near_expired;
	int id;
	int num_references;
	SimpleList<int> notification_tids;
	
	SimpleList<MyProxyEntry*> myproxy_entries;
	ProxySubject *subject;
};

struct ProxySubject {
	char *subject_name;
	Proxy *master_proxy;
	List<Proxy> proxies;
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

Proxy *AcquireProxy( const ClassAd *job_ad, MyString &error,
					 int notify_tid = -1 );
Proxy *AcquireProxy( Proxy *proxy, int notify_tid = -1 );
void ReleaseProxy( Proxy *proxy, int notify_tid = -1 );
void DeleteProxy (Proxy *& proxy);
void DeleteMyProxyEntry (MyProxyEntry *& proxy);

int RefreshProxyThruMyProxy (Proxy * proxy);
int MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus);
int GetMyProxyPasswordFromSchedD (int cluster_id, int proc_id, char ** password);
extern int myproxyGetDelegationReaperId;

//int SetMyProxyHostForProxy ( const char* hostport, const char * server_dn, const char * myproxy_pwd, const Proxy * proxy );

void doCheckProxies();

#endif // defined PROXYMANAGER_H
