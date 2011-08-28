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


#define SCHEDD_STATS_ENTRY(name, as)   GENERIC_STATS_ENTRY(ScheddStatistics, "", name, as)
//#define SCHEDD_STATS_ENTRY_TQ(name, as) GENERIC_STATS_ENTRY_TQ(ScheddStatistics, "", name, as)
#define SCHEDD_STATS_ENTRY_RECENT(name, as) GENERIC_STATS_ENTRY_RECENT(ScheddStatistics, "", name, as)
#define SCHEDD_STATS_ENTRY_PEAK(name, as) GENERIC_STATS_ENTRY_PEAK(ScheddStatistics, "", name, as)

// this describes and refers to ScheddStatistics so that we can use
// generic worker functions to update and publish it.
//
static const GenericStatsEntry ScheddStatsTable[] = {
   SCHEDD_STATS_ENTRY(StatsUpdateTime,       AS_ABSTIME),
   SCHEDD_STATS_ENTRY(RecentStatsWindowTime, AS_RELTIME),

   SCHEDD_STATS_ENTRY(JobsSubmitted,        AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsStarted,          AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsExited,           AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsCompleted,        AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsAccumTimeToStart, AS_RELTIME),
   SCHEDD_STATS_ENTRY(JobsAccumRunningTime, AS_RELTIME),

   SCHEDD_STATS_ENTRY_RECENT(JobsSubmitted,  AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsStarted,    AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsExited,     AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsCompleted,  AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsAccumTimeToStart, AS_RELTIME),
   SCHEDD_STATS_ENTRY_RECENT(JobsAccumRunningTime, AS_RELTIME),

   SCHEDD_STATS_ENTRY(JobsExitedNormally,        AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsKilled,                AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsExitException,         AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsExecFailed,            AS_COUNT),
   SCHEDD_STATS_ENTRY(JobsCheckpointed,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsShadowNoMemory,        AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsShouldRequeue,         AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsNotStarted,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsShouldHold,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsShouldRemove,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsCoredumped,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsMissedDeferralTime,    AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY(JobsExitedAndClaimClosing, AS_COUNT /*| IF_NONZERO*/),

   SCHEDD_STATS_ENTRY_RECENT(JobsExitedNormally,        AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsKilled,                AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsExitException,         AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(JobsExecFailed,            AS_COUNT),

   SCHEDD_STATS_ENTRY_RECENT(JobsShadowNoMemory,        AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsCheckpointed,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsShouldRequeue,         AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsNotStarted,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsShouldRemove,          AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsExitedAndClaimClosing, AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsCoredumped,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsMissedDeferralTime,    AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsShouldHold,            AS_COUNT /*| IF_NONZERO*/),
   SCHEDD_STATS_ENTRY_RECENT(JobsDebugLogError,         AS_COUNT /*| IF_NONZERO*/),

   SCHEDD_STATS_ENTRY(ShadowsRunning,                   AS_COUNT),
   SCHEDD_STATS_ENTRY_PEAK(ShadowsRunning,              AS_COUNT),

   SCHEDD_STATS_ENTRY(ShadowsStarted,                   AS_COUNT),
   SCHEDD_STATS_ENTRY(ShadowsRecycled,                  AS_COUNT),
   //SCHEDD_STATS_ENTRY(ShadowExceptions,                 AS_COUNT),
   SCHEDD_STATS_ENTRY(ShadowsReconnections,             AS_COUNT),

   SCHEDD_STATS_ENTRY_RECENT(ShadowsStarted,            AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(ShadowsRecycled,           AS_COUNT),
   //SCHEDD_STATS_ENTRY_RECENT(ShadowExceptions,          AS_COUNT),
   SCHEDD_STATS_ENTRY_RECENT(ShadowsReconnections,      AS_COUNT),

};

#ifndef NUMELMS
 #define NUMELMS(aa) (sizeof(aa)/sizeof((aa)[0]))
#endif

static const int ScheddStatsEntryCount = NUMELMS(ScheddStatsTable);

void generic_stats_SetWindowSize(const GenericStatsEntry * pTable, int cTable, char * pdata, int window);

void ScheddStatistics::SetWindowSize(int window)
{
   this->RecentWindowMax = window;

   // TJ kill this when we get rid of timed queues.
   // generic_stats_SetWindowSize(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char*)this, window);

   int cMax = window / schedd_stats_window_quantum;
   generic_stats_SetRBMax(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char*)this, cMax);
}

// NOTE: this is NOT safe to call after you have begun collecting data!!
// 
void ScheddStatistics::Init() 
{ 
   memset((char*)this, 0, sizeof(*this)); 
   this->InitTime = time(NULL); 
}

void ScheddStatistics::Clear()
{
   generic_stats_ClearAll(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char*)this);
   this->InitTime = time(NULL);
   this->PrevUpdateTime = 0;
   this->StatsUpdateTime = 0;
   this->RecentStatsWindowTime = 0;
}

// call this when time may have changed to update StatsUpdateTime, etc.
//
// when the time since last update exceeds the time quantum, then we Advance the ring
// buffers so that we throw away the oldest value and begin accumulating the latest
// value in a new slot. 
//
// we also need to advance/update StatsUpdateTime so that it has an 
//
void ScheddStatistics::Tick()
{
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
         generic_stats_ClearRecent(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char*)this);
         this->PrevUpdateTime = this->StatsUpdateTime;
         this->RecentStatsWindowTime = this->RecentWindowMax;
         delta = 0;
         }
      else if (delta >= schedd_stats_window_quantum)
         {
         for (int ii = 0; ii < delta / schedd_stats_window_quantum; ++ii)
            {
            generic_stats_AdvanceRecent(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char*)this, 1);
            }
         this->PrevUpdateTime = now - (delta % schedd_stats_window_quantum);
         }

      time_t recent_window = (int)(this->RecentStatsWindowTime + now - this->StatsUpdateTime);
      this->RecentStatsWindowTime = min(recent_window, (time_t)this->RecentWindowMax);
      this->StatsUpdateTime = now;
      }
}

// accumulate the values of timed_queue fields into their corresponding .recent field.
//
/* 
void ScheddStatistics::Accumulate(time_t now)
{
   if (this->StatsUpdateTime != now)
      {
      Tick();
      }

   schedd statistics doesn't have any timed_queues anymore.
   time_t tmin = now - this->RecentWindowMax;
   generic_stats_AccumulateTQ(ScheddStatsTable, NUMELMS(ScheddStatsTable), (char *)this, tmin);  
}
*/

void ScheddStatistics::Publish(ClassAd & ad) const
{
   generic_stats_PublishToClassAd(ad, ScheddStatsTable, NUMELMS(ScheddStatsTable), (const char *)this);  
}

void ScheddStatistics::Unpublish(ClassAd & ad) const
{
   generic_stats_DeleteInClassAd(ad, ScheddStatsTable, NUMELMS(ScheddStatsTable), (const char *)this);  
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
