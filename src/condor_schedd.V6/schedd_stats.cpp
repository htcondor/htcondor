 /***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "scheduler.h"
#include "schedd_stats.h"
#include "condor_config.h"
#include "classad_helpers.h"

void ScheddStatistics::Reconfig()
{
    int quantum = param_integer("STATISTICS_WINDOW_QUANTUM_SCHEDULER", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM_SCHEDD", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
    this->RecentWindowQuantum = quantum;

    this->RecentWindowMax = param_integer("STATISTICS_WINDOW_SECONDS", 1200, quantum, INT_MAX);



    this->PublishFlags    = IF_BASICPUB | IF_RECENTPUB;
    char * tmp = param("STATISTICS_TO_PUBLISH");
    if (tmp) {
       this->PublishFlags = generic_stats_ParseConfigString(tmp, "SCHEDD", "SCHEDULER", this->PublishFlags);
       free(tmp);
    }
    SetWindowSize(this->RecentWindowMax);

    //stats_histogram_sizes::init_sizes_from_param("MAX_HIST_SIZES_LEVELS");
    //JobSizes.reconfig();
    //JobSizesGoodput.reconfig();
    //JobSizesBadput.reconfig();
}

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, this->RecentWindowQuantum);
}


#define SCHEDD_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "", name, as) 
#define SCHEDD_STATS_ADD_VAL(pool,name,as)     STATS_POOL_ADD_VAL(pool, "", name, as) 
#define SCHEDD_STATS_PUB_PEAK(pool,name,as)    STATS_POOL_PUB_PEAK(pool, "", name, as) 
#define SCHEDD_STATS_PUB_DEBUG(pool,name,as)   STATS_POOL_PUB_DEBUG(pool, "", name, as) 

static const char default_sizes_set[] = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb, 1Gb, 4Gb, 16Gb, 64Gb, 256Gb";
static const char default_lifes_set[] = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr, 6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day";

//
//
void ScheddStatistics::Init(int fOtherPool)
{
   static const int64_t sizes[] = {
      (int64_t)0x10000 * 0x1,        (int64_t)0x10000 * 0x4,      // 64Kb, 256Kb
      (int64_t)0x10000 * 0x10,       (int64_t)0x10000 * 0x40,     //  1Mb,   4Mb
      (int64_t)0x10000 * 0x100,      (int64_t)0x10000 * 0x400,    // 16Mb,  64Mb
      (int64_t)0x10000 * 0x1000,     (int64_t)0x10000 * 0x4000,   //256Mb,   1Gb  
      (int64_t)0x10000 * 0x10000,    (int64_t)0x10000 * 0x40000,  //  4Gb,  16Gb
      (int64_t)0x10000 * 0x100000,   (int64_t)0x10000 * 0x400000, // 64Gb, 256Gb
      };
   JobsRunningSizes.set_levels(sizes, COUNTOF(sizes));
   JobsCompletedSizes.set_levels(sizes, COUNTOF(sizes));
   JobsBadputSizes.set_levels(sizes, COUNTOF(sizes));

   static const time_t lifes[] = {
      (time_t)30,            (time_t) 1 * 60,        // 30 Sec,  1 Min
      (time_t) 3 * 60,       (time_t)10 * 60,        //  3 Min, 10 Min,
      (time_t)30 * 60,       (time_t) 1 * 60*60,     // 30 Min,  1 Hr,
      (time_t) 3 * 60*60,    (time_t) 6 * 60*60,     //  3 Hr    6 Hr,
      (time_t)12 * 60*60,    (time_t) 1 * 24*60*60,  // 12 Hr    1 Day,
      (time_t) 2 * 24*60*60, (time_t) 4 * 24*60*60,  //  2 Day   4 Day,
      (time_t) 8 * 24*60*60, (time_t)16 * 24*60*60,  //  8 Day  16 Day,
      };
   JobsRunningRuntimes.set_levels(lifes, COUNTOF(lifes));
   JobsCompletedRuntimes.set_levels(lifes, COUNTOF(lifes));
   JobsBadputRuntimes.set_levels(lifes, COUNTOF(lifes));

   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   if ( ! this->RecentWindowQuantum) this->RecentWindowQuantum = 1;
   this->RecentWindowMax = this->RecentWindowQuantum;

   // publish primary statistics (!fOtherPool) at BASIC (verbosity 1)
   // but publish OtherPool (BY_nnn and FOR_xxx) sets at verbosity 2 or 3.
   // Prior to 8.1.2 we didn't pay any attention to the fOtherPool
   // flag when setting the verbosity of the stat, but this resulted in
   // too many verbosity 1 stats and the schedd would take too long to
   // update the collector. So for 8.1.2 we flattened the verbosity levels
   // for the main stats so that the otherpool stats could all be at a higher level.
   int if_poolbasic = fOtherPool ? IF_VERBOSEPUB : IF_BASICPUB;
   int if_poolverbose = fOtherPool ? IF_NEVER : IF_BASICPUB;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   SCHEDD_STATS_ADD_RECENT(Pool, JobsSubmitted,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsStarted,          if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExited,           if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompleted,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, Autoclusters,         if_poolbasic);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumTimeToStart, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumBadputTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExceptionalBadputTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumRunningTime, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExecuteTime, if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumPreExecuteTime,  if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumPostExecuteTime,  if_poolbasic);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumChurnTime,      if_poolverbose);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumExecuteAltTime, if_poolverbose);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsWierdTimestamps,     if_poolverbose);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedNormally,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsKilled,                if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitException,         if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExecFailed,            if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCheckpointed,          if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShadowNoMemory,        if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRequeue,         if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsNotStarted,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldHold,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRemove,          if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCoredumped,            if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsMissedDeferralTime,    if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedAndClaimClosing, if_poolbasic | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsDebugLogError,         if_poolbasic | IF_NONZERO);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompletedSizes,        if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsBadputSizes,           if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompletedRuntimes,     if_poolbasic);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsBadputRuntimes,        if_poolbasic);

   SCHEDD_STATS_ADD_VAL(Pool, JobsRunning,                  if_poolbasic);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRunningSizes,             if_poolbasic);
   SCHEDD_STATS_ADD_VAL(Pool, JobsRunningRuntimes,          if_poolbasic);

   if ( ! fOtherPool){
      SCHEDD_STATS_ADD_RECENT(Pool, ShadowsStarted,            IF_BASICPUB);
      SCHEDD_STATS_ADD_RECENT(Pool, ShadowsRecycled,           IF_VERBOSEPUB);
      SCHEDD_STATS_ADD_RECENT(Pool, ShadowsReconnections,      IF_VERBOSEPUB);

      SCHEDD_STATS_ADD_VAL(Pool, ShadowsRunning,               IF_BASICPUB);
      SCHEDD_STATS_PUB_PEAK(Pool, ShadowsRunning,              IF_BASICPUB);

   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsSubmitted,  IF_BASICPUB);
   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsStarted,  IF_BASICPUB);
   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsCompleted,  IF_BASICPUB);
   //SCHEDD_STATS_PUB_DEBUG(Pool, JobsCompletedSizes,  IF_BASICPUB);
   }
}

void ScheddStatistics::Clear()
{
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
   Pool.Clear();
}

// call this when time may have changed to update StatsUpdateTime, etc.
//
// when the time since last update exceeds the time quantum, then we Advance the ring
// buffers so that we throw away the oldest value and begin accumulating the latest
// value in a new slot. 
//
time_t ScheddStatistics::Tick(time_t now)
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

   return now;
}

void ScheddStatistics::Publish(ClassAd & ad) const
{
   this->Publish(ad, this->PublishFlags);
}

void ScheddStatistics::Publish(ClassAd & ad, const char * config) const
{
   int flags = this->PublishFlags;
   if (config && config[0]) {
      flags = generic_stats_ParseConfigString(config, "SCHEDD", "SCHEDULER", IF_BASICPUB | IF_RECENTPUB);
   }
   this->Publish(ad, flags);
}

void ScheddStatistics::Publish(ClassAd & ad, int flags) const
{
   if ((flags & IF_PUBLEVEL) > 0) {
      ad.Assign("StatsLifetime", (int)StatsLifetime);
      ad.Assign("JobsSizesHistogramBuckets", default_sizes_set);
      ad.Assign("JobsRuntimesHistogramBuckets", default_lifes_set);
      if (flags & IF_VERBOSEPUB)
         ad.Assign("StatsLastUpdateTime", (int)StatsLastUpdateTime);
      if (flags & IF_RECENTPUB) {
         ad.Assign("RecentStatsLifetime", (int)RecentStatsLifetime);
         if (flags & IF_VERBOSEPUB) {
            ad.Assign("RecentWindowMax", (int)RecentWindowMax);
            ad.Assign("RecentStatsTickTime", (int)RecentStatsTickTime);
         }
      }
   }
   Pool.Publish(ad, flags);
}

#if 0  // obsolete
void ScheddStatistics::Unpublish(ClassAd & ad) const
{
   ad.Delete("StatsLifetime");
   ad.Delete("StatsLastUpdateTime");
   ad.Delete("RecentStatsLifetime");
   ad.Delete("RecentStatsTickTime");
   ad.Delete("RecentWindowMax");
   Pool.Unpublish(ad);
}
#endif

ScheddOtherStats::ScheddOtherStats()
	: next(NULL)
	, trigger_expr(NULL)
	, enabled(false)
	, by(false)
	, last_match_time(0)
	, lifetime(0)
{
}

ScheddOtherStats::~ScheddOtherStats()
{
	delete trigger_expr; trigger_expr = NULL;
	for (std::map<std::string, ScheddOtherStats*>::iterator it = sets.begin();
		 it != sets.end();
		 ++it) {
		delete it->second;
		it->second = NULL;
	}
	sets.clear();
}

void ScheddOtherStatsMgr::Clear()
{
	pools.clear();
}

time_t ScheddOtherStatsMgr::Tick(time_t now/*=0*/) // call this when time may have changed to update StatsUpdateTime, etc.
{
	if ( ! now) now = time(NULL);

	// update deferred counts of submitted jobs by/for expression
	CountJobsSubmitted();

	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {
		po->stats.Tick(now);
		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end(); 
				 ++it) {
				ScheddOtherStats* po2 = it->second;
				// if the statistics set has expired, disable it 
				// otherwise, call its tick method.
				if ((po2->lifetime > 0) && (po2->last_match_time > 0) &&
					(po2->last_match_time < now) && 
					((po2->last_match_time + po2->lifetime) < now)) {
					po2->enabled = false;
				} else {
					po2->stats.Tick(now);
				}
			}
		}
	}
	return now;
}

