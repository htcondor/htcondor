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
namespace hadoop {

struct HadoopStats {
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

const char * const ATTR_HADOOP_TYPE = "HadoopType";
const char * const ATTR_NAME_NODE = "NameNode";
const char * const ATTR_NAME_NODE_ADDRESS = "NameNodeAddress";
const char * const ATTR_DATA_NODE = "DataNode";
const char * const ATTR_JOB_TRACKER = "JobTracker";
const char * const ATTR_TASK_TRACKER = "TaskTracker";

class HadoopObject {
public:


	void update(const ClassAd &ad);

    static HadoopObject* getInstance();

	const char* getPool() {return m_pool.c_str(); }
	const char* getName() {return m_name.c_str(); }

	~HadoopObject();

private:
    HadoopObject();
	HadoopObject(HadoopObject const&);
	HadoopObject& operator=(HadoopObject const&);

    string m_pool;
    string m_name;
	Codec* m_codec;
    HadoopStats m_stats;
    static HadoopObject* m_instance;

};


}} /* aviary::hadoop */

#endif /* _SCHEDULEROBJECT_H */
