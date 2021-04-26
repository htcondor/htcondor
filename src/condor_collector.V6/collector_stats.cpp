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

#include "condor_common.h"

//-------------------------------------------------------------

#include "condor_classad.h"
#include "condor_attributes.h"
#include "extArray.h"
#include "internet.h"
#include "collector_stats.h"
#include "condor_config.h"
#include "classad/classadCache.h"
#include "ccb_server.h"

// The hash function to use
static size_t hashFunction (const StatsHashKey &key)
{
    size_t result = 0;
	const char *p;

    for (p = key.type.c_str(); p && *p;
	     result = (result<<5) + result + (size_t)(*(p++))) { }

    for (p = key.name.c_str(); p && *p;
	     result = (result<<5) + result + (size_t)(*(p++))) { }

    for (p = key.ip_addr.c_str(); p && *p;
	     result = (result<<5) + result + (size_t)(*(p++))) { }

    return result;
}

bool operator== (const StatsHashKey &lhs, const StatsHashKey &rhs)
{
    return ( ( lhs.name == rhs.name) && ( lhs.ip_addr == rhs.ip_addr) &&
			 ( lhs.type == rhs.type) );
}

// Instantiate things


// ************************************
// Collector Statistics base class
// ************************************

// Constructor
CollectorBaseStats::CollectorBaseStats ( int history_size )
{
	// Initialize & copy...
	historySize = history_size;
	historyWordBits = 8 * sizeof(unsigned);

	// Allocate a history buffer
	if ( historySize ) {
		historyWords = ( historySize + historyWordBits - 1) / historyWordBits;
		historyMaxbit = historyWordBits * historyWords - 1;
		historyBuffer = new unsigned[historyWords];
		historyBitnum = 0;
	} else {
		historyBuffer = NULL;
		historyWords = 0;
		historyMaxbit = 0;
		historyBitnum = 0;
	}

	// Reset vars..
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;

	// Reset the stats
	reset( );

	m_recently_updated = true;
}

// Destructor
CollectorBaseStats::~CollectorBaseStats ( void )
{
	// Free up the history buffer
	if ( historyBuffer ) {
		delete [] historyBuffer;
		historyBuffer = NULL;
	}
}

// Reset our statistics counters
void
CollectorBaseStats::reset ( void )
{
	// Free up the history buffer
	if ( historyBuffer ) {
		memset( historyBuffer, 0, historyWords * sizeof(unsigned) );
	}
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;
}

// Change the history size
int
CollectorBaseStats::setHistorySize ( int new_size )
{
	int	required_words =
		( ( new_size + historyWordBits - 1 ) / historyWordBits );

	// If new & old are equal, nothing to do
	if ( new_size == historySize ) {
		return 0;

		// New is zero, old non-zero
	} else if ( 0 == new_size ) {
		if ( historyBuffer) {
			delete[] historyBuffer;
			historyBuffer = NULL;
			historySize = historyWords = 0;
			historyMaxbit = 0;
		}
		return 0;

		// New size requires equal or less memory
	} else if ( required_words <= historyWords ) {
		historySize = new_size;
		return 0;

		// New size is larger than we have...
	} else {
		unsigned *newbuf = new unsigned[ required_words ];
		memset( newbuf, 0, required_words * sizeof(unsigned) );
		if ( historyBuffer ) {
			memcpy( newbuf, historyBuffer, historyWords );
			delete[] historyBuffer;
		} else {
			historyBitnum = 0;
		}
		historyBuffer = newbuf;
		historySize = new_size;
		historyMaxbit = historyWordBits * historyWords - 1;
	}

	return 0;
}

// Update our statistics
int
CollectorBaseStats::updateStats ( bool sequenced, int dropped )
{
	m_recently_updated = true;

	// Store this update
	storeStats( sequenced, dropped );

	// Done
	return 0;
}

