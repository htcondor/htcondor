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


#include "condor_common.h"
#include "condor_debug.h"
#include "compat_classad_util.h"
#include "condor_attributes.h"

#include "NewClassAdJobLogConsumer.h"

#include <string>

#include "classad/classad_distribution.h"

NewClassAdJobLogConsumer::NewClassAdJobLogConsumer(classad::ClassAdCollection & jobads) : m_collection(jobads), m_reader(0) { }


void
NewClassAdJobLogConsumer::Reset()
{
	classad::LocalCollectionQuery query;
	std::string key;

	query.Bind(&m_collection);
	query.Query("root", NULL);
	query.ToFirst();

	if (query.Current(key)) {
		do {
			m_collection.RemoveClassAd(key);
		} while(query.Next(key));
	}

	m_userAds.clear();
	m_jobsetAds.clear();
	m_genericAds.clear();
	m_user_to_userid.clear();
}

bool
NewClassAdJobLogConsumer::NewClassAd(const char *key,
									 const char * mytype,
									 const char * /*target*/)
{
	classad::ClassAd* ad;
	bool using_existing_ad = false;

	JOB_ID_KEY jid;
	auto table = key_to_table(key, jid);
	if (table != JOB) {
		// handle the non job ads, storing them into the appropriate table
		if (table == USERREC) {
			m_userAds.emplace(USERRECID_from_key(jid), UserRecord{jid, mytype});
		} else if (table == JOBSET) {
			m_jobsetAds.emplace(JOBSETID_from_key(jid), JobSetClassAd{jid});
		} else {
			m_genericAds.emplace(key, GenericAd{jid, mytype});
		}
		return true;
	}

	// now handle all of the records that go into the jobs table
	// this is both cluster ads and proc ads

	ad = m_collection.GetClassAd(key);
	if (ad && (jid.proc == CLUSTERID_qkey2)) {
			// This is a cluster ad created in advance so that
			// children could be chained from it.
		using_existing_ad = true;
	} else {
		ad = new JobClassAd(jid);
	}

		// Chain this ad to its parent, if any.
	if (jid.proc >= 0) {
		char cluster_key[PROC_ID_STR_BUFLEN];
			// NOTE: cluster keys start with a 0: e.g. 021.-1
		snprintf(cluster_key, sizeof(cluster_key), "0%d.-1", jid.cluster);
		auto * cluster_ad = m_collection.GetClassAd(cluster_key);
		if (!cluster_ad) {
				// The cluster ad doesn't exist yet.  This is expected.
				// For example, once the job queue is rewritten (i.e.
				// truncated), the order of entries in it is arbitrary.
			cluster_ad = new JobClassAd(JOB_ID_KEY(jid.cluster,CLUSTERID_qkey2));
			if (!m_collection.AddClassAd(cluster_key, cluster_ad)) {
				dprintf(D_ALWAYS,
						"error processing %s: failed to add '%s' to "
						"ClassAd collection.\n",
						m_reader ? m_reader->GetClassAdLogFileName() : "(null)", cluster_key);
				delete ad;
				return true; // XXX: why is this ok?
			}
		}

		ad->ChainToAd(cluster_ad);
	}

	if (!using_existing_ad) {
		if (!m_collection.AddClassAd(key, ad)) {
			dprintf(D_ALWAYS,
					"error processing %s: failed to add '%s' to "
					"ClassAd collection.\n",
					m_reader ? m_reader->GetClassAdLogFileName() : "(null)",
					key);
				// XXX: why is this ok?
		}
	}

	return true;
}

bool
NewClassAdJobLogConsumer::DestroyClassAd(const char *key)
{
	JOB_ID_KEY jid;
	auto table = key_to_table(key, jid);
	if (table == JOB) {
		m_collection.RemoveClassAd(key);
	} else if (table == USERREC) {
		m_userAds.erase(jid.proc);
	} else if (table == JOBSET) {
		m_jobsetAds.erase(jid.cluster);
	} else {
		m_genericAds.erase(key);
	}
	return true;
}

