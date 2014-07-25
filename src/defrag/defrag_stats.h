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

#ifndef _DEFRAG_STATS_H
#define _DEFRAG_STATS_H

#include <generic_stats.h>

class DefragStats {
 public:
	DefragStats();

	StatisticsPool Pool;
    time_t InitTime;
	time_t Lifetime;            // the total time covered by this set of statistics
    time_t LastUpdateTime;      // last time that statistics were updated. (a freshness time)
    time_t RecentLifetime;      // actual time span of current RecentXXX data.
    time_t RecentTickTime;      // last time Recent values Advanced
	int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
	int    RecentWindowQuantum; // span of time between samples in recent buffer
	int    PublishFlags;

	stats_entry_recent<int> DrainSuccesses;
	stats_entry_recent<int> DrainFailures;

	stats_entry_abs<int> MachinesDraining;
	stats_entry_abs<int> WholeMachines;

	stats_entry_abs<double> AvgDrainingBadput;
	stats_entry_abs<double> AvgDrainingUnclaimed;

	stats_entry_abs<double> MeanDrainedArrival;
	stats_entry_abs<double> MeanDrainedArrivalSD;

	stats_entry_recent<int> DrainedMachines;

	void Init();
	void Clear();
	time_t Tick(time_t now=0);
	void Reconfig();
	void SetWindowSize(int window,int quantum);
	void Publish(ClassAd & ad) const;
	void Publish(ClassAd & ad, int flags) const;
};

#endif
