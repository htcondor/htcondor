/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include <vector>

#include "condor_attributes.h"
#include "JobRouter.h"
#include "Scheduler.h"
#include "condor_md.h"
#include "my_username.h"
#include "condor_uid.h"

#include "condor_config.h"
#include "VanillaToGrid.h"
#include "submit_job.h"
#include "schedd_v7_utils.h"

template class HashTable<std::string,RoutedJob *>;
template HashTable<std::string,JobRoute *>;
template HashTable<std::string,std::string>;
template std::vector<JobRoute *>;

const char JR_ATTR_MAX_JOBS[] = "MaxJobs";
const char JR_ATTR_ROUTED_FROM_JOB_ID[] = "RoutedFromJobId";
const char JR_ATTR_ROUTED_TO_JOB_ID[] = "RoutedToJobId";
const char JR_ATTR_ROUTED_FROM_MANAGER[] = "RoutedFromManager";
const char JR_ATTR_ROUTED_FROM_ROUTE[] = "RoutedFromRoute";

JobRouter::JobRouter(Scheduler *scheduler): m_jobs(5000,hashFuncStdString,rejectDuplicateKeys) {
	m_scheduler = scheduler;
	m_job_router_polling_timer = -1;
	m_job_router_polling_period = 10;
	m_enable_job_routing = true;

	m_routes = AllocateRoutingTable();
	m_poll_count = 0;

	m_router_lock_fd = -1;
	m_router_lock = NULL;
}

JobRouter::~JobRouter() {
	RoutedJob *job;

	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		RemoveJob(job);
	}

	DeallocateRoutingTable(m_routes);

	if(m_router_lock) {
		unlink(m_router_lock_fname.c_str());
		delete m_router_lock;
	}
	if(m_router_lock_fd != -1) {
		close(m_router_lock_fd);
	}
}

void
JobRouter::init() {
	config();
	GetInstanceLock();
}

void
JobRouter::GetInstanceLock() {
	std::string lock_fullname;
	std::string lock_basename;
	char *lock_dir;

	// Since JobRouterName() may contain characters that are not allowed
	// (or not portable) in a filename, use an MD5 hash of it for the
	// lock file name.
	Condor_MD_MAC MD5;
	std::string router_name = JobRouterName();
	unsigned char *md5_str;
	MD5.addMD((const unsigned char *)router_name.c_str(),router_name.size());
	md5_str = MD5.computeMD();
	if(md5_str) {
		int i;
		for(i=0;i<MAC_SIZE;i++) {
			char buf[10];
			sprintf(buf,"%x",md5_str[i]);
			lock_basename += buf;
		}
		free(md5_str);
	}
	else {
		// MD5 may not be supported in all ports of Condor.  Fake it.
		unsigned long n = 0;
		char const *s = router_name.c_str();
		while(*s) {
			n += (unsigned char)*s;
		}
		char n_buf[100];
		sprintf(n_buf,"%lu",n);
		lock_basename = n_buf;
	}
	lock_basename += ".JobRouter.lock";

	// We may be an ordinary user, so cannot lock in $(LOCK)
	lock_dir = "/tmp";
	lock_fullname = lock_dir;
	lock_fullname += DIR_DELIM_CHAR;
	lock_fullname += lock_basename;

	m_router_lock_fd = open(lock_fullname.c_str(),O_CREAT|O_APPEND|O_WRONLY,0600);
	if(m_router_lock_fd == -1) {
		EXCEPT("Failed to open lock file %s: %s",lock_fullname.c_str(),strerror(errno));
	}
	FileLock *lock = new FileLock(m_router_lock_fd);
	m_router_lock = lock;
	m_router_lock_fname = lock_fullname;

	lock->set_blocking(FALSE);
	if(!lock->obtain(WRITE_LOCK)) {
		EXCEPT("Failed to get lock on %s.\n",lock_fullname.c_str());
	}
}

char const PARAM_JOB_ROUTER_ENTRIES[] = "JOB_ROUTER_ENTRIES";
char const PARAM_JOB_ROUTER_DEFAULTS[] = "JOB_ROUTER_DEFAULTS";

