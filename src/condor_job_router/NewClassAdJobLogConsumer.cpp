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

#include "NewClassAdJobLogConsumer.h"

#include <string>

#include "classad/classad_distribution.h"

NewClassAdJobLogConsumer::NewClassAdJobLogConsumer() : m_reader(0) { }

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
}

bool
NewClassAdJobLogConsumer::NewClassAd(const char *key,
									 const char * /*type*/,
									 const char * /*target*/)
{
	classad::ClassAd* ad;
	bool using_existing_ad = false;

	ad = m_collection.GetClassAd(key);
	if (ad && ad->size() == 0) {
			// This is a cluster ad created in advance so that
			// children could be chained from it.
		using_existing_ad = true;
	} else {
		ad = new classad::ClassAd();
	}

		// Chain this ad to its parent, if any.
	PROC_ID proc = getProcByString(key);
	if(proc.proc >= 0) {
		char cluster_key[PROC_ID_STR_BUFLEN];
			// NOTE: cluster keys start with a 0: e.g. 021.-1
		sprintf(cluster_key,"0%d.-1", proc.cluster);
		classad::ClassAd* cluster_ad =
			m_collection.GetClassAd(cluster_key);
		if (!cluster_ad) {
				// The cluster ad doesn't exist yet.  This is expected.
				// For example, once the job queue is rewritten (i.e.
				// truncated), the order of entries in it is arbitrary.
			cluster_ad = new classad::ClassAd();
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
	m_collection.RemoveClassAd(key);

	return true;
}

bool
NewClassAdJobLogConsumer::SetAttribute(const char *key,
									   const char *name,
									   const char *value)
{
	classad::ClassAd *ad = m_collection.GetClassAd(key);
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
	ad->Insert(name,expr);

	return true;
}

bool
NewClassAdJobLogConsumer::DeleteAttribute(const char *key,
										  const char *name)
{
	classad::ClassAd *ad = m_collection.GetClassAd(key);
	if (!ad) {
		dprintf(D_ALWAYS,
				"error reading %s: no such ad in collection: %s\n",
				m_reader ? m_reader->GetClassAdLogFileName() : "(null)", key);
		// The schedd has been known to write log entries for bogus job ids.
		// Ignore them and continue processing the log.
		return true;
	}
	ad->Delete(name);

		// The above will return false if the attribute doesn't exist
		// in the ad.  However, this is expected, because the schedd
		// sometimes will blindly delete attributes that do not exist
		// (e.g. RemoteSlotID).  Therefore, we ignore the return
		// value.

	return true;
}

