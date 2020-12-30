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

#include <vector>
#include <queue>

#include "condor_classad.h"
#include "totals.h"
#include "forkwork.h"

#include "collector_engine.h"
#include "collector_stats.h"
#include "dc_collector.h"
#include "offline_plugin.h"
#include "ad_transforms.h"

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
	int getCount( void ) const;
	int setMax( CollectorUniverseStats & );
	const char *getName( int univ );
	int publish( const char *label, ClassAd *cad );

  private:
	int perUniverse[CONDOR_UNIVERSE_MAX];
	int count;

};


/**----------------------------------------------------------------
 *Collector daemon class declaration
 *
 * TODO: Eval notes and refactor when time permits.
 * 
 * REVIEW NOTES per TSTCLAIR
 * DESIGN (General):
 * 1.) It seems rather odd to have such a large static interface then
 * a few virtual functions.  This basically violates all rules of
 * practical design and should likely be cleaned up.  Either is is
 * a static singleton (something which is omni-present), or it is not.
 * There are templated header only constructs namely boost::bind &&
 * boost::function which get around the idea of generic function
 * pointers and should likely be used.  This will likely reduce the
 * public interface to just a few functions.
 *
 * 2.) I think that each daemon detailed knowledge of *any* communications
 * protocol is a bad thing.  It makes the daemon tightly bound to any
 * mode of transport & protocol, and very difficult to adapt over time.
 *
 * 3.) doxygen comments with real comments to explain why something
 * was done.
 *
 * 4.) consider a new object which performs the scanning functionality so all
 * scans within the collector access that object.  It seems sloppy to have it parseled
 * throughout the collector, on different calls to "CollectorEngine::walkHasTable"  You could
 * also consolidate the stats under that umbrella b/c the data is there. 
 *
 *----------------------------------------------------------------*/
class CollectorDaemon {

public:

	CollectorDaemon() {};
	virtual ~CollectorDaemon() {};

	virtual void Init();             // main_init
	virtual void Config();           // main_config
	virtual void Exit();             // main__shutdown_fast
	virtual void Shutdown();         // main_shutdown_graceful
	// command handlers
	static int receive_query_cedar(int, Stream*);
	static int receive_query_cedar_worker_thread(void *, Stream*);
	static AdTypes receive_query_public( int );
	static int receive_invalidation(int, Stream*);
	static int receive_update(int, Stream*);
    static int receive_update_expect_ack(int, Stream*);

	static void process_query_public(AdTypes, ClassAd*, List<ClassAd>*);
	static ClassAd * process_global_query( const char *constraint, void *arg );
	static int select_by_match( ClassAd *cad );
	static void process_invalidation(AdTypes, ClassAd&, Stream*);

	static int query_scanFunc(ClassAd*);
	static int invalidation_scanFunc(ClassAd*);
	static int expiration_scanFunc(ClassAd*);

	static int reportStartdScanFunc(ClassAd*);
	static int reportSubmittorScanFunc(ClassAd*);
	static int reportMiniStartdScanFunc(ClassAd *cad);

	static int sigint_handler(Service*, int);
	static void unixsigint_handler();
	
	static void init_classad(int interval);
	static void sendCollectorAd();

	static void forward_classad_to_view_collector(int cmd, const char *filterAttr, ClassAd *ad);

		// Take an incoming session and forward a token request to the schedd.
	static int schedd_token_request(int, Stream *stream);


	// A get method to support SOAP
	static CollectorEngine & getCollector( void ) { return collector; };

    // data pertaining to each view collector entry
    struct vc_entry {
        std::string name;
        DCCollector* collector;
        Sock* sock;
    };

    static OfflineCollectorPlugin offline_plugin_;

	static const int HandleQueryInProcNever = 0x0000;
	static const int HandleQueryInProcSmallTable = 0x0001;
	static const int HandleQueryInProcSmallQuery = 0x0002;
	static const int HandleQueryInProcSmallTableAndQuery = 0x0003;
	static const int HandleQueryInProcSmallTableOrQuery = 0x0004;
	static const int HandleQueryInProcAlways = 0xFFFF;

	typedef struct pending_query_entry {
		ClassAd *cad;
		Stream *sock;
		AdTypes whichAds;
		bool is_locate;
		char subsys[15];
	} pending_query_entry_t;

	static std::queue<pending_query_entry_t *> query_queue_high_prio;
	static std::queue<pending_query_entry_t *> query_queue_low_prio;
	static int ReaperId;
	static int QueryReaper(int pid, int exit_status);
	static int max_query_workers;  // from config file
	static int max_pending_query_workers;  // from config file
	static int max_query_worktime;  // from config file
	static int reserved_for_highprio_query_workers; // from config file
	static int active_query_workers;
	static int pending_query_workers;

#ifdef TRACK_QUERIES_BY_SUBSYS
	static bool want_track_queries_by_subsys;
#endif


protected:
	static CollectorStats collectorStats;
	static CollectorEngine collector;
	static Timeslice view_sock_timeslice;
    static std::vector<vc_entry> vc_list;
	static ConstraintHolder vc_projection;

	static int HandleQueryInProcPolicy;	// one of above HandleQueryInProc* constants
	static int ClientTimeout;
	static int QueryTimeout;
	static char* CollectorName;

	static ClassAd* __query__;
	static List<ClassAd>* __ClassAdResultList__;
	static int __numAds__;
	static int __resultLimit__;
	static int __failed__;
	static std::string __adType__;
	static ExprTree *__filter__;

	static TrackTotals* normalTotals;
	static int submittorRunningJobs;
	static int submittorIdleJobs;
	static int submittorNumAds;

	static int machinesTotal,machinesUnclaimed,machinesClaimed,machinesOwner;
	static int startdNumAds;

	static CollectorUniverseStats ustatsAccum;
	static CollectorUniverseStats ustatsMonthly;

	static ClassAd *ad;
	static CollectorList* collectorsToUpdate;
	static int UpdateTimerId;

	static int stashSocket( ReliSock* sock );

	static class CCBServer *m_ccb_server;

	static bool filterAbsentAds;
	static bool forwardClaimedPrivateAds;

private:

	static int setAttrLastHeardFrom( ClassAd* cad, unsigned long time );

	static AdTransforms m_forward_ad_xfm;
};

#endif
