/***************************************************************
 *
 * Copyright (C) 1990-2019, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_attributes.h"
#include "qmgmt.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "jobsets.h"
#include "classad_merge.h"


// in schedd.cpp
void IncrementLiveJobCounter(LiveJobCounters & num, int universe, int status, int increment /*, JobQueueJob * job*/);

JobSets::JobSet::JobSet(int _id, std::string _name, std::string _owner, ClassAd *ad) 
	: id(_id), name(_name), owner(_owner), setAd(ad)
{
	if (!setAd) {
		ClassAd ad;
		ad.EnableDirtyTracking();
		ad.Assign(ATTR_MY_TYPE, JOB_SET_ADTYPE);
		ad.Assign(ATTR_TARGET_TYPE, JOB_SET_ADTYPE);
		ad.Assign(ATTR_JOB_SET_NAME, name);
		ad.Assign(ATTR_JOB_SET_ID, id);
		ad.Assign(ATTR_OWNER, owner);
		JobSetStoreAllDirtyAttrs(id, ad, true);
	}
}

void
JobSets::setNextSetId(int id) 
{
	next_setid_num = id;
	dprintf(D_FULLDEBUG, "JobSet setNextSetId restored to id %d\n", id);
}

void
JobSets::reconfig()
{

}

JobSets::~JobSets()
{
	// Deallocate each JobSet
	for (auto const & s : mapIdToSet) {
		delete s.second;
	}

}

bool
JobSets::restoreJobSet(ClassAd *setAd)
{
	int id = 0;
	std::string setName;
	std::string setOwner;

	if (!setAd->LookupInteger(ATTR_JOB_SET_ID, id) ||
		!setAd->LookupString(ATTR_OWNER, setOwner) ||
		!setAd->LookupString(ATTR_JOB_SET_NAME, setName))
	{
		dprintf(D_ALWAYS, "ERROR: restoreJobSet - log has malformed JobSet  (%s,%d)\n",
			setName.c_str(), id);
		return false;
	}

	// Add into our name to id map, making sure it is not already there
	unsigned int &setId = mapAliasToId[setOwner + setName];
	if (setId) {
		dprintf(D_ALWAYS, "ERROR: restoreJobSet - log has duplicate JobSet names (%s,%d)\n",
			setName.c_str(), setId);
		return false;
	}
	setId = id;

	// Add into our id to set object map, making sure it is not already there
	JobSet* &set = mapIdToSet[setId];
	if (set) {
		dprintf(D_ALWAYS, "ERROR: restoreJobSet - log has duplicate JobSet ids (%s,%d)\n",
			setName.c_str(), setId);
		return false;
	}
	set = new JobSet(setId, setName, setOwner, setAd);

	if (setId >= next_setid_num) {
		dprintf(D_ALWAYS,"Warning: restoreJobSet - JobSet id larger than expected (%s,%d)\n",
			setName.c_str(), setId);
		next_setid_num = setId + 1;
	}

	// Increment aggregates for removed and completed jobs from the past
	std::string prefix = "Scheduler";
	setAd->LookupInteger(ATTR_TOTAL_COMPLETED_JOBS, set->jobStatusAggregates.JobsCompleted);
	setAd->LookupInteger(prefix+ATTR_TOTAL_COMPLETED_JOBS, set->jobStatusAggregates.SchedulerJobsCompleted);
	setAd->LookupInteger(ATTR_TOTAL_REMOVED_JOBS, set->jobStatusAggregates.JobsRemoved);
	setAd->LookupInteger(prefix+ATTR_TOTAL_REMOVED_JOBS, set->jobStatusAggregates.SchedulerJobsRemoved);
	
	dprintf(D_FULLDEBUG, "Restored JobSet %s id=%d for owner %s\n",
		set->name.c_str(), set->id, set->owner.c_str());

	return true;
}


