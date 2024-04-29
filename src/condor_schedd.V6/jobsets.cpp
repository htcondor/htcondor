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


// static helper function
std::string JobSets::makeAlias(const std::string& name, const OwnerInfo &owni) {
	std::string ret = name + "/" + owni.Name();
	lower_case(ret);
	return ret;
}

void
JobSets::reconfig()
{

}

// add this jobset to the map of JobSets
bool
JobSets::restoreJobSet(JobQueueJobSet *setAd)
{
	std::string name;
	if ( ! setAd->LookupString(ATTR_JOB_SET_NAME, name) || !setAd->ownerinfo)
	{
		dprintf(D_ERROR, "ERROR: restoreJobSet - log has malformed JobSet %d key=(%d,%d)\n",
			setAd->Jobset(), setAd->jid.cluster, setAd->jid.proc);
		return false;
	}

	setAd->PopulateFromAd(); // pull fields from the classad

	// Add into our per-user name to id map, making sure it is not already there
	std::string alias = makeAlias(name, *setAd->ownerinfo);
	unsigned int &setId = mapAliasToId[alias];
	if (setId && (setId != setAd->Jobset())) {
		dprintf(D_ERROR, "ERROR: restoreJobSet - log has duplicate JobSet names (%s,%d) for set %d for user %s\n",
			name.c_str(), setId, setAd->Jobset(), setAd->ownerinfo->Name());
		return false;
	}

	setId = setAd->Jobset();
	return true;
}


// load cached fields in the JobQueueJobSet from the classad and/or key
void JobQueueJobSet::PopulateFromAd()
{
	this->LookupInteger(ATTR_TOTAL_COMPLETED_JOBS, jobStatusAggregates.JobsCompleted);
	this->LookupInteger("Scheduler" ATTR_TOTAL_COMPLETED_JOBS, jobStatusAggregates.SchedulerJobsCompleted);
	this->LookupInteger(ATTR_TOTAL_REMOVED_JOBS, jobStatusAggregates.JobsRemoved);
	this->LookupInteger("Scheduler" ATTR_TOTAL_REMOVED_JOBS, jobStatusAggregates.SchedulerJobsRemoved);
}


bool JobSets::removeSet(JobQueueJobSet* & jobset)
{
	std::string name, alias;
	jobset->LookupString(ATTR_JOB_SET_NAME, name);
	if (name.empty() || ! jobset->ownerinfo) {
		// we don't know the alias, so remove all elements that match the set id
		dprintf(D_STATUS, "Removing JobSet id=%d (%u refs)\n", jobset->Jobset(), jobset->member_count);
		for (auto it = mapAliasToId.begin(); it != mapAliasToId.end(); /* it updated inside loop */) {
			if (it->second == jobset->Jobset()) { it = mapAliasToId.erase(it); }
			else { ++it; }
		}
	} else {
		alias = makeAlias(name, *jobset->ownerinfo);
		dprintf(D_STATUS, "Removing JobSet %s id=%d (%u refs)\n", alias.c_str(), jobset->Jobset(), jobset->member_count);
		mapAliasToId.erase(alias);
	}
	bool rval = JobSetDestroy(jobset->Jobset());
	if ( ! rval) {
		dprintf(D_ALWAYS, "WARNING: unable to remove from log JobSet id=%d\n", jobset->Jobset());
	}
	jobset = nullptr;
	return rval;
}


// called by CommitTransaction when jobsets are enabled
// to init the jobset id of the JobQueueCluster object
//
bool JobSets::addToSet(JobQueueJob & job)
{
	// if the job has a set_id of 0, that means "look it up"
	if ( ! job.set_id) {
		// remember this job is not in a set for fast bailout next time
		job.set_id = -1;

		std::string setName;
		if (!job.LookupString(ATTR_JOB_SET_NAME, setName) || setName.empty()) {
			return false;
		}
		// here we want to get the set, but not create it
		std::string alias = makeAlias(setName, *job.ownerinfo);
		auto it = mapAliasToId.find(alias);
		if (it != mapAliasToId.end()) {
			job.set_id = it->second;
		}
	}

	if (job.set_id <= 0)
		return false;

	JobQueueJobSet * set = GetJobSetAd(job.set_id);
	if (!set) {
		// nothing to do, this job is not a member of a set
		if (job.set_id > 0) {
			// hmm. job has a setid, but that set does not exist
		}
		return false;
	}
	set->member_count += 1;
	if (job.IsJob()) {
		IncrementLiveJobCounter(set->jobStatusAggregates, job.Universe(), job.Status(), 1);
	}
	return true;
}


void JobSets::status_change(JobQueueJob & job, int new_status)
{
	if ( ! job.IsJob())
		return;

	JobQueueJobSet * jobset = GetJobSetAd(job.set_id);
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
	JobQueueJobSet* jobset = GetJobSetAd(job.set_id);
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
			int ix = 2 * (job.Status() - REMOVED) + (job.Universe() == CONDOR_UNIVERSE_SCHEDULER);
			const char * attr = attrs[ix];
			int val = 0;
			jobset->LookupInteger(attr, val);
			val++;
			SetSecureAttributeInt(jobset->jid.cluster, jobset->jid.proc, attr, val);

			// accumulate CPUTime of completed jobs as CompletedJobsCpuTime and removed jobs as RemovedJobsCpuTime
			attr = (job.Status() == REMOVED) ? "RemovedJobsCpuTime" : "CompletedJobsCpuTime";
			double accum = 0.0, cputime;
			jobset->LookupFloat(attr, accum);
			// add the jobs' CumulativeRemoteUserCpu + CumulativeRemoteSysCpu to the jobset cumulative value
			cputime = 0.0;
			if (job.LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU, cputime) && cputime > 0.0) {
				accum += cputime;
			}
			cputime = 0.0;
			if (job.LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU, cputime) && cputime > 0.0) {
				accum += cputime;
			}

			char buf[100];
			snprintf(buf,100,"%f",accum);
			SetSecureAttribute(jobset->jid.cluster, jobset->jid.proc, attr, buf);
		}

		if (jobset->member_count > 0) {
			jobset->member_count -= 1;
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