void
JobRouter::config() {
	bool allow_empty_requirements = false;
	m_enable_job_routing = true;

	char *constraint = param("JOB_ROUTER_SOURCE_JOB_CONSTRAINT");
	if(!constraint) {
		m_constraint = "";
	}
	else {
		m_constraint = constraint;
		free(constraint);
		// Since there is a global constraint, make the specification of
		// requirements in the individual routes optional.
		allow_empty_requirements = true;
	}


	RoutingTable *new_routes = new RoutingTable(200,hashFuncStdString,rejectDuplicateKeys);

	char *router_defaults_str = param(PARAM_JOB_ROUTER_DEFAULTS);
	classad::ClassAd router_defaults_ad;
	if(router_defaults_str) {
		classad::ClassAdParser parser;
		if(!parser.ParseClassAd(router_defaults_str,router_defaults_ad)) {
			dprintf(D_ALWAYS,"JobRouter CONFIGURATION ERROR: Disabling job routing, because failed to parse %s classad: '%s'\n",PARAM_JOB_ROUTER_DEFAULTS,router_defaults_str);
			m_enable_job_routing = false;
		}
		free(router_defaults_str);
	}
	if(!m_enable_job_routing) return;

	char *routing_str = param(PARAM_JOB_ROUTER_ENTRIES);
	if(!routing_str) {
		dprintf(D_ALWAYS,"JobRouter WARNING: %s not found, so job routing will not take place.\n",PARAM_JOB_ROUTER_ENTRIES);
		m_enable_job_routing = false;
	}
	else {
		std::string routing_string = routing_str;
		free(routing_str);

		// Now parse a list of routing entries.  The expected syntax is
		// a list of ClassAds, optionally delimited by commas and or
		// whitespace.

		unsigned offset = 0;
		while(1) {
			// Eat delimiters and whitespace
			while(routing_string.size() > offset && (routing_string[offset] == ' ' || routing_string[offset] == ','))
			{
				offset++;
			}
			if(offset >= routing_string.size()) break;


			JobRoute route;
			JobRoute *existing_route;
			unsigned this_offset = offset; //save offset before eating an ad.

			if(!route.ParseClassAd(routing_string,offset,&router_defaults_ad,allow_empty_requirements))
			{
				dprintf(D_ALWAYS,"JobRouter CONFIGURATION ERROR: Ignoring the malformed route entry, starting here: %s\n",routing_string.c_str() + this_offset);
				EXCEPT("DEBUG");

				// skip any junk and try parsing the next route in the list
				while(routing_string.size() > offset && routing_string[offset] != '[') offset++;
			}
			else if(new_routes->lookup(route.Name(),existing_route)!=-1)
			{
				// Two routes have the same name.  Since route names
				// are optional, these names may have been
				// auto-generated from other portions of the route ad.
				// Warn the user about that.

				dprintf(D_ALWAYS,"JobRouter CONFIGURATION ERROR: two route entries have the same name '%s'; if you have not already explicitly given these routes a name with name=\"blah\", you may want to do so.\n",route.Name());
			}
			else {
				new_routes->insert(route.Name(),new JobRoute(route));
			}
		}
	}
	if(!m_enable_job_routing) return;

	SetRoutingTable(new_routes);

	char *max_jobs = param("JOB_ROUTER_MAX_JOBS");
	if(max_jobs) {
		m_max_jobs = atoi(max_jobs);
		free(max_jobs);
	}
	else {
		m_max_jobs = -1; // no maximum
	}

	char *max_job_mirror_update_lag = param("MAX_JOB_MIRROR_UPDATE_LAG");
	if(max_job_mirror_update_lag) {
		m_max_job_mirror_update_lag = atoi(max_job_mirror_update_lag);
		free(max_job_mirror_update_lag);
	}
	else {
		m_max_job_mirror_update_lag = 600;
	}


		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *polling_period_str = param("JOB_ROUTER_POLLING_PERIOD");
	m_job_router_polling_period = 10;
	if(polling_period_str) {
		m_job_router_polling_period = atoi(polling_period_str);
		free(polling_period_str);
	}

		// clear previous timers
	if (m_job_router_polling_timer >= 0) {
		daemonCore->Cancel_Timer(m_job_router_polling_timer);
	}
		// register timer handlers
	m_job_router_polling_timer = daemonCore->Register_Timer(
								  0, 
								  m_job_router_polling_period,
								  (Eventcpp)&JobRouter::Poll, 
								  "JobRouter::Poll", this);

	char *name = param("JOB_ROUTER_NAME");
	if(name) {
		m_job_router_name = name;
		free(name);
	}
	else {
		m_job_router_name = DaemonIdentityString();
	}
}

JobRoute *
JobRouter::GetRouteByName(char const *name) {
	JobRoute *route = NULL;
	if(m_routes->lookup(name,route) == -1) {
		return NULL;
	}
	return route;
}

void
JobRouter::SetRoutingTable(RoutingTable *new_routes) {
	// Now we have a set of new routes in new_routes.
	// Replace our existing routing table with these.

	JobRoute *route=NULL;

	// look for routes that have been dropped
	m_routes->startIterations();
	while(m_routes->iterate(route)) {
		JobRoute *new_route = NULL;
		if(new_routes->lookup(route->Name(),new_route) == -1) {
			dprintf(D_ALWAYS,"JobRouter Note: dropping route '%s'\n",route->RouteString().c_str());
		}
	}

	// look for routes that have been added
	new_routes->startIterations();
	while(new_routes->iterate(route)) {
		JobRoute *old_route = NULL;
		if(m_routes->lookup(route->Name(),old_route) == -1) {
			dprintf(D_ALWAYS,"JobRouter Note: adding new route '%s'\n",route->RouteString().c_str());
		}
	}

	DeallocateRoutingTable(m_routes);
	m_routes = new_routes;

	UpdateRouteStats();
}

void
JobRouter::DeallocateRoutingTable(RoutingTable *routes) {
	JobRoute *route;
	routes->startIterations();
	while(routes->iterate(route)) {
		delete route;
	}
	delete routes;
}

RoutingTable *
JobRouter::AllocateRoutingTable() {
	return new RoutingTable(200,hashFuncStdString,rejectDuplicateKeys);
}

void
JobRouter::GracefullyRemoveJob(RoutedJob *job) {
	job->state = RoutedJob::CLEANUP;
}

bool
JobRouter::AddJob(RoutedJob *job) {
	return m_jobs.insert(job->src_key,job) == 0;
}

RoutedJob *
JobRouter::LookupJobWithSrcKey(std::string const &src_key) {
	RoutedJob *job = NULL;
	if(m_jobs.lookup(src_key,job) == -1) {
		return NULL;
	}
	return job;
}

