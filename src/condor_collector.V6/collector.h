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
	static int receive_query_cedar(Service*, int, Stream*);
	static int receive_query_public(int, ClassAd*, List<ClassAd>*);
	static int receive_invalidation(Service*, int, Stream*);
	static int receive_update(Service*, int, Stream*);

	static void process_query(AdTypes, ClassAd*, List<ClassAd>*);
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
	static DCCollector* updateCollector;
	static int UpdateTimerId;

	static ForkWork forkQuery;

	static SocketCache* sock_cache;
	static int sockCacheHandler( Service*, Stream* sock );
	static int stashSocket( Stream* sock );

};

#endif
