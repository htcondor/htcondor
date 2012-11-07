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
#include "util_lib_proto.h"
#include "my_popen.h"
#include "file_lock.h"
#include "user_job_policy.h"
#include "get_daemon_name.h"
#include "filename_tools.h"
#include "condor_holdcodes.h"


const char JR_ATTR_MAX_JOBS[] = "MaxJobs";
const char JR_ATTR_MAX_IDLE_JOBS[] = "MaxIdleJobs";
const char JR_ATTR_ROUTED_FROM_JOB_ID[] = "RoutedFromJobId";
const char JR_ATTR_ROUTED_TO_JOB_ID[] = "RoutedToJobId";
const char JR_ATTR_ROUTED_BY[] = "RoutedBy";
const char JR_ATTR_ROUTE_NAME[] = "RouteName";
//NOTE: if we ever want to insert the route name in the src job ad,
//call it somethin different, such as "DestRouteName".  This makes
//it possible to have jobs get routed multiple times.
const char JR_ATTR_FAILURE_RATE_THRESHOLD[] = "FailureRateThreshold";
const char JR_ATTR_JOB_FAILURE_TEST[] = "JobFailureTest";
const char JR_ATTR_JOB_SANDBOXED_TEST[] = "JobShouldBeSandboxed";
const char JR_ATTR_USE_SHARED_X509_USER_PROXY[] = "UseSharedX509UserProxy";
const char JR_ATTR_SHARED_X509_USER_PROXY[] = "SharedX509UserProxy";
const char JR_ATTR_OVERRIDE_ROUTING_ENTRY[] = "OverrideRoutingEntry";
const char JR_ATTR_TARGET_UNIVERSE[] = "TargetUniverse";
const char JR_ATTR_EDIT_JOB_IN_PLACE[] = "EditJobInPlace";

const int THROTTLE_UPDATE_INTERVAL = 600;

JobRouter::JobRouter(): m_jobs(5000,hashFuncStdString,rejectDuplicateKeys) {
	m_scheduler = NULL;
	m_scheduler2 = NULL;
	m_release_on_hold = true;
	m_job_router_polling_timer = -1;
	m_periodic_timer_id = -1;
	m_job_router_polling_period = 10;
	m_enable_job_routing = true;

	m_job_router_entries_refresh = 0;
	m_job_router_refresh_timer = -1;

        m_public_ad_update_timer = -1;
        m_public_ad_update_interval = -1;

	m_routes = AllocateRoutingTable();
	m_poll_count = 0;

	m_router_lock_fd = -1;
	m_router_lock = NULL;
	m_max_jobs = -1;
	m_max_job_mirror_update_lag = 600;

#if HAVE_JOB_HOOKS
	m_hook_mgr = NULL;
#endif
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
	if(m_job_router_refresh_timer >= 0) {
		daemonCore->Cancel_Timer(m_job_router_refresh_timer);
	}
	if(m_public_ad_update_timer >= 0) {
		daemonCore->Cancel_Timer(m_public_ad_update_timer);
	}

#if HAVE_JOB_HOOKS
        if (NULL != m_hook_mgr)
	{
		delete m_hook_mgr;
	}
#endif
	InvalidatePublicAd();

	m_scheduler->stop();
	delete m_scheduler;
	m_scheduler = NULL;
	if( m_scheduler2 ) {
		m_scheduler2->stop();
		delete m_scheduler2;
		m_scheduler2 = NULL;
	}
}

void
JobRouter::init() {
#if HAVE_JOB_HOOKS
	m_hook_mgr = new JobRouterHookMgr;
	m_hook_mgr->initialize();
#endif
	config();
	GetInstanceLock();
}

void
JobRouter::GetInstanceLock() {
	std::string lock_fullname;
	std::string lock_basename;

	// We may be an ordinary user, so cannot lock in $(LOCK)
	param(lock_fullname,"JOB_ROUTER_LOCK");
	ASSERT( !lock_fullname.empty() );
	canonicalize_dir_delimiters(const_cast<char *>(lock_fullname.c_str()));

	m_router_lock_fd = safe_open_wrapper_follow(lock_fullname.c_str(),O_CREAT|O_APPEND|O_WRONLY,0600);
	if(m_router_lock_fd == -1) {
		EXCEPT("Failed to open lock file %s: %s",lock_fullname.c_str(),strerror(errno));
	}
	FileLock *lock = new FileLock(m_router_lock_fd, NULL,lock_fullname.c_str());
	m_router_lock = lock;
	m_router_lock_fname = lock_fullname;

	lock->setBlocking(FALSE);
	if(!lock->obtain(WRITE_LOCK)) {
		EXCEPT("Failed to get lock on %s.\n",lock_fullname.c_str());
	}
}

char const PARAM_JOB_ROUTER_ENTRIES[] = "JOB_ROUTER_ENTRIES";
char const PARAM_JOB_ROUTER_DEFAULTS[] = "JOB_ROUTER_DEFAULTS";
char const PARAM_JOB_ROUTER_ENTRIES_CMD[] = "JOB_ROUTER_ENTRIES_CMD";
char const PARAM_JOB_ROUTER_ENTRIES_FILE[] = "JOB_ROUTER_ENTRIES_FILE";
char const PARAM_JOB_ROUTER_ENTRIES_REFRESH[] = "JOB_ROUTER_ENTRIES_REFRESH";