// Update our statistics
int
CollectorBaseStats::storeStats ( bool sequenced, int dropped )
{
	// updatesTotal is the total number of updates we have received
	// updatesSequenced is the number of ads we can DETECTED we should have received, it is effectively (Total+Dropped-Initial)
	// updatesDropped is the number of updates DETECTED as dropped by examining sequence numbers.

	// sequenced means that there was a previous ad in the pool
	// if sequenced is false, dropped will always be 0 because we don't know how many updates may have been lost

	updatesTotal++;
	updatesSequenced += dropped + (sequenced ? 1 : 0);
	updatesDropped += dropped;

	// now update the history bitmap for this update.

	// set 1 bits to indicate dropped updates
	if (dropped) { setHistoryBits( true, dropped ); }
	// write a single 0 bit to indicate a successful update
	setHistoryBits ( false, 1 );

	return 0;
}

int
CollectorBaseStats::setHistoryBits ( bool dropped, int count )
{

	// Update the history buffer
	if ( historyBuffer ) {
		int		update_num;

		// For each update to shift unsignedo our barrell register..
		for	( update_num = 0;  update_num < count; update_num++ ) {
			unsigned		word_num = historyBitnum / historyWordBits;
			unsigned		bit_num = historyBitnum % historyWordBits;
			unsigned		mask = (1 << bit_num);

			// Set the bit...
			if ( dropped ) {
				historyBuffer[word_num] |= mask;
			} else {
				historyBuffer[word_num] &= (~ mask);
			}

			// Next bit
			if ( ++historyBitnum >= historyMaxbit ) {
				historyBitnum = 0;
			}
		}
	}

	return 0;
}

// Get the history as a hex string
char *
CollectorBaseStats::getHistoryString ( char *buf )
{
	unsigned		outbit = 0;				// Current bit # to output
	unsigned		outword = 0x0;			// Current word to ouput
	int			outoff = 0;				// Offset char * in output str.
	int			offset = historyBitnum;	// History offset
	int			loop;					// Loop variable

	// If history's not enable, nothing to do..
	if ( ( ! historyBuffer ) || ( ! historySize ) ) {
		*buf = '\0';
		return buf;
	}

	// Calculate the "last" offset
	if ( --offset < 0 ) {
		offset = historyMaxbit;
	}

	// And, the starting "word" and bit numbers
	int		word_num = offset / historyWordBits;
	int		bit_num = offset % historyWordBits;

	// Walk through 1 bit at a time...
	for( loop = 0;  loop < historySize;  loop++ ) {
		unsigned	mask = (1 << bit_num);

		if ( historyBuffer[word_num] & mask ) {
			outword |= ( 0x8 >> outbit );
		}
		if ( --bit_num < 0 ) {
			bit_num = (historyWordBits - 1);
			if ( --word_num < 0 ) {
				word_num = (historyWords - 1);
			}
		}

		// Convert to a char
		if ( ++outbit == 4 ) {
			buf[outoff++] =
				( outword <= 9 ) ? ( '0' + outword ) : ( 'a' + outword - 10 );
			outbit = 0;
			outword = 0x0;
		}

	}
	if ( outbit ) {
		buf[outoff++] =
			outword <= 9 ? ( '0' + outword ) : ( 'a' + outword - 10 );
		outbit = 0;
		outword = 0x0;
	}
	buf[outoff++] = '\0';
	return buf;
}

void stats_entry_lost_updates::Publish(ClassAd & ad, const char * pattr, int flags) const
{
	if ( ! flags) flags = PubDefault;
	if ((flags & IF_NONZERO) && stats_entry_is_zero(this->value)) return;

	std::string rattr("Recent"); rattr += pattr;

	// for Loss, publish the Sum without a suffix, the Avg is called Ratio
	// and Max is called max.
	// this sort of probe is useful for counting lost updates
	if (flags & PubValue) {
		ad.Assign(pattr, (long long)value.Sum);
		ad.Assign(rattr, (long long)recent.Sum);
	}
	if (flags & PubRatio) {
		double avg = value.Avg();
		if ( ! (flags & IF_NONZERO) || avg > 0.0 || avg < 0.0) {
			size_t ix = rattr.size();
			rattr += "Ratio";
			const char * p = rattr.c_str();
			ad.Assign(p+6, avg);
			ad.Assign(p, recent.Avg());
			rattr.erase(ix);
		}
	}
	if (flags & PubMax) {
		int val = MAX(0.0, (int)value.Max);
		if ( ! (flags & IF_NONZERO) || val != 0) {
			size_t ix = rattr.size();
			rattr += "Max";
			const char * p = rattr.c_str();
			ad.Assign(p+6, val);
			val = MAX(0, (int)recent.Max);
			ad.Assign(p, val);
			rattr.erase(ix);
		}
	}
}


