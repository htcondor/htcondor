/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"

#include "clerk_stats.h"
#include "clerk.h"

void CmStatistics::Reconfig()
{
	int quantum = param_integer("STATISTICS_WINDOW_QUANTUM_" MY_SUBSYSTEM, INT_MAX, 1, INT_MAX);
	if (quantum >= INT_MAX)
		quantum = param_integer("STATISTICS_WINDOW_QUANTUM_COLLECTOR", INT_MAX, 1, INT_MAX);
	if (quantum >= INT_MAX)
		quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
	this->RecentWindowQuantum = quantum;

	this->RecentWindowMax = param_integer("STATISTICS_WINDOW_SECONDS", 1200, quantum, INT_MAX);

	this->PublishFlags    = IF_BASICPUB | IF_RECENTPUB;
	char * tmp = param("STATISTICS_TO_PUBLISH");
	if (tmp) {
		this->PublishFlags = generic_stats_ParseConfigString(tmp, MY_SUBSYSTEM, "COLLECTOR", this->PublishFlags);
		free(tmp);
	}
	SetWindowSize(this->RecentWindowMax);
}

void CmStatistics::SetWindowSize(int window)
{
	this->RecentWindowMax = window;
	Pool.SetRecentMax(window, this->RecentWindowQuantum);
}

void CmStatistics::Clear()
{
	this->InitTime = time(NULL);
	this->StatsLifetime = 0;
	this->StatsLastUpdateTime = 0;
	this->RecentStatsTickTime = 0;
	this->RecentStatsLifetime = 0;
	Pool.Clear();
}

#define CM_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "", name, as) 
#define CM_STATS_ADD_VAL(pool,name,as)     STATS_POOL_ADD_VAL(pool, "", name, as) 

void CmStatistics::Init()
{
	Clear();
	// default window size to 1 quantum, we may set it to something else later.
	if ( ! this->RecentWindowQuantum) this->RecentWindowQuantum = 1;
	this->RecentWindowMax = this->RecentWindowQuantum;

	CM_STATS_ADD_VAL(Pool, TotalAds, IF_BASICPUB);
}

void CmStatistics::Publish(ClassAd & ad) const
{
   this->Publish(ad, this->PublishFlags);
}

void CmStatistics::Publish(ClassAd & ad, const char * config) const
{
   int flags = this->PublishFlags;
   if (config && config[0]) {
      flags = generic_stats_ParseConfigString(config, MY_SUBSYSTEM, "COLLECTOR", IF_BASICPUB | IF_RECENTPUB);
   }
   this->Publish(ad, flags);
}

void CmStatistics::Publish(ClassAd & ad, int flags) const
{
   if ((flags & IF_PUBLEVEL) > 0) {
      ad.Assign("StatsLifetime", (int)StatsLifetime);
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