void
JobRouter::config() {
	bool allow_empty_requirements = false;
	m_enable_job_routing = true;

	if( !m_scheduler ) {
		NewClassAdJobLogConsumer *log_consumer = new NewClassAdJobLogConsumer();
		m_scheduler = new Scheduler(log_consumer,"JOB_ROUTER_SCHEDD1_SPOOL");
		m_scheduler->init();
	}

	std::string spool2;
	if( param(spool2,"JOB_ROUTER_SCHEDD2_SPOOL") ) {
		if( !m_scheduler2 ) {
				// schedd2_spool is configured, but we have no schedd2, so create it
			dprintf(D_ALWAYS,"Reading destination schedd spool %s\n",spool2.c_str());
			NewClassAdJobLogConsumer *log_consumer2 = new NewClassAdJobLogConsumer();
			m_scheduler2 = new Scheduler(log_consumer2,"JOB_ROUTER_SCHEDD2_SPOOL");
			m_scheduler2->init();
		}
	}
	else if( m_scheduler2 ) {
			// schedd2_spool is not configured, but we have a schedd2, so delete it
		dprintf(D_ALWAYS,"Destination schedd spool is now same as source schedd spool\n");
		delete m_scheduler2;
		m_scheduler2 = NULL;
	}

	m_scheduler->config();
	if( m_scheduler2 ) {
		m_scheduler2->config();
	}

#if HAVE_JOB_HOOKS
	m_hook_mgr->reconfig();
#endif

	m_job_router_entries_refresh = param_integer(PARAM_JOB_ROUTER_ENTRIES_REFRESH,0);
	if( m_job_router_refresh_timer >= 0 ) {
		daemonCore->Cancel_Timer(m_job_router_refresh_timer);
		m_job_router_refresh_timer = -1;
	}
	if( m_job_router_entries_refresh > 0 ) {
		m_job_router_refresh_timer = 
			daemonCore->Register_Timer(
				m_job_router_entries_refresh,
				m_job_router_entries_refresh,
				(TimerHandlercpp)&JobRouter::config,
				"JobRouter::config", this);
	}

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
	if(!m_enable_job_routing) {
		delete new_routes;
		return;
	}

	char *routing_str_s = param(PARAM_JOB_ROUTER_ENTRIES);
	char *routing_file_s = param(PARAM_JOB_ROUTER_ENTRIES_FILE);
	char *routing_cmd_s = param(PARAM_JOB_ROUTER_ENTRIES_CMD);
	std::string routing_str = routing_str_s ? routing_str_s : "";
	std::string routing_file = routing_file_s ? routing_file_s : "";
	std::string routing_cmd = routing_cmd_s ? routing_cmd_s : "";
	bool routing_entries_defined = routing_str_s || routing_file_s || routing_cmd_s;
	free( routing_str_s );
	free( routing_file_s );
	free( routing_cmd_s );

	if(!routing_entries_defined) {
		dprintf(D_ALWAYS,"JobRouter WARNING: none of %s, %s, or %s are defined"
				", so job routing will not take place.\n",
				PARAM_JOB_ROUTER_ENTRIES,
				PARAM_JOB_ROUTER_ENTRIES_FILE,
				PARAM_JOB_ROUTER_ENTRIES_CMD);
		m_enable_job_routing = false;
	}

	if( routing_cmd.size() ) {
		ArgList args;
		MyString error_msg;
		if(!args.AppendArgsV1RawOrV2Quoted(routing_cmd.c_str(),&error_msg)) {
			EXCEPT("Invalid value specified for %s: %s",
				   PARAM_JOB_ROUTER_ENTRIES_CMD,
				   error_msg.Value());
		}

			// I have tested with want_stderr 0 and 1, but I have not observed
			// any difference.  I still get stderr mixed into the stdout.
			// Although this is annoying, it is better than simply throwing
			// away stderr altogether.  What generally happens is that the
			// stderr produces a parse error, and we skip to the next
			// entry.
		FILE *fp = my_popen(args, "r", 1);

		if( !fp ) {
			EXCEPT("Failed to run command '%s' specified for %s.",
				   routing_cmd.c_str(), PARAM_JOB_ROUTER_ENTRIES_CMD);
		}
		std::string routing_file_str;
		char buf[200];
		int n;
		while( (n=fread(buf,1,sizeof(buf)-1,fp)) > 0 ) {
			buf[n] = '\0';
			routing_file_str += buf;
		}
		n = my_pclose( fp );
		if( n != 0 ) {
			EXCEPT("Command '%s' specified for %s returned non-zero status %d",
				   routing_cmd.c_str(), PARAM_JOB_ROUTER_ENTRIES_CMD, n);
		}

		ParseRoutingEntries( routing_file_str, PARAM_JOB_ROUTER_ENTRIES_CMD, router_defaults_ad, allow_empty_requirements, new_routes );
	}

	if( routing_file.size() ) {
		FILE *fp = safe_fopen_wrapper_follow(routing_file.c_str(),"r");
		if( !fp ) {
			EXCEPT("Failed to open '%s' file specified for %s.",
				   routing_file.c_str(), PARAM_JOB_ROUTER_ENTRIES_FILE);
		}
		std::string routing_file_str;
		char buf[200];
		int n;
		while( (n=fread(buf,1,sizeof(buf)-1,fp)) > 0 ) {
			buf[n] = '\0';
			routing_file_str += buf;
		}
		fclose( fp );

		ParseRoutingEntries( routing_file_str, PARAM_JOB_ROUTER_ENTRIES_FILE, router_defaults_ad, allow_empty_requirements, new_routes );
	}

	if( routing_str.size() ) {
		ParseRoutingEntries( routing_str, PARAM_JOB_ROUTER_ENTRIES, router_defaults_ad, allow_empty_requirements, new_routes );
	}

	if(!m_enable_job_routing) return;

	SetRoutingTable(new_routes);

		// Whether to release the source job if the routed job
		// goes on hold
	m_release_on_hold = param_boolean("JOB_ROUTER_RELEASE_ON_HOLD", true);

		// default is no maximum (-1)
	m_max_jobs = param_integer("JOB_ROUTER_MAX_JOBS",-1);

	m_max_job_mirror_update_lag = param_integer("MAX_JOB_MIRROR_UPDATE_LAG",600);

	Timeslice periodic_interval;
	periodic_interval.setMinInterval(param_integer("PERIODIC_EXPR_INTERVAL", 60));
	periodic_interval.setMaxInterval(param_integer("MAX_PERIODIC_EXPR_INTERVAL", 1200) );
	periodic_interval.setTimeslice(param_double("PERIODIC_EXPR_TIMESLICE", 0.01, 0, 1));

		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	m_job_router_polling_period = param_integer("JOB_ROUTER_POLLING_PERIOD",10);

		// clear previous timers
	if (m_job_router_polling_timer >= 0) {
		daemonCore->Cancel_Timer(m_job_router_polling_timer);
	}
	if (m_periodic_timer_id >= 0) {
		daemonCore->Cancel_Timer(m_periodic_timer_id);
	}
		// register timer handlers
	m_job_router_polling_timer = daemonCore->Register_Timer(
								  0, 
								  m_job_router_polling_period,
								  (TimerHandlercpp)&JobRouter::Poll, 
								  "JobRouter::Poll", this);

	if (periodic_interval.getMinInterval() > 0) {
		m_periodic_timer_id = daemonCore->Register_Timer(periodic_interval, 
								(TimerHandlercpp)&JobRouter::EvalAllSrcJobPeriodicExprs,
								"JobRouter::EvalAllSrcJobPeriodicExprs",
								this);
		dprintf(D_FULLDEBUG, "JobRouter: Registered EvalAllSrcJobPeriodicExprs() to evaluate periodic expressions.\n");
	}
	else {
		dprintf(D_FULLDEBUG, "JobRouter: Evaluation of periodic expressions disabled.\n");
	}

		// NOTE: if you change the default name, then you are breaking
		// JobRouter's ability to adopt jobs ("orphans") left behind
		// by the previous version of JobRouter.  At least a warning
		// in the release notes is warranted.
	param(m_job_router_name,"JOB_ROUTER_NAME");
		// In order not to get confused by jobs belonging to
		// gridmanager in AdoptOphans(), the job router name must not
		// be empty.
	if( m_job_router_name.empty() ) {
		EXCEPT("JOB_ROUTER_NAME must not be empty");
	}

	InitPublicAd();

	int update_interval = param_integer("UPDATE_INTERVAL", 60);
	if(m_public_ad_update_interval != update_interval) {
		m_public_ad_update_interval = update_interval;

		if(m_public_ad_update_timer >= 0) {
			daemonCore->Cancel_Timer(m_public_ad_update_timer);
			m_public_ad_update_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting update interval to %d\n",
			m_public_ad_update_interval);
		m_public_ad_update_timer = daemonCore->Register_Timer(
			0,
			m_public_ad_update_interval,
			(TimerHandlercpp)&JobRouter::TimerHandler_UpdateCollector,
			"JobRouter::TimerHandler_UpdateCollector",
			this);
	}

	param(m_schedd2_name_buf,"JOB_ROUTER_SCHEDD2_NAME");
	param(m_schedd2_pool_buf,"JOB_ROUTER_SCHEDD2_POOL");
	m_schedd2_name = m_schedd2_name_buf.empty() ? NULL : m_schedd2_name_buf.c_str();
	m_schedd2_pool = m_schedd2_pool_buf.empty() ? NULL : m_schedd2_pool_buf.c_str();
	if( m_schedd2_name ) {
		dprintf(D_ALWAYS,"Routing jobs to schedd %s in pool %s\n",
				m_schedd2_name_buf.c_str(),m_schedd2_pool_buf.c_str());
	}

	param(m_schedd1_name_buf,"JOB_ROUTER_SCHEDD1_NAME");
	param(m_schedd1_pool_buf,"JOB_ROUTER_SCHEDD1_POOL");
	m_schedd1_name = m_schedd1_name_buf.empty() ? NULL : m_schedd1_name_buf.c_str();
	m_schedd1_pool = m_schedd1_pool_buf.empty() ? NULL : m_schedd1_pool_buf.c_str();
	if( m_schedd1_name ) {
		dprintf(D_ALWAYS,"Routing jobs from schedd %s in pool %s\n",
				m_schedd1_name_buf.c_str(),m_schedd1_pool_buf.c_str());
	}
}

classad::ClassAdCollection *
JobRouter::GetSchedd1ClassAds() {
	return m_scheduler->GetClassAds();
}
classad::ClassAdCollection *
JobRouter::GetSchedd2ClassAds() {
	return m_scheduler2 ? m_scheduler2->GetClassAds() : m_scheduler->GetClassAds();
}

void
JobRouter::InitPublicAd()
{
	ASSERT (m_job_router_name.size() > 0);

	char *valid_name = build_valid_daemon_name(m_job_router_name.c_str());
	ASSERT( valid_name );
	daemonName = valid_name;
	delete [] valid_name;

	m_public_ad = ClassAd();

	m_public_ad.SetMyTypeName("Job_Router");
	m_public_ad.SetTargetTypeName("");

	m_public_ad.Assign(ATTR_NAME,daemonName.c_str());

	daemonCore->publish(&m_public_ad);
}