#define ADD_EXTERN_RUNTIME(pool, name, ifpub) extern collector_runtime_probe name##_runtime;\
	name##_runtime.Clear();\
	(pool).AddProbe( #name, &name##_runtime, #name, ifpub | IF_RT_SUM)

#define ADD_RECENT_PROBE(pool, probe, verb, suffix, recent_max) \
	probe.Clear(); \
	probe.SetRecentMax(recent_max); \
	attr = #probe; \
	if (suffix) { attr += "_"; attr += suffix; } \
	(pool).AddProbe(attr.c_str(), &probe, NULL, verb | probe.PubDefault)

// update counters that are tracked globally, and per Ad type into the given stats pool
//
void UpdatesCounters::RegisterCounters(StatisticsPool &Pool, const char * className, int recent_max)
{
	std::string attr; // used by the macros below.

	ADD_RECENT_PROBE(Pool, UpdatesTotal, IF_BASICPUB, className, recent_max);
	ADD_RECENT_PROBE(Pool, UpdatesInitial, IF_BASICPUB, className, recent_max);
	if (className) {
		ADD_RECENT_PROBE(Pool, UpdatesLost, IF_BASICPUB, className, recent_max);
	} else {
		// for overall UpdatesLost counter, publish loss ratio & max also
		int pub_fields = stats_entry_lost_updates::PubRatio | stats_entry_lost_updates::PubMax;
		ADD_RECENT_PROBE(Pool, UpdatesLost, IF_BASICPUB | pub_fields, className, recent_max);
	}

};

void UpdatesCounters::UnregisterCounters(StatisticsPool &Pool)
{
	Pool.RemoveProbesByAddress(&UpdatesTotal, &UpdatesLost);
}

void UpdatesStats::Init()
{
	Clear();
	// default window size to 1 quantum, we may set it to something else later.
	if ( ! this->RecentWindowQuantum) this->RecentWindowQuantum = 1;
	this->RecentWindowMax = this->RecentWindowQuantum;

	Any.RegisterCounters(Pool, NULL, 1);

	STATS_POOL_ADD(Pool, "", MachineAds, IF_BASICPUB);
	STATS_POOL_ADD(Pool, "", SubmitterAds, IF_BASICPUB);

	// stats for fork workers.
	STATS_POOL_ADD(Pool, "", ActiveQueryWorkers, IF_BASICPUB);
	STATS_POOL_ADD(Pool, "", PendingQueries, IF_BASICPUB);
	STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", DroppedQueries, IF_BASICPUB);

	ADD_EXTERN_RUNTIME(Pool, HandleQuery, IF_VERBOSEPUB);
	ADD_EXTERN_RUNTIME(Pool, HandleLocate, IF_VERBOSEPUB);

	ADD_EXTERN_RUNTIME(Pool, HandleQueryForked, IF_VERBOSEPUB);
	ADD_EXTERN_RUNTIME(Pool, HandleQueryMissedFork, IF_VERBOSEPUB);
	ADD_EXTERN_RUNTIME(Pool, HandleLocateForked, IF_VERBOSEPUB);
	ADD_EXTERN_RUNTIME(Pool, HandleLocateMissedFork, IF_VERBOSEPUB);

#ifdef TRACK_QUERIES_BY_SUBSYS
    #define ADD_SUBSYS_PROBES(pool,subsys,as) \
	   pool.AddProbe("InProcQueriesFrom" #subsys, &InProcQueriesFrom[SUBSYSTEM_ID_##subsys], "InProcQueriesFrom" #subsys, as | InProcQueriesFrom[SUBSYSTEM_ID_##subsys].PubDefault); \
	   pool.AddProbe("ForkQueriesFrom" #subsys, &ForkQueriesFrom[SUBSYSTEM_ID_##subsys], "ForkQueriesFrom" #subsys, as | ForkQueriesFrom[SUBSYSTEM_ID_##subsys].PubDefault);

	ADD_SUBSYS_PROBES(Pool,UNKNOWN,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,MASTER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,COLLECTOR,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,NEGOTIATOR,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,SCHEDD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,SHADOW,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,STARTD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,STARTER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,CREDD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,KBDD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,GRIDMANAGER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,HAD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,REPLICATION,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,TRANSFERER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,TRANSFERD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,ROOSTER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,SHARED_PORT,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,JOB_ROUTER,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,DEFRAG,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,GANGLIAD,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,PANDAD,IF_BASICPUB | IF_NONZERO);

	ADD_SUBSYS_PROBES(Pool,DAGMAN,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,TOOL,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,SUBMIT,IF_BASICPUB | IF_NONZERO);
	ADD_SUBSYS_PROBES(Pool,ANNEXD,IF_BASICPUB | IF_NONZERO);

	ADD_SUBSYS_PROBES(Pool,GAHP,IF_BASICPUB | IF_NONZERO);
#endif


	// receive_update and task breakdown
	bool enable = param_boolean("PUBLISH_COLLECTOR_ENGINE_PROFILING_STATS",false);
	int prof_publevel = enable ? IF_BASICPUB : IF_VERBOSEPUB;
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_receive_update, prof_publevel);
#ifdef PROFILE_RECEIVE_UPDATE
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ru_pre_collect, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ru_collect, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ru_plugins, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ru_forward, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ru_stash_socket, prof_publevel);

	// ru_collect subtask breakdown
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ruc, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ruc_getAd, prof_publevel);
	//ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ruc_replaceAd5, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ruc_authid, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_ruc_collect, prof_publevel);

	// ruc_collect subtask breakdown
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_validateAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_makeHashKey, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_insertAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_updateAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_getPvtAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_insertPvtAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_updatePvtAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_repeatAd, prof_publevel);
	ADD_EXTERN_RUNTIME(Pool, CollectorEngine_rucc_other, prof_publevel);
