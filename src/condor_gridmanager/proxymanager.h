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

#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "simplelist.h"

#include "gahp-client.h"

struct MyProxyEntry {
	char * myproxy_host;
	//int myproxy_port;
	char * myproxy_server_dn;
	char * myproxy_credential_name;

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
	int gahp_proxy_id;
	int num_references;
	SimpleList<int> notification_tids;
	
	SimpleList<MyProxyEntry*> myproxy_entries;
};

#define PROXY_NEAR_EXPIRED( p ) \
    ((p)->expiration_time - time(NULL) <= minProxy_time)
#define PROXY_IS_EXPIRED( p ) \
    ((p)->expiration_time - time(NULL) <= 180)

extern int CheckProxies_interval;
extern int minProxy_time;

bool UseMultipleProxies( const char *proxy_dir,
						 bool(*init_gahp_func)(const char *proxy) );
bool UseSingleProxy( const char *proxy_path,
					 bool(*init_gahp_func)(const char *proxy) );

Proxy *AcquireProxy( const char *proxy_path, int notify_tid = -1 );
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
