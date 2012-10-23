/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

// condor includes
#include "condor_attributes.h"
#include "compat_classad_util.h"
#include "stl_string_utils.h"

// local includes
#include "ODSJobLogConsumer.h"
#include "ODSHistoryProcessors.h"
#include "ODSDBNames.h"

using namespace std;
using namespace mongo;
using namespace plumage::etl;
using namespace plumage::history;

#define IS_JOB(key) ((key) && '0' != (key)[0])

ODSJobLogConsumer::ODSJobLogConsumer(const string& url): m_reader(NULL),m_history_db(NULL), m_queue_db(NULL)
{ 
    m_queue_db = new ODSMongodbOps(DB_JOBS_QUEUE);
    m_queue_db->init(url);
    m_history_db = new ODSMongodbOps(DB_JOBS_HISTORY);
    m_history_db->init(url);
}

ODSJobLogConsumer::~ODSJobLogConsumer()
{
    if (m_queue_db) delete m_queue_db;
    if (m_history_db) delete m_history_db;
}

void
ODSJobLogConsumer::Reset()
{
	dprintf(D_FULLDEBUG, "ODSJobLogConsumer::Reset()\n");

	// reload our history
	initHistoryFiles();
}

bool
ODSJobLogConsumer::NewClassAd(const char *_key,
									const char */*type*/,
									const char */*target*/)
{

	// ignore the marker and cluster
	if (!IS_JOB(_key)) {
        return true;
    }

	PROC_ID proc = getProcByString(_key);
    dprintf(D_FULLDEBUG, "ODSJobLogConsumer::NewClassAd processing - key '%d.%d'\n", proc.cluster,proc.proc);

    mongo::BSONObjBuilder bsonkey;
    bsonkey.append(ATTR_CLUSTER_ID, proc.cluster).append(ATTR_PROC_ID, proc.proc);
    return m_queue_db->createAd(bsonkey);
}

bool
ODSJobLogConsumer::DestroyClassAd(const char *_key)
{

	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

   // TODO: doubt that we want to destory job record
    PROC_ID proc = getProcByString(_key);
    dprintf ( D_FULLDEBUG, "ODSJobLogConsumer::DestroyClassAd - key '%d.%d'\n", proc.cluster,proc.proc);

    mongo::BSONObjBuilder bsonkey;
    bsonkey.append(ATTR_CLUSTER_ID, proc.cluster).append(ATTR_PROC_ID, proc.proc);
    return m_queue_db->deleteAd(bsonkey);
}

bool
ODSJobLogConsumer::SetAttribute(const char *_key,
									  const char *_name,
									  const char *_value)
{

	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

	if (0 == strcmp(_name,ATTR_NEXT_CLUSTER_NUM) ) {
		return true;
	}
	
	// need to ignore these since they were set by NewClassAd and confuse the indexing
	if ((0 == strcmp(_name,ATTR_CLUSTER_ID)) || (0 == strcmp(_name,ATTR_PROC_ID))) {
        return true;
    }

    PROC_ID proc = getProcByString(_key);
    dprintf ( D_FULLDEBUG, "ODSJobLogConsumer::SetAttribute - key '%d.%d'; '%s=%s'\n", proc.cluster,proc.proc,_name,_value);
    
    mongo::BSONObjBuilder bsonkey;
    bsonkey.append(ATTR_CLUSTER_ID, proc.cluster);
    return m_queue_db->updateAttr(bsonkey, _name, _value);
}

bool
ODSJobLogConsumer::DeleteAttribute(const char *_key,
										 const char *_name)
{
	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

    PROC_ID proc = getProcByString(_key);
    dprintf ( D_FULLDEBUG, "ODSJobLogConsumer::DeleteAttribute - key '%d.%d'; '%s'\n", proc.cluster,proc.proc,_name);

    mongo::BSONObjBuilder bsonkey;
    bsonkey.append(ATTR_CLUSTER_ID, proc.cluster).append(ATTR_PROC_ID, proc.proc);
    return m_queue_db->deleteAttr(bsonkey, _name);
}
