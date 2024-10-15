/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.	You may
 * obtain a copy of the License at
 * 
 *		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __DAGMAN_STATS_H__
#define __DAGMAN_STATS_H__

#include "condor_classad.h"
#include "generic_stats.h"


class DagmanStats {
public:
	DagmanStats() {
		Pool.AddProbe("EventCycleTime", &EventCycleTime, "EventCycleTime", IS_CLS_PROBE);
		Pool.AddProbe("LogProcessCycleTime", &LogProcessCycleTime, "LogProcessCycleTime", IS_CLS_PROBE);
		Pool.AddProbe("SleepCycleTime", &SleepCycleTime, "SleepCycleTime", IS_CLS_PROBE);
		Pool.AddProbe("SubmitCycleTime", &SubmitCycleTime, "SubmitCycleTime", IS_CLS_PROBE);
	};

	~DagmanStats() = default;

	void Publish(ClassAd & ad) const { Pool.Publish(ad, IF_ALWAYS); }
	void Publish(ClassAd & ad, char* attr) const { Pool.Publish(ad, attr, IF_ALWAYS); }

	stats_entry_probe<double> EventCycleTime;
	stats_entry_probe<double> LogProcessCycleTime;
	stats_entry_probe<double> SleepCycleTime;
	stats_entry_probe<double> SubmitCycleTime;

	StatisticsPool Pool;
};

#endif
