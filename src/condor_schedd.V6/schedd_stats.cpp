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

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, schedd_stats_window_quantum);
}


#define SCHEDD_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "", name, as) 
#define SCHEDD_STATS_ADD_VAL(pool,name,as)     STATS_POOL_ADD_VAL(pool, "", name, as) 
#define SCHEDD_STATS_PUB_PEAK(pool,name,as)    STATS_POOL_PUB_PEAK(pool, "", name, as) 

// 
// 
void ScheddStatistics::Init() 
{ 
   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   this->RecentWindowMax = schedd_stats_window_quantum;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   SCHEDD_STATS_ADD_RECENT(Pool, JobsSubmitted, 0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsStarted,          0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExited,           0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCompleted,        0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumTimeToStart, 0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsAccumRunningTime, 0);

   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedNormally,        0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsKilled,                0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitException,         0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExecFailed,            0);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCheckpointed,          0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShadowNoMemory,        0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRequeue,         0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsNotStarted,            0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldHold,            0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsShouldRemove,          0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsCoredumped,            0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsMissedDeferralTime,    0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsExitedAndClaimClosing, 0 /*| IF_NONZERO*/);
   SCHEDD_STATS_ADD_RECENT(Pool, JobsDebugLogError,         0 /*| IF_NONZERO*/);

   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsStarted,            0);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsRecycled,           0);
   //SCHEDD_STATS_ADD_RECENT(Pool, ShadowExceptions,          0);
   SCHEDD_STATS_ADD_RECENT(Pool, ShadowsReconnections,      0);

   SCHEDD_STATS_ADD_VAL(Pool, ShadowsRunning,               0);
   SCHEDD_STATS_PUB_PEAK(Pool, ShadowsRunning,              0);
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
   ad.Assign("StatsLifetime", (int)StatsLifetime);
   ad.Assign("StatsLastUpdateTime", (int)StatsLastUpdateTime);
   ad.Assign("RecentStatsLifetime", (int)RecentStatsLifetime);
   ad.Assign("RecentStatsTickTime", (int)RecentStatsTickTime);
   ad.Assign("RecentWindowMax", (int)RecentWindowMax);
   Pool.Publish(ad);
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
