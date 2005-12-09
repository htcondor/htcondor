/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _JOB_ROUTER_H
#define _JOB_ROUTER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "HashTable.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

class RoutedJob;
class Scheduler;
class JobRoute;

typedef HashTable<std::string,JobRoute *> RoutingTable;

/*
 * The JobRouter is responsible for finding idle jobs of one flavor
 * (e.g. vanilla), converting them to another flavor (e.g. Condor-C),
 * submitting the new job, and feeding back the results to the
 * original job.
 */

class JobRouter: public Service {
 public:
	JobRouter(Scheduler *scheduler);
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

	void config();
	void init();

	//The JobRouter name is used to distinguish this daemon from
	//other daemons that claim jobs in the originating schedd's job
	//collection.
	std::string JobRouterName() {return m_job_router_name;}

 private:
	HashTable<std::string,RoutedJob *> m_jobs;  //key="src job id"
	RoutingTable *m_routes; //key="route name"

	Scheduler *m_scheduler;        // provides us with a mirror of the real schedd's job collection

	std::string m_constraint;
	int m_max_jobs;
	int m_max_job_mirror_update_lag; // time before giving up on mirror to update

	bool m_enable_job_routing;

	int m_job_router_polling_timer;
	int m_job_router_polling_period;

	std::string m_job_router_name;

	int m_poll_count;

	int m_router_lock_fd;
	class FileLock *m_router_lock;
	std::string m_router_lock_fname;


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

	// Sweep away memory of jobs that finished, once it is
	// safe to do so.
	void CleanupRetiredJob(RoutedJob *job);

	void SetRoutingTable(RoutingTable *new_routes);

	JobRoute *GetRouteByName(char const *name);

	// Deletes routing table and all of its contents.
	static void DeallocateRoutingTable(RoutingTable *routes);

	// Creates a new routing table.
	static RoutingTable *AllocateRoutingTable();

	// Update job counts on each route.
	void UpdateRouteStats();

	// Pick a matching route.
	JobRoute *ChooseRoute(classad::ClassAd *job_ad,bool *all_routes_full);

	//Produce a default JobRouter name.
	std::string DaemonIdentityString();

	// True if we haven't hit the maximum configured number of jobs.
	// This only tests JobRouter as  a whole.  Individual routes
	// may have their own maximums which are not reflected here.
	bool AcceptingMoreJobs();

	// Will except if there is already a JobRouter running with the
	// same name as this one.
	void GetInstanceLock();

};

class JobRoute {
 public:
	JobRoute();
	virtual ~JobRoute();

	classad::ClassAd *RouteAd() {return &m_route_ad;}
	char const *Name() {return m_name.c_str();}
	int MaxJobs() {return m_max_jobs;}
	int CurrentRoutedJobs() {return m_num_jobs;}
	char const *GridResource() {return m_grid_resource.c_str();}
	classad::ExprTree *RouteRequirementExpr() {return m_route_requirements;}
	char const *RouteRequirementsString() {return m_route_requirements_str.c_str();}
	std::string RouteString(); // returns a string describing the route

	// copy state (e.g. num_jobs) from another route
	void CopyState(JobRoute *route);

	bool ParseClassAd(std::string routing_string,unsigned &offset,classad::ClassAd *router_defaults_ad,bool allow_empty_requirements);

	// ApplyRoutingJobEdits() allows the router to edit the job ad before
	// resubmitting it.  It does so by having attributes of the following form:
	//   set_XXX = Value   (assigns XXX = Value in job ad, replace existing)
	//   copy_XXX = YYY    (copies value of XXX to YYY in job ad)
	bool ApplyRoutingJobEdits(classad::ClassAd *src_ad);

	bool AcceptingMoreJobs() {return m_num_jobs < m_max_jobs;}
	void IncrementCurrentRoutedJobs() {m_num_jobs++;}
	void ResetCurrentRoutedJobs() {m_num_jobs = 0;}

 private:
	int m_num_jobs;               // current number of jobs on this route

	classad::ClassAd m_route_ad;  // ClassAd describing the route

	// stuff extracted from the route_ad:
	std::string m_name;           // name distinguishing this route from others
	std::string m_grid_resource;  // the target to which jobs are routed
	int m_max_jobs;               // maximum jobs to route
	classad::ExprTree *m_route_requirements; // jobs must match these requirements
	std::string m_route_requirements_str;    // unparsed version of above

	// Extract data from the route_ad into class variables.
	bool DigestRouteAd(bool allow_empty_requirements);
};

class RoutedJob {
 public:
	RoutedJob();
	virtual ~RoutedJob();

	std::string src_key;  // job id of original vanilla job
	std::string dest_key; // job id of grid job
	PROC_ID src_proc_id;
	PROC_ID dest_proc_id;
	classad::ClassAd src_ad;
	classad::ClassAd dest_ad;

	enum ManagementStateEnum {
		UNCLAIMED,     // job is not yet officially ours to manage
		CLAIMED,       // we are in control of this job now
		SUBMITTED,     // we have submitted this job to the grid
		FINISHED,      // grid job is done, need to feed back results
		CLEANUP,       // unclaim, etc.
		RETIRED        // hangs around in this state to prevent incorrect
		               // identification of orphans
	};

	ManagementStateEnum state;

	bool is_claimed;  // true if we have ever gotten into the CLAIMED state
	                  // and therefore need to unclaim the job eventually.

	bool is_done;     // true if src job should be marked finished

	time_t submission_time;
	time_t retirement_time;

	std::string grid_resource; // In ATTR_GRID_RESOURCE format
	std::string route_name;

	// return a description of the job keys, useful for debug trace messages
	// Format: src=X,dest=X
	std::string JobDesc();

	bool SetSrcJobAd(char const *key,classad::ClassAd *ad,classad::ClassAdCollection *ad_collection);
};

#endif

