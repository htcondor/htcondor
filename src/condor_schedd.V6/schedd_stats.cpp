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

void ScheddStatistics::Reconfig()
{
    this->RecentWindowMax = param_integer("STATISTICS_WINDOW_SECONDS", 1200, 
                                          schedd_stats_window_quantum, INT_MAX);
    this->PublishFlags    = IF_BASICPUB | IF_RECENTPUB;
    char * tmp = param("STATISTICS_TO_PUBLISH");
    if (tmp) {
       this->PublishFlags = generic_stats_ParseConfigString(tmp, "SCHEDD", "SCHEDULER", this->PublishFlags);
       free(tmp);
    }
    SetWindowSize(this->RecentWindowMax);
}

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, schedd_stats_window_quantum);
}


#define SCHEDD_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "", name, as) 
#define SCHEDD_STATS_ADD_VAL(pool,name,as)     STATS_POOL_ADD_VAL(pool, "", name, as) 
#define SCHEDD_STATS_PUB_PEAK(pool,name,as)    STATS_POOL_PUB_PEAK(pool, "", name, as) 
#define SCHEDD_STATS_PUB_DEBUG(pool,name,as)   STATS_POOL_PUB_DEBUG(pool, "", name, as) 

// 
// 
void ScheddStatistics::Init() 
{ 
   static const int64_t sizes[] = {
      (int64_t)0x10000 * 0x1,        (int64_t)0x10000 * 0x4,      // 64Kb, 256Kb
      (int64_t)0x10000 * 0x10,       (int64_t)0x10000 * 0x40,     //  1Mb,   4Mb
      (int64_t)0x10000 * 0x100,      (int64_t)0x10000 * 0x400,    // 16Mb,  64Mb
      (int64_t)0x10000 * 0x1000,     (int64_t)0x10000 * 0x4000,   //256Mb,   1Gb  
      (int64_t)0x10000 * 0x10000,    (int64_t)0x10000 * 0x40000,  //  4Gb,  16Gb
      (int64_t)0x10000 * 0x100000,   (int64_t)0x10000 * 0x400000, // 64Gb, 256Gb
      };
   //JobSizes.value.set_levels(COUNTOF(sizes), sizes);
   //JobSizes.recent.set_levels(COUNTOF(sizes), sizes);

   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   this->RecentWindowMax = schedd_stats_window_quantum;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   SCHEDD_STATS_ADD_RECENT(Pool, JobsSubmitted,        IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsStarted,          IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExited,           IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompleted,        IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumTimeToStart, IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumRunningTime, IF_BASICPUB);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedNormally,        IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsKilled,                IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitException,         IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExecFailed,            IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCheckpointed,          IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShadowNoMemory,        IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRequeue,         IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsNotStarted,            IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldHold,            IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRemove,          IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCoredumped,            IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsMissedDeferralTime,    IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedAndClaimClosing, IF_BASICPUB | IF_NONZERO);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsDebugLogError,         IF_BASICPUB | IF_NONZERO);

   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsStarted,            IF_BASICPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsRecycled,           IF_VERBOSEPUB);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsReconnections,      IF_VERBOSEPUB);

   //SCHEDD_STATS_ADD_RECENT(Pool, JobSizes,                  IF_BASICPUB);

   SCHEDD_STATS_ADD_VAL(Pool, ShadowsRunning,               IF_BASICPUB);
   SCHEDD_STATS_PUB_PEAK(Pool, ShadowsRunning,              IF_BASICPUB);

   SCHEDD_STATS_PUB_DEBUG(Pool, JobsSubmitted,  IF_BASICPUB);
   SCHEDD_STATS_PUB_DEBUG(Pool, JobsStarted,  IF_BASICPUB);

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
void ScheddStatistics::Tick()
{
   int cAdvance = generic_stats_Tick(
      this->RecentWindowMax,   // RecentMaxTime
      schedd_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);
}

void ScheddStatistics::Publish(ClassAd & ad) const
{
   if ((this->PublishFlags & IF_PUBLEVEL) > 0) {
      ad.Assign("StatsLifetime", (int)StatsLifetime);
      if (this->PublishFlags & IF_VERBOSEPUB)
         ad.Assign("StatsLastUpdateTime", (int)StatsLastUpdateTime);
      if (this->PublishFlags & IF_RECENTPUB) {
         ad.Assign("RecentStatsLifetime", (int)RecentStatsLifetime);
         if (this->PublishFlags & IF_VERBOSEPUB) {
            ad.Assign("RecentWindowMax", (int)RecentWindowMax);
            ad.Assign("RecentStatsTickTime", (int)RecentStatsTickTime);
         }
      }
   }
   Pool.Publish(ad, this->PublishFlags);
}

void ScheddStatistics::Unpublish(ClassAd & ad) const
{
   ad.Delete("StatsLifetime");
   ad.Delete("StatsLastUpdateTime");
   ad.Delete("RecentStatsLifetime");
   ad.Delete("RecentStatsTickTime");
   ad.Delete("RecentWindowMax");
   Pool.Unpublish(ad);
}

//#define UNIT_TEST
#ifdef UNIT_TEST
void schedd_stats_unit_test (ClassAd * pad)
{
   ScheddStatistics stats;
   stats.Init();

   int stat_window = 300; //param_integer("WINDOWED_STAT_WIDTH", 300, 1, INT_MAX);
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
