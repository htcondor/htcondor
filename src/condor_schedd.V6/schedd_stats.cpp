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

#define SCHEDD_STATS_PUB(name, as)         GENERIC_STATS_PUB(ScheddStatistics, "", name, as)
#define SCHEDD_STATS_PUB_RECENT(name, as)  GENERIC_STATS_PUB_RECENT(ScheddStatistics, "", name, as)
#define SCHEDD_STATS_PUB_PEAK(name, as)    GENERIC_STATS_PUB_PEAK(ScheddStatistics, "", name, as)
#define SCHEDD_STATS_PUB_TYPE(name, T, as) GENERIC_STATS_PUB_TYPE(ScheddStatistics, "", name, as, T)

// this describes and refers to ScheddStatistics so that we can use
// generic worker functions to update and publish it.
//
static const GenericStatsPubItem ScheddStatsPub[] = {
   SCHEDD_STATS_PUB_TYPE(StatsLifetime,       time_t,  AS_RELTIME),
   SCHEDD_STATS_PUB_TYPE(StatsLastUpdateTime, time_t,  AS_ABSTIME),
   SCHEDD_STATS_PUB_TYPE(RecentStatsLifetime, time_t,  AS_RELTIME),

   SCHEDD_STATS_PUB(JobsSubmitted,        AS_COUNT),
   SCHEDD_STATS_PUB(JobsStarted,          AS_COUNT),
   SCHEDD_STATS_PUB(JobsExited,           AS_COUNT),
   SCHEDD_STATS_PUB(JobsCompleted,        AS_COUNT),
   SCHEDD_STATS_PUB(JobsAccumTimeToStart, AS_RELTIME),
   SCHEDD_STATS_PUB(JobsAccumRunningTime, AS_RELTIME),

   SCHEDD_STATS_PUB_RECENT(JobsSubmitted,  AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsStarted,    AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExited,     AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsCompleted,  AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsAccumTimeToStart, AS_RELTIME),
   SCHEDD_STATS_PUB_RECENT(JobsAccumRunningTime, AS_RELTIME),

   SCHEDD_STATS_PUB(JobsExitedNormally,        AS_COUNT),
   SCHEDD_STATS_PUB(JobsKilled,                AS_COUNT),
   SCHEDD_STATS_PUB(JobsExitException,         AS_COUNT),
   SCHEDD_STATS_PUB(JobsExecFailed,            AS_COUNT),
   SCHEDD_STATS_PUB(JobsCheckpointed,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsShadowNoMemory,        AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsShouldRequeue,         AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsNotStarted,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsShouldHold,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsShouldRemove,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsCoredumped,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsMissedDeferralTime,    AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB(JobsExitedAndClaimClosing, AS_COUNT /*| IF_NONZERO*/),

   SCHEDD_STATS_PUB_RECENT(JobsExitedNormally,        AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsKilled,                AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExitException,         AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExecFailed,            AS_COUNT),

   SCHEDD_STATS_PUB_RECENT(JobsShadowNoMemory,        AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsCheckpointed,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsShouldRequeue,         AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsNotStarted,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsShouldRemove,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsExitedAndClaimClosing, AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsCoredumped,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsMissedDeferralTime,    AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsShouldHold,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_PUB_RECENT(JobsDebugLogError,         AS_COUNT /*| IF_NONZERO*/),

   SCHEDD_STATS_PUB(ShadowsRunning,                   AS_COUNT),
   SCHEDD_STATS_PUB_PEAK(ShadowsRunning,              AS_COUNT),

   SCHEDD_STATS_PUB(ShadowsStarted,                   AS_COUNT),
   SCHEDD_STATS_PUB(ShadowsRecycled,                  AS_COUNT),
   //SCHEDD_STATS_PUB(ShadowExceptions,                 AS_COUNT),
   SCHEDD_STATS_PUB(ShadowsReconnections,             AS_COUNT),

   SCHEDD_STATS_PUB_RECENT(ShadowsStarted,            AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(ShadowsRecycled,           AS_COUNT),
   //SCHEDD_STATS_PUB_RECENT(ShadowExceptions,          AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(ShadowsReconnections,      AS_COUNT),

};

static const int ScheddStatsEntryCount = COUNTOF(ScheddStatsPub);

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   generic_stats_SetRecentMax(ScheddStatsPub, 
                              COUNTOF(ScheddStatsPub), 
                              (char *)this,
                              window, 
                              schedd_stats_window_quantum);
}

// NOTE: this is NOT safe to call after you have begun collecting data!!
// 
void ScheddStatistics::Init() 
{ 
   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   this->RecentWindowMax = schedd_stats_window_quantum;
}

void ScheddStatistics::Clear()
{
   generic_stats_Clear(ScheddStatsPub, COUNTOF(ScheddStatsPub), (char*)this);
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
}

// call this when time may have changed to update StatsUpdateTime, etc.
//
// when the time since last update exceeds the time quantum, then we Advance the ring
// buffers so that we throw away the oldest value and begin accumulating the latest
// value in a new slot. 
//
void ScheddStatistics::Tick()
{
#if 1

const GenericStatsPubItem StatsTick[] = {
   SCHEDD_STATS_PUB_RECENT(JobsSubmitted,  AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsStarted,    AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExited,     AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsCompleted,  AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsAccumTimeToStart, AS_RELTIME),
   SCHEDD_STATS_PUB_RECENT(JobsAccumRunningTime, AS_RELTIME),

   SCHEDD_STATS_PUB_RECENT(JobsExitedNormally,        AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsKilled,                AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExitException,         AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExecFailed,            AS_COUNT),

   SCHEDD_STATS_PUB_RECENT(JobsShadowNoMemory,        AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsCheckpointed,          AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsShouldRequeue,         AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsNotStarted,            AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsShouldRemove,          AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsExitedAndClaimClosing, AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsCoredumped,            AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsMissedDeferralTime,    AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsShouldHold,            AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(JobsDebugLogError,         AS_COUNT),

   SCHEDD_STATS_PUB_RECENT(ShadowsStarted,            AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(ShadowsRecycled,           AS_COUNT),
   //SCHEDD_STATS_PUB_RECENT(ShadowExceptions,          AS_COUNT),
   SCHEDD_STATS_PUB_RECENT(ShadowsReconnections,      AS_COUNT),
   };

   int cAdvance = generic_stats_Tick(
      StatsTick, COUNTOF(StatsTick), 
      (char*)this,
      this->RecentWindowMax,   // RecentMaxTime
      schedd_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);
#else

   time_t now = time(NULL);

   // when working from freshly initialized stats, the first Tick should not Advance.
   //
   if (this->StatsUpdateTime == 0)
      {
      this->StatsUpdateTime = now;
      this->PrevUpdateTime = now;
      this->RecentStatsWindowTime = 0;
      return;
      }

   // whenever 'now' changes, we want to check to see how much time has passed
   // since the last Advance, and if that time exceeds the quantum, we advance.
   //
   if (this->StatsUpdateTime != now) 
      {
      time_t delta = now - this->PrevUpdateTime;

      // if the time since last update exceeds the window size, just throw away the recent data
      if (delta >= this->RecentWindowMax)
         {
         generic_stats_ClearRecent(ScheddStatsTable, COUNTOF(ScheddStatsTable), (char*)this);
         this->PrevUpdateTime = this->StatsUpdateTime;
         this->RecentStatsWindowTime = this->RecentWindowMax;
         delta = 0;
         }
      else if (delta >= schedd_stats_window_quantum)
         {
         for (int ii = 0; ii < delta / schedd_stats_window_quantum; ++ii)
            {
            generic_stats_AdvanceRecent(ScheddStatsTable, COUNTOF(ScheddStatsTable), (char*)this, 1);
            }
         this->PrevUpdateTime = now - (delta % schedd_stats_window_quantum);
         }

      time_t recent_window = (int)(this->RecentStatsWindowTime + now - this->StatsUpdateTime);
      this->RecentStatsWindowTime = min(recent_window, (time_t)this->RecentWindowMax);
      this->StatsUpdateTime = now;
      }
#endif
}

void ScheddStatistics::Publish(ClassAd & ad) const
{
   generic_stats_PublishToClassAd(ad, ScheddStatsPub, COUNTOF(ScheddStatsPub), (const char *)this);  
}

void ScheddStatistics::Unpublish(ClassAd & ad) const
{
   generic_stats_DeleteInClassAd(ad, ScheddStatsPub, COUNTOF(ScheddStatsPub), (const char *)this);  
}

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

   // stats.Accumulate();

   if (pad) {
      stats.Publish(*pad);
   }
}
