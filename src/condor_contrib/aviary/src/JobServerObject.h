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

struct a;
struct a;
struct a;
struct a;
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

enum UserFileType {
	ERR = 0,
	LOG = 1,
	OUT = 2
};

struct JobSummaryFields {
	int status;
	string cmd;
	string args1;
	string args2;
	int queued;
	int last_update;
	string hold_reason;
	string release_reason;
	string remove_reason;
	string submission_id;
	string owner;
};

typedef pair<const char*,JobSummaryFields*> JobSummaryPair;
typedef vector<JobSummaryPair> JobSummaryPairCollection;

class JobServerObject
{
public:

	void update(const ClassAd &ad);

	bool getStatus(const char* id, int& status, AviaryStatus &_status);
	bool getSummary(const char* key, JobSummaryFields& _summary, AviaryStatus &_status);
	bool getJobAd(const char* id, AttributeMapType& _map, AviaryStatus &_status);
	bool fetchJobData(const char* key,
					   const UserFileType ftype,
					   std::string& fname,
					   int max_bytes,
					   bool from_end,
					   int& fsize,
					   std::string &data,
			           AviaryStatus &_status);

    ~JobServerObject();
	static JobServerObject* getInstance();

	const char* getName() { return m_name.c_str(); }
	const char* getPool() { return m_pool.c_str(); }

private:
    JobServerObject();
	JobServerObject(JobServerObject const&);
	JobServerObject& operator=(JobServerObject const&);

	string m_name;
	string m_pool;
	JobServerStats m_stats;
	Codec* m_codec;

	static JobServerObject* m_instance;

};

}} /* aviary::query */

#endif /* _JOBSERVEROBJECT_H */
