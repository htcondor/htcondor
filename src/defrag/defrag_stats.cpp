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

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "util_lib_proto.h"
#include "defrag_stats.h"

DefragStats::DefragStats():
	InitTime(0),
	Lifetime(0),
	LastUpdateTime(0),
	RecentLifetime(0),
	RecentTickTime(0),
	RecentWindowMax(1),
	RecentWindowQuantum(1),
	PublishFlags(IF_BASICPUB | IF_RECENTPUB)
{
}

void DefragStats::SetWindowSize(int window,int quantum)
{
   this->RecentWindowMax = window;
   this->RecentWindowQuantum = quantum;
   Pool.SetRecentMax(window, quantum);
}

void DefragStats::Init() 
{ 
   Clear();
   // default window size to 1 quantum, we may set it to something else later.
   this->RecentWindowMax = this->RecentWindowQuantum;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", DrainSuccesses, IF_BASICPUB);
   STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", DrainFailures, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", MachinesDraining, IF_BASICPUB);
   STATS_POOL_PUB_PEAK(Pool, "", MachinesDraining, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", WholeMachines, IF_BASICPUB);
   STATS_POOL_PUB_PEAK(Pool, "", WholeMachines, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", AvgDrainingBadput, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", AvgDrainingUnclaimed, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", MeanDrainedArrival, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", MeanDrainedArrivalSD, IF_BASICPUB);
   STATS_POOL_ADD_VAL(Pool, "", DrainedMachines, IF_BASICPUB);
}

void DefragStats::Clear()
{
   this->InitTime = time(NULL);
   this->Lifetime = 0;
   this->LastUpdateTime = 0;
   this->RecentTickTime = 0;
   this->RecentLifetime = 0;
   Pool.Clear();
}

// call this when time may have changed to update LastUpdateTime, etc.
//
// when the time since last update exceeds the time quantum, then we advance the ring
// buffers so that we throw away the oldest value and begin accumulating the latest
// value in a new slot. 
//
time_t DefragStats::Tick(time_t now)
{
   if ( ! now) now = time(NULL);

   int cAdvance = generic_stats_Tick(
      now,
      this->RecentWindowMax,
      this->RecentWindowQuantum,
      this->InitTime,
      this->LastUpdateTime,
      this->RecentTickTime,
      this->Lifetime,
      this->RecentLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);

   return now;
}

void DefragStats::Publish(ClassAd & ad) const
{
   this->Publish(ad, this->PublishFlags);
}

void DefragStats::Publish(ClassAd & ad, int flags) const
{
   if ((flags & IF_PUBLEVEL) > 0) {
      ad.Assign("StatsLifetime", (int)Lifetime);
      if (flags & IF_VERBOSEPUB)
         ad.Assign("StatsLastUpdateTime", (int)LastUpdateTime);
      if (flags & IF_RECENTPUB) {
         ad.Assign("RecentStatsLifetime", (int)RecentLifetime);
         if (flags & IF_VERBOSEPUB) {
            ad.Assign("RecentWindowMax", (int)RecentWindowMax);
            ad.Assign("RecentStatsTickTime", (int)RecentTickTime);
         }
      }
   }
   Pool.Publish(ad, flags);
}
