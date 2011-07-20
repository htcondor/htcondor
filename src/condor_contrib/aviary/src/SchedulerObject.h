/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SCHEDULEROBJECT_H
#define _SCHEDULEROBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"

// local includes
#include "Codec.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary::util;
using namespace aviary::codec;

namespace aviary {
namespace job {

struct SchedulerStats {
    // Properties
    string      CondorPlatform;
    string      CondorVersion;
    int64_t     DaemonStartTime;
    string      Pool;
    string      System;
    int64_t     JobQueueBirthdate;
    uint32_t    MaxJobsRunning;
    string      Machine;
    string      MyAddress;
    string      Name;

    // Statistics
    uint32_t    MonitorSelfAge;
    double      MonitorSelfCPUUsage;
    double      MonitorSelfImageSize;
    uint32_t    MonitorSelfRegisteredSocketCount;
    uint32_t    MonitorSelfResidentSetSize;
    int64_t     MonitorSelfTime;
    uint32_t    NumUsers;
    uint32_t    TotalHeldJobs;
    uint32_t    TotalIdleJobs;
    uint32_t    TotalJobAds;
    uint32_t    TotalRemovedJobs;
    uint32_t    TotalRunningJobs;
};

class SchedulerObject {
public:


	void update(const ClassAd &ad);
	bool submit(AttributeMapType& jobAdMap, string& id, string& text);
	bool setAttribute(string id,
                      string name,
                      string value,
                      string &text);
	bool hold(string id, string &reason, string &text);
	bool release(string id, string &reason, string &text);
	bool remove(string id, string &reason, string &text);

    static SchedulerObject* getInstance();

	const char* getPool() {return m_pool.c_str(); }
	const char* getName() {return m_name.c_str(); }

	~SchedulerObject();

private:
    SchedulerObject();
	SchedulerObject(SchedulerObject const&);
	SchedulerObject& operator=(SchedulerObject const&);

    string m_pool;
    string m_name;
	Codec* m_codec;
    SchedulerStats m_stats;
    static SchedulerObject* m_instance;

};


}} /* aviary::job */

#endif /* _SCHEDULEROBJECT_H */