JobSets::JobSet* JobSets::getOrCreateSet(JobQueueJob & job)
{
	int setId = job.set_id;
	bool newSet = false;
	bool appendJobToSet = false;
	std::string setName;

	if (setId == -1) {
		// job is not in a set, bail out quickly now before ad lookups etc
		return nullptr;
	}

	if (setId == 0) {
		appendJobToSet = true;
		// We have not yet added this job to any set
		if (!job.LookupString(ATTR_JOB_SET_NAME, setName)) {
			// default to cluster id...
			setName = std::to_string(job.jid.cluster);
		}
		if (setName.empty()) {
			// remember this job is not in a set for fast bailout next time
			job.set_id = -1; 
			return nullptr;
		}
		std::string key = job.ownerinfo->name + setName;
		auto it = mapAliasToId.find(key);
		if (it == mapAliasToId.end()) {
			// Create a new set with a new set id!
			setId = next_setid_num;
			SetSecureAttributeInt(0, 0, ATTR_NEXT_JOBSET_NUM, ++next_setid_num);
			newSet = true;
			mapAliasToId[key] = setId;
		}
		else {
			// Appened this job to an existing set
			setId = it->second;
		}
	}

	JobSet* &set = mapIdToSet[setId];

	// Assert that mapAliasToID and mapIdToSet are in sync with each other
	ASSERT(newSet ? set==nullptr : set!=nullptr); 

	if (newSet) {
		set = new JobSet(setId, setName, job.ownerinfo->name, nullptr);
		dprintf(D_ALWAYS, "Created new JobSet %s id=%d for owner %s\n",
			set->name.c_str(), set->id, set->owner.c_str());		
	}

	if (appendJobToSet) {
		set->jobsInSet.insert(job.jid);
		job.Assign(ATTR_JOB_SET_ID, setId);
		job.set_id = setId;
	}

	if (!set->setAd) {
		// Note - it is still possible GetJobAd() will return a NULL here
		// if the set is being created as part of a transaction that has not yet
		// been committed.  So a NULL setAd should not be an error.
		set->setAd = GetJobAd(0, 0 - setId);
	}

	return set;
}

bool JobSets::removeSet(JobSet* & set)
{
	if (!set) return false;	
	dprintf(D_ALWAYS, "Removing JobSet %s id=%d (%zu jobs) for owner %s\n",
		set->name.c_str(), set->id, set->jobsInSet.size(), set->owner.c_str());
	mapAliasToId.erase(set->owner + set->name);
	mapIdToSet.erase(set->id);
	bool ret = JobSetDestroy(set->id);
	if (!ret) {
		dprintf(D_ALWAYS, "WARNING: unable to remove from log JobSet %s id=%d (%zu jobs) for owner %s\n",
			set->name.c_str(), set->id, set->jobsInSet.size(), set->owner.c_str());
	}
	delete set;
	set = nullptr;  // set is destroyed, get rid of pointer to it
	return ret;
}

bool JobSets::update(JobQueueJob & job, int old_status, int new_status)
{
	if (old_status == new_status) {
		// nothing to do, since job status did not change
		return true;
	}

	if (job.set_id <= 0) {
		// If this job has not yet been placed into a set, then do not decrement 
		// job counters for leaving the old_status.
		old_status = -1;
	}

	JobSet * set = getOrCreateSet(job);

	if (!set) {
		// nothing to do, this job is not a member of a set
		return false;
	}

	IncrementLiveJobCounter(set->jobStatusAggregates, job.Universe(), old_status, -1);
	IncrementLiveJobCounter(set->jobStatusAggregates, job.Universe(), new_status, 1);

	if (IsDebugLevel(D_JOB) && old_status >= 0 ) {
		ClassAd s;
		set->jobStatusAggregates.publish(s, "Live");
		if (set->setAd) {
			MergeClassAds(&s, set->setAd, true);
		}
		dprintf(D_JOB, "Updating JobSet %s (id=%u) after status change in job %d.%d to be:\n",
			set->name.c_str(), set->id, job.jid.cluster, job.jid.proc);
		dPrintAd(D_JOB, s);
	}
	return true;
}

bool JobSets::removeJobFromSet(JobQueueJob & job)
{
	if (job.set_id <= 0) return true;

	JobSet* jobset = getOrCreateSet(job);
	ASSERT(jobset);

	// update historical counters now to disk, as this job is leaving
	if (jobset->setAd) {
		std::string key;
	
		if (job.Status() == COMPLETED) {
			key = ATTR_TOTAL_COMPLETED_JOBS;
		}
		if (job.Status() == REMOVED) {
			key = ATTR_TOTAL_REMOVED_JOBS;
		}
		if (!key.empty()) {
			if (job.Universe() == CONDOR_UNIVERSE_SCHEDULER) {
				key = "Scheduler" + key;
			}
			int val = 0;
			jobset->setAd->LookupInteger(key, val);
			val++;
			jobset->setAd->Assign(key, val);
			jobset->dirty = true;
		}

		jobset->persistSetInfo();
	}

	jobset->jobsInSet.erase(job.jid);
	job.set_id = -1;

	// Check if time to destroy this set...
	if (jobset->jobsInSet.size() == 0 &&
		jobset->garbagePolicy == garbagePolicyEnum::immediateAfterEmpty)
	{
		removeSet(jobset);
	}

	return true;
}

bool JobSets::JobSet::persistSetInfo()
{
	
	if (dirty) {		
		dprintf(D_ALWAYS, "Updating JobSet %s id=%d (%zu jobs) for owner %s\n",
			name.c_str(), id, jobsInSet.size(), owner.c_str());

		if (!setAd) {
			setAd = GetJobAd(0, 0 - id);
		}
		ASSERT(setAd);

		JobSetStoreAllDirtyAttrs(id, *setAd, false);	
	}

	dirty = false;

	return true;
}