#endif

	getClassAdEx_clearProfileStats();
	getClassAdEx_addProfileStatsToPool(&Pool, prof_publevel);

	AddCCBStatsToPool(Pool, IF_BASICPUB);
}

void UpdatesStats::Clear()
{
	this->InitTime = time(NULL);
	this->StatsLifetime = 0;
	this->StatsLastUpdateTime = 0;
	this->RecentStatsTickTime = 0;
	this->RecentStatsLifetime = 0;

	Pool.Clear();
	PerClass.clear();
}

time_t UpdatesStats::Tick(time_t now) // call this when time may have changed to update StatsUpdateTime, etc.
{
   if ( ! now) now = time(NULL);

   int cAdvance = generic_stats_Tick(
      now,
      this->RecentWindowMax,   // RecentMaxTime
      this->RecentWindowQuantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);

   AdvanceAtLastTick = cAdvance;
   return now;
}

void UpdatesStats::Reconfig()
{
	this->RecentWindowQuantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
	this->RecentWindowMax = param_integer("STATISTICS_WINDOW_SECONDS", 1200, this->RecentWindowQuantum, INT_MAX);

	this->PublishFlags    = IF_BASICPUB | IF_RECENTPUB | IF_NONZERO;
	auto_free_ptr tmp(param("STATISTICS_TO_PUBLISH"));
	if (tmp) {
		this->PublishFlags = generic_stats_ParseConfigString(tmp, "COLLECTOR", "COLLECTOR", this->PublishFlags);
	}

	std::string strWhitelist;
	if (param(strWhitelist, "STATISTICS_TO_PUBLISH_LIST")) {
		this->Pool.SetVerbosities(strWhitelist.c_str(), this->PublishFlags, true);
	}

	SetWindowSize(this->RecentWindowMax);
}

void UpdatesStats::SetWindowSize(int window)
{
	Pool.SetRecentMax(window, this->RecentWindowQuantum);
}

