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
#include "classad_merge.h"
#include "startd_cron_job.h"
#include "startd_named_classad.h"

// Named classAds
StartdNamedClassAd::StartdNamedClassAd( const char *name,
										StartdCronJob &job,
										ClassAd * ad /*=NULL*/)
		: NamedClassAd( name, ad ), m_job( job )
{
}

// attributes that are defined only for StartdNamedClassAds (i.e STARTD_CRON ads)
//
#define ATTR_SLOT_NAME "SlotName"
#define ATTR_SLOT_MERGE_CONSTRAINT "SlotMergeConstraint"

// initialize the list of attributes that should never be merged from a STARTD_CRON script output
// into the slot ad.  It's more efficient if these are sorted.
static const char * const dont_merge_these[] = {
	ATTR_NAME,
	ATTR_SLOT_ID,
	ATTR_SLOT_MERGE_CONSTRAINT,
	ATTR_SLOT_NAME,
	ATTR_SLOT_TYPE_ID,
};
AttrNameSet StartdNamedClassAd::dont_merge_attrs(dont_merge_these, dont_merge_these+(sizeof(dont_merge_these)/sizeof(const char *)));

bool
StartdNamedClassAd::InSlotList( unsigned slot_id ) const
{
	return m_job.Params().InSlotList( slot_id );
}


bool
StartdNamedClassAd::ShouldMergeInto(ClassAd * merge_into, const char ** pattr_used)
{
	if (pattr_used) *pattr_used = NULL;

	ClassAd * merge_from = this->GetAd();
	if ( ! merge_from || ! merge_into)
		return false;

	// if there is a SlotMergeContraint in the source, then merge into slots
	// for which the constraint evaluates to true, and don't merge otherwise.
	if (merge_from->Lookup(ATTR_SLOT_MERGE_CONSTRAINT)) {
		int matches = false;
		if (pattr_used) *pattr_used = ATTR_SLOT_MERGE_CONSTRAINT;
		merge_from->EvalBool(ATTR_SLOT_MERGE_CONSTRAINT, merge_into, matches);
		return matches;
	}

	// if there is a "Name" or "SlotName" attribute in the source, then merge into slots
	// whose name matches it as a prefix.  (i.e. if merge_from("Name") is "xfer", the merge into all slots
	// whose name starts with "xfer".  this is most useful if you use SLOT_TYPE_n_NAME_PREFIX to name slots.
	std::string from_str, to_str;
	if ( ( merge_from->LookupString(ATTR_SLOT_NAME, from_str) ||
		   merge_from->LookupString(ATTR_NAME, from_str))
		&& merge_into->LookupString(ATTR_NAME, to_str))
	{
		if (from_str.length() > 0) {
			if (pattr_used) *pattr_used = merge_from->Lookup(ATTR_SLOT_NAME) ? ATTR_SLOT_NAME : ATTR_NAME;
			return starts_with(to_str, from_str);
		}
	}

	// if there is a "SlotTypeId" attribute, merge into slots with the same type id.
	int from_id, to_id;
	if (merge_from->LookupInteger(ATTR_SLOT_TYPE_ID, from_id) &&
		merge_into->LookupInteger(ATTR_SLOT_TYPE_ID, to_id)) {
		if (pattr_used) *pattr_used = ATTR_SLOT_TYPE_ID;
		return from_id == to_id;
	}

	// if there is a "SlotId" attribute, merge into slots with the same type id.
	if (merge_from->LookupInteger(ATTR_SLOT_ID, from_id) &&
		merge_into->LookupInteger(ATTR_SLOT_ID, to_id)) {
		if (pattr_used) *pattr_used = ATTR_SLOT_ID;
		return from_id == to_id;
	}

	return true;
}

bool
StartdNamedClassAd::MergeInto(ClassAd *merge_into)
{
	ClassAd * merge_from = this->GetAd();
	if ( ! merge_into || ! merge_from)
		return false;

	int cMerged = MergeClassAdsIgnoring(merge_into, merge_from, this->dont_merge_attrs);
	return cMerged > 0;
}