RoutedJob *
JobRouter::LookupJobWithKeys(std::string const &src_key,std::string const &dest_key) {
	RoutedJob *job = LookupJobWithSrcKey(src_key);
	if(!job) return NULL;

	// Currently, there is only ever one dest job per src job, so
	// no need to do anything but verify dest_key.
	if(job->dest_key == dest_key) return job;
	return NULL;
}

bool
JobRouter::RemoveJob(RoutedJob *job) {
	int success;
	ASSERT(job);
	success = m_jobs.remove(job->src_key);
	delete job;
	return success != 0;
}


RoutedJob::RoutedJob() {
	state = UNCLAIMED;
	src_proc_id.cluster = -1;
	src_proc_id.proc = -1;
	dest_proc_id.cluster = -1;
	dest_proc_id.proc = -1;
	is_claimed = false;
	is_done = false;
	submission_time = 0;
}
RoutedJob::~RoutedJob() {
}

int
JobRouter::NumManagedJobs() {
	int count = 0;
	RoutedJob *job = NULL;
	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		if(job->state != RoutedJob::RETIRED) count++;
	}
	return count;
}

void
JobRouter::Poll() {
	dprintf(D_FULLDEBUG,"JobRouter: polling state of (%d) managed jobs.\n",NumManagedJobs());

	m_poll_count++;
	if((m_poll_count % 5) == 1) {
		//Every now and then (especially when we start up), make sure
		//there aren't any orphan jobs left around.  The reason we don't only
		//do this at startup is that race conditions may prevent us from
		//seeing orphans right at startup time.
		AdoptOrphans();
	}

	UpdateRouteStats();
	GetCandidateJobs();

	RoutedJob *job;
	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		// The following functions only do something if the job is in a state
		// where it needs the action to be done.
		TakeOverJob(job);
		SubmitJob(job);
		CheckSubmittedJobStatus(job);
		FinalizeJob(job);
		CleanupJob(job);
		CleanupRetiredJob(job); //NOTE: this may delete job
	}
}

bool
JobRouter::AcceptingMoreJobs() {
	return m_max_jobs < 0 || NumManagedJobs() < m_max_jobs;
}

bool
CombineParentAndChildClassAd(classad::ClassAd *dest,classad::ClassAd *ad,classad::ClassAd *parent) {
	classad::AttrList::const_iterator itr;
	classad::ExprTree *tree;

	if(parent) *dest = *parent;
	if(!ad) return true;

	dest->DisableDirtyTracking();
	for( itr = ad->begin( ); itr != ad->end( ); itr++ ) {
		if( !( tree = itr->second->Copy( ) ) ) {
		dprintf(D_FULLDEBUG,"failed to copy %s value\n",itr->first.c_str());
			return false;
		}
		if(!dest->Insert(itr->first,tree)) {
		dprintf(D_FULLDEBUG,"failed to insert %s\n",itr->first.c_str());	
		return false;
		}
	}
	dest->EnableDirtyTracking();

	return true;
}

