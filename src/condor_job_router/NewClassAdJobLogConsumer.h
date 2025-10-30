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

#ifndef __NEW_CLASSAD_JOB_LOG_CONSUMER_H_
#define __NEW_CLASSAD_JOB_LOG_CONSUMER_H_

#include "condor_common.h"
#include "condor_debug.h"

#include "ClassAdLogReader.h"

#include <string>

#include "classad/classad_distribution.h"


 // Copied from qmgmt.h Schedd version 25.0
const int USERRECID_qkey1 = 0;
const int JOBSETID_qkey2 = -100;
const int CLUSTERID_qkey2 = -1;
const int CLUSTERPRIVATE_qkey2 = -2;

constexpr unsigned int JOBSETID_from_key(const JOB_ID_KEY & jid) { return (unsigned int)jid.cluster; }
constexpr unsigned int USERRECID_from_key(const JOB_ID_KEY & jid) { return (unsigned int)jid.proc; }

 // JOB_ADTYPE, includes cluster ads and the header ad
class JobClassAd : public ClassAd
{
public:
	JobClassAd() = default;
	JobClassAd(const JOB_ID_KEY & _jid) : jid(_jid) { }

	JOB_ID_KEY jid{0,0};
	unsigned int userrec_id{0}; // so we can cache the userrec id for a job
};

// JOB_SET_ADTYPE
class JobSetClassAd : public ClassAd
{
public:
	JobSetClassAd() = default;
	JobSetClassAd(const JOB_ID_KEY & _jid) : jobset_id(JOBSETID_from_key(_jid)) {}

	unsigned int jobset_id{0};
};

// OWNER_ADTYPE and PROJECT_ADTYPE
class UserRecord : public ClassAd
{
public:
	UserRecord() = default;
	UserRecord(const JOB_ID_KEY & _jid, const char * mytype)
		: userrec_id(USERRECID_from_key(_jid))
		, is_user(MATCH == strcasecmp(OWNER_ADTYPE, mytype))
	{}
	unsigned int userrec_id{0};
	bool is_user{false};
};

class GenericAd : public ClassAd
{
public:
	GenericAd() = default;
	GenericAd(const JOB_ID_KEY & _jid, const char * _mytype) : jid(_jid), mytype(_mytype) {}
	JOB_ID_KEY jid{0,0};
	std::string mytype;
};



/*

	// Schedd's classadLog::New() as of Aug 2025
	JOB_ID_KEY jid(key);
	if (jid.cluster > 0) {
		if (jid.proc >= 0) {
			return new JobQueueJob(jid); // JOB_ADTYPE
		} else if (jid.proc == CLUSTERID_qkey2) {
			return new JobQueueCluster(jid); // JOB_ADTYPE
		} else if (jid.proc == CLUSTERPRIVATE_qkey2) {
			return new JobQueueClusterPrivate(jid); // CLUSTERPVT_ADTYPE
		} else if (jid.proc == JOBSETID_qkey2) {
			return new JobQueueJobSet(qkey1_to_JOBSETID(jid.cluster));
		}
	} else if (jid.cluster == USERRECID_qkey1 && jid.proc > 0) {
		if (schedd) { return schedd->jobqueue_newUserRec(jid.proc, mytype); } // OWNER_ADTYPE or PROJECT_ADTYPE
		if (YourStringNoCase(PROJECT_ADTYPE) == mytype) { return new JobQueueProjectRec(jid.proc); }
		if (YourStringNoCase(mytype) != OWNER_ADTYPE) {
			// assume future rec, make this a no-type ad
			return new JobQueueBase(jid, JobQueueBase::entry_type_unknown);
		}
		return new JobQueueUserRec(jid.proc);
	}
	return new JobQueueBase(jid, JobQueueBase::TypeOfJid(jid));

*/


class NewClassAdJobLogConsumer: public ClassAdLogConsumer
{
private:

	classad::ClassAdCollection & m_collection; // holds only JOB_ADTYPE ads (but not header ad)
	std::map<unsigned int, UserRecord> m_userAds; // holds OWNER_ADTYPE and PROJECT_ADTYPE ads
	std::map<unsigned int, JobSetClassAd> m_jobsetAds; // holds JOB_SET_ADTYPE
	std::map<std::string, GenericAd> m_genericAds; // holds all other ads (header and cluster private ads)
	ClassAdLogReader *m_reader{nullptr};

	std::map<std::string, unsigned int> m_user_to_userid;

	// rectype tell us which collection to put the ad in
	enum rectype { JOB, USERREC, JOBSET, GENERIC };

	rectype key_to_table(const char * key, JOB_ID_KEY & jid) {
		jid.cluster = jid.proc = -1;
		if ( ! key) { return GENERIC; }
		jid = JOB_ID_KEY(key);
		if (jid.cluster > 0) {
			if (jid.proc >= 0) {
				return JOB;
			} else if (jid.proc == CLUSTERID_qkey2) {
				return JOB; // actually CLUSTER
			} else if (jid.proc == CLUSTERPRIVATE_qkey2) {
				return GENERIC; // 
			} else if (jid.proc == JOBSETID_qkey2) {
				return JOBSET;
			}
		} else if (jid.cluster == USERRECID_qkey1 && jid.proc > 0) {
			return USERREC;
		}
		return GENERIC;
	}

	ClassAd * getAd(const char * key, rectype & table) {
		JOB_ID_KEY jid;
		table = key_to_table(key, jid);
		if (table == JOB) {
			return m_collection.GetClassAd(key);
		} else if (table == USERREC) {
			auto found = m_userAds.find(jid.proc);
			if (found != m_userAds.end()) return &found->second;
		} else if (table == JOBSET) {
			auto found = m_jobsetAds.find(jid.cluster);
			if (found != m_jobsetAds.end()) return &found->second;
		} else {
			auto found = m_genericAds.find(key);
			if (found != m_genericAds.end()) return &found->second;
		}
		return nullptr;
	}

public:

	NewClassAdJobLogConsumer(classad::ClassAdCollection & jobads);

	UserRecord * GetUserAd(const std::string & username);
	UserRecord * GetJobUser(ClassAd* jobad);

	void Reset();

	bool NewClassAd(const char *key,
					const char * type,
					const char * /*target*/);

	bool DestroyClassAd(const char *key);

	bool SetAttribute(const char *key,
					  const char *attr,
					  const char *value);

	bool DeleteAttribute(const char *key,
						 const char *attr);

	void SetClassAdLogReader(ClassAdLogReader *_reader) { m_reader = _reader; }
};

#endif