void ScheddOtherStatsMgr::Reconfig()
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {
		po->stats.Reconfig();
		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {
				// don't really want to re-parse the same config knob for each child
				// so instead, just copy the flags and windows max from the base set.
				// and resize the recent window arrays if needed.
				//
				ScheddOtherStats* po2 = it->second;
				po2->stats.PublishFlags = po->stats.PublishFlags;
				if (po2->stats.RecentWindowMax != po->stats.RecentWindowMax ||
					po2->stats.RecentWindowQuantum != po->stats.RecentWindowQuantum) {
					po2->stats.RecentWindowQuantum = po->stats.RecentWindowQuantum;
					po2->stats.SetWindowSize(po->stats.RecentWindowMax);
				}
			}
		}
	}
}

void ScheddOtherStatsMgr::Publish(ClassAd & ad)
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled)
			continue;

		po->stats.Pool.Publish(ad, po->prefix.Value(), po->stats.PublishFlags);
		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {

				ScheddOtherStats* po2 = it->second;
				if (po2->enabled) {
					po2->stats.Pool.Publish(ad, po2->prefix.Value(), po->stats.PublishFlags);
				}

			}
		}
	}
}

void ScheddOtherStatsMgr::Publish(ClassAd & ad, int flags)
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled)
			continue;

		po->stats.Pool.Publish(ad, po->prefix.Value(), flags);
		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {

				ScheddOtherStats* po2 = it->second;
				if (po2->enabled) {
					po2->stats.Pool.Publish(ad, po2->prefix.Value(), flags);
				}

			}
		}
	}
}

