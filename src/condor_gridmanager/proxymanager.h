
#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "simplelist.h"

#include "gahp-client.h"

struct Proxy {
	char *proxy_filename;
	int expiration_time;
	bool near_expired;
	int gahp_proxy_id;
	int num_references;
	SimpleList<int> notification_tids;
};

#define PROXY_NEAR_EXPIRED( p ) \
    ((p)->expiration_time - time(NULL) <= minProxy_time)

extern int CheckProxies_interval;
extern int minProxy_time;

bool UseMultipleProxies( const char *proxy_dir,
						 bool(*init_gahp_func)(const char *proxy) );
bool UseSingleProxy( const char *proxy_path,
					 bool(*init_gahp_func)(const char *proxy) );

Proxy *AcquireProxy( const char *proxy_path, int notify_tid = -1 );
void ReleaseProxy( Proxy *proxy, int notify_tid = -1 );

void doCheckProxies();

#endif // defined PROXYMANAGER_H
