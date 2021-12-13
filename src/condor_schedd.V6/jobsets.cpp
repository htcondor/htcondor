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


void
JobSets::setNextSetId(int id) 
{
	if (id <= 0) {
		dprintf(D_ALWAYS /* | D_BACKTRACE*/, "JobSet rejecting attempt to setNextSetId(%d)\n", id);
		return;
	}
	next_setid_num = id;
	dprintf(D_FULLDEBUG, "JobSet setNextSetId restored to id %d\n", id);
}

void
JobSets::reconfig()
{

}

// add this jobset to the map of JobSets
bool
JobSets::restoreJobSet(JobQueueJobSet *setAd)
{
	setAd->PopulateFromAd(); // pull fields from the classad

	std::string name;
	if ( ! setAd->LookupString(ATTR_JOB_SET_NAME, name) || setAd->Jobset() <= 0 || !setAd->ownerinfo)
	{
		dprintf(D_ALWAYS, "ERROR: restoreJobSet - log has malformed JobSet %d key=(%d,%d)\n",
			setAd->Jobset(), setAd->jid.cluster, setAd->jid.proc);
		return false;
	}

	// Add into our per-user name to id map, making sure it is not already there
	std::string alias = makeAlias(name, setAd->ownerinfo->name);
	unsigned int &setId = mapAliasToId[alias];
	if (setId) {
		dprintf(D_ALWAYS, "ERROR: restoreJobSet - log has duplicate JobSet names (%s,%d) for set %d for user %s\n",
			name.c_str(), setId, setAd->set_id, setAd->ownerinfo->Name());
		return false;
	}

	setId = setAd->Jobset();
	return true;
}


// load cached fields in the JobQueueJobSet from the classad and/or key
void JobQueueJobSet::PopulateFromAd()
{
	ASSERT(this->Jobset() == qkey2_to_JOBSETID(jid.proc));
	this->LookupInteger(ATTR_TOTAL_COMPLETED_JOBS, jobStatusAggregates.JobsCompleted);
	this->LookupInteger("Scheduler" ATTR_TOTAL_COMPLETED_JOBS, jobStatusAggregates.SchedulerJobsCompleted);
	this->LookupInteger(ATTR_TOTAL_REMOVED_JOBS, jobStatusAggregates.JobsRemoved);
	this->LookupInteger("Scheduler" ATTR_TOTAL_REMOVED_JOBS, jobStatusAggregates.SchedulerJobsRemoved);
}


JobQueueJobSet* JobSets::getSet(JobQueueJob & job)
{
	int setId = job.set_id;
	if (setId == -1) {
		// job is not in a set, bail out quickly now before ad lookups etc
		return nullptr;
	}
	return GetJobSetAd(setId);
}

// lookup or allocate a setid, for use by the qmgmt commit transaction code
int JobSets::getOrCreateSet(const std::string & setName, const std::string & user, std::vector<unsigned int> & new_ids)
{
	std::string alias = makeAlias(setName, user);
	auto it = mapAliasToId.find(alias);
	if (it != mapAliasToId.end()) return it->second;

	// Create a new set with a new set id!
	int setId = next_setid_num;
	SetSecureAttributeInt(0, 0, ATTR_NEXT_JOBSET_NUM, ++next_setid_num);
	JobSetCreate(setId, setName.c_str(), user.c_str());
	new_ids.push_back(setId);
	mapAliasToId[alias] = setId;
	return setId;
}

bool JobSets::removeSet(JobQueueJobSet* & jobset)
{
	std::string name, alias;
	jobset->LookupString(ATTR_JOB_SET_NAME, name);
	if (name.empty() || ! jobset->ownerinfo) {
		// we don't know the alias, so remove all elements that match the set id
		dprintf(D_ALWAYS, "Removing JobSet id=%d (%zu refs)\n", jobset->Jobset(), jobset->member_count);
		for (auto it = mapAliasToId.begin(); it != mapAliasToId.end(); /* it updated inside loop */) {
			if (it->second == jobset->Jobset()) { it = mapAliasToId.erase(it); }
			else { ++it; }
		}
	} else {
		alias = makeAlias(name, jobset->ownerinfo->name);
		dprintf(D_ALWAYS, "Removing JobSet %s id=%d (%zu refs)\n", alias.c_str(), jobset->Jobset(), jobset->member_count);
		mapAliasToId.erase(alias);
	}
	bool rval = JobSetDestroy(jobset->Jobset());
	if ( ! rval) {
		dprintf(D_ALWAYS, "WARNING: unable to remove from log JobSet id=%d\n", jobset->Jobset());
	}
	jobset = nullptr;
	return rval;
}


