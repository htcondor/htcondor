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

#ifndef _COLLECTOR_DAEMON_H_
#define _COLLECTOR_DAEMON_H_

#include "condor_classad.h"
#include "condor_commands.h"
#include "../condor_status.V6/totals.h"
#include "forkwork.h"

#include "collector_engine.h"
#include "collector_stats.h"
#include "dc_collector.h"
#include "offline_plugin.h"

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
	int publish( const char *label, ClassAd *cad );

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
	static int receive_query_cedar(Service*, int, Stream*);
	static AdTypes receive_query_public( int );
	static int receive_invalidation(Service*, int, Stream*);
	static int receive_update(Service*, int, Stream*);
    static int receive_update_expect_ack(Service*, int, Stream*);

	static void process_query_public(AdTypes, ClassAd*, List<ClassAd>*);
	static ClassAd * process_global_query( const char *constraint, void *arg );
	static int select_by_match( ClassAd *cad );
	static void process_invalidation(AdTypes, ClassAd&, Stream*);

	static int query_scanFunc(ClassAd*);
	static int invalidation_scanFunc(ClassAd*);

	static int reportStartdScanFunc(ClassAd*);
	static int reportSubmittorScanFunc(ClassAd*);
	static int reportMiniStartdScanFunc(ClassAd *cad);

	static void reportToDevelopers();

	static int sigint_handler(Service*, int);
	static void unixsigint_handler();
	
	static void init_classad(int interval);
	static void sendCollectorAd();

	static void send_classad_to_sock( int cmd, Daemon * d, ClassAd* theAd);	

	// A get method to support SOAP
	static CollectorEngine & getCollector( void ) { return collector; };

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
	static List<ClassAd>* __ClassAdResultList__;
	static int __numAds__;
	static int __failed__;

	static TrackTotals* normalTotals;
	static int submittorRunningJobs;
	static int submittorIdleJobs;

	static int machinesTotal,machinesUnclaimed,machinesClaimed,machinesOwner;

	static CollectorUniverseStats ustatsAccum;
	static CollectorUniverseStats ustatsMonthly;

	static ClassAd *ad;
	static CollectorList* updateCollectors;
	static DCCollector* updateRemoteCollector;
	static int UpdateTimerId;

	static ForkWork forkQuery;

	static int stashSocket( ReliSock* sock );

	static class CCBServer *m_ccb_server;

private:

#if defined ( HAVE_HIBERNATION )
    static OfflineCollectorPlugin offline_plugin_;
#endif

};

#endif