void UpdatesStats::Publish(ClassAd & ad, int flags) const
{
	ad.Assign("StatsLifetime", (int)StatsLifetime);
	ad.Assign("StatsLastUpdateTime", (int)StatsLastUpdateTime);
	if (flags & IF_RECENTPUB) {
		ad.Assign("RecentStatsLifetime", (int)RecentStatsLifetime);
		if (flags & IF_VERBOSEPUB) {
			ad.Assign("RecentWindowMax", (int)RecentWindowMax);
			ad.Assign("RecentStatsTickTime", (int)RecentStatsTickTime);
		}
	}
	Pool.Publish(ad, flags);

	if (param_boolean("PUBLISH_COLLECTOR_ENGINE_PROFILING_STATS",false)) {
		long dpf_skipped=-1, dpf_logged=-1;
		double dpf_skipped_rt=-1, dpf_logged_rt=-1;
		if (_condor_dprintf_runtime (dpf_skipped_rt, dpf_skipped, dpf_logged_rt, dpf_logged, false)) {
			ad.Assign("DprintfSkipped", dpf_skipped);
			ad.Assign("DprintfSkippedRuntime", dpf_skipped_rt);
			ad.Assign("DprintfLogged", dpf_logged);
			ad.Assign("DprintfLoggedRuntime", dpf_logged_rt);
		}

	#ifdef CLASSAD_CACHE_PROFILING
		unsigned long hits, misses, querys, hitdels, removals, unparse;
		if (classad::CachedExprEnvelope::_debug_get_counts(hits, misses, querys, hitdels, removals, unparse))
		{
			ad.Assign("ClassadCacheHits",hits);
			ad.Assign("ClassadCacheMisses",misses);
			ad.Assign("ClassadCacheQuerys",querys);
			ad.Assign("ClassadCacheHitDeletes",hitdels);
			ad.Assign("ClassadCacheRemovals",removals);
			ad.Assign("ClassadCacheUnparse",unparse);
		}
	#endif
	}
}

void UpdatesStats::Publish(ClassAd & ad, const char * config) const
{
   int flags = this->PublishFlags;
   if (config && config[0]) {
      flags = generic_stats_ParseConfigString(config, "COLLECTOR", "COLLECTOR", IF_BASICPUB | IF_RECENTPUB);
   }
   this->Publish(ad, flags);
}

// this is called when a new update arrives.
int  UpdatesStats::updateStats( const char * className, bool sequenced, int dropped )
{
	Any.UpdatesTotal += 1;
	Any.UpdatesLost += dropped;
	if ( ! sequenced) Any.UpdatesInitial += 1;

	if (className) {
		std::map<std::string, UpdatesCounters>::iterator found = PerClass.find(className);
		if (found == PerClass.end()) {
			int cRecent = RecentWindowQuantum ? RecentWindowMax / RecentWindowQuantum : RecentWindowMax;
			PerClass[className].RegisterCounters(Pool, className, cRecent);
			found = PerClass.find(className);
		}
		found->second.UpdatesTotal += 1;
		found->second.UpdatesLost += dropped;
		if ( ! sequenced) found->second.UpdatesInitial += 1;
	}
	return 0;
}


// **********************************************
// Per daemon List of Collector Statistics
// **********************************************

// Constructor
CollectorDaemonStatsList::CollectorDaemonStatsList( bool nable,
													int history_size )
{
	enabled = nable;
	historySize = history_size;
	if ( enabled ) {
		hashTable = new StatsHashTable( &hashFunction );
	} else {
		hashTable = NULL;
	}
}

// Destructor
CollectorDaemonStatsList::~CollectorDaemonStatsList( void )
{
	if ( hashTable ) {

		// iterate through hash table
		CollectorBaseStats *ent;
		StatsHashKey key;
	
		hashTable->startIterations();
		while ( hashTable->iterate(key, ent) ) {
			delete ent;
			hashTable->remove(key);
		}
	
		delete hashTable;
		hashTable = NULL;
	}
}

