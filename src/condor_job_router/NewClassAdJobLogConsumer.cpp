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
#include "classad_newold.h"

#include "NewClassAdJobLogConsumer.h"

#include <string>

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

NewClassAdJobLogConsumer::NewClassAdJobLogConsumer() { }

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
									 const char */*type*/,
									 const char */*target*/)
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
						m_reader->GetJobLogFileName(), cluster_key);
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
					m_reader->GetJobLogFileName(),
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
				m_reader->GetJobLogFileName(), key);
		return false;
	}
	MyString new_classad_value, err_msg;
	if (!old_classad_value_to_new_classad_value(value,
												new_classad_value,
												&err_msg)) {
		dprintf(D_ALWAYS,
				"error reading %s: failed to convert expression from "
				"old to new ClassAd format: %s\n",
				m_reader->GetJobLogFileName(), err_msg.Value());
		return false;
	}

	classad::ClassAdParser ad_parser;

	classad::ExprTree *expr =
		ad_parser.ParseExpression(new_classad_value.Value());
	if (!expr) {
		dprintf(D_ALWAYS,
				"error reading %s: failed to parse expression: %s\n",
				m_reader->GetJobLogFileName(), value);
		ASSERT(expr);
		return false;
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
				m_reader->GetJobLogFileName(), key);
		return false;
	}
	ad->Delete(name);

		// The above will return false if the attribute doesn't exist
		// in the ad.  However, this is expected, because the schedd
		// sometimes will blindly delete attributes that do not exist
		// (e.g. RemoteSlotID).  Therefore, we ignore the return
		// value.

	return true;
}