void
JobRouter::AdoptOrphans() {
    classad::LocalCollectionQuery query;
	classad::ExprTree *constraint_tree;
	classad::ClassAdParser parser;
	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();
	std::string dest_key,src_key;
	std::string dest_jobs,src_jobs;

	// Find all jobs submitted by a JobRouter with same name as me.
	// Maybe some of them were from a previous life and are now orphans.
	dest_jobs += "other.ProcId >= 0 && other.";
	dest_jobs += JR_ATTR_ROUTED_FROM_MANAGER;
	dest_jobs += " == \"";
	dest_jobs += m_job_router_name;
	dest_jobs += "\"";

	constraint_tree = parser.ParseExpression(dest_jobs.c_str());
	if(!constraint_tree) {
		EXCEPT("JobRouter: Failed to parse orphan dest job constraint: '%s'\n",dest_jobs.c_str());
	}

    query.Bind(ad_collection);
    if(!query.Query("root",constraint_tree)) {
		dprintf(D_ALWAYS,"JobRouter: Error running orphan dest job query: %s\n",dest_jobs.c_str());
		delete constraint_tree;
		return;
	}
	delete constraint_tree;

    query.ToFirst();
    if( query.Current(dest_key) ) do {
		std::string route_name;
		classad::ClassAd *dest_ad = ad_collection->GetClassAd(dest_key);
		ASSERT(dest_ad);

		if(!dest_ad->EvaluateAttrString(JR_ATTR_ROUTED_FROM_JOB_ID, src_key) ||
		   !dest_ad->EvaluateAttrString(JR_ATTR_ROUTED_FROM_ROUTE, route_name))
		{
			// Not expected.  Dest job doesn't have routing information.
			dprintf(D_ALWAYS,"JobRouter failure (dest=%s): no routing information found in routed job!\n",dest_key.c_str());
			continue;
		}

		// The following relies on the fact that finished jobs hang
		// around in RETIRED state until we are in sync with the job
		// collection mirror.  Otherwise, we might incorrectly think
		// finished jobs, which haven't been updated yet, are actually
		// orphans.

		if(LookupJobWithKeys(src_key,dest_key)) continue; // not an orphan

		PROC_ID dest_proc_id = getProcByString(dest_key.c_str());

		classad::ClassAd *src_ad = ad_collection->GetClassAd(src_key);
		if(!src_ad) {
			dprintf(D_ALWAYS,"JobRouter (src=%s,dest=%s): removing orphaned destination job with no matching source job.\n",src_key.c_str(),dest_key.c_str());
			MyString err_desc;
			if(!remove_job(*dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",NULL,NULL,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (src=%s,dest=%s): failed to remove dest job: %s\n",src_key.c_str(),dest_key.c_str(),err_desc.Value());
			}
			continue;
		}

		//If we get here, we have enough information to recover the routed job.
		RoutedJob *job = new RoutedJob();
		job->state = RoutedJob::SUBMITTED;
		if(!job->SetSrcJobAd(src_key.c_str(),src_ad,ad_collection)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): error processing orphan src job ad\n",job->JobDesc().c_str());
			delete job;
			continue;
		}
		job->SetDestJobAd(dest_ad);
		job->dest_key = dest_key;
		job->dest_proc_id = getProcByString(dest_key.c_str());
		job->route_name = route_name;
		job->submission_time = time(NULL); //not true; but good enough
		job->is_claimed = true;

		if(!AddJob(job)) {
			dprintf(D_ALWAYS,"JobRouter (%s): failed to add orphaned job to my routed job list; aborting it.\n",job->JobDesc().c_str());
			MyString err_desc;
			if(!remove_job(job->dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",NULL,NULL,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.Value());
			}
			delete job;
		}
		else {
			dprintf(D_FULLDEBUG,"JobRouter (%s): adopted orphaned job (from previous run?)\n",job->JobDesc().c_str());
		}
	} while (query.Next(dest_key));


	src_jobs = "other.Managed == \"External\" && other.ManagedManager == \"";
	src_jobs += m_job_router_name;
	src_jobs += "\"";

	constraint_tree = parser.ParseExpression(src_jobs.c_str());
	if(!constraint_tree) {
		EXCEPT("JobRouter: Failed to parse orphan constraint: '%s'\n",src_jobs.c_str());
	}

    query.Bind(ad_collection);
    if(!query.Query("root",constraint_tree)) {
		dprintf(D_ALWAYS,"JobRouter: Error running orphan query: %s\n",src_jobs.c_str());
		delete constraint_tree;
		return;
	}
	delete constraint_tree;

    query.ToFirst();
    if( query.Current(src_key) ) do {
		if(LookupJobWithSrcKey(src_key)) continue;

		classad::ClassAd *src_ad = ad_collection->GetClassAd(src_key);
		if(!src_ad) continue;

		dprintf(D_ALWAYS,"JobRouter (src=%s): found orphan with no routed destination job; yielding management of it.\n",src_key.c_str());

		//Yield management of this job so that it doesn't sit there
		//forever in the queue.

		MyString error_details;
		PROC_ID src_proc_id = getProcByString(src_key.c_str());
		if(!yield_job(*src_ad,NULL,NULL,false,src_proc_id.cluster,src_proc_id.proc,&error_details,JobRouterName().c_str())) {
			dprintf(D_ALWAYS,"JobRouter (src=%s): failed to yield orphan job: %s\n",
					src_key.c_str(),
					error_details.Value());
		}
	} while (query.Next(src_key));
}

