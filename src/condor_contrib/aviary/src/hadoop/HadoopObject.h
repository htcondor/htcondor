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

#ifndef _HADOOP_OBJECT_H
#define _HADOOP_OBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"

// local includes
#include "ClassadCodec.h"
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

typedef enum htype
{
    NAME_NODE=0,
    DATA_NODE,
    JOB_TRACKER,
    TASK_TRACKER
}tHadoopType;

///< Input 
typedef struct href
{
    string id;          ///< ClusterId
    string ipcid;       ///< ipc url
    string http;        /// http url
    tHadoopType type;   ///< input type
    string tarball;     ///< input tarball
}tHadoopRef;

///< Initialization structure for starting a hadoop job
typedef struct hinit
{
    unsigned int count;  ///< input count
    tHadoopRef idref;    ///< input(ipcid)
    string newcluster;   ///< output new clusterid
    string owner;        ///< owner field
    string description;  ///< description field
    bool unmanaged;      ///< unmanaged placeholder.
}tHadoopInit;

typedef struct hstatus
{
    string owner;
    string description;
    int uptime;
    string state;
    tHadoopRef idref;
    tHadoopRef idparent;
    int qdate;
    string http;
}tHadoopJobStatus;

const char * const ATTR_HADOOP_TYPE = "HadoopType";
const char * const ATTR_NAME_NODE = "NameNode";
const char * const ATTR_NAME_NODE_ADDRESS = "NameNodeAddress";
const char * const ATTR_DATA_NODE = "DataNode";
const char * const ATTR_JOB_TRACKER = "JobTracker";
const char * const ATTR_TASK_TRACKER = "TaskTracker";
const char * const ATTR_HADOOP_BIN_VERSION = "HadoopVersion";
const char * const ATTR_HTTP_ADDRESS = "HTTPAddress";
const char * const ATTR_HADOOP_DESCRIPTION = "JobDescription";
const char * const ATTR_HADOOP_REQUEST_NAMENODE = "RequestNameNode";
const char * const ATTR_HADOOP_REQUEST_DATANODE = "RequestDataNode";
const char * const ATTR_HADOOP_REQUEST_JOBTRACKER = "RequestJobTracker";
const char * const ATTR_HADOOP_REQUEST_TASKTRACKER = "RequestTaskTracker";

const char * const HADOOP_NAMENODE_REQUIREMENTS="NAME_NODE_REQUIREMENTS";
const char * const HADOOP_DATANODE_REQUIREMENTS="DATA_NODE_REQUIREMENTS";
const char * const HADOOP_JOBTRACKER_REQUIREMENTS="JOB_TRACKER_REQUIREMENTS";
const char * const HADOOP_TASKTRACKER_REQUIREMENTS="TASK_TRACKER_REQUIREMENTS";

class HadoopObject 
{
public:

    void update(const ClassAd &ad);
    static HadoopObject* getInstance();

    const char* getPool() {return m_pool.c_str(); }
    const char* getName() {return m_name.c_str(); }

    /**
     * start() - Will attempt to start the appropriate hadoop job
     */
    int start( tHadoopInit & hInit );

    /**
     * stop() - Will stop a running instance.
     */
    bool stop( const tHadoopRef & hRef );
    
    /**
     * status() - Will get the status results on a job
     */
    bool query (const tHadoopRef & hRef, std::vector<tHadoopJobStatus> & vhStatus);
    
    /**
     * Used to obtain some user readable error message
     */
    void getLastError(string & szError) 
    { szError = m_lasterror;
      m_lasterror.clear();
    }
    
    ~HadoopObject();

private:
    HadoopObject();
    HadoopObject(HadoopObject const&);
    HadoopObject& operator=(HadoopObject const&);
    
    bool status (ClassAd* cAd, const tHadoopType & type, tHadoopJobStatus & hStatus);
    
    string m_pool;
    string m_name;
    string m_lasterror;
    ClassadCodec* m_codec;
    HadoopStats m_stats;
    static HadoopObject* m_instance;

};


}} /* aviary::hadoop */

#endif /* _HADOOP_OBJECT_H */