bool
NewClassAdJobLogConsumer::SetAttribute(const char *key,
									   const char *attr,
									   const char *value)
{
	rectype table;
	classad::ClassAd *ad = getAd(key, table);
	if (!ad) {
		dprintf(D_ALWAYS,
				"error reading %s: no such ad in collection: %s\n",
				m_reader ? m_reader->GetClassAdLogFileName() : "(null)", key);
		// The schedd has been known to set attributes for bogus job ids.
		// Ignore them and continue processing the log.
		return true;
	}
	classad::ExprTree *expr;
	ParseClassAdRvalExpr(value, expr);
	if (!expr) {
		dprintf(D_ALWAYS,
				"error reading %s: failed to parse expression: %s\n",
				m_reader ? m_reader->GetClassAdLogFileName() : "(null)", value);
		// If the schedd writes a bad attribute value, ignore it and keep
		// processing the log.
		return true;
	}
	ad->Insert(attr,expr);

	// USER records need to be indexed by USER name.
	// Since we want to be lazy about cleaning up the name to id index
	// we will map name to userrec_id rather than mapping name to classad.
	// Then it won't matter if we have stale entries in the name to id map
	if (table == USERREC) {
		std::string username;
		UserRecord * userad = dynamic_cast<UserRecord*>(ad);
		if (userad->is_user && (MATCH == strcasecmp(ATTR_USER, attr))) {
			if (ad->LookupString(ATTR_USER, username)) {
				m_user_to_userid[username] = userad->userrec_id;
			}
		} else {
			// TODO: map project records by ATTR_NAME attribute?
		}
	}

	return true;
}

bool
NewClassAdJobLogConsumer::DeleteAttribute(const char *key,
										  const char *attr)
{
	rectype table;
	classad::ClassAd *ad = getAd(key, table);
	if (!ad) {
		dprintf(D_ALWAYS,
				"error reading %s: no such ad in collection: %s\n",
				m_reader ? m_reader->GetClassAdLogFileName() : "(null)", key);
		// The schedd has been known to write log entries for bogus job ids.
		// Ignore them and continue processing the log.
		return true;
	}
	ad->Delete(attr);

		// The above will return false if the attribute doesn't exist
		// in the ad.  However, this is expected, because the schedd
		// sometimes will blindly delete attributes that do not exist
		// (e.g. RemoteSlotID).  Therefore, we ignore the return
		// value.

	return true;
}

UserRecord * NewClassAdJobLogConsumer::GetUserAd(const std::string & username)
{
	auto found = m_user_to_userid.find(username);
	if (found != m_user_to_userid.end()) {
		auto it = m_userAds.find(found->second);
		if (it != m_userAds.end()) {
			return &it->second;
		}
	}
	return nullptr;
}

UserRecord * NewClassAdJobLogConsumer::GetJobUser(const ClassAd* ad)
{
	// if we were passed a jobad, and it has a cached userrec id we just use it.
	unsigned int userrec_id = 0;
	const JobClassAd * jobad = dynamic_cast<const JobClassAd*>(ad);
	if (jobad && jobad->userrec_id) {
		userrec_id = jobad->userrec_id;
	} else {
		std::string username;
		if (ad->LookupString(ATTR_USER, username)) {
			auto found = m_user_to_userid.find(username);
			if (found != m_user_to_userid.end()) {
				userrec_id = found->second;
			}
		}
	}

	if (userrec_id) {
		// stash the userrec id in the jobad if we were passed one
		if (jobad) { const_cast<JobClassAd*>(jobad)->userrec_id = userrec_id; }
		// and return the ad if we have it
		auto it = m_userAds.find(userrec_id);
		if (it != m_userAds.end()) return &it->second;
	}
	return nullptr;
}

