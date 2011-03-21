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

#ifndef _JOBSERVEROBJECT_H
#define _JOBSERVEROBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"

// local includes
#include "Codec.h"

using namespace std;
using namespace compat_classad;
using namespace aviary::codec;

namespace aviary {
namespace query {

struct JobServerStats {
    // Properties
    string      CondorPlatform;
    string      CondorVersion;
    int64_t     DaemonStartTime;
    string      Pool;
    string      System;
    uint32_t    MaxJobsRunning;
    string      Machine;
    string      MyAddress;
    string      Name;
	string		PublicNetworkIpAddr;

    // Statistics
    uint32_t    MonitorSelfAge;
    double      MonitorSelfCPUUsage;
    double      MonitorSelfImageSize;
    uint32_t    MonitorSelfRegisteredSocketCount;
    uint32_t    MonitorSelfResidentSetSize;
    int64_t     MonitorSelfTime;
    uint32_t    NumUsers;
};

class JobServerObject
{
public:

	void update(const ClassAd &ad);

	bool getSummary(string id, AttributeMapType& _map, string &text);
	bool getJobAd(string id, AttributeMapType& _map, string &text);
	bool fetchJobData(string id, string &file, int32_t start, int32_t end,
				string &data, string &text);

    ~JobServerObject();
	static JobServerObject* getInstance();

private:
    JobServerObject();
	JobServerObject(JobServerObject const&){};
	JobServerObject& operator=(JobServerObject const&){};

	string m_name;
	JobServerStats m_stats;
	Codec* m_codec;

	static JobServerObject* m_instance;

};

}} /* aviary::query */

#endif /* _JOBSERVEROBJECT_H */