// called by InitJobQueue if jobsets are enabled
// to add this job to the jobset member count and aggregates
// 
bool JobSets::addJobToSet(JobQueueJob & job, std::vector<unsigned int> & new_ids)
{
	if (job.IsJob() || job.IsCluster()) {
		// if the job has a set_id of 0, that means "look it up, creating the set if needed"
		if ( ! job.set_id) {
			std::string setName;
			if (!job.LookupString(ATTR_JOB_SET_NAME, setName)) {
				// uncomment to use clusterid as default setname
				// setName = std::to_string(job.jid.cluster);
			}
			if (setName.empty()) {
				// remember this job is not in a set for fast bailout next time
				job.set_id = -1;
				return false;
			}
			job.set_id = getOrCreateSet(setName, job.ownerinfo->name, new_ids);
		}

		JobQueueJobSet * set = getSet(job);
		if (!set) {
			// nothing to do, this job is not a member of a set
			if (job.set_id > 0) {
				// hmm. job has a setid, but that set does not exist
			}
			return false;
		}
		set->member_count += 1;
		set->Assign(ATTR_REF_COUNT, set->member_count);
		if (job.IsJob()) {
			IncrementLiveJobCounter(set->jobStatusAggregates, job.Universe(), job.Status(), 1);
		}
		return true;
	}
	return false;
}

void JobSets::status_change(JobQueueJob & job, int new_status)
{
	if ( ! job.IsJob())
		return;

	JobQueueJobSet * jobset = getSet(job);
	if (jobset) {
		IncrementLiveJobCounter(jobset->jobStatusAggregates, job.Universe(), job.Status(), -1);
		IncrementLiveJobCounter(jobset->jobStatusAggregates, job.Universe(), new_status, 1);

		if (IsDebugCatAndVerbosity(D_JOB | D_VERBOSE)) {
			ClassAd s;
			jobset->jobStatusAggregates.publish(s, "Live");
			MergeClassAds(&s, jobset, true);
			std::string name;
			if ( ! jobset->LookupString(ATTR_JOB_SET_NAME, name)) { name = std::to_string(jobset->Jobset()); }
			dprintf(D_JOB | D_VERBOSE, "Updating JobSet %s (id=%u) after status change in job %d.%d to be:\n",
				name.c_str(), jobset->Jobset(), job.jid.cluster, job.jid.proc);
			dPrintAd(D_JOB, s);
		}
	}
}

bool JobSets::removeJobFromSet(JobQueueJob & job)
{
	if (job.set_id <= 0) return true;

	JobQueueJobSet* jobset = getSet(job);
	if ( ! jobset) {
		// if job is not in a set, we don't necessarily create one ;)
		return true;
	}

	job.set_id = -1;

	// update historical counters now to disk, as this job is leaving
	// We need a separate count of jobs that are no longer in the set
	// This is used to initialized the 
	if (jobset) {
		if (job.IsJob() && job.Status() >= REMOVED && job.Status() <= COMPLETED) {
			static const char * const attrs[]{
				ATTR_TOTAL_REMOVED_JOBS, "Scheduler" ATTR_TOTAL_REMOVED_JOBS,
				ATTR_TOTAL_COMPLETED_JOBS, "Scheduler" ATTR_TOTAL_COMPLETED_JOBS,
			};
			int ix = 2 * (job.Status() - REMOVED) + (job.Universe() == CONDOR_UNIVERSE_SCHEDULER) ? 1 : 0;
			const char * attr = attrs[ix];
			int val = 0;
			jobset->LookupInteger(attr, val);
			val++;
			SetSecureAttributeInt(jobset->jid.cluster, jobset->jid.proc, attr, val);
		}

		if (jobset->member_count > 0) {
			jobset->member_count -= 1;
			jobset->Assign(ATTR_REF_COUNT, jobset->member_count);
		}

		// Check if time to destroy this set...
		garbageCollectSet(jobset);
	}

	return true;
}

bool JobSets::garbageCollectSet(JobQueueJobSet* & jobset)
{
	// TODO: implement other garbage collection policies
	if (jobset && 
		jobset->member_count == 0 &&
		jobset->garbagePolicy == JobQueueJobSet::garbagePolicyEnum::immediateAfterEmpty)
	{
		removeSet(jobset);
		return true;
	}
	return false;
}