void ScheddOtherStatsMgr::UnpublishDisabled(ClassAd & ad)
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		if ( ! po->enabled) {
			po->stats.Pool.Unpublish(ad);
		}

		if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {

				ScheddOtherStats* po2 = it->second;
				if ( ! po2->enabled || ! po->enabled) {
					po2->stats.Pool.Unpublish(ad, po2->prefix.Value());
				}

			}
		}
	}

}


// keep track of jobs that have been submitted but not yet counted
void ScheddOtherStatsMgr::DeferJobsSubmitted(int cluster, int proc)
{
	// keep track of the last proc seen for each cluster 
	// since we processed (and cleared) the deferred_jobs set
	if (pools.getNumElements() > 0) {
		deferred_jobs_submitted[cluster] = proc;
	}
}

// from condor_qmgr.h...
extern ClassAd *GetJobAd(int cluster_id, int proc_id, bool expStardAttrs = false, bool persist_expansions = true );
extern void FreeJobAd(ClassAd *&ad);

// finish deferred counting of submitted jobs.
void ScheddOtherStatsMgr::CountJobsSubmitted()
{
	if ( ! deferred_jobs_submitted.empty() &&
		pools.getNumElements() > 0) {
		time_t now = time(NULL);

		for (std::map<int, int>::iterator it = deferred_jobs_submitted.begin();
			 it != deferred_jobs_submitted.end();
			 ++it) {
			int cluster = it->first;
			int last_proc = it->second;
			for (int proc = 0; proc <= last_proc; ++proc) {
				ClassAd * job_ad = GetJobAd(cluster, proc);
				if (job_ad) {
					ScheddOtherStats *po = Matches(*job_ad, now);
					while (po) {
						po->stats.JobsSubmitted += 1;
						po = po->next;
					}
					FreeJobAd(job_ad);
				}
			}
		}
	}
	deferred_jobs_submitted.clear();
}

