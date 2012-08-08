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

#ifndef _JOB_ROUTER_H
#define _JOB_ROUTER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "HashTable.h"
#include "RoutedJob.h"

#include "classad/classad_distribution.h"

#if HAVE_JOB_HOOKS
#include "JobRouterHookMgr.h"
#endif /* HAVE_JOB_HOOKS */

class RoutedJob;
class Scheduler;
class JobRouterHookMgr;

typedef HashTable<std::string,JobRoute *> RoutingTable;

/*
 * The JobRouter is responsible for finding idle jobs of one flavor
 * (e.g. vanilla), converting them to another flavor (e.g. Condor-C),
 * submitting the new job, and feeding back the results to the
 * original job.
 */

class JobRouter: public Service {
 public:
	JobRouter();
	virtual ~JobRouter();

	// Add a new job to be managed by JobRouter.
	// Takes ownership of job, so caller should not delete it.
	bool AddJob(RoutedJob *job);

	// Mark a job entry for removal, but do not remove it immediately.
	// The job will be removed after cleaning up any necessary things
	// in the source schedd.
	void GracefullyRemoveJob(RoutedJob *job);

	// Return a pointer to entry for job with given src key in routed job list
	RoutedJob *LookupJobWithSrcKey(std::string const &key);

	// Return a pointer to entry for job with given keys in routed job list
	RoutedJob *LookupJobWithKeys(std::string const &src_key,std::string const &dest_key);

	// This is called in a timer to periodically manage the jobs.
	void Poll();

	// This is called in a timer to evaluate periodic expressions for the
	// jobs the JobRouter manages.
	void EvalAllSrcJobPeriodicExprs();

	void config();
	void init();

	//The JobRouter name is used to distinguish this daemon from
	//other daemons that claim jobs in the originating schedd's job
	//collection.
	std::string JobRouterName() {return m_job_router_name;}

#if HAVE_JOB_HOOKS
	JobRouterHookMgr* m_hook_mgr;
#endif /* HAVE_JOB_HOOKS */

	// Finish the job submission process.
	void FinishSubmitJob(RoutedJob *job);

	// Update the status of the job.
	void UpdateRoutedJobStatus(RoutedJob *job, classad::ClassAd update);

	// Finish the job status update.
	void FinishCheckSubmittedJobStatus(RoutedJob *job);

	// Finish finalizing the job.
	void FinishFinalizeJob(RoutedJob *job);
	
	// Finish cleaning up the job.
	void FinishCleanupJob(RoutedJob *job);

	// Have the job be rerouted.
	void RerouteJob(RoutedJob *job);

	// Push the updated attributes to the job queue.
	bool PushUpdatedAttributes(classad::ClassAd& src);

		// Provide access to the Scheduler, which holds the
		// ClassAdCollection
	Scheduler *GetScheduler() { return m_scheduler; }

	classad::ClassAdCollection *GetSchedd1ClassAds();
	classad::ClassAdCollection *GetSchedd2ClassAds();

 private:
	HashTable<std::string,RoutedJob *> m_jobs;  //key="src job id"
	RoutingTable *m_routes; //key="route name"

	Scheduler *m_scheduler;        // provides us with a mirror of the real schedd's job collection
	Scheduler *m_scheduler2;       // if non-NULL, mirror of job queue in destination schedd

	char const *m_schedd2_name;
	char const *m_schedd2_pool;
	std::string m_schedd2_name_buf;
	std::string m_schedd2_pool_buf;

	char const *m_schedd1_name;
	char const *m_schedd1_pool;
	std::string m_schedd1_name_buf;
	std::string m_schedd1_pool_buf;

	std::string m_constraint;
	int m_max_jobs;
	int m_max_job_mirror_update_lag; // time before giving up on mirror to update

	bool m_enable_job_routing;
	bool m_release_on_hold;

	int m_job_router_entries_refresh;
	int m_job_router_refresh_timer;

	int m_job_router_polling_timer;
	int m_periodic_timer_id;
	int m_job_router_polling_period;

	int m_public_ad_update_interval;
	int m_public_ad_update_timer;

	std::string m_job_router_name;
	std::string daemonName;

	int m_poll_count;

	int m_router_lock_fd;
	class FileLock *m_router_lock;
	std::string m_router_lock_fname;

	ClassAd m_public_ad;

	// Count jobs being managed.  (Excludes RETIRED jobs.)
	int NumManagedJobs();

	// Obliterate job entry in JobRouter's list of jobs to manage.
	// Consider using GracefullyRemoveJob() instead, unless you need this,
	// because this function does not clean up the state of this job in
	// the source schedd.
	bool RemoveJob(RoutedJob *job);

	// Find jobs to route.  Function calls AddJob() on each one.
	void GetCandidateJobs();

	// Resume management of any jobs we were routing in a previous life.
	void AdoptOrphans();

	// Take ownership of jobs (e.g. by marking them as "managed" in src schedd)
	void TakeOverJob(RoutedJob *job);

	// Submit jobs to target resources/sites.
	void SubmitJob(RoutedJob *job);

	// Check if submitted jobs are finished.
	void CheckSubmittedJobStatus(RoutedJob *job);

	// Remove finished "destination" jobs and feed back results into
	// the source jobs.
	void FinalizeJob(RoutedJob *job);

	// Handle any jobs that are on the way out of JobRouter.
	// Clean up any state in the source schedd etc.
	void CleanupJob(RoutedJob *job);

	// Set the source job back to the idle state.
	void SetJobIdle(RoutedJob *job);

	// Sweep away memory of jobs that finished, once it is
	// safe to do so.
	void CleanupRetiredJob(RoutedJob *job);

	bool EvalSrcJobPeriodicExpr(RoutedJob* job);

	bool SetJobRemoved(classad::ClassAd& ad, const char* remove_reason);
	bool SetJobHeld(classad::ClassAd& ad, const char* hold_reason,
			int hold_code = 0, int sub_code = 0);


	void SetRoutingTable(RoutingTable *new_routes);

	void ParseRoutingEntries( std::string const &entries, char const *param_name, classad::ClassAd const &router_defaults_ad, bool allow_empty_requirements, RoutingTable *new_routes );

	JobRoute *GetRouteByName(char const *name);

	// Deletes routing table and all of its contents.
	static void DeallocateRoutingTable(RoutingTable *routes);

	// Creates a new routing table.
	static RoutingTable *AllocateRoutingTable();

	// Update job counts on each route.
	void UpdateRouteStats();

	// Pick a matching route.
	JobRoute *ChooseRoute(classad::ClassAd *job_ad,bool *all_routes_full);

	// Return true if job exit state indicates that it was a success.
	bool TestJobSuccess(RoutedJob *job);

	// Return true if job should be spooled.
	bool TestJobSandboxed(RoutedJob *job);

	// Return true if job should be edited in place rather than
	// having a new copy submitted
	bool TestEditJobInPlace(RoutedJob *job);

	//Produce a default JobRouter name.
	std::string DaemonIdentityString();

	// True if we haven't hit the maximum configured number of jobs.
	// This only tests JobRouter as  a whole.  Individual routes
	// may have their own maximums which are not reflected here.
	bool AcceptingMoreJobs();

	// Will except if there is already a JobRouter running with the
	// same name as this one.
	void GetInstanceLock();

	void InitPublicAd();
	void TimerHandler_UpdateCollector();
	void InvalidatePublicAd();
};

#endif