void
JobRouter::EvalAllSrcJobPeriodicExprs()
{
	RoutedJob *job;
	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();
	classad::ClassAd *orig_ad;

	dprintf(D_FULLDEBUG, "JobRouter: Evaluating all managed jobs periodic "
			"job policy expressions.\n");

	m_jobs.startIterations();
	while(m_jobs.iterate(job))
	{
		orig_ad = ad_collection->GetClassAd(job->src_key);
		if (false == EvalSrcJobPeriodicExpr(job))
		{
			dprintf(D_ALWAYS, "JobRouter failure (%s): Unable to "
					"evaluate job's periodic policy "
					"expressions.\n", job->JobDesc().c_str());
			if( !orig_ad ) {
				dprintf(D_ALWAYS, "JobRouter failure (%s): "
					"failed to reset src job "
					"attributes, because ad not found"
					"in collection.\n",job->JobDesc().c_str());
				continue;
			}

			job->SetSrcJobAd(job->src_key.c_str(), orig_ad, ad_collection);
			if (false == push_dirty_attributes(job->src_ad,m_schedd1_name,m_schedd1_pool))
			{
				dprintf(D_ALWAYS, "JobRouter failure (%s): "
						"failed to reset src job "
						"attributes in the schedd.\n",
						job->JobDesc().c_str());
			}
			else
			{
				dprintf(D_ALWAYS, "JobRouter (%s): reset src "
						"job attributes in the "
						"schedd\n", job->JobDesc().c_str());
				job->src_ad.ClearAllDirtyFlags();
			}
		}
	}

	dprintf(D_FULLDEBUG, "JobRouter: Evaluated all managed jobs periodic expressions.\n");
	return;
}

bool
JobRouter::EvalSrcJobPeriodicExpr(RoutedJob* job)
{
	UserPolicy user_policy;
	ClassAd converted_ad;
	int action;
	MyString reason;
	int reason_code;
	int reason_subcode;
	bool ret_val = false;

	converted_ad = job->src_ad;
	user_policy.Init(&converted_ad);

	action = user_policy.AnalyzePolicy(PERIODIC_ONLY);

	user_policy.FiringReason(reason,reason_code,reason_subcode);
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch(action)
	{
		case UNDEFINED_EVAL:
			ret_val = SetJobHeld(job->src_ad, reason.Value(), reason_code, reason_subcode);
			break;
		case STAYS_IN_QUEUE:
			// do nothing
			ret_val = true;
			break;
		case REMOVE_FROM_QUEUE:
			ret_val = SetJobRemoved(job->src_ad, reason.Value());
			break;
		case HOLD_IN_QUEUE:
			ret_val = SetJobHeld(job->src_ad, reason.Value(), reason_code, reason_subcode);
			break;
		case RELEASE_FROM_HOLD:
			// When a job that is managed by the job router is
			// held, the job router cleans up the routed job and
			// releases control of the job.  Releasing the job
			// from hold will cause the job router to claim
			// the job as if it is a new job from the schedd.
			ret_val = true;
			break;
		default:
			EXCEPT("Unknown action (%d) in "
				"JobRouter::EvalSrcJobPeriodicExpr(%s)",
				 action, job->JobDesc().c_str());
	}

	return ret_val;
}

bool
JobRouter::SetJobHeld(classad::ClassAd& ad, const char* hold_reason, int hold_code, int sub_code)
{
	int status, num_holds;
	int cluster, proc;
	bool ret_val = false;
	std::string release_reason;

	ad.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster);
	ad.EvaluateAttrInt(ATTR_PROC_ID, proc);

	if (false == ad.EvaluateAttrInt(ATTR_JOB_STATUS, status))
	{
		dprintf(D_ALWAYS, "JobRouter failure (%d.%d): Unable to "					"retrieve current job status.\n", cluster,proc);
		return false;
	}
	if (HELD != status)
	{
		if (REMOVED == status)
		{
			ad.InsertAttr(ATTR_JOB_STATUS_ON_RELEASE, REMOVED);
		}
		ad.InsertAttr(ATTR_JOB_STATUS, HELD);
		ad.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL));
		ad.InsertAttr(ATTR_HOLD_REASON, hold_reason);
		ad.InsertAttr(ATTR_HOLD_REASON_CODE, hold_code);
		ad.InsertAttr(ATTR_HOLD_REASON_SUBCODE, sub_code);
		if (true == ad.EvaluateAttrString(ATTR_RELEASE_REASON, release_reason))
		{
			ad.InsertAttr(ATTR_LAST_RELEASE_REASON, release_reason.c_str());
		}
		ad.InsertAttr(ATTR_RELEASE_REASON, "Undefined");
		ad.EvaluateAttrInt(ATTR_NUM_SYSTEM_HOLDS, num_holds);
		num_holds++;
		ad.InsertAttr(ATTR_NUM_SYSTEM_HOLDS, num_holds);

		WriteHoldEventToUserLog(ad);

		if(false == push_dirty_attributes(ad,m_schedd1_name,m_schedd1_pool))
		{
			dprintf(D_ALWAYS,"JobRouter failure (%d.%d): failed to "
					"place job on hold.\n", cluster, proc);
			ret_val = false;
		}
		else
		{
			dprintf(D_FULLDEBUG, "JobRouter (%d.%d): Placed job "
					"on hold.\n", cluster, proc);
			ad.ClearAllDirtyFlags();
			ret_val = true;
		}
	}
	return ret_val;
}

bool
JobRouter::SetJobRemoved(classad::ClassAd& ad, const char* remove_reason)
{
	int status;
	int cluster, proc;
	bool ret_val = false;

	ad.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster);
	ad.EvaluateAttrInt(ATTR_PROC_ID, proc);

	if (false == ad.EvaluateAttrInt(ATTR_JOB_STATUS, status))
	{
		dprintf(D_ALWAYS, "JobRouter failure (%d.%d): Unable to "					"retrieve current job status.\n", cluster,proc);
		return false;
	}
	if (REMOVED != status)
	{
		ad.InsertAttr(ATTR_JOB_STATUS, REMOVED);
		ad.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL));
		ad.InsertAttr(ATTR_REMOVE_REASON, remove_reason);
		if(false == push_dirty_attributes(ad,m_schedd1_name,m_schedd1_pool))
		{
			dprintf(D_ALWAYS,"JobRouter failure (%d.%d): failed to "
					"remove job.\n", cluster, proc);
			ret_val = false;
		}
		else
		{
			dprintf(D_FULLDEBUG, "JobRouter (%d.%d): Removed job.\n", cluster, proc);
			ad.ClearAllDirtyFlags();
			ret_val = true;
		}
	}

	return ret_val;
}

