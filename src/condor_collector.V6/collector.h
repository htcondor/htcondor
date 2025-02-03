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

#if 1
	struct collect_op {
		ClassAd* __query__ = nullptr;
		std::deque<CollectorRecord*> * __results__;
		ExprTree *__filter__ = nullptr;
		const char * __mytype__ = nullptr; // implicit filter, if non-null return only ads with this mytype
		bool __skip_absent__ = false; // implicit filter
		int __numAds__ = 0;
		int __resultLimit__ = INT_MAX;
		int __failed__ = 0;
		int __absent__ = 0;

		//void process_query_public(AdTypes, ClassAd *query, std::deque<CollectorRecord*> * results);
		int query_scanFunc(CollectorRecord*);
		void process_invalidation(AdTypes, ClassAd&, Stream*);
		int invalidation_scanFunc(CollectorRecord*);
		int expiration_scanFunc(CollectorRecord*);
		int setAttrLastHeardFrom( ClassAd* cad, unsigned long time );
	};
#else
	static void process_query_public(AdTypes, ClassAd*, List<CollectorRecord>*);
	static void process_invalidation(AdTypes, ClassAd&, Stream*);
	static int query_scanFunc(CollectorRecord*);
	static int invalidation_scanFunc(CollectorRecord*);
	static int expiration_scanFunc(CollectorRecord*);
#endif
	static ClassAd * process_global_query( const char *constraint, void *arg );
	static int select_by_match( ClassAd *cad );

#if 1
#else
	static int reportStartdScanFunc(CollectorRecord*);
	static int reportSubmittorScanFunc(CollectorRecord*);
	static int reportMiniStartdScanFunc(CollectorRecord*);
#endif

	static int sigint_handler(Service*, int);
	static void unixsigint_handler();
	
	static void init_classad(int interval);
	static void sendCollectorAd(int tid);

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

	// declare a struct suitable for malloc/free that holds
	// information needed to perform the query on a thread.
	// nothing in this struct other than the ClassAd and sock are destructed
	typedef struct pending_query_entry {
		const char * label;
		ClassAd *cad;
		Stream *sock;
		bool is_locate;
		bool is_multi;
		char subsys[15];
		int  limit;           // overall result limit
		int num_adtypes;
		struct adtype_query_props {
			char whichAds;
			bool skip_absent;    // skip over absent ads
			bool match_mytype;   // skip ads that don't have a mytype that matches the query target
			bool pvt_ad;         // send results from the private ad (future)
			int  limit;          // result limit for this sub-query
			const char * tag;    // indicates the table for GENERIC and query attribute prefix for MULTI
			ExprTree * constraint; // this points into the cad above, and is valid as long as that ad is
		} adt[1]; // actual size of this array is defined by num_adtypes

		// return the number of bytes needed to hold a struct of this type with num adtypes
		static size_t size(int num=1) {
			if (num < 1) num = 1;
			return sizeof(struct pending_query_entry) + sizeof(struct adtype_query_props)*(num-1);
		}
	} pending_query_entry_t;
	static pending_query_entry_t * make_query_entry(AdTypes whichAds, ClassAd * query, bool allow_pvt=false);
	static ExprTree * get_query_filter(ClassAd* query, const std::string & attr, bool & skip_absent);

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

#if 1
#else
	static ClassAd* __query__;
	static List<CollectorRecord>* __ClassAdResultList__;
	static int __numAds__;
	static int __resultLimit__;
	static int __failed__;
	static std::string __adType__;
	static ExprTree *__filter__;
	static bool __hidePvtAttrs__;

	static TrackTotals* normalTotals;
	static int submittorRunningJobs;
	static int submittorIdleJobs;
	static int submittorNumAds;

	static int machinesTotal,machinesUnclaimed,machinesClaimed,machinesOwner;
	static int startdNumAds;

	static CollectorUniverseStats ustatsAccum;
#endif
	static CollectorUniverseStats ustatsMonthly;

	static ClassAd *ad;
	static CollectorList* collectorsToUpdate;
	static int UpdateTimerId;

	static int stashSocket( ReliSock* sock );

	static class CCBServer *m_ccb_server;

	static bool filterAbsentAds;
	static bool forwardClaimedPrivateAds;

private:


	static AdTransforms m_forward_ad_xfm;
};

#endif
