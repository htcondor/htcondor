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

#ifndef _CLERK_STATS_H_
#define _CLERK_STATS_H_

#include <generic_stats.h>

// this struct is used to contain statistics values for the Scheduler class.
// the values are published using the names as shown here. so for instance
// the ClassAd that we publish into will have "JobsSubmitted=32" if the
// JobsSubmitted field below contains the value 32. 
//
typedef struct CmStatistics {

   // these are used by generic tick
   time_t StatsLifetime;         // the total time covered by this set of statistics
   time_t StatsLastUpdateTime;   // last time that statistics were last updated. (a freshness time)
   time_t RecentStatsLifetime;   // actual time span of current RecentXXX data.
   time_t RecentStatsTickTime;   // last time Recent values Advanced

   stats_entry_abs<int>          TotalAds;
   stats_entry_sum_ema_rate<int64_t> UpdatesReceived;
   stats_entry_sum_ema_rate<int64_t> QueriesReceived;

   // non-published values
   time_t InitTime;            // last time we init'ed the structure
   int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
   int    RecentWindowQuantum;
   int    PublishFlags;
   classy_counted_ptr<stats_ema_config> ema_config;	// Exponential moving average config for this pool.

   // basic job counts
   StatisticsPool          Pool;          // pool of statistics probes and Publish attrib names

   // methods
   //
   void Init();
   void Clear();
   time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
   void Reconfig();
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Publish(ClassAd & ad, int flags) const;
   void Publish(ClassAd & ad, const char * config) const;

} CmStatistics;

#endif