// reset jobs-running counters/histograms in preparation for count_jobs
void ScheddOtherStatsMgr::ResetJobsRunning()
{
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		// don't bother to reset disabled stats.
		if ( ! po->enabled)
			continue;

		po->stats.JobsRunning = 0;
		po->stats.JobsRunningRuntimes = 0;
		po->stats.JobsRunningSizes = 0;

		if (po->sets.empty())
			continue;

		for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				it != po->sets.end();
				++it) {

			ScheddOtherStats* po2 = it->second;
			if (po2->enabled) {
				po2->stats.JobsRunning = 0;
				po2->stats.JobsRunningRuntimes = 0;
				po2->stats.JobsRunningSizes = 0;
			}
		}
	}
}



/* don't use these.
void ScheddOtherStatsMgr::SetWindowSize(int window)
{
}

void Publish(ClassAd & ad, const char * config) const
{
}
*/

bool ScheddOtherStatsMgr::Enable(
	const char * pre, 
	const char * trig, 
	bool stats_by_trig_value, /*=false*/
	time_t lifetime /*=0*/)
{
	ExprTree *tree = NULL;
	classad::ClassAdParser  parser;
	if ( ! parser.ParseExpression(trig, tree)) {
		EXCEPT("Schedd_stats: Invalid trigger expression for '%s' stats: '%s'\n", pre, trig);
	}

	bool was_enabled = false;
	ScheddOtherStats* po = NULL;
	if (pools.lookup(pre, po) < 0) {
		po = new ScheddOtherStats();
		ASSERT(po);
		po->prefix = pre;
		po->stats.Init(true);
		po->stats.Reconfig();
		pools.insert(pre, po);
	} else {
		was_enabled = po->enabled;
		if (po->by != stats_by_trig_value ||
			po->trigger != trig) {
			po->stats.Clear();
		}
		delete po->trigger_expr; po->trigger_expr = NULL;
	}

	po->enabled = true;
	po->by = stats_by_trig_value;
	po->trigger = trig; // update trigger.
	po->trigger_expr = tree;
	po->lifetime = lifetime;
	return was_enabled;
}

bool ScheddOtherStatsMgr::DisableAll()
{
	bool any_enabled = false;
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {
		any_enabled = any_enabled && po->enabled;
		po->enabled = false;
	}
	return any_enabled;
}

bool ScheddOtherStatsMgr::RemoveDisabled()
{
	bool any_removed = false;
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {
		if ( ! po->enabled) {
			MyString key;
			pools.getCurrentKey(key);
			pools.remove(key);
			delete po;
			any_removed = true;
		} else if ( ! po->sets.empty()) {
			for (std::map<std::string, ScheddOtherStats*>::iterator it = po->sets.begin();
				 it != po->sets.end();
				 ++it) {
				if ( ! po->enabled) {
					delete it->second;
					it->second = NULL;
					po->sets.erase(it++);
				}
			}
		}
	}
	return any_removed;
}

bool ScheddOtherStatsMgr::AnyEnabled()
{
	if (pools.getNumElements() <= 0)
		return false;
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {
		if (po->enabled)
			return true;
	}
	return false;
}