// Update statistics
int
CollectorDaemonStatsList::updateStats( const char *class_name,
									   ClassAd *ad,
									   bool sequenced,
									   int dropped )
{
	StatsHashKey		key;
	bool				hash_ok = false;
	CollectorBaseStats	*daemon;

	// If we're not instantiated, do nothing..
	if ( ( ! enabled ) || ( ! hashTable ) ) {
		return 0;
	}

	// Generate the hash key for this ad
	hash_ok = hashKey ( key, class_name, ad );
	if ( ! hash_ok ) {
		return -1;
	}

	// Ok, we've got a valid hash key
	// Is it already in the hash?
	if ( hashTable->lookup ( key, daemon ) == -1 ) {
		daemon = new CollectorBaseStats( historySize );
		hashTable->insert( key, daemon );

		if (IsFulldebug(D_FULLDEBUG)) {
			std::string string;
			key.getstr( string );
			dprintf( D_FULLDEBUG,
				 "stats: Inserting new hashent for %s\n", string.c_str() );
		}
	}

	// Update the daemon object...
	daemon->updateStats( sequenced, dropped );

		// static const so we're not always new'ing/deleting them
	static const std::string UpdateStatsTotal(ATTR_UPDATESTATS_TOTAL);
	static const std::string UpdateStatsSequenced(ATTR_UPDATESTATS_SEQUENCED);
	static const std::string UpdateStatsLost(ATTR_UPDATESTATS_LOST);
	static const std::string UpdateStatsHistory(ATTR_UPDATESTATS_HISTORY);

	ad->InsertAttr(UpdateStatsTotal, daemon->getTotal());
	ad->InsertAttr(UpdateStatsSequenced, daemon->getSequenced());
	ad->InsertAttr(UpdateStatsLost, daemon->getDropped());

	// Get the history string & insert it if it's valid
	// Compute the size of the string we need..
	char *hist = new char [daemon->getHistoryStringLen()];
	if (daemon->getHistoryString(hist)) {
		ad->InsertAttr(UpdateStatsHistory, hist);
	}
	delete [] hist;

	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorDaemonStatsList::publish( ClassAd * /*ad*/ )
{
	return 0;
}

// Set the history size
int 
CollectorDaemonStatsList::setHistorySize( int new_size )
{
	if ( ! hashTable ) {
		return 0;
	}

	CollectorBaseStats *daemon;

	// walk the hash table
	hashTable->startIterations( );
	while ( hashTable->iterate ( daemon ) )
	{
		daemon->setHistorySize( new_size );
	}

	// Store it off for future reference
	historySize = new_size;
	return 0;
}

// Enable / disable daemon statistics
int 
CollectorDaemonStatsList::enable( bool nable )
{
	enabled = nable;
	if ( ( enabled ) && ( ! hashTable ) ) {
		dprintf( D_ALWAYS, "enable: Creating stats hash table\n" );
		hashTable = new StatsHashTable( &hashFunction );
	} else if ( ( ! enabled ) && ( hashTable ) ) {
		dprintf( D_ALWAYS, "enable: Destroying stats hash table\n" );

		// iterate through hash table, delete all entries
		CollectorBaseStats *ent;
		StatsHashKey key;

		hashTable->startIterations();
		while ( hashTable->iterate(key, ent) ) {
			delete ent;
			hashTable->remove(key);
		}

		delete hashTable;
		hashTable = NULL;
	}
	return 0;
}

// Get string of the hash key (for debugging)
void
StatsHashKey::getstr( std::string &buf ) const
{
	formatstr( buf, "'%s':'%s':'%s'",
				 type.c_str(), name.c_str(), ip_addr.c_str()  );
}

// Generate a hash key
bool
CollectorDaemonStatsList::hashKey (StatsHashKey &key,
								   const char *class_name,
								   ClassAd *ad )
{

	// Fill in pieces..
	key.type = class_name;

	// The 'name'
	char	buf[256]   = "";
	std::string slot_buf = "";
	if ( !ad->LookupString( ATTR_NAME, buf, sizeof(buf))  ) {

		// No "Name" found; fall back to Machine
		if ( !ad->LookupString( ATTR_MACHINE, buf, sizeof(buf))  ) {
			dprintf (D_ALWAYS,
					 "stats Error: Neither 'Name' nor 'Machine'"
					 " found in %s ad\n",
					 class_name );
			return false;
		}

		// If there is a slot ID, append it to Machine
		int	slot;
		if (ad->LookupInteger( ATTR_SLOT_ID, slot)) {
			formatstr(slot_buf, ":%d", slot);
		}
	}
	key.name = buf;
	key.name += slot_buf.c_str();

	// get the IP and port of the daemon
	if ( ad->LookupString (ATTR_MY_ADDRESS, buf, sizeof(buf) ) ) {
		std::string wtf( buf );
		char* host = getHostFromAddr(wtf.c_str());
		key.ip_addr = host;
		free(host);
	} else {
		return false;
	}

	// Ok.
	return true;
}


// ******************************************
//  Collector "all" stats
// ******************************************

// Constructor
CollectorStats::CollectorStats( bool enable_daemon_stats,
								int daemon_history_size )
{
	global.Init();
	global.Reconfig();
	daemonList = new CollectorDaemonStatsList( enable_daemon_stats,
											   daemon_history_size );
	m_last_garbage_time = time(NULL);
	m_garbage_interval = DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL;
}

// Destructor
CollectorStats::~CollectorStats( void )
{
	global.Clear();
	delete daemonList;
}

// Enable / disable per-daemon stats
int
CollectorStats::setDaemonStats( bool enable )
{
	return daemonList->enable( enable );
}

// Set the "daemon" history size
int
CollectorStats::setDaemonHistorySize( int new_size )
{
	return daemonList->setHistorySize( new_size );
}

// Update statistics
int
CollectorStats::update( const char *className,
						ClassAd *oldAd, ClassAd *newAd)
{
	time_t now = considerCollectingGarbage();

	// No old ad is trivial; handle it & get out
	if ( ! oldAd ) {
		global.Tick(now);
		global.updateStats(className, false, 0 );
		daemonList->updateStats( className, newAd, false, 0 );
		return 0;
	}

	// Check the sequence numbers..
	int	new_seq, old_seq;
	int	new_stime, old_stime;
	int	dropped = 0;
	bool	sequenced = false;

	// Compare sequence numbers..
	if ( newAd->LookupInteger( ATTR_UPDATE_SEQUENCE_NUMBER, new_seq ) &&
		 oldAd->LookupInteger( ATTR_UPDATE_SEQUENCE_NUMBER, old_seq ) &&
		 newAd->LookupInteger( ATTR_DAEMON_START_TIME, new_stime ) &&
		 oldAd->LookupInteger( ATTR_DAEMON_START_TIME, old_stime ) )
	{
		sequenced = true;
		int		expected = old_seq + 1;
		if (  ( new_stime == old_stime ) && ( expected != new_seq ) ) {
			dropped = ( new_seq < expected ) ? 1 : ( new_seq - expected );
		}
	}
	global.Tick(now);
	global.updateStats( className, sequenced, dropped );
	daemonList->updateStats( className, newAd, sequenced, dropped );

	// Done
	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorStats::publishGlobal( ClassAd *ad, const char * config ) const
{
	global.Publish(*ad, config);
	return 0;
}

time_t
CollectorStats::considerCollectingGarbage() {
	time_t now = time(NULL);
	if( m_garbage_interval <= 0 ) {
		return now; // garbage collection is disabled
	}
	if( now < m_last_garbage_time + m_garbage_interval ) {
		if( now < m_last_garbage_time ) {
				// last time is in the future -- clock must have been reset
			m_last_garbage_time = now;
		}
		return now;  // it is not time yet
	}

	m_last_garbage_time = now;
	if( daemonList ) {
		daemonList->collectGarbage();
	}
	return now;
}

void
CollectorDaemonStatsList::collectGarbage()
{
	if( !hashTable ) {
		return;
	}

	StatsHashKey key;
	CollectorBaseStats *value;
	int records_kept = 0;
	int records_deleted = 0;

	hashTable->startIterations();
	while( hashTable->iterate( key, value ) ) {
		if( value->wasRecentlyUpdated() ) {
				// Unset the flag.  Any update to this record before
				// the next call to this function will reset the flag.
			value->setRecentlyUpdated(false);
			records_kept++;
		}
		else {
				// This record has not been updated since the last call
				// to this function.  It is garbage.
			std::string desc;
			key.getstr( desc );
			dprintf( D_FULLDEBUG, "Removing stale collector stats for %s\n",
					 desc.c_str() );

			hashTable->remove( key );
			delete value;
			records_deleted++;
		}
	}
	dprintf( D_ALWAYS,
			 "COLLECTOR_STATS_SWEEP: kept %d records and deleted %d.\n",
			 records_kept,
			 records_deleted );
}

