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
#ifndef _COLLECTOR_DAEMON_H_
#define _COLLECTOR_DAEMON_H_

#include "condor_classad.h"
#include "condor_commands.h"
#include "../condor_status.V6/totals.h"
#include "forkwork.h"

#include "collector_engine.h"
#include "collector_stats.h"
#include "dc_collector.h"


//----------------------------------------------------------------
// Simple job universe stats
//----------------------------------------------------------------
class CollectorUniverseStats {
  public:
	CollectorUniverseStats( void );
	CollectorUniverseStats( CollectorUniverseStats & );
	~CollectorUniverseStats( void );
	void Reset( void );
	void accumulate( int univ );
	int getValue( int univ );
	int getCount( void );
	int setMax( CollectorUniverseStats & );
	const char *getName( int univ );
	int publish( const char *label, ClassAd *ad );

  private:
	int perUniverse[CONDOR_UNIVERSE_MAX];
	int count;

};


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

	static CollectorStats collectorStats;
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

	static CollectorUniverseStats ustatsAccum;
	static CollectorUniverseStats ustatsMonthly;

	static ClassAd *ad;
	static DCCollector* updateCollector;
	static int UpdateTimerId;

	static MatchClassAd mad;
	static ForkWork forkQuery;

	static SocketCache* sock_cache;
	static int sockCacheHandler( Service*, Stream* sock );
	static int stashSocket( Stream* sock );

};

#endif