void
JobRouter::GetCandidateJobs() {
	if(!m_enable_job_routing) return;

    classad::LocalCollectionQuery query;
	classad::ClassAdParser parser;
	classad::ExprTree *constraint_tree;
    std::string key;
	classad::ClassAd *ad;
	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();
	JobRoute *route;

	HashTable<std::string,std::string> constraint_list(200,hashFuncStdString,rejectDuplicateKeys);
	std::string umbrella_constraint;

	m_routes->startIterations();
	while(m_routes->iterate(route)) {
		dprintf(D_FULLDEBUG,"JobRouter (route=%s): %d/%d jobs.\n",route->Name(),route->CurrentRoutedJobs(),route->MaxJobs());
	}

	if(!AcceptingMoreJobs()) return; //router is full

	// Generate the list of routing constraints.
	// Each route may have its own constraint, but in case many of them
	// are the same, add only unique constraints to the list.
	std::string route_constraints;
	m_routes->startIterations();
	while(m_routes->iterate(route)) {
		if(route->AcceptingMoreJobs()) {
			std::string existing_constraint;
			std::string this_constraint = route->RouteRequirementsString();
			if(this_constraint.empty()) {
				this_constraint = "True";
			}
			if(constraint_list.lookup(this_constraint,existing_constraint)==-1)
			{
				constraint_list.insert(this_constraint,this_constraint);
				if(!route_constraints.empty()) route_constraints += " || ";
				route_constraints += "(";
				route_constraints += this_constraint;
				route_constraints += ")";
			}
		}
	}

	// The overall "umbrella" constraint matches the main JobRouter
	// constraint (if any) and at least one constraint from an
	// individual route.
	if(!m_constraint.empty()) {
		umbrella_constraint = "(";
		umbrella_constraint += m_constraint;
		umbrella_constraint += ")";
	}

	if(route_constraints.empty()) {
		dprintf(D_FULLDEBUG,"JobRouter: no routes can accept more jobs at the moment.\n");
		return; // No routes are accepting jobs.
	}

	if(!umbrella_constraint.empty()) {
		umbrella_constraint += " && ";
	}
	umbrella_constraint += "( ";
	umbrella_constraint += route_constraints;
	umbrella_constraint += " )";

	//Add on basic requirements to keep things sane.
	//For now, require vanilla universe, but in future this may be removed.
	umbrella_constraint += " && (other.ProcId >= 0 && other.JobStatus == 1 && other.JobUniverse == 5 && other.Managed isnt \"ScheddDone\" && other.Managed isnt \"External\")";

	if(!can_switch_ids()) {
			// We are not running as root.  Ensure that we only try to
			// manage jobs submitted by the same user we are running as.

		char *username = my_username();
		char *domain = my_domainname();

		ASSERT(username);

		umbrella_constraint += " && (other.";
		umbrella_constraint += ATTR_OWNER;
		umbrella_constraint += " == \"";
		umbrella_constraint += username;
		umbrella_constraint += "\"";
		if(domain) {
			umbrella_constraint += " && other.";
			umbrella_constraint += ATTR_NT_DOMAIN;
			umbrella_constraint += " == \"";
			umbrella_constraint += domain;
			umbrella_constraint += "\"";
		}
		umbrella_constraint += ")";

		free(username);
		free(domain);
	}

	dprintf(D_FULLDEBUG,"JobRouter: umbrella constraint: %s\n",umbrella_constraint.c_str());

	constraint_tree = parser.ParseExpression(umbrella_constraint.c_str());
	if(!constraint_tree) {
		EXCEPT("JobRouter: Failed to parse umbrella constraint: '%s'\n",umbrella_constraint.c_str());
	}

    query.Bind(ad_collection);
    if(!query.Query("root",constraint_tree)) {
		dprintf(D_ALWAYS,"JobRouter: Error running query: %s\n",umbrella_constraint.c_str());
		delete constraint_tree;
		return;
	}
	delete constraint_tree;

    query.ToFirst();
    if( query.Current(key) ) do {
		if(!AcceptingMoreJobs()) {
			dprintf(D_FULLDEBUG,"JobRouter: Reached maximum managed jobs (%d).  Skipping further searches for candidate jobs.\n",m_max_jobs);
			return; //router is full
		}

		if(LookupJobWithSrcKey(key)) {
			// We are already managing this job.
			continue;
		}

		ad = ad_collection->GetClassAd(key);
		ASSERT(ad);

		bool all_routes_full;
		JobRoute *route = ChooseRoute(ad,&all_routes_full);
		if(!route) {
			if(all_routes_full) {
				dprintf(D_FULLDEBUG,"JobRouter: all routes are full (%d managed jobs).  Skipping further searches for candidate jobs.\n",NumManagedJobs());
				break;
			}
			dprintf(D_FULLDEBUG,"JobRouter (src=%s): no route found\n",key.c_str());
			continue;
		}

		RoutedJob *job = new RoutedJob();
		job->state = RoutedJob::UNCLAIMED;
		job->grid_resource = route->GridResource();
		job->route_name = route->Name();

		if(!job->SetSrcJobAd(key.c_str(),ad,ad_collection)) {
			delete job;
			continue;
		}

		/*
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): parent = %s\n",job->JobDesc().c_str(),ClassAdToString(parent).c_str());
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): child = %s\n",job->JobDesc().c_str(),ClassAdToString(ad).c_str());
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): combined = %s\n",job->JobDesc().c_str(),ClassAdToString(&job->src_ad).c_str());
		*/

		// from now on, any updates to src_ad should be propogated back
		// to the originating schedd.
		job->src_ad.ClearAllDirtyFlags();
		job->src_ad.EnableDirtyTracking();

		dprintf(D_FULLDEBUG,"JobRouter (%s): found candidate job\n",job->JobDesc().c_str());
		AddJob(job);
    } while (query.Next(key));
}

JobRoute *
JobRouter::ChooseRoute(classad::ClassAd *job_ad,bool *all_routes_full) {
	std::vector<JobRoute *> matches;
	JobRoute *route=NULL;
	m_routes->startIterations();
	*all_routes_full = true;
	while(m_routes->iterate(route)) {
		classad::MatchClassAd mad;
		bool match = false;

		if(!route->AcceptingMoreJobs()) continue;
		*all_routes_full = false;

		mad.ReplaceLeftAd(route->RouteAd());
		mad.ReplaceRightAd(job_ad);

		if(mad.EvaluateAttrBool("RightMatchesLeft", match) && match) {
			matches.push_back(route);
		}

		mad.RemoveLeftAd();
		mad.RemoveRightAd();
	}

	if(!matches.size()) return NULL;
	unsigned choice = (unsigned)(rand()/(RAND_MAX + 1.0) * matches.size());
	ASSERT(choice < matches.size());
	route = matches[choice];

	route->IncrementCurrentRoutedJobs();
	return route;
}

void
JobRouter::UpdateRouteStats() {
	RoutedJob *job;
	JobRoute *route;
	m_routes->startIterations();
	while(m_routes->iterate(route)) {
		route->ResetCurrentRoutedJobs();
	}
	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		if(!job->route_name.empty()) {
			if(m_routes->lookup(job->route_name,route) != -1) {
				route->IncrementCurrentRoutedJobs();
			}
		}
	}
}

std::string
JobRouter::DaemonIdentityString() {
	std::string identity;
	identity += m_scheduler->Name();
	return identity;
}