// returns a linked list of matches, the caller does not free
// the returned list, the returned list is only valid until
// the next time a method on this class is called.
ScheddOtherStats * ScheddOtherStatsMgr::Matches(ClassAd & ad, time_t updateTime)
{
	ScheddOtherStats* poLast = NULL;
	ScheddOtherStats* po = NULL;
	pools.startIterations();
	while (pools.iterate(po)) {

		// if we have not yet built a parse tree for this expression, do that now.
		if ( ! po->trigger_expr) {
			ExprTree *tree = NULL;
			classad::ClassAdParser  parser;
			if ( ! parser.ParseExpression(po->trigger, tree)) {
				dprintf(D_ALWAYS, "Schedd_stats: Failed to parse expression for %s stats: '%s'\n", 
						po->prefix.Value(), po->trigger.Value());
				continue;
			}
			po->trigger_expr = tree;
		}

		classad::Value val;
		if ( ! ad.EvaluateExpr(po->trigger_expr, val)) 
			continue;

		// check to see if this is a _FOR_XXX type or _BY_XXX type. 
		// if the expression evaluates to a bool, then count it
		// in the overall stats collection (like it was a _FOR_XXX type)
		// even if it's a _BY_XXX type.
		//
		if ( ! po->by || val.IsBooleanValue()) {
			bool bb;
			if (val.IsBooleanValueEquiv(bb) && bb) {
				po->last_match_time = updateTime;
				po->next = poLast;
				poLast = po;
			}
			continue;
		} 

		// If we get here, this is a _BY_xxx type stats pool. 
		// for this type we want to separate set of counters for each
		// unique value that the trigger expression evaluates to
		//
		if (val.IsUndefinedValue()) {
			continue;
		}

		// convert the expression result to a string so we 
		// can use it to do a lookup in the po->sets map
		//
		std::string str;
		if (val.IsStringValue(str)) {
			// str already has value.
		} else if (val.IsErrorValue()) {
			str = "ERROR"; // so that errors become visible
		} else {
			classad::Value strVal;
			if ( ! convertValueToStringValue(val, strVal)) {
				// ignore things that can't be converted to strings
				continue;
			}
			strVal.IsStringValue(str);
		}

		// treat empty strings as not matching anything.
		//
		if (str.empty())
			continue;

		// for non-empty strings, we want to add the current
		// statistics set, and the set at po->sets[str];
		// we may need to create the set at po->sets[str] if 
		// this is the first time we have seen this value of str

		// add the current stats pool to the return list
		// this will accumulate and publish the attributes
		// that will contain totals for all counters in the stats set for "Prefix"
		po->last_match_time = updateTime;
		po->next = poLast;
		poLast = po;

		// locate or create a stats set for the key str
		ScheddOtherStats* po2 = NULL;
		std::map<std::string, ScheddOtherStats*>::const_iterator it = po->sets.find(str);
		if (it == po->sets.end()) {
			po2 = new ScheddOtherStats();
			ASSERT(po2);
			po->sets[str] = po2;

			po2->prefix.formatstr("%s_%s_", po->prefix.Value(), str.c_str());
			cleanStringForUseAsAttr(po2->prefix, '_', false);

			po2->stats.Init(true);
			po2->stats.PublishFlags = po->stats.PublishFlags;
			po2->stats.RecentWindowQuantum = po->stats.RecentWindowQuantum;
			po2->stats.SetWindowSize(po->stats.RecentWindowMax);
			po2->lifetime = po->lifetime;

			po2->enabled = true;
			po2->by = false;
			po2->next = NULL;
			po2->trigger_expr = NULL;
		} else {
			po2 = it->second;
		}

		// add the by<str> stats to the return list.
		// This will accumulate and publish the "Prefix_str_JobsEtc" attributes
		if (po2) {
			po2->last_match_time = updateTime;
			po2->next = poLast;
			poLast = po2;
		}

	} // end while

	// return the linked list of matches
	return poLast;
}


//#define UNIT_TEST
#ifdef UNIT_TEST
void schedd_stats_unit_test (ClassAd * pad)
{
   ScheddStatistics stats;
   stats.Init();

   int stat_window = 300; //param_integer("STATISTICS_WINDOW_SECONDS", 1200, 1, INT_MAX);
   stats.SetWindowSize(stat_window);

   stats.Tick();
   stats.JobsStarted += 1;
   stats.JobsCompleted += 1;
   stats.JobSizes += 99;

   stats.ShadowsRunning = 32;

   stats.Tick();

   stats.ShadowsRunning = 30;
   stats.JobsStarted += 1;
   stats.JobsCompleted += 1;

   if (pad) {
      stats.Publish(*pad);
   }
}
#endif
