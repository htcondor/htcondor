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

#ifndef __COLLECTOR_STATS_H__
#define __COLLECTOR_STATS_H__

#include "extArray.h"
#include "generic_stats.h"

#define TRACK_QUERIES_BY_SUBSYS 1 // for testing, we may want to turn this code off...
#ifdef TRACK_QUERIES_BY_SUBSYS
#include "subsystem_info.h"
#endif

// Enable a series of fine-grained timing probes of the details of the
// receive_update() CEDAR command handler.
//#define PROFILE_RECEIVE_UPDATE 1

#define DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL (3600*4)

// probes for doing timing analysis, enable one, the probe is more detailed.
#define collector_runtime_probe stats_entry_probe<double>
//#define collector_runtime_probe stats_recent_counter_timer

// Base
class CollectorBaseStats
{
  public:
	CollectorBaseStats( int history_size = 0 );
	virtual ~CollectorBaseStats( void  );
	int updateStats( bool sequened, int dropped );
	void reset( void );
	int setHistorySize( int size );
	int getTotal( void ) const { return updatesTotal; };
	int getSequenced( void ) const { return updatesSequenced; };
	int getDropped( void ) const { return updatesDropped; };
	//char *getHistoryString( void );
	char *getHistoryString( char * );
	int getHistoryStringLen( void ) const { return 1 + ( (historySize + 3) / 4 ); };
	bool wasRecentlyUpdated() const { return m_recently_updated; }
	void setRecentlyUpdated(bool value) { m_recently_updated=value; }

  private:
	int storeStats( bool sequened, int dropped );
	int setHistoryBits( bool dropped, int count );

	int			updatesTotal;			// Total # of updates received
	int			updatesSequenced;		// # of updates "sequenced" (Total+dropped-Initial) expected to match UpdateSequenceNumber if Initial==1
	int			updatesDropped;			// # of updates dropped

	// History info
	unsigned	*historyBuffer;			// History buffer
	int			historySize;			// Size of history to report
	int			historyWords;			// # of words allocated
	int			historyWordBits;		// # of bits / word (used a lot)
	int			historyBitnum;			// Current bit #
	int			historyMaxbit;			// Max bit #

	bool		m_recently_updated;		// true if not updated since last sweep
};

// Per "class" update stats
class CollectorClassStats : public CollectorBaseStats
{
  public:
	CollectorClassStats( const char *class_name, int history_size = 0 );
	~CollectorClassStats( void );
	bool match( const char *class_name );
	const char *getName ( void ) { return className; };

  private:
	const char	*className;
};

// List of the above
class CollectorClassStatsList
{
  public:
	CollectorClassStatsList( int history_size );
	~CollectorClassStatsList( void );
	int updateStats( const char *class_name, bool sequened, int dropped );
	int publish ( ClassAd *ad );
	int setHistorySize( int size );

  private:
	ExtArray<CollectorClassStats *> classStats;
	int		historySize;
};

// This is the tuple that we will be hashing on
class StatsHashKey
{
  public:
    std::string type;
    std::string name;
    std::string ip_addr;
    friend bool operator== (const StatsHashKey &, const StatsHashKey &);
    void getstr( std::string & ) const;
};

// Type for the hash tables ...
typedef HashTable <StatsHashKey, CollectorBaseStats *> StatsHashTable;

// Container of the hash table
class CollectorDaemonStatsList
{
  public:
	CollectorDaemonStatsList( bool enable, int history_size );
	~CollectorDaemonStatsList( void );
	int updateStats( const char *class_name,
					 ClassAd *ad,
					 bool sequened,
					 int dropped );
	int publish ( ClassAd *ad );
	int setHistorySize( int size );
	int enable( bool enabled );

		// Remove all records that have not been updated since the
		// last sweep.  Mark all remaining records for next sweep.
	void collectGarbage();

  private:
	bool hashKey( StatsHashKey &key, const char *class_name, ClassAd *ad );

	StatsHashTable		*hashTable;
	int				historySize;
	bool				enabled;
};


class stats_entry_lost_updates : public stats_entry_recent<Probe> {
public:
	static const int PubRatio = 4;  // publish loss ratio. value between 0 and 1 where 1 is all loss.
	static const int PubMax = 8;    // publish largest loss gap
	void Publish(ClassAd & ad, const char * pattr, int flags) const;
};


// update counters that are tracked globally, and per Ad type.
struct UpdatesCounters {
	stats_entry_recent<long>   UpdatesTotal;
	stats_entry_recent<long>   UpdatesInitial;
	//stats_entry_recent<Probe> UpdatesLost;
	stats_entry_lost_updates  UpdatesLost;

	void RegisterCounters(StatisticsPool &Pool, const char * className, int recent_max);
	void UnregisterCounters(StatisticsPool &Pool);
};

struct UpdatesStats {

	// aggregation of updates stats for the whole collector
	UpdatesCounters Any;
	stats_entry_abs<int> MachineAds;
	stats_entry_abs<int> SubmitterAds;

	stats_entry_abs<int> ActiveQueryWorkers;
	stats_entry_abs<int> PendingQueries;
	stats_entry_recent<long> DroppedQueries;

#ifdef TRACK_QUERIES_BY_SUBSYS
	stats_entry_recent<long> InProcQueriesFrom[SUBSYSTEM_ID_COUNT]; // Track subsystems < the AUTO subsys.
	stats_entry_recent<long> ForkQueriesFrom[SUBSYSTEM_ID_COUNT]; // Track subsystems < the AUTO subsys.
#endif

	// per-ad-type counters
	std::map<std::string, UpdatesCounters> PerClass;

	// these are used by generic tick
	time_t StatsLifetime;         // the total time covered by this set of statistics
	time_t StatsLastUpdateTime;   // last time that statistics were last updated. (a freshness time)
	time_t RecentStatsLifetime;   // actual time span of current RecentXXX data.
	time_t RecentStatsTickTime;   // last time Recent values Advanced

	StatisticsPool Pool;

	// non-published values
	time_t InitTime;            // last time we init'ed the structure
	int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
	int    RecentWindowQuantum {0};
	int    PublishFlags;
	int    AdvanceAtLastTick;

	// methods
	//
	void Init();
	void Clear();
	time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
	void Reconfig();
	void SetWindowSize(int window);
	void Publish(ClassAd & ad, int flags) const;
	void Publish(ClassAd & ad, const char * config) const;

	// this is called when a new update arrives.
	int  updateStats( const char * className, bool sequenced, int dropped );
};


// Collector stats
class CollectorStats
{
  public:
	CollectorStats( bool enable_daemon_stats,
					int daemon_history_size );
	virtual ~CollectorStats( void );
	int update( const char *className, ClassAd *oldAd, ClassAd *newAd );
	int publishGlobal( ClassAd *Ad, const char * config ) const;
	int setDaemonStats( bool );
	int setDaemonHistorySize( int size );
	void setGarbageCollectionInterval( time_t interval ) {
		m_garbage_interval = interval;
	}

		// If it is time to collect garbage, remove all records that
		// have not been updated since the last sweep.
	time_t considerCollectingGarbage();

	UpdatesStats				global;

  private:

	CollectorDaemonStatsList	*daemonList;

	time_t m_garbage_interval;
	time_t m_last_garbage_time;
};


#endif /* _CONDOR_COLLECTOR_STATS_H */