void
JobRouter::TakeOverJob(RoutedJob *job) {
	if(job->state != RoutedJob::UNCLAIMED) return;

	MyString error_details;
	ClaimJobResult cjr = claim_job(job->src_ad,NULL,NULL,job->src_proc_id.cluster, job->src_proc_id.proc, &error_details, JobRouterName().c_str());

	switch(cjr) {
	case CJR_ERROR: {
		dprintf(D_ALWAYS,"JobRouter failure (%s): candidate job could not be claimed by JobRouter: %s\n",job->JobDesc().c_str(),error_details.Value());
		GracefullyRemoveJob(job);
		break;
	}
	case CJR_BUSY: {
		dprintf(D_FULLDEBUG,"JobRouter failure (%s): candidate job could not be claimed by JobRouter because it is already claimed by somebody else.\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		break;
	}
	case CJR_OK: {
		dprintf(D_FULLDEBUG,"JobRouter (%s): claimed job\n",job->JobDesc().c_str());
		job->state = RoutedJob::CLAIMED;
		job->is_claimed = true;
		break;
	}
	}
}

void
JobRouter::SubmitJob(RoutedJob *job) {
	if(job->state != RoutedJob::CLAIMED) return;

	job->dest_ad = job->src_ad;
	VanillaToGrid::vanillaToGrid(&job->dest_ad,job->grid_resource.c_str());

	// Now we apply any edits to the job ClassAds as defined in the route ad.

	JobRoute *route = GetRouteByName(job->route_name.c_str());
	if(!route) {
		dprintf(D_FULLDEBUG,"JobRouter (%s): route has been removed before job could be submitted.\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	// The route ClassAd may change some things in the routed ad.
	if(!route->ApplyRoutingJobEdits(&job->dest_ad)) {
		dprintf(D_FULLDEBUG,"JobRouter failure (%s): failed to apply route ClassAd modifications to target ad.\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	// Record the src job id in the new job's ad, so we can recover
	// in case of a crash or restart.
	job->dest_ad.InsertAttr(JR_ATTR_ROUTED_FROM_JOB_ID,job->src_key.c_str());
	job->dest_ad.InsertAttr(JR_ATTR_ROUTED_FROM_MANAGER,JobRouterName().c_str());
	job->dest_ad.InsertAttr(JR_ATTR_ROUTED_FROM_ROUTE,route->Name());

	int dest_cluster_id = -1;
	int dest_proc_id = -1;
	if(!submit_job(job->dest_ad,NULL,NULL,&dest_cluster_id,&dest_proc_id)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to submit job\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}
	char buf[50];
	sprintf(buf,"%d.%d",dest_cluster_id,dest_proc_id);
	job->dest_key = buf;
	job->dest_proc_id = getProcByString(job->dest_key.c_str());
	dprintf(D_FULLDEBUG,"JobRouter (%s): submitted job\n",job->JobDesc().c_str());


	// Store info in the src job that identifies the dest job.
	// This is for informational purposes, so there is no need
	// to update the src job now.  Just wait for the next update.
	job->src_ad.InsertAttr(JR_ATTR_ROUTED_TO_JOB_ID,job->dest_key.c_str());

	job->state = RoutedJob::SUBMITTED;
	job->submission_time = time(NULL);
}

static bool ClassAdHasDirtyAttributes(classad::ClassAd *ad) {
	return ad->dirtyBegin() != ad->dirtyEnd();
}

void
JobRouter::CheckSubmittedJobStatus(RoutedJob *job) {
	if(job->state != RoutedJob::SUBMITTED) return;

	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();
	classad::ClassAd *src_ad = ad_collection->GetClassAd(job->src_key);

	if(!src_ad) {
		dprintf(D_ALWAYS,"JobRouter (%s): failed to find src ad in job collection mirror.\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	int job_status = 0;
	if( !src_ad->EvaluateAttrInt( ATTR_JOB_STATUS, job_status ) ) {
		dprintf(D_ALWAYS, "JobRouter failure (%s): cannot evaluate JobStatus in src job\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	if(job_status == REMOVED) {
		dprintf(D_FULLDEBUG, "JobRouter (%s): found src job marked for removal\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	classad::ClassAd *ad = ad_collection->GetClassAd(job->dest_key);

	// If ad is not found, this could be because Quill hasn't seen
	// it yet, in which case this is not a problem.  The following
	// attempts to ensure this by seeing if enough time has passed
	// since we submitted the job.

	if(!ad) {
		int age = time(NULL) - job->submission_time;
		if(age > m_max_job_mirror_update_lag) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): giving up, because submitted job is still not in job queue mirror (submitted %d seconds ago)\n",job->JobDesc().c_str(),age);
			GracefullyRemoveJob(job);
			return;
		}
		dprintf(D_FULLDEBUG,"JobRouter (%s): submitted job has not yet appeared in job queue mirror (submitted %d seconds ago)\n",job->JobDesc().c_str(),age);
		return;
	}

	job->SetDestJobAd(ad);
	if(!update_job_status(job->src_ad,job->dest_ad)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to update job status for finished job\n",job->JobDesc().c_str());
	}
	else if(ClassAdHasDirtyAttributes(&job->src_ad)) {
		if(!push_dirty_attributes(job->src_ad,NULL,NULL)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): failed to update src job\n",job->JobDesc().c_str());

			GracefullyRemoveJob(job);
			return;
		}
		else {
			dprintf(D_FULLDEBUG,"JobRouter (%s): updated job status\n",job->JobDesc().c_str());
			job->src_ad.ClearAllDirtyFlags();
		}
	}

	job_status = 0;
	if( !ad->EvaluateAttrInt( ATTR_JOB_STATUS, job_status ) ) {
		dprintf(D_ALWAYS, "JobRouter failure (%s): cannot evaluate JobStatus in target job\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	int job_finished = 0;
	if( !ad->EvaluateAttrInt( ATTR_JOB_FINISHED_HOOK_DONE, job_finished ) ) {
		job_finished = 0;
	}

	if(job_status == COMPLETED && job_finished != 0) {
		dprintf(D_FULLDEBUG, "JobRouter (%s): found target job finished\n",job->JobDesc().c_str());

		job->state = RoutedJob::FINISHED;
	}
}


void
JobRouter::FinalizeJob(RoutedJob *job) {
	if(job->state != RoutedJob::FINISHED) return;

	if(!finalize_job(job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,NULL,NULL)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to finalize job\n",job->JobDesc().c_str());

			// Put the src job back in idle state to prevent it from
			// exiting the queue.
		job->src_ad.InsertAttr(ATTR_JOB_STATUS,IDLE);
		if(!push_dirty_attributes(job->src_ad,NULL,NULL)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): failed to set src job status back to idle\n",job->JobDesc().c_str());
		}
	}
	else {
		dprintf(D_ALWAYS,"JobRouter (%s): finalized job\n",job->JobDesc().c_str());
		job->is_done = true;
	}

	GracefullyRemoveJob(job);
}

void
JobRouter::CleanupJob(RoutedJob *job) {
	if(job->state != RoutedJob::CLEANUP) return;

	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();

	if(!job->is_done && job->dest_proc_id.cluster != -1) {
		// Remove (abort) destination job.
		MyString err_desc;
		if(!remove_job(job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,"JobRouter aborted job",NULL,NULL,err_desc)) {
			dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.Value());
		}
		else {
			job->dest_proc_id.cluster = -1;
		}
	}

	if(job->is_claimed) {
		MyString error_details;
		bool keep_trying = true;
		if(!yield_job(job->src_ad,NULL,NULL,job->is_done,job->src_proc_id.cluster,job->src_proc_id.proc,&error_details,JobRouterName().c_str(),&keep_trying))
		{
			dprintf(D_ALWAYS,"JobRouter (%s): failed to yield job: %s\n",
					job->JobDesc().c_str(),
					error_details.Value());

			classad::ClassAd *src_ad = ad_collection->GetClassAd(job->src_key);
			if(!src_ad) {
				// The src job has gone away, so do not keep trying.
				keep_trying = false;
			}
			if(keep_trying) {
				return;
			}
		}
		else {
			dprintf(D_FULLDEBUG,"JobRouter (%s): yielded job (done=%d)\n",job->JobDesc().c_str(),job->is_done);
		}

		job->is_claimed = false;
	}

	if(!job->is_claimed) {
		dprintf(D_FULLDEBUG,"JobRouter (%s): Cleaned up and removed routed job.\n",job->JobDesc().c_str());

		// Actually, we need to leave this job in the list for a while to
		// prevent lag in the job collection mirror from causing us to
		// think this job is an orphan.
		job->state = RoutedJob::RETIRED;
		job->retirement_time = time(NULL);
		job->route_name = "";
	}
}

void
JobRouter::CleanupRetiredJob(RoutedJob *job) {
	if(job->state != RoutedJob::RETIRED) return;

	// Our job here is to check if the jobs that are hanging around in
	// retirement state are safe to forget about.  We don't want to
	// forget about them until our mirrror of the originating schedd's
	// job collection is in sync.  Otherwise, the jobs may be
	// misidentified as orphans, belonging to this JobRouter, but not
	// being actively managed by this JobRouter.  We don't want to
	// hold them in retirement for an unnecessarily long time, in case
	// the first attempt to route them failed and we want to try
	// again.

	bool src_job_synchronized = false;
	bool dest_job_synchronized = false;

	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();
	classad::ClassAd *src_ad = ad_collection->GetClassAd(job->src_key);

	// If src_ad cannot be found in the mirror, then the ad has probably
	// been deleted, and we could just count that as being in sync.
	// However, there is no penalty to keeping the job waiting around in
	// retirement in this case, because without src_ad, we can't possibly
	// try to route this job again or anything like that.  Therefore,
	// play it safe and only count the mirror as synchronized if we
	// can find src_ad and directly observe that it is not managed by us.

	if(src_ad) {
		std::string managed;
		std::string manager;
		src_ad->EvaluateAttrString(ATTR_JOB_MANAGED,managed);
		src_ad->EvaluateAttrString(ATTR_JOB_MANAGED_MANAGER,manager);

		if(managed != MANAGED_EXTERNAL || manager != m_job_router_name) {
			// Our mirror of the schedd's job collection shows this
			// job as not being managed by us.  Good.
			src_job_synchronized = true;
		}
	}

	classad::ClassAd *dest_ad = ad_collection->GetClassAd(job->dest_key);
	if(!dest_ad) {
		dest_job_synchronized = true;
	}
	else if(dest_ad) {
		std::string manager;
		dest_ad->EvaluateAttrString(JR_ATTR_ROUTED_FROM_MANAGER,manager);

		if(manager != m_job_router_name) {
			// Our mirror of the schedd's job collection shows this
			// job as not being managed by us.  Good.
			dest_job_synchronized = true;
		}
	}

	if(src_job_synchronized && dest_job_synchronized) {
		dprintf(D_FULLDEBUG,"JobRouter (%s): job mirror synchronized; removing job from internal 'retirement' status\n",job->JobDesc().c_str());
		RemoveJob(job);
		return;
	}

	if(time(NULL) - job->retirement_time >= m_max_job_mirror_update_lag) {
		// We have waited for a long time to ensure that the mirror is
		// synchronized.  It may be that the job doesn't even exist anymore
		// in the schedd's job collection.  In any case, it is time to
		// forget about it.

		if(src_ad) {
			dprintf(D_FULLDEBUG,"JobRouter (%s): job mirror still not synchronized after %ld seconds; removing job from internal 'retirement' status\n",job->JobDesc().c_str(),(long)(time(NULL)-job->retirement_time));
		}

		RemoveJob(job);
		return;
	}
}

JobRoute::JobRoute() {
}

JobRoute::~JobRoute() {
}

bool
JobRoute::DigestRouteAd(bool allow_empty_requirements) {
	if( !m_route_ad.EvaluateAttrInt( JR_ATTR_MAX_JOBS, m_max_jobs ) ) {
		m_max_jobs = 100;
	}
	if( !m_route_ad.EvaluateAttrString( ATTR_GRID_RESOURCE, m_grid_resource ) ) {
		dprintf(D_ALWAYS, "JobRouter: Missing or invalid %s in job route.\n",ATTR_GRID_RESOURCE);
		return false;
	}	
	if( !m_route_ad.EvaluateAttrString( ATTR_NAME, m_name ) ) {
		// If no name is specified, use the GridResource as the name.
		m_name = m_grid_resource;
	}
	m_route_requirements = m_route_ad.Lookup( ATTR_REQUIREMENTS );
	if(!m_route_requirements) {
		m_route_requirements_str = "";
		if(!allow_empty_requirements) {
			dprintf(D_ALWAYS, "JobRouter CONFIGURATION ERROR: Missing %s in job route.\n",ATTR_REQUIREMENTS);
			return false;
		}
	}
	else {
		classad::ClassAdUnParser unparser;
		unparser.Unparse(m_route_requirements_str,m_route_requirements);
	}
	return true;
}

std::string
JobRoute::RouteString() {
	std::string route_string;
	classad::ClassAdUnParser unparser;
	unparser.Unparse(route_string,&m_route_ad);
	return route_string;
}

bool
JobRoute::ParseClassAd(std::string routing_string,unsigned &offset,classad::ClassAd *router_defaults_ad,bool allow_empty_requirements) {
	classad::ClassAdParser parser;
	classad::ClassAd ad;
	if(!parser.ParseClassAd(routing_string,ad,(int)offset)) {
		return false;
	}
	m_route_ad = *router_defaults_ad;
	m_route_ad.Update(ad);
	return DigestRouteAd(allow_empty_requirements);
}

bool
JobRoute::ApplyRoutingJobEdits(classad::ClassAd *src_ad) {
	classad::AttrList::const_iterator itr;
	classad::ExprTree *tree;

	src_ad->DisableDirtyTracking();
	// Do attribute copies
	for( itr = m_route_ad.begin( ); itr != m_route_ad.end( ); itr++ ) {
		char const *attr = itr->first.c_str();
		std::string new_attr;
		classad::ExprTree *expr;
		if(strncmp(attr,"copy_",5)) continue;
		attr = attr + 5;
		if(!m_route_ad.EvaluateAttrString( itr->first, new_attr )) {
			dprintf(D_ALWAYS,"JobRouter failure (route=%s): ApplyRoutingJobEdits failed to evaluate %s to a string.\n",Name(),itr->first.c_str());
			continue;
		}
		dprintf(D_FULLDEBUG,"JobRouter (route=%s): Copying attribute %s to %s\n",Name(),attr,new_attr.c_str());
		if(! (expr = src_ad->Lookup(attr)) ) {
			classad::ClassAdParser parser;
			expr = parser.ParseExpression( "undefined" );
		}
		else {
			expr = expr->Copy();
		}
		if(!src_ad->Insert(new_attr,expr)) {
			return false;
		}
	}
	// Do attribute assignments
	for( itr = m_route_ad.begin( ); itr != m_route_ad.end( ); itr++ ) {
		char const *attr = itr->first.c_str();
		if(strncmp(attr,"set_",4)) continue;
		attr = attr + 4;
		dprintf(D_FULLDEBUG,"JobRouter (route=%s): Setting attribute %s\n",Name(),attr);
		if( !( tree = itr->second->Copy( ) ) ) {
			return false;
		}
		if(!src_ad->Insert(attr,tree)) {
			return false;
		}
	}
	src_ad->EnableDirtyTracking();

	return true;
}

std::string
RoutedJob::JobDesc() {
	std::string desc;
	if(!src_key.empty()) {
		desc += "src=";
		desc += src_key;
	}
	if(!dest_key.empty()) {
		desc += ",dest=";
		desc += dest_key;
	}
	if(!route_name.empty()) {
		desc += ",route=";
		desc += route_name;
	}
	return desc;
}

bool
RoutedJob::SetSrcJobAd(char const *key,classad::ClassAd *ad,classad::ClassAdCollection *ad_collection) {

	this->src_key = key;
	this->src_proc_id = getProcByString(key);

	//Set the src_ad to include all attributes from cluster plus proc ads.
	if(!src_ad.CopyFromChain(*ad)) {
		dprintf(D_FULLDEBUG,"JobRouter failure (%s): failed to combine cluster and proc ad.\n",JobDesc().c_str());
		return false;
	}
	// From here on, keep track of any changes to src_ad, so we can push
	// changes back to the schedd.
	src_ad.EnableDirtyTracking();
	return true;
}

void
RoutedJob::SetDestJobAd(classad::ClassAd const *ad) {
	// We do not want to just do dest_ad = *ad, among other reasons,
	// because this copies the pointer to the chained parent, which
	// could get deleted before we are done with this ad.

	ASSERT(dest_ad.CopyFromChain(*ad));
}
