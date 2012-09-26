/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _ROUTED_JOB_H
#define _ROUTED_JOB_H

//#include "condor_common.h"

#include "classad/classad_distribution.h"

class JobRoute;

class RoutedJob {
 public:
	RoutedJob(); virtual ~RoutedJob();

	std::string src_key;  // job id of original vanilla job
	std::string dest_key; // job id of grid job
	PROC_ID src_proc_id;
	PROC_ID dest_proc_id;
	classad::ClassAd src_ad;
	classad::ClassAd dest_ad;
	std::string proxy_file_copy;
	bool proxy_file_copy_chowned;

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
	bool is_running;  // true if job status is RUNNING
	bool is_success;  // true if job finished successfully

	bool is_sandboxed;// true if dest copy of job has a separate sandbox

	bool saw_dest_job;// true if dest job ad showed up in job mirror

	bool edit_job_in_place; // edit src job in place; do not submit a routed copy of the job

	time_t submission_time;
	time_t retirement_time;

	int target_universe;
	std::string grid_resource; // In ATTR_GRID_RESOURCE format
	std::string route_name;

	// return a description of the job keys, useful for debug trace messages
	// Format: src=X,dest=X
	std::string JobDesc();

	bool SetSrcJobAd(char const *key,classad::ClassAd *ad,classad::ClassAdCollection *ad_collection);
	void SetDestJobAd(classad::ClassAd const *ad);
	bool IsRunning() {return is_running;}
	bool SawDestJob() {return saw_dest_job;}

	bool PrepareSharedX509UserProxy(JobRoute *route);
	bool CleanupSharedX509UserProxy(JobRoute *route);

};


class JobRoute {
 public:
	JobRoute();
	virtual ~JobRoute();

	classad::ClassAd *RouteAd() {return &m_route_ad;}
	char const *Name() {return m_name.c_str();}
	int MaxJobs() {return m_max_jobs;}
	int MaxIdleJobs() {return m_max_idle_jobs;}
	int CurrentRoutedJobs() {return m_num_jobs;}
	int TargetUniverse() {return m_target_universe;}
	char const *GridResource() {return m_grid_resource.c_str();}
	classad::ExprTree *RouteRequirementExpr() {return m_route_requirements;}
	char const *RouteRequirementsString() {return m_route_requirements_str.c_str();}
	std::string RouteString(); // returns a string describing the route

	// copy state from another route
	void CopyState(JobRoute *route);

	bool ParseClassAd(std::string routing_string,int &offset,classad::ClassAd const *router_defaults_ad,bool allow_empty_requirements);

	// ApplyRoutingJobEdits() allows the router to edit the job ad before
	// resubmitting it.  It does so by having attributes of the following form:
	//   set_XXX = Value   (assigns XXX = Value in job ad, replace existing)
	//   copy_XXX = YYY    (copies value of XXX to attribute YYY in job ad)
	bool ApplyRoutingJobEdits(classad::ClassAd *src_ad);

	bool AcceptingMoreJobs();
	void IncrementCurrentRoutedJobs() {m_num_jobs++;}
	void IncrementRoutedJobs() {IncrementCurrentRoutedJobs(); m_recent_jobs_routed++;}
	void ResetCurrentRoutedJobs() {m_num_jobs = 0;m_num_running_jobs=0;}

	void IncrementCurrentRunningJobs() {m_num_running_jobs++;}
	int CurrentRunningJobs() {return m_num_running_jobs;}
	int CurrentIdleJobs() {return m_num_jobs - m_num_running_jobs;}
	void IncrementSuccesses() {m_recent_jobs_succeeded++;}
	void IncrementFailures() {m_recent_jobs_failed++;}
	int RecentRoutedJobs() {return m_recent_jobs_routed;}
	int RecentSuccesses() {return m_recent_jobs_succeeded;}
	int RecentFailures() {return m_recent_jobs_failed;}
	void AdjustFailureThrottles();
	double Throttle() {return m_throttle;}
	std::string ThrottleDesc();
	std::string ThrottleDesc(double throttle);

	bool EvalUseSharedX509UserProxy(RoutedJob *job);
	bool EvalSharedX509UserProxy(RoutedJob *job,std::string &proxy_file);

		// true if this entry is intended to override an entry with the
		// same name further up in the routing table definition
	int OverrideRoutingEntry() { return m_override_routing_entry; }

 private:
	int m_num_jobs;                // current number of jobs on this route
	int m_num_running_jobs;        // number of jobs currently running

	time_t m_recent_stats_begin_time;
	int m_recent_jobs_routed;
	int m_recent_jobs_failed;
	int m_recent_jobs_succeeded;
	double m_failure_rate_threshold;// failures/sec --> throttling
	double m_throttle;              // limit new jobs/sec (0 means no throttle)

	classad::ClassAd m_route_ad;   // ClassAd describing the route

	// stuff extracted from the route_ad:
	std::string m_name;           // name distinguishing this route from others
	int m_target_universe;        // universe of routed job
	std::string m_grid_resource;  // if routing to grid universe, the grid site
	int m_max_jobs;               // maximum jobs to route
	int m_max_idle_jobs;          // maximum jobs in the idle state (requirement for submitting more jobs)
	classad::ExprTree *m_route_requirements; // jobs must match these requirements
	std::string m_route_requirements_str;    // unparsed version of above

		// true if this entry is intended to override an entry with the
		// same name further up in the routing table definition
	int m_override_routing_entry;

	// Extract data from the route_ad into class variables.
	bool DigestRouteAd(bool allow_empty_requirements);
};


#endif