void
JobRouter::ParseRoutingEntries( std::string const &routing_string, char const *param_name, classad::ClassAd const &router_defaults_ad, bool allow_empty_requirements, RoutingTable *new_routes ) {

		// Now parse a list of routing entries.  The expected syntax is
		// a list of ClassAds, optionally delimited by commas and or
		// whitespace.

	dprintf(D_FULLDEBUG,"Parsing %s=%s\n",param_name,routing_string.c_str());

	int offset = 0;
	while(1) {
		if(offset >= (int)routing_string.size()) break;


		JobRoute route;
		JobRoute *existing_route;
		int this_offset = offset; //save offset before eating an ad.
		bool ignore_route = false;

		if(!route.ParseClassAd(routing_string,offset,&router_defaults_ad,allow_empty_requirements))
		{
			classad::ClassAdParser parser;
			classad::ClassAd ad;
			int final_offset = this_offset;
			std::string final_routing_string = routing_string;
			final_routing_string += "\n[]"; // add an empty ClassAd

			if(parser.ParseClassAd(final_routing_string,ad,final_offset)) {
					// There must have been some trailing whitespace or
					// comments after the last ClassAd, so the only reason
					// ParseClassAd() failed was because there was no ad.
					// Therefore, we are done.
				break;
			}

			dprintf(D_ALWAYS,"JobRouter CONFIGURATION ERROR: Ignoring the malformed route entry in %s, starting here: %s\n",param_name,routing_string.c_str() + this_offset);

			// skip any junk and try parsing the next route in the list
			while((int)routing_string.size() > offset && routing_string[offset] != '[') offset++;

			ignore_route = true;
		}
		else if(new_routes->lookup(route.Name(),existing_route)!=-1)
		{
			// Two routes have the same name.  Since route names
			// are optional, these names may have been
			// auto-generated from other portions of the route ad.
			// Warn the user about that.

			int override = route.OverrideRoutingEntry();
			if( override < 0 ) {
				dprintf(D_ALWAYS,"JobRouter CONFIGURATION WARNING while parsing %s: two route entries have the same name '%s' so the second one will override the first one; if you have not already explicitly given these routes a name with name=\"blah\", you may want to give them different names.  If you just want to suppress this warning, then define OverrideRoutingEntry=True/False in the second routing entry.\n",param_name,route.Name());
				override = 1;
			}
			if( override > 0 ) {  // OverrideRoutingEntry=true
				new_routes->remove(route.Name());
				delete existing_route;
			}
			if( override == 0 ) { // OverrideRoutingEntry=false
				ignore_route = true;
			}
		}

		if( !ignore_route ) {
			new_routes->insert(route.Name(),new JobRoute(route));
		}
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
		else {
				// preserve state from the old route entry
			route->CopyState(old_route);
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
	is_running = false;
	is_success = false;
	is_sandboxed = false;
	submission_time = 0;
	retirement_time = 0;
	proxy_file_copy_chowned = false;
	target_universe = CONDOR_UNIVERSE_GRID;
	saw_dest_job = false;
	edit_job_in_place = false;
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
		if(!dest->Insert(itr->first,tree, false)) {
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
	classad::ClassAdCollection *ad_collection = GetSchedd1ClassAds();
	classad::ClassAdCollection *ad_collection2 = GetSchedd2ClassAds();
	std::string dest_key,src_key;
	std::string dest_jobs,src_jobs;

	// Find all jobs submitted by a JobRouter with same name as me.
	// Maybe some of them were from a previous life and are now orphans.
	dest_jobs += "target.ProcId >= 0 && target.";
	dest_jobs += JR_ATTR_ROUTED_BY;
	dest_jobs += " == \"";
	dest_jobs += m_job_router_name;
	dest_jobs += "\"";
		//Have observed problems in which we get inconsistent snapshot
		//of the job queue; ensure that we at least have the Owner
		//attribute, or we'll run into trouble.
	dest_jobs += " && target.Owner isnt Undefined";

	constraint_tree = parser.ParseExpression(dest_jobs.c_str());
	if(!constraint_tree) {
		EXCEPT("JobRouter: Failed to parse orphan dest job constraint: '%s'\n",dest_jobs.c_str());
	}

    query.Bind(ad_collection2);
    if(!query.Query("root",constraint_tree)) {
		dprintf(D_ALWAYS,"JobRouter: Error running orphan dest job query: %s\n",dest_jobs.c_str());
		delete constraint_tree;
		return;
	}
	delete constraint_tree;

    query.ToFirst();
    if( query.Current(dest_key) ) do {
		std::string route_name;
		classad::ClassAd *dest_ad = ad_collection2->GetClassAd(dest_key);
		ASSERT(dest_ad);

		if(!dest_ad->EvaluateAttrString(JR_ATTR_ROUTED_FROM_JOB_ID, src_key) ||
		   !dest_ad->EvaluateAttrString(JR_ATTR_ROUTE_NAME, route_name))
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
			if(!remove_job(*dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",m_schedd2_name,m_schedd2_pool,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (src=%s,dest=%s): failed to remove dest job: %s\n",src_key.c_str(),dest_key.c_str(),err_desc.Value());
			}
			continue;
		}

		//If we get here, we have enough information to recover the routed job.
		RoutedJob *job = new RoutedJob();
		job->state = RoutedJob::SUBMITTED;
		job->dest_key = dest_key;
		job->dest_proc_id = getProcByString(dest_key.c_str());
		job->route_name = route_name;
		job->submission_time = time(NULL); //not true; but good enough
		job->is_claimed = true;
		if(!job->SetSrcJobAd(src_key.c_str(),src_ad,ad_collection)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): error processing orphan src job ad\n",job->JobDesc().c_str());
			delete job;
			continue;
		}
		job->SetDestJobAd(dest_ad);

		bool is_sandboxed_test = TestJobSandboxed(job);

		std::string submit_iwd;
		std::string submit_iwd_attr = "SUBMIT_";
		submit_iwd_attr += ATTR_JOB_IWD;
		if(dest_ad->EvaluateAttrString(submit_iwd_attr, submit_iwd)) {
			job->is_sandboxed = true;
		}
		else {
			job->is_sandboxed = false;
		}
		if( job->is_sandboxed != is_sandboxed_test ) {
			dprintf(D_ALWAYS,
					"JobRouter warning (%s): %s evaluated to %s, "
					"but %s is %s target job, so assuming job "
					"is %ssandboxed.\n",
					job->JobDesc().c_str(),
					JR_ATTR_JOB_SANDBOXED_TEST,
					is_sandboxed_test ? "true" : "false",
					submit_iwd_attr.c_str(),
					job->is_sandboxed ? "present in" : "missing from",
					job->is_sandboxed ? "" : "not ");
		}

		if(!AddJob(job)) {
			dprintf(D_ALWAYS,"JobRouter (%s): failed to add orphaned job to my routed job list; aborting it.\n",job->JobDesc().c_str());
			MyString err_desc;
			if(!remove_job(job->dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",m_schedd2_name,m_schedd2_pool,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.Value());
			}
			delete job;
		}
		else {
			dprintf(D_FULLDEBUG,"JobRouter (%s): adopted orphaned job (from previous run?)\n",job->JobDesc().c_str());
		}
	} while (query.Next(dest_key));


	src_jobs = "target.Managed == \"External\" && target.ManagedManager == \"";
	src_jobs += m_job_router_name;
	src_jobs += "\"";
		//Have observed problems in which we get inconsistent snapshot
		//of the job queue; ensure that we at least have the Owner
		//attribute, or we'll run into trouble.
	src_jobs += " && target.Owner isnt Undefined";

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
		if(!yield_job(*src_ad,m_schedd1_name,m_schedd1_pool,false,src_proc_id.cluster,src_proc_id.proc,&error_details,JobRouterName().c_str(),true,m_release_on_hold)) {
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
		dprintf(D_ALWAYS,
		      "JobRouter (route=%s): %d submitted (max %d), %d idle (max %d), throttle: %s, recent stats: %d started, %d succeeded, %d failed.\n",
		      route->Name(),
		      route->CurrentRoutedJobs(),
		      route->MaxJobs(),
		      route->CurrentIdleJobs(),
		      route->MaxIdleJobs(),
		      route->ThrottleDesc().c_str(),
			  route->RecentRoutedJobs(),
		      route->RecentSuccesses(),
			  route->RecentFailures());
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
	umbrella_constraint += " && (target.ProcId >= 0 && target.JobStatus == 1 && (target.StageInStart is undefined || target.StageInFinish isnt undefined) && target.Managed isnt \"ScheddDone\" && target.Managed isnt \"External\" && target.Owner isnt Undefined && target.";
	umbrella_constraint += JR_ATTR_ROUTED_BY;
	umbrella_constraint += " isnt \"";
	umbrella_constraint += m_job_router_name;
	umbrella_constraint += "\")";

	if(!can_switch_ids()) {
			// We are not running as root.  Ensure that we only try to
			// manage jobs submitted by the same user we are running as.

		char *username = my_username();
		char *domain = my_domainname();

		ASSERT(username);

		umbrella_constraint += " && (target.";
		umbrella_constraint += ATTR_OWNER;
		umbrella_constraint += " == \"";
		umbrella_constraint += username;
		umbrella_constraint += "\"";
		if(domain) {
			umbrella_constraint += " && target.";
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

	constraint_tree = parser.ParseExpression(umbrella_constraint);
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
		route = ChooseRoute(ad,&all_routes_full);
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
		job->target_universe = route->TargetUniverse();
		job->grid_resource = route->GridResource();
		job->route_name = route->Name();

		if(!job->SetSrcJobAd(key.c_str(),ad,ad_collection)) {
			delete job;
			continue;
		}
		job->is_sandboxed = TestJobSandboxed(job);
		job->edit_job_in_place = TestEditJobInPlace(job);

		/*
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): parent = %s\n",job->JobDesc().c_str(),ClassAdToString(parent).c_str());
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): child = %s\n",job->JobDesc().c_str(),ClassAdToString(ad).c_str());
		dprintf(D_FULLDEBUG,"JobRouter DEBUG (%s): combined = %s\n",job->JobDesc().c_str(),ClassAdToString(&job->src_ad).c_str());
		*/

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

	static unsigned round_robin = 0;
	unsigned choice = (round_robin++) % matches.size();
	ASSERT(choice < matches.size());
	route = matches[choice];

	route->IncrementRoutedJobs();
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
				if(job->IsRunning()) {
					route->IncrementCurrentRunningJobs();
				}
			}
		}
	}

	m_routes->startIterations();
	while(m_routes->iterate(route)) {
		route->AdjustFailureThrottles();
	}
}

/*
std::string
JobRouter::DaemonIdentityString() {
	std::string identity;
	identity += m_scheduler->Name();
	return identity;
}
*/

void
JobRouter::TakeOverJob(RoutedJob *job) {
	if(job->state != RoutedJob::UNCLAIMED) return;

	if(job->edit_job_in_place) {
			// we do not claim the job if we are just editing the src job
		job->state = RoutedJob::CLAIMED;
		return;
	}

	MyString error_details;
	ClaimJobResult cjr = claim_job(job->src_ad,m_schedd1_name,m_schedd1_pool,job->src_proc_id.cluster, job->src_proc_id.proc, &error_details, JobRouterName().c_str(), job->is_sandboxed);

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

#if HAVE_JOB_HOOKS
	if (NULL != m_hook_mgr)
	{
	        std::string route_info;

	        // Retrieve the routing definition
	        JobRoute *route = GetRouteByName(job->route_name.c_str());
        	if(!route) {
        		dprintf(D_FULLDEBUG,"JobRouter (%s): Unable to retrieve route information for translation hook.\n",job->JobDesc().c_str());
        		GracefullyRemoveJob(job);
        		return;
        	}

		route_info = route->RouteString();
		int rval = m_hook_mgr->hookTranslateJob(job, route_info);
		switch (rval)
		{
			case -1:    // Error
					// No need to print status messages
					// as the lower levels should be
					// handling that.
				return;
				break;
			case 0:    // Hook not configured
				break;
			case 1:    // Spawned the hook
					// Done for now.  Let the handler call
					// FinishSubmitJob() when the hook
					// exits.
				return;
				break;
		}
	}
#endif
	job->dest_ad = job->src_ad;
		// If we are not just editing the src job, then we need to do
		// the standard transformations to create a new job now.
	if(!job->edit_job_in_place) {
		VanillaToGrid::vanillaToGrid(&job->dest_ad,job->target_universe,job->grid_resource.c_str(),job->is_sandboxed);
	}
	FinishSubmitJob(job);
}

void
JobRouter::FinishSubmitJob(RoutedJob *job) {
	// Apply any edits to the job ClassAds as defined in the route ad.
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

	if(job->edit_job_in_place) {
		if(!push_classad_diff(job->src_ad,job->dest_ad,m_schedd1_name,m_schedd1_pool)) {
			dprintf(D_ALWAYS, "JobRouter failure (%s): "
					"Failed to edit job.\n",
					job->JobDesc().c_str());
		}
		else {
				// Update our local copy in the job queue mirror
			classad::ClassAdCollection *ad_collection = GetSchedd1ClassAds();
			ad_collection->RemoveClassAd(job->src_key);
			ad_collection->AddClassAd(job->src_key, new ClassAd(job->dest_ad));

			dprintf(D_ALWAYS, "JobRouter (%s): Done editing job.\n",
					job->JobDesc().c_str());
			job->is_success = true;
			job->is_done = true;
		}
			// All done editing the job.
		GracefullyRemoveJob(job);
		return;
	}

	// Record the src job id in the new job's ad, so we can recover
	// in case of a crash or restart.
	job->dest_ad.InsertAttr(JR_ATTR_ROUTED_FROM_JOB_ID,job->src_key.c_str());
	job->dest_ad.InsertAttr(JR_ATTR_ROUTED_BY,JobRouterName().c_str());
	job->dest_ad.InsertAttr(JR_ATTR_ROUTE_NAME,route->Name());

	// In case this attribute was in the src job from a previous run,
	// get rid of it.
	job->dest_ad.Delete(JR_ATTR_ROUTED_TO_JOB_ID);

	if(!job->PrepareSharedX509UserProxy(route)) {
		GracefullyRemoveJob(job);
		return;
	}

	int dest_cluster_id = -1;
	int dest_proc_id = -1;
	bool rc;
	rc = submit_job(job->dest_ad,m_schedd2_name,m_schedd2_pool,job->is_sandboxed,&dest_cluster_id,&dest_proc_id);

		// Now that the job is submitted, we can clean up any temporary
		// x509 proxy files, because these will have been copied into
		// the job's sandbox.
	if(!job->CleanupSharedX509UserProxy(route)) {
		GracefullyRemoveJob(job);
		return;
	}

	if(!rc) {
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

bool
RoutedJob::PrepareSharedX509UserProxy(JobRoute *route)
{
	if(!route->EvalUseSharedX509UserProxy(this)) {
		return true;
	}

	std::string src_proxy_file;
	if(!route->EvalSharedX509UserProxy(this,src_proxy_file)) {
		dprintf(D_ALWAYS,
				"JobRouter failure (%s): %s is true, but %s is invalid!\n",
				JobDesc().c_str(),
				JR_ATTR_USE_SHARED_X509_USER_PROXY,
				JR_ATTR_SHARED_X509_USER_PROXY);
		return false;
	}

	if(!is_sandboxed) {
		dprintf(D_ALWAYS,
				"JobRouter failure (%s): %s is true, but %s is false.\n",
				JobDesc().c_str(),
				JR_ATTR_USE_SHARED_X509_USER_PROXY,
				JR_ATTR_JOB_SANDBOXED_TEST);
		return false;
	}

	proxy_file_copy = src_proxy_file;
	proxy_file_copy += ".";
	proxy_file_copy += src_key;
	proxy_file_copy_chowned = false;

		// This file better be owned by our effective uid (e.g. condor).
		// Rather than switching to root priv and forcing this to succeed,
		// it is better to make admin chown it to condor, so they are
		// reminding that access to this file is being managed by
		// condor now.
	if( copy_file(src_proxy_file.c_str(),proxy_file_copy.c_str()) != 0 ) {
		dprintf(D_ALWAYS,
				"JobRouter failure (%s): failed to copy %s to %s.\n",
				JobDesc().c_str(),
				src_proxy_file.c_str(),
				proxy_file_copy.c_str());
		return false;
	}

		// Now chown() the proxy file to the user
#if !defined(WIN32)
	std::string owner;
	src_ad.EvaluateAttrString(ATTR_OWNER,owner);

	uid_t dst_uid;
	gid_t dst_gid;
	passwd_cache* p_cache = pcache();
	if( ! p_cache->get_user_ids(owner.c_str(), dst_uid, dst_gid) ) {
		dprintf( D_ALWAYS,
				 "JobRouter failure (%s): Failed to find UID and GID for "
				 "user %s. Cannot chown %s to user.\n",
				 JobDesc().c_str(),owner.c_str(), proxy_file_copy.c_str() );
		CleanupSharedX509UserProxy(route);
		return false;
	}

	if( getuid() != dst_uid ) {
		priv_state old_priv = set_root_priv();

		int chown_rc = chown(proxy_file_copy.c_str(),dst_uid,dst_gid);
		int chown_errno = errno;

		set_priv(old_priv);

		if(chown_rc != 0) {
			dprintf( D_ALWAYS,
					 "JobRouter failure (%s): Failed to change "
					 "ownership of %s for user %s: %s\n",
					 JobDesc().c_str(),proxy_file_copy.c_str(),
					 owner.c_str(),strerror(chown_errno));
			CleanupSharedX509UserProxy(route);
			return false;
		}

		proxy_file_copy_chowned = true;
	}
#endif

	dest_ad.InsertAttr(ATTR_X509_USER_PROXY,proxy_file_copy.c_str());

	return true;
}

bool
RoutedJob::CleanupSharedX509UserProxy(JobRoute * /*route*/)
{
	if(proxy_file_copy.size()) {
		priv_state old_priv = PRIV_UNKNOWN;
		if(proxy_file_copy_chowned) {
			old_priv = set_root_priv();
		}

		int remove_rc = remove(proxy_file_copy.c_str());

		if(proxy_file_copy_chowned) {
			set_priv(old_priv);
		}

		if(remove_rc != 0) {
			dprintf( D_ALWAYS,
					 "JobRouter failure (%s): Failed to remove %s\n",
					 JobDesc().c_str(),proxy_file_copy.c_str());
			return false;
		}
		proxy_file_copy = "";
		proxy_file_copy_chowned = false;
	}
	return true;
}



static bool ClassAdHasDirtyAttributes(classad::ClassAd *ad) {
	return ad->dirtyBegin() != ad->dirtyEnd();
}

void
JobRouter::UpdateRoutedJobStatus(RoutedJob *job, classad::ClassAd update) {
	classad::ClassAd *new_ad = NULL;
	classad::ClassAdCollection *ad_collection2 = GetSchedd2ClassAds();

	// The dest_key (dest_ad) may have changed while we are running,
	// meaning we'll be out of sync with the ClassAdCollection. To
	// avoid writing stale data back into the collection we MUST pull
	// from it before updating anything.
	new_ad = ad_collection2->GetClassAd(job->dest_key);
	if (NULL == new_ad)
	{
		dprintf (D_ALWAYS, "JobRouter failure (%s): Ad %s disappeared "
				"before update finished.  Nothing will be"
				"updated.\n", job->JobDesc().c_str(), job->dest_key.c_str());
		GracefullyRemoveJob(job);
		return;
	}
	job->SetDestJobAd(new_ad);

	// Reset the dirty bits so only new or updated fields are
	// sent to the job queue.
	job->dest_ad.ClearAllDirtyFlags();

	// Update the routed job's status
	if (false == job->dest_ad.Update(update))
	{
		dprintf(D_ALWAYS, "JobRouter failure (%s): Failed to update"
				"routed job status.\n", job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	// Send the updates to the job queue
	if (false == PushUpdatedAttributes(job->dest_ad))
	{
		dprintf(D_ALWAYS, "JobRouter failure (%s): Failed to update "
				"routed job status.\n", job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	// Update the local copy
	ad_collection2->UpdateClassAd(job->dest_key, &job->dest_ad);
	dprintf(D_FULLDEBUG,"JobRouter (%s): updated routed job status\n",job->JobDesc().c_str());

	FinishCheckSubmittedJobStatus(job);
}

void
JobRouter::CheckSubmittedJobStatus(RoutedJob *job) {
	if(job->state != RoutedJob::SUBMITTED) return;

#if HAVE_JOB_HOOKS
	if (NULL != m_hook_mgr)
	{
		int rval = m_hook_mgr->hookUpdateJobInfo(job);
		switch (rval)
		{
			case -1:    // Error
					// No need to print status messages
					// as the lower levels should be
					// handling that.
				return;
				break;
			case 0:    // Hook not configured
				break;
			case 1:    // Spawned the hook
					// Done for now.  Let the handler call
					// FinishSubmitJob() when the hook
					// exits.
				return;
				break;
		}
	}
#endif

	FinishCheckSubmittedJobStatus(job);
}

void
JobRouter::FinishCheckSubmittedJobStatus(RoutedJob *job) {
	classad::ClassAdCollection *ad_collection = GetSchedd1ClassAds();
	classad::ClassAdCollection *ad_collection2 = GetSchedd2ClassAds();
	classad::ClassAd *src_ad = ad_collection->GetClassAd(job->src_key);
	std::string keyword;
	std::string copy_attr_param;
	char* custom_attrs = NULL;

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
		WriteAbortEventToUserLog( *src_ad );
		GracefullyRemoveJob(job);
		return;
	}

	if(job_status == HELD && !hold_copied_from_target_job(*src_ad) ) {
		dprintf(D_FULLDEBUG, "JobRouter (%s): found src job on hold\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	classad::ClassAd *ad = ad_collection2->GetClassAd(job->dest_key);

	// If ad is not found, this could be because Quill hasn't seen
	// it yet, in which case this is not a problem.  The following
	// attempts to ensure this by seeing if enough time has passed
	// since we submitted the job.

	if(!ad) {
		int age = time(NULL) - job->submission_time;
		if(job->SawDestJob()) {
				// we have seen the dest job before, but now it is gone,
				// so it must have been removed
			dprintf(D_ALWAYS,"JobRouter (%s): dest job was removed!\n",job->JobDesc().c_str());
			GracefullyRemoveJob(job);
			return;
		}
		if(age > m_max_job_mirror_update_lag) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): giving up, because submitted job is still not in job queue mirror (submitted %d seconds ago).  Perhaps it has been removed?\n",job->JobDesc().c_str(),age);
			GracefullyRemoveJob(job);
			return;
		}
		dprintf(D_FULLDEBUG,"JobRouter (%s): submitted job has not yet appeared in job queue mirror or was removed (submitted %d seconds ago)\n",job->JobDesc().c_str(),age);
		return;
	}

	job->SetDestJobAd(ad);
#if HAVE_JOB_HOOKS
	keyword = m_hook_mgr->getHookKeyword(job->src_ad);
	if(0 < keyword.length()) {
		copy_attr_param = keyword;
		copy_attr_param += "_ATTRS_TO_COPY";
		custom_attrs = param(copy_attr_param.c_str());
	}
#endif
	if(!update_job_status(*src_ad,job->dest_ad,job->src_ad,custom_attrs)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to update job status\n",job->JobDesc().c_str());
	}
	else if(ClassAdHasDirtyAttributes(&job->src_ad)) {
		if (false == PushUpdatedAttributes(job->src_ad)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): failed to update src job\n",job->JobDesc().c_str());

			GracefullyRemoveJob(job);
#if HAVE_JOB_HOOKS
			if (custom_attrs != NULL) {
				free(custom_attrs);
				custom_attrs = NULL;
			}
#endif
			return;
		}
		else {
			dprintf(D_FULLDEBUG,"JobRouter (%s): updated job status\n",job->JobDesc().c_str());
		}
	}
#if HAVE_JOB_HOOKS
	if (custom_attrs != NULL) {
		free(custom_attrs);
		custom_attrs = NULL;
	}
#endif

	job_status = 0;
	if( !ad->EvaluateAttrInt( ATTR_JOB_STATUS, job_status ) ) {
		dprintf(D_ALWAYS, "JobRouter failure (%s): cannot evaluate JobStatus in target job\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}
	job->is_running = (job_status == RUNNING || job_status == TRANSFERRING_OUTPUT);

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

#if HAVE_JOB_HOOKS
	if (NULL != m_hook_mgr)
	{
		int rval = m_hook_mgr->hookJobExit(job);
		switch (rval)
		{
			case -1:    // Error
					// No need to print status messages
					// as the lower levels should be
					// handling that.
				return;
				break;
			case 0:    // Hook not configured
				break;
			case 1:    // Spawned the hook
					// Done for now.  Let the handler call
					// FinishFinalizeJob() when the hook
					// exits.
				return;
				break;
		}
	}
#endif

	FinishFinalizeJob(job);
}

void
JobRouter::RerouteJob(RoutedJob *job) {
	SetJobIdle(job);
	GracefullyRemoveJob(job);
}

void
JobRouter::SetJobIdle(RoutedJob *job) {
	job->src_ad.InsertAttr(ATTR_JOB_STATUS,IDLE);
	if(false == PushUpdatedAttributes(job->src_ad)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to set src job status back to idle\n",job->JobDesc().c_str());
	}
}

bool
JobRouter::PushUpdatedAttributes(classad::ClassAd& src) {
	if(false == push_dirty_attributes(src,m_schedd1_name,m_schedd1_pool))
	{
		return false;
	}
	else
	{
		src.ClearAllDirtyFlags();
	}
	return true;
}

void
JobRouter::FinishFinalizeJob(RoutedJob *job) {
	if(!finalize_job(job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,m_schedd2_name,m_schedd2_pool,job->is_sandboxed)) {
		dprintf(D_ALWAYS,"JobRouter failure (%s): failed to finalize job\n",job->JobDesc().c_str());

			// Put the src job back in idle state to prevent it from
			// exiting the queue.
		SetJobIdle(job);
	}
	else if(!WriteTerminateEventToUserLog(job->src_ad)) {
	}
	else {
		EmailTerminateEvent(job->src_ad);

		dprintf(D_ALWAYS,"JobRouter (%s): finalized job\n",job->JobDesc().c_str());
		job->is_done = true;

		job->is_success = TestJobSuccess(job);
		if(!job->is_success) {
			dprintf(D_ALWAYS,"Job Router (%s): %s is true, so job will count as a failure\n",job->JobDesc().c_str(),JR_ATTR_JOB_FAILURE_TEST);
		}
	}

	GracefullyRemoveJob(job);
}

bool
JobRouter::TestJobSuccess(RoutedJob *job)
{
	JobRoute *route = GetRouteByName(job->route_name.c_str());
	if(!route) {
			// It doesn't matter what we decide here, because there is no longer
			// any route associated with this job.
		return true;
	}
	classad::MatchClassAd mad;
	bool test_result = false;

	mad.ReplaceLeftAd(route->RouteAd());
	mad.ReplaceRightAd(&job->src_ad);

	classad::ClassAd *upd;
	classad::ClassAdParser parser;
	std::string upd_str;
	upd_str = "[leftJobFailureTest = LEFT.";
	upd_str += JR_ATTR_JOB_FAILURE_TEST;
	upd_str += " ;]";
	upd = parser.ParseClassAd(upd_str);
	ASSERT(upd);

	mad.Update(*upd);
	delete upd;

	bool rc = mad.EvaluateAttrBool("leftJobFailureTest", test_result);
	if(!rc) {
			// UNDEFINED etc. are treated as NOT failure
		test_result = false;
	}

	mad.RemoveLeftAd();
	mad.RemoveRightAd();

	return !test_result;
}

bool
JobRouter::TestJobSandboxed(RoutedJob *job)
{
	JobRoute *route = GetRouteByName(job->route_name.c_str());
	if(!route) {
			// It doesn't matter what we decide here, because there is no longer
			// any route associated with this job.
		return true;
	}
	classad::MatchClassAd mad;
	bool test_result = false;

	mad.ReplaceLeftAd(route->RouteAd());
	mad.ReplaceRightAd(&job->src_ad);

	classad::ClassAd *upd;
	classad::ClassAdParser parser;
	std::string upd_str;
	upd_str = "[leftJobSandboxedTest = LEFT.";
	upd_str += JR_ATTR_JOB_SANDBOXED_TEST;
	upd_str += " ;]";
	upd = parser.ParseClassAd(upd_str);
	ASSERT(upd);

	mad.Update(*upd);
	delete upd;

	bool rc = mad.EvaluateAttrBool("leftJobSandboxedTest", test_result);
	if(!rc) {
			// UNDEFINED etc. are treated as NOT sandboxed
		test_result = false;
	}

	mad.RemoveLeftAd();
	mad.RemoveRightAd();

	return test_result;
}

bool
JobRouter::TestEditJobInPlace(RoutedJob *job)
{
	JobRoute *route = GetRouteByName(job->route_name.c_str());
	if(!route) {
			// It doesn't matter what we decide here, because there is no longer
			// any route associated with this job.
		return true;
	}
	classad::MatchClassAd mad;
	bool test_result = false;

	mad.ReplaceLeftAd(route->RouteAd());
	mad.ReplaceRightAd(&job->src_ad);

	classad::ClassAd *upd;
	classad::ClassAdParser parser;
	std::string upd_str;
	upd_str = "[leftEditJobInPlace = LEFT.";
	upd_str += JR_ATTR_EDIT_JOB_IN_PLACE;
	upd_str += " ;]";
	upd = parser.ParseClassAd(upd_str);
	ASSERT(upd);

	mad.Update(*upd);
	delete upd;

	bool rc = mad.EvaluateAttrBool("leftEditJobInPlace", test_result);
	if(!rc) {
			// UNDEFINED etc. are treated as false
		test_result = false;
	}

	mad.RemoveLeftAd();
	mad.RemoveRightAd();

	return test_result;
}

bool
JobRoute::EvalUseSharedX509UserProxy(RoutedJob *job)
{
	classad::MatchClassAd mad;
	bool test_result = false;

	mad.ReplaceLeftAd(RouteAd());
	mad.ReplaceRightAd(&job->src_ad);

	classad::ClassAd *upd;
	classad::ClassAdParser parser;
	std::string upd_str;
	upd_str = "[leftTest = LEFT.";
	upd_str += JR_ATTR_USE_SHARED_X509_USER_PROXY;
	upd_str += " ;]";
	upd = parser.ParseClassAd(upd_str);
	ASSERT(upd);

	mad.Update(*upd);
	delete upd;

	bool rc = mad.EvaluateAttrBool("leftTest", test_result);
	if(!rc) {
			// UNDEFINED etc. are treated as FALSE
		test_result = false;
	}

	mad.RemoveLeftAd();
	mad.RemoveRightAd();

	return test_result;
}

bool
JobRoute::EvalSharedX509UserProxy(RoutedJob *job,std::string &proxy_file)
{
	classad::MatchClassAd mad;

	mad.ReplaceLeftAd(RouteAd());
	mad.ReplaceRightAd(&job->src_ad);

	classad::ClassAd *upd;
	classad::ClassAdParser parser;
	std::string upd_str;
	upd_str = "[leftValue = LEFT.";
	upd_str += JR_ATTR_SHARED_X509_USER_PROXY;
	upd_str += " ;]";
	upd = parser.ParseClassAd(upd_str);
	ASSERT(upd);

	mad.Update(*upd);
	delete upd;

	bool rc = mad.EvaluateAttrString("leftValue", proxy_file);

	mad.RemoveLeftAd();
	mad.RemoveRightAd();

	return rc;
}

void
JobRouter::CleanupJob(RoutedJob *job) {
	if(job->state != RoutedJob::CLEANUP) return;

#if HAVE_JOB_HOOKS
	if (NULL != m_hook_mgr)
	{
		int rval = m_hook_mgr->hookJobCleanup(job);
		switch (rval)
		{
			case -1:    // Error
					// No need to print status messages
					// as the lower levels should be
					// handling that.
				return;
				break;
			case 0:    // Hook not configured
				break;
			case 1:    // Spawned the hook
					// Done for now.  Let the handler call
					// FinishCleanupJob() when the hook
					// exits.
				return;
				break;
		}
	}
#endif

	FinishCleanupJob(job);
}

void
JobRouter::FinishCleanupJob(RoutedJob *job) {
	classad::ClassAdCollection *ad_collection = m_scheduler->GetClassAds();

	if(!job->is_done && job->dest_proc_id.cluster != -1) {
		// Remove (abort) destination job.
		MyString err_desc;
		if(!remove_job(job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,"JobRouter aborted job",m_schedd2_name,m_schedd2_pool,err_desc)) {
			dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.Value());
		}
		else {
			job->dest_proc_id.cluster = -1;
		}
	}

	if(job->is_claimed) {
		MyString error_details;
		bool keep_trying = true;
		if(!yield_job(job->src_ad,m_schedd1_name,m_schedd1_pool,job->is_done,job->src_proc_id.cluster,job->src_proc_id.proc,&error_details,JobRouterName().c_str(),job->is_sandboxed,m_release_on_hold,&keep_trying))
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

		JobRoute *route = GetRouteByName(job->route_name.c_str());
		if(route) {
			if(job->is_success) {
				route->IncrementSuccesses();
			}
			else {
				route->IncrementFailures();
			}
		}

		// Now, we need to leave this job in the list for a while to
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

	classad::ClassAdCollection *ad_collection2 = GetSchedd2ClassAds();
	classad::ClassAd *dest_ad = ad_collection2->GetClassAd(job->dest_key);
	if(!dest_ad) {
		dest_job_synchronized = true;
	}
	else if(dest_ad) {
		std::string manager;
		dest_ad->EvaluateAttrString(JR_ATTR_ROUTED_BY,manager);

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

void
JobRouter::TimerHandler_UpdateCollector() {
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_public_ad);
}

void
JobRouter::InvalidatePublicAd() {
	ClassAd invalidate_ad;
	MyString line;

	invalidate_ad.SetMyTypeName(QUERY_ADTYPE);
	invalidate_ad.SetTargetTypeName("Job_Router");

	line.formatstr("%s == \"%s\"", ATTR_NAME, daemonName.c_str());
	invalidate_ad.AssignExpr(ATTR_REQUIREMENTS, line.Value());
	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad, NULL, false);
}

JobRoute::JobRoute() {
	m_num_jobs = 0;
	m_num_running_jobs = 0;
	m_max_jobs = 0;
	m_max_idle_jobs = 0;
	m_recent_stats_begin_time = time(NULL);
	m_recent_jobs_failed = 0;
	m_recent_jobs_succeeded = 0;
	m_recent_jobs_routed = 0;
	m_failure_rate_threshold = 0;
	m_throttle = 0;
	m_override_routing_entry = -1;
	m_target_universe = CONDOR_UNIVERSE_GRID;
	m_route_requirements = NULL;
}

JobRoute::~JobRoute() {
}
bool JobRoute::AcceptingMoreJobs()
{
	if( m_throttle > 0 && m_throttle <= m_recent_jobs_routed) {
		return false;
	}
	if( m_max_idle_jobs >= 0 && m_max_idle_jobs <= CurrentIdleJobs() ) {
		return false;
	}
	return m_num_jobs < m_max_jobs;
}
std::string
JobRoute::ThrottleDesc() {
	return ThrottleDesc(m_throttle);
}
std::string
JobRoute::ThrottleDesc(double throttle) {
	std::string desc;
	if(throttle <= 0) {
		desc = "none";
	}
	else {
		MyString buf;
		buf.formatstr("%g jobs/sec",throttle/THROTTLE_UPDATE_INTERVAL);
		desc = buf.Value();
	}
	return desc;
}
void
JobRoute::CopyState(JobRoute *r) {
		// Only copy state that can't be recreated in some other way.
		// Would be nice if we didn't have any such state at all,
		// because job router is supposed to be as stateless as possible,
		// but there is currently nowhere else to stash the following state.

	m_recent_jobs_routed = r->m_recent_jobs_routed;
	m_recent_stats_begin_time = r->m_recent_stats_begin_time;
	m_recent_jobs_failed = r->m_recent_jobs_failed;
	m_recent_jobs_succeeded = r->m_recent_jobs_succeeded;
	m_throttle = r->m_throttle;
}

void
JobRoute::AdjustFailureThrottles() {
	time_t now = time(NULL);
	double delta = now - m_recent_stats_begin_time;

	if(delta < THROTTLE_UPDATE_INTERVAL) {
		return;
	}

	double new_throttle = m_throttle;
	double recent_failure_rate = m_recent_jobs_failed*1.0/delta;

	dprintf(D_FULLDEBUG,"JobRouter (route=%s): checking throttle: recent failure rate %g vs. threshold %g; recent successes %d and failures %d\n",Name(),recent_failure_rate,m_failure_rate_threshold,m_recent_jobs_succeeded,m_recent_jobs_failed);

	if( (recent_failure_rate > m_failure_rate_threshold)
	    && m_recent_jobs_failed )
	{
			//Decelerate.  Failure rate is above threshold.
		int recent_non_failures = m_num_jobs + m_recent_jobs_succeeded;
		double failure_ratio = (double)m_recent_jobs_failed/((double)m_recent_jobs_failed + recent_non_failures);

				//Throttle to aim for max_failure_frequency.
		new_throttle = THROTTLE_UPDATE_INTERVAL * m_failure_rate_threshold / failure_ratio;
	}
	else {
        // heuristic for accelerating:
        //   - if all jobs are succeeding, accel by x5
        //   - if half are succeeding and half failing, accel x2.5
        //   - if no failures or successes, accel x2.0
        //   - if all jobs are failing, do not change
        float accel = (3.0*m_recent_jobs_succeeded - 2.0*m_recent_jobs_failed);
        if( accel > 0 ) {
            accel /= (m_recent_jobs_succeeded + m_recent_jobs_failed);
        }
        accel += 2.0;
		if( accel > 1.0 ) {
				//Accelerate.
			new_throttle *= accel;
		}

		if( new_throttle > THROTTLE_UPDATE_INTERVAL * m_failure_rate_threshold * 10000 ) {
				//Things seem to be going fine.  Remove throttle.
			new_throttle = 0;
		}
		if( new_throttle > 0 && new_throttle < 1 && m_recent_jobs_failed == 0) {
				//At least let 1 job run, or we may never get anywhere.
			new_throttle = 1;
		}
	}
	if (fabs(new_throttle - m_throttle) > 0.0001) {
		dprintf(D_ALWAYS,"JobRouter (route=%s): adjusting throttle from %s to %s.\n",
				Name(),
				ThrottleDesc(m_throttle).c_str(),
				ThrottleDesc(new_throttle).c_str());
		m_throttle = new_throttle;
	}
	m_recent_stats_begin_time = now;
	m_recent_jobs_routed = 0;
	m_recent_jobs_failed = 0;
	m_recent_jobs_succeeded = 0;
}

bool
JobRoute::DigestRouteAd(bool allow_empty_requirements) {
	if( !m_route_ad.EvaluateAttrInt( JR_ATTR_MAX_JOBS, m_max_jobs ) ) {
		m_max_jobs = 100;
	}
	if( !m_route_ad.EvaluateAttrInt( JR_ATTR_MAX_IDLE_JOBS, m_max_idle_jobs ) ) {
		m_max_idle_jobs = 50;
	}
	if( !m_route_ad.EvaluateAttrReal( JR_ATTR_FAILURE_RATE_THRESHOLD, m_failure_rate_threshold ) ) {
		m_failure_rate_threshold = 0.03;
	}
	if( !m_route_ad.EvaluateAttrInt( JR_ATTR_TARGET_UNIVERSE, m_target_universe ) ) {
		m_target_universe = CONDOR_UNIVERSE_GRID;
	}
	if( m_target_universe == CONDOR_UNIVERSE_GRID ) {
		if( !m_route_ad.EvaluateAttrString( ATTR_GRID_RESOURCE, m_grid_resource ) ) {
			dprintf(D_ALWAYS, "JobRouter: Missing or invalid %s in job route.\n",ATTR_GRID_RESOURCE);
			return false;
		}	
	}
	if( !m_route_ad.EvaluateAttrString( ATTR_NAME, m_name ) ) {
		// If no name is specified, use the GridResource as the name.
		m_name = m_grid_resource;

		if( m_name.empty() ) {
			dprintf(D_ALWAYS, "JobRouter: Missing or invalid %s in job route.\n",ATTR_NAME);
			return false;
		}
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
	bool override;
	if( !m_route_ad.EvaluateAttrBool( JR_ATTR_OVERRIDE_ROUTING_ENTRY, override ) ) {
		m_override_routing_entry = -1;
	}
	else {
		m_override_routing_entry = override;
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
JobRoute::ParseClassAd(std::string routing_string,int &offset,classad::ClassAd const *router_defaults_ad,bool allow_empty_requirements) {
	classad::ClassAdParser parser;
	classad::ClassAd ad;
	if(!parser.ParseClassAd(routing_string,ad,offset)) {
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
		if(!src_ad->Insert(new_attr,expr,false)) {
			return false;
		}
	}
	// Do attribute deletion
	for( itr = m_route_ad.begin( ); itr != m_route_ad.end( ); itr++ ) {
		char const *attr = itr->first.c_str();
		if(strncmp(attr,"delete_",7)) continue;
		attr = attr + 7;
		dprintf(D_FULLDEBUG,"JobRouter (route=%s): Deleting attribute %s\n",Name(),attr);

		src_ad->Delete(attr);
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
		if(!src_ad->Insert(attr,tree, false)) {
			return false;
		}
	}
	// Do attribute evaluation assignments
	for( itr = m_route_ad.begin( ); itr != m_route_ad.end( ); itr++ ) {
		char const *attr = itr->first.c_str();
		if(strncmp(attr,"eval_set_",9)) continue;
		attr = attr + 9;
		dprintf(D_FULLDEBUG,"JobRouter (route=%s): Setting attribute %s to an evaluated expression\n",Name(),attr);
		if( !( tree = itr->second->Copy( ) ) ) {
			return false;
		}
		if(!src_ad->Insert(attr,tree, false)) {
			return false;
		}
		classad::Value val;
		if(!src_ad->EvaluateAttr(attr,val)) {
			dprintf(D_ALWAYS,"JobRouter (route=%s): Failed to evaluate %s\n",Name(),attr);
			return false;
		}
		classad::ClassAdUnParser unparser;
		std::string valstr;
		unparser.Unparse( valstr, val );

		classad::ExprTree *valtree;
		classad::ClassAdParser parser;
		valtree=parser.ParseExpression(valstr);
		if( !valtree ) {
			dprintf(D_ALWAYS,"JobRouter (route=%s): Failed to parse unparsed evaluation of %s\n",Name(),attr);
			return false;
		}
		if(!src_ad->Insert(attr,valtree)) {
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
RoutedJob::SetSrcJobAd(char const *key,classad::ClassAd *ad,classad::ClassAdCollection * /*ad_collection*/ ) {

	this->src_key = key;
	this->src_proc_id = getProcByString(key);

	//Set the src_ad to include all attributes from cluster plus proc ads.
	if(!src_ad.CopyFromChain(*ad)) {
		dprintf(D_FULLDEBUG,"JobRouter failure (%s): failed to combine cluster and proc ad.\n",JobDesc().c_str());
		return false;
	}
	// From here on, keep track of any changes to src_ad, so we can push
	// changes back to the schedd.
	src_ad.ClearAllDirtyFlags();
	src_ad.EnableDirtyTracking();
	return true;
}

void
RoutedJob::SetDestJobAd(classad::ClassAd const *ad) {
	// We do not want to just do dest_ad = *ad, among other reasons,
	// because this copies the pointer to the chained parent, which
	// could get deleted before we are done with this ad.

	ASSERT(dest_ad.CopyFromChain(*ad));
	saw_dest_job = true;
}
