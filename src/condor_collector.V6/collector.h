#ifndef _COLLECTOR_DAEMON_H_
#define _COLLECTOR_DAEMON_H_

//#include "sched.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "../condor_status.V6/totals.h"

#include "collector_engine.h"
#include "dc_collector.h"

//----------------------------------------------------------------
// Collector daemon class declaration
//----------------------------------------------------------------

class CollectorDaemon {

public:
	virtual void Init();             // main_init
	virtual void Config();           // main_config
	virtual void Exit();             // main__shutdown_fast
	virtual void Shutdown();         // main_shutdown_graceful

	// command handlers
	static int receive_query(Service*, int, Stream*);
	static int receive_invalidation(Service*, int, Stream*);
	static int receive_update(Service*, int, Stream*);

	static void process_query(AdTypes, ClassAd&, Stream*);
	static ClassAd * process_global_query( const char *constraint, void *arg );
	static int select_by_match( ClassAd *ad );
	static void process_invalidation(AdTypes, ClassAd&, Stream*);

	static int query_scanFunc(ClassAd*);
	static int invalidation_scanFunc(ClassAd*);

	static int reportStartdScanFunc(ClassAd*);
	static int reportSubmittorScanFunc(ClassAd*);
	static int reportMiniStartdScanFunc(ClassAd *ad);

	static void reportToDevelopers();

	static int sigint_handler(Service*, int);
	static void unixsigint_handler();
	
	static void init_classad(int interval);
	static int sendCollectorAd();

	static void send_classad_to_sock( int cmd, Daemon * d, ClassAd* theAd);	
protected:

	static CollectorEngine collector;
	static Daemon* View_Collector;
	static Sock* view_sock;

	static int ClientTimeout;
	static int QueryTimeout;
	static char* CollectorName;

	static ClassAd query_any_request;
	static ClassAd *query_any_result;

	static ClassAd* __query__;
	static Stream* __sock__;
	static int __numAds__;
	static int __failed__;

	static TrackTotals* normalTotals;
	static int submittorRunningJobs;
	static int submittorIdleJobs;

	static int machinesTotal,machinesUnclaimed,machinesClaimed,machinesOwner;

	static ClassAd *ad;
	static DCCollector* updateCollector;
	static int UpdateTimerId;

	static SocketCache* sock_cache;
	static int sockCacheHandler( Service*, Stream* sock );
	static int stashSocket( Stream* sock );
};

#endif
