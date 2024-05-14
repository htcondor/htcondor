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
#include "my_username.h"
#include "condor_uid.h"

#include "condor_config.h"
#include "VanillaToGrid.h"
#include "submit_job.h"
#include "util_lib_proto.h"
#include "my_popen.h"
#include "file_lock.h"
#include "user_job_policy.h"
#include "get_daemon_name.h"
#include "filename_tools.h"
#include "condor_holdcodes.h"
#include "condor_auth_passwd.h"
#include "directory_util.h"
#include "truncate.h"


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
const char JR_ATTR_SEND_IDTOKENS[] = "SendIDTokens";
const char JR_ATTR_OVERRIDE_ROUTING_ENTRY[] = "OverrideRoutingEntry";
const char JR_ATTR_TARGET_UNIVERSE[] = "TargetUniverse";
const char JR_ATTR_EDIT_JOB_IN_PLACE[] = "EditJobInPlace";

const int THROTTLE_UPDATE_INTERVAL = 600;

JobRouter::JobRouter(unsigned int as_tool)
	: m_jobs(hashFunction)
	, m_schedd2_name(NULL)
	, m_schedd2_pool(NULL)
	, m_schedd1_name(NULL)
	, m_schedd1_pool(NULL)
	, m_round_robin_selection(true)
	, m_operate_as_tool(as_tool)
{
	m_scheduler = NULL;
	m_scheduler2 = NULL;
	m_release_on_hold = true;
	m_job_router_polling_timer = -1;
	m_periodic_timer_id = -1;
	m_job_router_polling_period = 10;
	m_enable_job_routing = true;

	m_job_router_idtoken_refresh = 0;
	m_job_router_idtoken_refresh_timer_id = -1;

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

	m_route_order.clear();
	DeallocateRoutingTable(m_routes);

	if(m_router_lock) {
		IGNORE_RETURN unlink(m_router_lock_fname.c_str());
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
	if ( ! m_operate_as_tool) { InvalidatePublicAd(); }

	delete m_scheduler;
	m_scheduler = NULL;
	if( m_scheduler2 ) {
		delete m_scheduler2;
		m_scheduler2 = NULL;
	}

	clear_pre_and_post_xfms();
}

void JobRouter::clear_pre_and_post_xfms()
{
	for (MacroStreamXFormSource *xfm : m_pre_route_xfms) {
		delete xfm;
	}
	m_pre_route_xfms.clear();

	for (MacroStreamXFormSource *xfm : m_post_route_xfms) {
		delete xfm;
	}
	m_post_route_xfms.clear();
}

void
JobRouter::init() {
#if HAVE_JOB_HOOKS
	m_hook_mgr = new JobRouterHookMgr;
	m_hook_mgr->initialize();
#endif
	config();
	if ( ! m_operate_as_tool) { GetInstanceLock(); }
}

void
JobRouter::GetInstanceLock() {
	std::string lock_fullname;
	std::string lock_basename;

	// We may be an ordinary user, so cannot lock in $(LOCK)
	param(lock_fullname,"JOB_ROUTER_LOCK");
	ASSERT( !lock_fullname.empty() );
	canonicalize_dir_delimiters(lock_fullname);

	m_router_lock_fd = safe_open_wrapper_follow(lock_fullname.c_str(),O_CREAT|O_APPEND|O_WRONLY,0600);
	if(m_router_lock_fd == -1) {
		EXCEPT("Failed to open lock file %s: %s",lock_fullname.c_str(),strerror(errno));
	}
	FileLock *lock = new FileLock(m_router_lock_fd, NULL,lock_fullname.c_str());
	m_router_lock = lock;
	m_router_lock_fname = lock_fullname;

	lock->setBlocking(FALSE);
	if(!lock->obtain(WRITE_LOCK)) {
		EXCEPT("Failed to get lock on %s.",lock_fullname.c_str());
	}
}


// load the old style ROUTER_DEFAULTS ad
static bool initRouterDefaultsAd(classad::ClassAd & router_defaults_ad)
{
	bool valid_defaults = true;
	bool merge_defaults = param_boolean("MERGE_JOB_ROUTER_DEFAULT_ADS", false);

	bool using_defaults = param_defined("JOB_ROUTER_DEFAULTS");
	if (using_defaults) {
		dprintf(D_ALWAYS, "JobRouter WARNING: JOB_ROUTER_DEFAULTS is deprecated and will be removed for V24 of HTCondor. New configuration\n");
		dprintf(D_ALWAYS, "                   syntax for the job router is defined using JOB_ROUTER_ROUTE_NAMES and JOB_ROUTER_ROUTE_<name>.\n");
		dprintf(D_ALWAYS, "             Note: The removal will occur during the lifetime of the HTCondor V23 feature series\n");
	}

	bool use_entries = param_boolean("JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES", false);
	if ( ! use_entries && using_defaults) {
		dprintf(D_ALWAYS, "JobRouter WARNING: JOB_ROUTER_DEFAULTS is defined, but will be ignored because JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES is false\n");
		return true;
	}

	auto_free_ptr router_defaults(param("JOB_ROUTER_DEFAULTS"));
	if (router_defaults) {
		char * p = router_defaults.ptr();
		int length = (int)strlen(p);
		while (isspace(*p)) ++p;
		// if the param doesn't start with [, then wrap it in [] before parsing, so that the parser knows to expect new classad syntax.
		if (*p != '[') {
			char * tmp = (char *)malloc(length + 4);
			*tmp = '[';
			strcpy(tmp + 1, p);
			strcat(tmp + length, "]");
			length = (int)strlen(tmp);
			router_defaults.set(tmp);
			merge_defaults = false;
		}
		classad::ClassAdParser parser;
		int offset = 0;
		if ( ! parser.ParseClassAd(router_defaults, router_defaults_ad, offset)) {
			dprintf(D_ALWAYS|D_ERROR,"JobRouter CONFIGURATION ERROR: Disabling job routing, failed to parse at offset %d in JOB_ROUTER_DEFAULTS classad:\n%s\n",
				offset, router_defaults.ptr());
			valid_defaults = false;
		} else if (merge_defaults && (offset < length)) {
			// whoh! we appear to have received multiple classads as a hacky way to append to the defaults ad
			// so go ahead and parse the remaining ads and merge them into the defaults ad.
			const char * ads = router_defaults;
			do {
				// skip trailing whitespace and ] and look for an open [
				bool parse_err = false;
				for ( ; offset < length; ++offset) {
					int ch = ads[offset];
					if (ch == '[') break;
					if ( ! isspace(ads[offset]) && ch != ']') {
						parse_err = true;
						break;
					}
					// TODO: skip comments?
				}

				if (offset < length && ! parse_err) {
					classad::ClassAd other_ad;
					if ( ! parser.ParseClassAd(ads, other_ad, offset)) {
						parse_err = true;
					} else {
						router_defaults_ad.Update(other_ad);
					}
				}

				if (parse_err) {
					valid_defaults = false;
					dprintf(D_ALWAYS|D_ERROR,
						"JobRouter CONFIGURATION ERROR: Disabling job routing, failed to parse at offset %d in JOB_ROUTER_DEFAULTS ad : \n%s\n",
						offset, ads + offset);
					break;
				}
			} while (offset < length);
		}
	}
	return valid_defaults;
}

void
JobRouter::config( int /* timerID */ ) {
	bool allow_empty_requirements = false;
	m_enable_job_routing = true;

	// NOTE the Scheduler() class and the underlying JobLogMirror class does not handle
	// a reconfig that changes what file that class is following, so we will param for
	// the filename only once.  It is a restart knob, not a reconfig knob
	// the param calls happen inside the constructors of the Scheduler() class.

	if (!m_scheduler) {
		// we pass 0 here so that it will param for JOB_QUEUE_LOG if there is
		// no configured JOB_ROUTER_SCHEDD1_* knobs to define the log
		m_scheduler = new Scheduler(0);
		if ( ! m_scheduler->following()) {
			// if there was no configured file to follow, we abort
			EXCEPT("No job_queue.log to follow for source schedd (SCHEDD1)");
		}
		m_scheduler->init();

		m_scheduler2 = new Scheduler(2);
		if (m_scheduler2->following()) {
			m_scheduler2->init();
		} else {
			// SCHEDD2 is not configured, so just delete the class
			delete m_scheduler2;
			m_scheduler2 = nullptr;
		}
	}

	dprintf(D_ALWAYS, "Following source schedd (SCHEDD1): %s\n", m_scheduler->following());
	if (m_scheduler2) {
		dprintf(D_ALWAYS, "Watching destination schedd (SCHEDD2): %s\n", m_scheduler2->following());
	} else {
		dprintf(D_ALWAYS, "Destination schedd (SCHEDD2) not configured, using source schedd as destintation also\n");
	}

	m_scheduler->config();
	m_scheduler->stop();
	if( m_scheduler2 ) {
		m_scheduler2->config();
		m_scheduler2->stop();
	}

#if HAVE_JOB_HOOKS
	m_hook_mgr->reconfig();
#endif

	m_job_router_idtoken_refresh = param_integer("JOB_ROUTER_IDTOKEN_REFRESH",0);
	if ( ! m_operate_as_tool) {
		bool initial_refresh = true;
		if (m_job_router_idtoken_refresh_timer_id >= 0) {
			daemonCore->Cancel_Timer(m_job_router_idtoken_refresh_timer_id);
			m_job_router_idtoken_refresh_timer_id = -1;
			initial_refresh = false;
		}
		if (m_job_router_idtoken_refresh > 0) {
			m_job_router_idtoken_refresh_timer_id =
				daemonCore->Register_Timer(
					initial_refresh ? 0 : m_job_router_idtoken_refresh,
					m_job_router_idtoken_refresh,
					(TimerHandlercpp)&JobRouter::refreshIDTokens,
					"JobRouter::refreshIDTokens", this);
		}
	}


	m_job_router_entries_refresh = param_integer("JOB_ROUTER_ENTRIES_REFRESH",0);
	if ( ! m_operate_as_tool ) {
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


	RoutingTable *new_routes = new RoutingTable();

	// for backward compatibility with 8.8.6, build a name table in the order that 8.8.6 would use.
	// If JOB_ROUTER_ROUTE_NAMES is not configured, this will be the order that routes are matched
	HashTable<std::string,int> hash_order(hashFunction);

	classad::ClassAd router_defaults_ad;
	m_enable_job_routing = initRouterDefaultsAd(router_defaults_ad);
	if ( ! m_enable_job_routing) {
		delete new_routes;
		return;
	}

	auto_free_ptr route_names(param("JOB_ROUTER_ROUTE_NAMES"));
	auto_free_ptr routing_file(param("JOB_ROUTER_ENTRIES_FILE"));
	auto_free_ptr routing_cmd(param("JOB_ROUTER_ENTRIES_CMD"));
	auto_free_ptr routing_entries(param("JOB_ROUTER_ENTRIES"));

	std::string used_deprecated = "";
	if (routing_file) used_deprecated += "JOB_ROUTER_ENTRIES_FILE";
	if (routing_cmd) {
		if (! used_deprecated.empty()) used_deprecated += ", ";
		used_deprecated += "JOB_ROUTER_ENTRIES_CMD";
	}
	if (routing_entries) {
		if (! used_deprecated.empty()) used_deprecated += ", ";
		used_deprecated += "JOB_ROUTER_ENTRIES";
	}

	if (! used_deprecated.empty()) {
		dprintf(D_ALWAYS, "JobRouter WARNING: %s are deprecated and will be removed for V24 of HTCondor. New configuration\n", used_deprecated.c_str());
		dprintf(D_ALWAYS, "                   syntax for the job router is defined using JOB_ROUTER_ROUTE_NAMES and JOB_ROUTER_ROUTE_<name>.\n");
		dprintf(D_ALWAYS, "             Note: The removal will occur during the lifetime of the HTCondor V23 feature series.\n");
	}

	bool use_entries = param_boolean("JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES", false);
	if ( ! use_entries && (routing_file || routing_cmd || routing_entries)) {
		dprintf(D_ALWAYS,
			"JobRouter WARNING: one or more of JOB_ROUTER_ENTRIES, JOB_ROUTER_ENTRIES_FILE or JOB_ROUTER_ENTRIES_CMD are defined "
			"but will be ignored because JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES is false\r\n");
		routing_file.set(NULL);
		routing_cmd.set(NULL);
		routing_entries.set(NULL);
	}

	bool routing_entries_defined = route_names || routing_file || routing_cmd || routing_entries;
	if(!routing_entries_defined) {
		dprintf(D_ALWAYS,
			"JobRouter WARNING: none of JOB_ROUTER_ROUTE_NAMES, JOB_ROUTER_ENTRIES, JOB_ROUTER_ENTRIES_FILE,"
			" or JOB_ROUTER_ENTRIES_CMD are defined, so job routing will not take place.\n");
		m_enable_job_routing = false;
	}

	// the order in which we parse these knobs matters, because name collisions allow for a route to be overridden.

	if (routing_cmd) {
		ArgList args;
		std::string error_msg;
		if ( ! args.AppendArgsV1RawOrV2Quoted(routing_cmd, error_msg)) {
			EXCEPT("Invalid value specified for JOB_ROUTER_ENTRIES_CMD: %s", error_msg.c_str());
		}

			// I have tested with want_stderr 0 and 1, but I have not observed
			// any difference.  I still get stderr mixed into the stdout.
			// Although this is annoying, it is better than simply throwing
			// away stderr altogether.  What generally happens is that the
			// stderr produces a parse error, and we skip to the next
			// entry.
		FILE *fp = my_popen(args, "r", MY_POPEN_OPT_WANT_STDERR);

		if( !fp ) {
			EXCEPT("Failed to run command '%s' specified for JOB_ROUTER_ENTRIES_CMD.", routing_cmd.ptr());
		}
		std::string routing_file_str;
		char buf[200];
		size_t n;
		while( (n=fread(buf,1,sizeof(buf)-1,fp)) > 0 ) {
			buf[n] = '\0';
			routing_file_str += buf;
		}
		n = my_pclose( fp );
		if( n != 0 ) {
			EXCEPT("Command '%s' specified for JOB_ROUTER_ENTRIES_CMD returned non-zero status %d",
				   routing_cmd.ptr(), (int)n);
		}

		ParseRoutingEntries(
			routing_file_str,
			"_CMD",
			router_defaults_ad,
			allow_empty_requirements,
			hash_order,
			new_routes);
	}

	if (routing_file) {
		FILE *fp = safe_fopen_wrapper_follow(routing_file,"r");
		if( !fp ) {
			EXCEPT("Failed to open '%s' file specified for JOB_ROUTER_ENTRIES_FILE.", routing_file.ptr());
		}
		std::string routing_file_str;
		char buf[200];
		size_t n;
		while( (n=fread(buf,1,sizeof(buf)-1,fp)) > 0 ) {
			buf[n] = '\0';
			routing_file_str += buf;
		}
		fclose( fp );

		ParseRoutingEntries(
			routing_file_str,
			"_FILE",
			router_defaults_ad,
			allow_empty_requirements,
			hash_order,
			new_routes);
	}

	if (routing_entries) {
		ParseRoutingEntries(
			routing_entries.ptr(),
			"",
			router_defaults_ad,
			allow_empty_requirements,
			hash_order,
			new_routes);
	}

	clear_pre_and_post_xfms();

	classad::References xfm_names; // a set of all pre and post route transform names (used for error checking)

	// load the pre-route transforms
	std::vector<std::string> xfm_tags;
	std::string xfm_param;
	param_and_insert_unique_items("JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES", xfm_tags);
	for (auto& tag: xfm_tags) {
		xfm_names.insert(tag); 
		xfm_param = "JOB_ROUTER_TRANSFORM_"; xfm_param += tag;
		const char * xfm_text = param_unexpanded(xfm_param.c_str());
		if (xfm_text) {
			std::string errmsg;
			int offset = 0;
			MacroStreamXFormSource *xfm = new MacroStreamXFormSource(tag.c_str());
			if (xfm->open(xfm_text, offset, errmsg) < 0) {
				dprintf( D_ALWAYS, "%s load error: %s\n", xfm_param.c_str(), errmsg.c_str());
				m_enable_job_routing = false;
				delete xfm;
			} else {
				m_pre_route_xfms.push_back(xfm);
			}
		} else {
			dprintf(D_ALWAYS, "ERROR: %s not found\n", xfm_param.c_str());
			m_enable_job_routing = false;
		}
	}

	// load the post route transforms
	xfm_tags.clear();
	param_and_insert_unique_items("JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES", xfm_tags);
	for (auto& tag: xfm_tags) {
		xfm_names.insert(tag); 
		xfm_param = "JOB_ROUTER_TRANSFORM_"; xfm_param += tag;
		const char * xfm_text = param_unexpanded(xfm_param.c_str());
		if (xfm_text) {
			std::string errmsg;
			int offset = 0;
			MacroStreamXFormSource *xfm = new MacroStreamXFormSource(tag.c_str());
			if (xfm->open(xfm_text, offset, errmsg) < 0) {
				dprintf( D_ALWAYS, "%s load error: %s\n", xfm_param.c_str(), errmsg.c_str());
				m_enable_job_routing = false;
				delete xfm;
			} else {
				m_post_route_xfms.push_back(xfm);
			}
		} else {
			dprintf(D_ALWAYS, "ERROR: %s not found\n", xfm_param.c_str());
			m_enable_job_routing = false;
		}
	}

	// last we look for params of the form JOB_ROUTER_ROUTE_<name> where <name> is one of the names listed in JOB_ROUTER_ROUTE_NAMES
	// This the the way submit transforms works in the schedd
	StringTokenIterator names(route_names);
	std::string knob;
	for (const char * name = names.first(); name != NULL; name = names.next()) {
		if (YourStringNoCase(name) == "NAMES") continue;
		knob = "JOB_ROUTER_ROUTE_"; knob += name;
		const char * route_str = param_unexpanded(knob.c_str());
		if ( ! route_str) continue; // this is probably in JOB_ROUTER_ENTRIES or such

		while (isspace(*route_str)) ++route_str;
		if ( ! *route_str) {
			dprintf( D_ALWAYS, "JOB_ROUTER_ROUTE_%s definition is empty, ignoring.\n", name );
			continue;
		}

		ParseRoute(route_str, name, allow_empty_requirements, new_routes);
	}

	if(!m_enable_job_routing) {
		delete new_routes;
		return;
	}

	SetRoutingTable(new_routes, hash_order);

		// Whether to release the source job if the routed job
		// goes on hold
	m_release_on_hold = param_boolean("JOB_ROUTER_RELEASE_ON_HOLD", true);

	m_round_robin_selection = param_boolean("JOB_ROUTER_ROUND_ROBIN_SELECTION", false);

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
	if ( ! m_operate_as_tool) {
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
	} // ! m_operate_as_tool

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

	if ( ! m_operate_as_tool) {
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
	} // ! m_operate_as_tool

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

void
JobRouter::refreshIDTokens( int /* timerID */ ) {
	std::vector<std::string> items;
	param_and_insert_unique_items("JOB_ROUTER_CREATE_IDTOKEN_NAMES", items);

	if (IsDebugLevel(D_ALWAYS)) {
		std::string tokenids = join(items, ",");
		dprintf(D_ALWAYS, "JobRouter::refreshIDTokens - %s\n", tokenids.c_str());
	}

	// Build a map of existing tokens that we will remove items from when we refresh these tokens
	std::map<std::string, std::string> delete_tokens;
	for (auto it : m_idtokens) { delete_tokens[it.first] = it.second; }

	// Create or overwrite token files
	for (auto& item: items) {
		if (strcasecmp(item.c_str(), "NAMES") == 0) {
			continue;
		}
		std::string knob("JOB_ROUTER_CREATE_IDTOKEN_"); knob += item;
		auto_free_ptr props = param(knob.c_str());
		if (props && CreateIDTokenFile(item.c_str(), props)) {
			// no need to delete this one because we are going to overwrite it
			delete_tokens.erase(item);
		}
	}

	for (auto it : delete_tokens) {
		RemoveIDTokenFile(it.first);
	}
}

bool JobRouter::CreateIDTokenFile(const char * name, const char * props)
{
	// props can be a new classad that has attributes to define the scope of the token
	ClassAd ad;
	time_t lifetime = -1;
	std::string subject, key, token, fname, dir, tmp, owner, domain;
	std::vector<std::string> authz_list;
	if (initAdFromString(props, ad) && ad.size()) {
		ad.LookupString("sub", subject);
		ad.LookupString("kid", key);
		ad.LookupInteger("lifetime", lifetime);
		if (ad.LookupString("scope", tmp)) {
			StringTokenIterator it(tmp);
			for (auto str = it.first(); str; str = it.next()) {
				authz_list.push_back(str);
			}
		}
		ad.LookupString("dir", dir);
		ad.LookupString("filename", fname);
		ad.LookupString(ATTR_OWNER, owner);
		ad.LookupString(ATTR_NT_DOMAIN, domain);
	} else {
		// initAdFromString will print the lines it can't parse, we just need to report that
		// the we will not be creating the token at all as a result of the config failure
		dprintf(D_ALWAYS, "Ignoring invalid %s IDTOKEN config : %s\n", name, props);
		return false;
	}

	if (subject.empty()) { subject = name; }
	if (fname.empty()) { fname = name; }
	if (dir.empty()) { param(dir, "SEC_TOKEN_DIRECTORY"); }

	CondorError err;
	if (!Condor_Auth_Passwd::generate_token(subject, key, authz_list, lifetime, token, 0, &err)) {
		dprintf(D_ALWAYS, "failed to create token %s : %s\n", name, err.getFullText(false).c_str());
		return false;
	}
	token += "\n"; // technically token files *can* have multiple tokens separated by newline

	dircat(dir.c_str(), fname.c_str(), tmp);

	bool setpriv = !owner.empty();
	priv_state old_priv = PRIV_UNKNOWN;
	if (setpriv) {
	#ifdef WIN32
		// Windows always needs a domain name for init_user_ids
		if (domain.empty()) { param(domain, "UID_DOMAIN"); }
	#endif
		if (!init_user_ids(owner.c_str(), domain.c_str()))
		{
			dprintf(D_ALWAYS, "Failed in init_user_ids(%s,%s) for CREATE_IDTOKEN_%s\n",
				owner.c_str(), domain.c_str(), name);
			return false;
		}
		old_priv = set_priv(PRIV_USER);
	}

	bool success = false;
	int fd = safe_create_keep_if_exists(tmp.c_str(), O_CREAT | O_WRONLY | _O_BINARY, 0600);
	if (fd >= 0) {
		(void)full_write(fd, token.c_str(), token.size());
		int r = ftruncate(fd, token.size()); // incase the data in the file is less than it was before.
		if (r < 0) {
			dprintf(D_ALWAYS, "Cannot ftruncate %s %d: %s\n", tmp.c_str(), errno, strerror(errno)); 
		}
		close(fd);

		// save the filepath
		m_idtokens[name] = tmp;
		success = true;
	} else {
		dprintf(D_ALWAYS, "Cannot write token to %s: %s (errno=%d)\n", tmp.c_str(), strerror(errno), errno);
	}

	if (setpriv) {
		set_priv(old_priv);
		uninit_user_ids();
	}

	return success;
}

bool JobRouter::RemoveIDTokenFile(const std::string & name)
{
	auto it = m_idtokens.find(name);
	if (it != m_idtokens.end()) {
		dprintf(D_ALWAYS, "deleting %s IDTOKEN file : %s\n", name.c_str(), it->second.c_str());
		unlink(it->second.c_str());
		m_idtokens.erase(it);
	}

	return true;
}



void JobRouter::dump_routes(FILE* hf) // dump the routing information to the given file.
{
	// build a set of route names, we will remove names from this list as we print them
	classad::References remaining_names;
	for (auto & m_route : *m_routes) {
		JobRoute * route = m_route.second;
		remaining_names.insert(route->Name());
	}

	// dump pre route transforms
	std::string buf;
	if ( ! m_pre_route_xfms.empty()) {
		for (MacroStreamXFormSource *xfm : m_pre_route_xfms) {
			fprintf(hf, "Pre-Route Transform : %s\n", xfm->getName());
			buf.clear();
			xfm->getFormattedText(buf, "\t", true);
			fprintf(hf, "%s", buf.c_str());
			fprintf(hf, "\n\n");
		}
	}

	// dump post route transforms
	if ( ! m_post_route_xfms.empty()) {
		for (MacroStreamXFormSource *xfm : m_post_route_xfms) {
			fprintf(hf, "Post-Route Transform : %s\n", xfm->getName());
			buf.clear();
			xfm->getFormattedText(buf, "\t", true);
			fprintf(hf, "%s", buf.c_str());
			fprintf(hf, "\n\n");
		}
	}


	// print the enabled routes in the order that they will be used
	int ixRoute = 1;
	for (auto & it : m_route_order) {
		JobRoute *route = safe_lookup_route(it);
		if ( ! route) continue;
		remaining_names.erase(route->Name());
		/*
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
		*/
		fprintf(hf, "Route %d\n", ixRoute);
		fprintf(hf, "Name         : \"%s\"\n", route->Name());
		fprintf(hf, "Source       : %s\n", route->Source());
		fprintf(hf, "Universe     : %d\n", route->TargetUniverse());
		//fprintf(hf, "RoutedJobs   : %d\n", route->CurrentRoutedJobs());
		fprintf(hf, "MaxJobs      : %d\n", route->MaxJobs());
		fprintf(hf, "MaxIdleJobs  : %d\n", route->MaxIdleJobs());
		fprintf(hf, "GridResource : %s\n", route->GridResource());
		fprintf(hf, "Requirements : %s\n", route->RouteRequirementsString());

		fprintf(hf, "Route        : %s\n", route->UsesPreRouteTransform() ? "uses Pre/Post-Route Transforms" : "");
		std::string route_ad_string;
		if (route->RouteStringPretty(route_ad_string)) {
			fprintf(hf, "%s", route_ad_string.c_str());
		}
		fprintf(hf, "\n");

		fprintf(hf, "\n");
		++ixRoute;
	}

	// remaining nams now has the list of routes in the route table that are disabled
	// bacause they aren't in the route_order list.
	if ( ! remaining_names.empty()) {
		fprintf(hf, "Disabled routes:\n");
		for (auto it = remaining_names.begin(); it != remaining_names.end(); ++it) {
			JobRoute * route = safe_lookup_route(*it);
			if ( ! route) continue;
			fprintf(hf, "Name         : \"%s\"\n", route->Name());
			fprintf(hf, "Source       : %s\n", route->Source());
			fprintf(hf, "\n");
		}
	}
}


void
JobRouter::set_schedds(Scheduler* schedd, Scheduler* schedd2)
{
	ASSERT(m_operate_as_tool);
	m_scheduler = schedd;
	m_scheduler2 = schedd2;
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
	ASSERT( ! m_operate_as_tool);
	ASSERT (m_job_router_name.size() > 0);

	char *valid_name = build_valid_daemon_name(m_job_router_name.c_str());
	ASSERT( valid_name );
	daemonName = valid_name;
	free(valid_name);

	m_public_ad = ClassAd();

	SetMyTypeName(m_public_ad, JOB_ROUTER_ADTYPE);

	m_public_ad.Assign(ATTR_NAME,daemonName);

	daemonCore->publish(&m_public_ad);
}

void
JobRouter::EvalAllSrcJobPeriodicExprs( int /* timerID */ )
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
		
		// Forward any update of TimerRemove from the source schedd's
		// job ad to our other copy of the ad.
		// This brute-force update assumes that if  TimerRemove initially
		// evaluates to an integer, it will continue to do so throughout
		// the job's life.
		// Do the same for x509UserProxyExpiration, which is used in some
		// users' job policy expressions.
		int timer_remove = -1;
		if (orig_ad && orig_ad->EvaluateAttrInt(ATTR_TIMER_REMOVE_CHECK, timer_remove)) {
			job->src_ad.InsertAttr(ATTR_TIMER_REMOVE_CHECK, timer_remove);
			job->src_ad.MarkAttributeClean(ATTR_TIMER_REMOVE_CHECK);
		}
		if (orig_ad && orig_ad->EvaluateAttrInt(ATTR_X509_USER_PROXY_EXPIRATION, timer_remove)) {
			job->src_ad.InsertAttr(ATTR_X509_USER_PROXY_EXPIRATION, timer_remove);
			job->src_ad.MarkAttributeClean(ATTR_X509_USER_PROXY_EXPIRATION);
		}
		if (false == EvalSrcJobPeriodicExpr(job))
		{
			dprintf(D_ALWAYS, "JobRouter failure (%s): Unable to "
					"evaluate job's periodic policy "
					"expressions.\n", job->JobDesc().c_str());

			if (!orig_ad) {
				dprintf(D_ALWAYS, "JobRouter failure (%s): failed to reset src job attributes, because ad not found in collection.\n",
						job->JobDesc().c_str());
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
	std::string reason;
	int reason_code;
	int reason_subcode;
	bool ret_val = false;

	converted_ad = job->src_ad;
	user_policy.Init();

	action = user_policy.AnalyzePolicy(converted_ad, PERIODIC_ONLY);

	user_policy.FiringReason(reason,reason_code,reason_subcode);
	if ( reason == "" ) {
		reason = "Unknown user policy expression";
	}

	switch(action)
	{
		case UNDEFINED_EVAL:
			ret_val = SetJobHeld(job->src_ad, reason.c_str(), reason_code, reason_subcode);
			break;
		case STAYS_IN_QUEUE:
		case VACATE_FROM_RUNNING:
			// do nothing
			ret_val = true;
			break;
		case REMOVE_FROM_QUEUE:
			ret_val = SetJobRemoved(job->src_ad, reason.c_str());
			break;
		case HOLD_IN_QUEUE:
			ret_val = SetJobHeld(job->src_ad, reason.c_str(), reason_code, reason_subcode);
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
	if (HELD != status && REMOVED != status && COMPLETED != status)
	{
		ad.InsertAttr(ATTR_JOB_STATUS, HELD);
		ad.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, time(nullptr));
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
		ad.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, time(nullptr));
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


void JobRouter::ParseRoute(
	const char * route_text,
	const char * name,
	bool allow_empty_requirements,
	//HashTable<std::string,int> & hash_order,
	RoutingTable * new_routes)
{
	if ( ! route_text)
		return;
	while (isspace(*route_text)) ++route_text;
	if ( ! *route_text)
		return;

	JobRoute * route = new JobRoute(name);

	const char * text = route_text;
	int offset = 0;
	auto_free_ptr ad_text; // in case we need to macro expand
	std::string errmsg;

	// If classad syntax, we have to do macro expansion on the route text
	// some configs put c-style comments before the classad, so treat /* as signalling a classad config
	if (*text == '[' || (text[0] == '/' && text[1] == '*')) {
		// first we need to do macro expansion
		ad_text.set(expand_param(route_text));
		text = ad_text.ptr();
	}

	// parse the route
	if ( ! route->ParseNext(text, offset, NULL, allow_empty_requirements, name, errmsg)) {
		if ( ! errmsg.empty()) {
			dprintf(D_ALWAYS, "JobRouter CONFIGURATION ERROR: %s\n", errmsg.c_str());
			dprintf(D_ALWAYS, "Ignoring route entry in JOB_ROUTER_ROUTE_%s\n", name);
		}
		delete route; route = NULL;
		return;
	}
	ASSERT(MATCH == strcasecmp(route->Name(), name));

	const char * route_name = route->Name();
	//int hash_index = hash_order.getNumElements(); // in case we are not replacing
	auto found = new_routes->find(route_name);
	if (found != new_routes->end()) {
		JobRoute* existing_route = found->second;
		// Two routes have the same name. This can only happen if the first route was a pre-8.9.4 route
		// so one we just parsed will always override it.
		dprintf(D_ALWAYS,"JobRouter CONFIGURATION WARNING: while parsing JOB_ROUTER_ROUTE_%s found and overwrite a route from %s with the same name\n",
				name, existing_route->Source());

		// preserve the original hash index if we replace a route
		//hash_order.lookup(existing_route->Name(), hash_index);
		//hash_order.remove(existing_route->Name());
		new_routes->erase(route_name);
		delete existing_route;
	}

	(*new_routes)[route->Name()] = route;

	// also insert the name into hash_order hashtable so we know what that order would be
	//hash_order.insert(route->Name(), hash_index);
}


void
JobRouter::ParseRoutingEntries(
	std::string const &routing_string,
	char const *param_tag,
	classad::ClassAd const &router_defaults_ad,
	bool allow_empty_requirements,
	HashTable<std::string,int> & hash_order,
	RoutingTable *new_routes )
{

		// Now parse a list of routing entries.  The expected syntax is
		// a list of ClassAds, optionally delimited by commas and or
		// whitespace.

	dprintf(D_FULLDEBUG,"Parsing JOB_ROUTER_ENTRIES%s=%s\n",param_tag,routing_string.c_str());

	// prepare to make a source label indicating the param and index
	const char *  source_fmt = "entries:%d";
	if (MATCH == strcmp(param_tag, "_CMD")) { source_fmt = "cmd:%d"; }
	else if (MATCH == strcmp(param_tag, "_FILE")) { source_fmt = "file:%d"; }

	int source_index = 0;
	int offset = 0;
	while(1) {
		// skip leading whitespace
		while (offset < (int)routing_string.size() && isspace(routing_string[offset])) ++offset;
		if (offset >= (int)routing_string.size()) break;

		std::string source_name;
		formatstr(source_name, source_fmt, source_index++);
		JobRoute * route = new JobRoute(source_name.c_str());
		JobRoute *existing_route = NULL;
		int this_offset = offset; //save offset before eating an ad.
		bool ignore_route = false;

		std::string errmsg;
		if ( ! route->ParseNext(routing_string,offset,&router_defaults_ad,allow_empty_requirements, source_name.c_str(), errmsg)) {
			if ( ! errmsg.empty()) {
				dprintf(D_ALWAYS, "JobRouter CONFIGURATION ERROR: %s\n", errmsg.c_str());
				// HTCONDOR-864, make sure we don't use an out-of-range offset to substr or we will crash
				std::string err_here;
				if (this_offset >= (int)routing_string.size()) {
					err_here = "offset ";
					err_here += std::to_string(this_offset);
				} else {
					int len = (int)routing_string.size() - this_offset;
					if (len > 79) len = 79;
					err_here = routing_string.substr(this_offset, len);
				}
				dprintf(D_ALWAYS, "Ignoring the malformed route entry in JOB_ROUTER_ENTRIES%s, at offset %d starting here:\n%s\n",
					param_tag, this_offset, err_here.c_str());
			}
			delete route; route = NULL;
			// prevent an infinite loop of we don't advance the offset
			if (offset <= this_offset) break;
			continue;
		}

		const char * route_name = route->Name();
		int hash_index = hash_order.getNumElements(); // in case we are not replacing
		auto found = new_routes->find(route_name);
		if (found != new_routes->end()) {
			existing_route = found->second;
			// Two routes have the same name, we will replace the first instance unless the second one has 
			// OverrideRoutingEntry=false

			int override_entry = route->OverrideRoutingEntry();
			if (override_entry < 0) {
				dprintf(D_ALWAYS,"JobRouter CONFIGURATION WARNING: while parsing JOB_ROUTER_ENTRIES%s two route entries have the same name '%s' so the second one will override the first one; if you have not already explicitly given these routes a name with name=\"blah\", you may want to give them different names.  If you just want to suppress this warning, then define OverrideRoutingEntry=True/False in the second routing entry.\n",
					param_tag,route_name);
				override_entry = 1;
			}
			if (override_entry > 0) {  // OverrideRoutingEntry=true
				// preserve the original hash index if we replace a route
				hash_order.lookup(existing_route->Name(), hash_index);
				hash_order.remove(existing_route->Name());
				new_routes->erase(route_name);
				delete existing_route;
			}
			if (override_entry == 0) { // OverrideRoutingEntry=false
				ignore_route = true;
			}
		}

		if ( ignore_route) {
			delete route; route = NULL;
		} else {
			(*new_routes)[route->Name()] = route;
			// also insert the name into hash_order hashtable so we know what that order would be
			hash_order.insert(route->Name(), hash_index);
		}
	}
}

JobRoute *
JobRouter::GetRouteByName(char const *name) {
	auto found = m_routes->find(name);
	if (found != m_routes->end()) {
		return found->second;
	}
	return NULL;
}

void JobRouter::SetRoutingTable(RoutingTable *new_routes, HashTable<std::string,int> & hash_order)
{
	// Now we have a set of new routes in new_routes.
	// Replace our existing routing table with these.

	JobRoute *route=NULL;

	// look for routes that have been dropped
	for (auto it = m_routes->begin(); it != m_routes->end(); ++it) {
		route = it->second;
		if (0 == new_routes->count(route->Name())) {
			dprintf(D_ALWAYS,"JobRouter Note: dropping route '%s'\n",route->RouteDescription().c_str());
		}
	}

	// look for routes that have been added
	for (auto it = new_routes->begin(); it != new_routes->end(); ++it) {
		route = it->second;
		auto found = m_routes->find(route->Name());
		if (found == m_routes->end()) {
			dprintf(D_ALWAYS,"JobRouter Note: adding new route '%s'\n",route->RouteDescription().c_str());
		}
		else {
				// preserve state from the old route entry
			route->CopyState(found->second);
		}
	}
	m_route_order.clear();
	DeallocateRoutingTable(m_routes);
	m_routes = new_routes;

	// build the m_route_order list, containing the names of the routes in the order in which they whould be matched
	// This is controlled by a knob 
	auto_free_ptr order(param("JOB_ROUTER_ROUTE_NAMES"));
	if ( ! order) {
		dprintf(D_ALWAYS, "Routes will be matched in hashtable order because JOB_ROUTER_ROUTE_NAMES was not configured.\n");
		// routes in case-sensitive hashtable order for backward compatibility with 8.8.6
		hash_order.startIterations();
		std::string name;
		int declaration_order;
		while(hash_order.iterate(name, declaration_order)) {
			m_route_order.push_back(name);
		}
	} else {
		// routes in specified order
		std::string tmp; tmp.reserve(200);

		// build a set of route names, we will remove names from this list as we put them into the order list
		classad::References remaining_names;
		for (auto it = m_routes->begin(); it != m_routes->end(); ++it) {
			route = it->second;
			remaining_names.insert(route->Name());
		}

#ifdef ROUTE_ORDER_CONFIG_WITH_STAR
		// build a route_order list using names from the JOB_ROUTER_ROUTE_NAMES param that exist in the routing table
		// put routes in the order list if they are before the * entry, and in the final list if they are after it
		// we also remove routes them from the remaining_names set.
		std::list<std::string> final_routes;
		bool is_final = false;
		StringTokenIterator it(order);
		for (const char * name = it.first(); name; name = it.next()) {
			auto rt = m_routes->find(name);
			if (rt != m_routes->end()) {
				route = rt->second;
				if (is_final) {
					final_routes.push_back(route->Name());
				} else {
					m_route_order.push_back(route->Name());
				}
				remaining_names.erase(route->Name());
			} else if (*name == '*') {
				is_final = true;
			} else {
				dprintf(D_ALWAYS, "route '%s' from JOB_ROUTER_ROUTE_NAMES not found in the routing table\n", name);
			}
		}

		// remaining_names now has the route names that are not in the JOB_ROUTER_ROUTE_NAMES list
		// append the routes not named in the JOB_ROUTER_ROUTE_NAMES into the order list
		for (auto rn = remaining_names.begin(); rn != remaining_names.end(); ++rn) {
			m_route_order.push_back(*rn);
		}

		// now append the final routes into the order list
		for (auto rn = final_routes.begin(); rn != final_routes.end(); ++rn) {
			m_route_order.push_back(*rn);
		}
#else
		// Use only the routes listed in JOB_ROUTER_ROUTE_NAMES
		// in the order they are specified there.
		StringTokenIterator it(order);
		for (const char * name = it.first(); name; name = it.next()) {
			auto rt = m_routes->find(name);
			if (rt != m_routes->end()) {
				route = rt->second;
				m_route_order.emplace_back(route->Name());
				remaining_names.erase(route->Name());
			} else {
				dprintf(D_ALWAYS, "route '%s' from JOB_ROUTER_ROUTE_NAMES not found in the routing table.\n", name);
			}
		}

		// if there are defined routes not listed in the route order, print their names now
		if ( ! remaining_names.empty()) {
			tmp.clear();
			for (auto rn = remaining_names.begin(); rn != remaining_names.end(); ++rn) {
				if ( ! tmp.empty()) { tmp += ", "; }
				tmp += *rn;
			}
			dprintf(D_ALWAYS, "Routes not in JOB_ROUTER_ROUTE_NAMES will be disabled: %s\n", tmp.c_str());
		}
#endif

		// print the resulting route order
		tmp.clear();
		for (auto rn = m_route_order.begin(); rn != m_route_order.end(); ++rn) {
			if ( ! tmp.empty()) { tmp += ", "; }
			tmp += *rn;
		}
		dprintf(D_ALWAYS, "Routes will be matched in this order: %s\n", tmp.c_str());
	}

	UpdateRouteStats();
}


void
JobRouter::DeallocateRoutingTable(RoutingTable *routes) {
	for (auto it = routes->begin(); it != routes->end(); ++it) {
		JobRoute * route = it->second;
		it->second = NULL;
		delete route;
	}
	delete routes;
}

RoutingTable *
JobRouter::AllocateRoutingTable() {
	return new RoutingTable();
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
	is_interrupted = false;
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
JobRouter::Poll( int /* timerID */ ) {
	dprintf(D_FULLDEBUG,"JobRouter: polling state of (%d) managed jobs.\n",NumManagedJobs());

	// Update our mirror(s) of the job queue(s).
	m_scheduler->poll();
	if ( m_scheduler2 ) {
		m_scheduler2->poll();
	}

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

void JobRouter::SimulateRouting()
{
	ASSERT(m_operate_as_tool);
	RoutedJob *job;
	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		// The following functions only do something if the job is in a state
		// where it needs the action to be done.
		TakeOverJob(job);
		SubmitJob(job);
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
		EXCEPT("JobRouter: Failed to parse orphan dest job constraint: '%s'",dest_jobs.c_str());
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
			std::string err_desc;
			if(!remove_job(*dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",m_schedd2_name,m_schedd2_pool,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (src=%s,dest=%s): failed to remove dest job: %s\n",src_key.c_str(),dest_key.c_str(),err_desc.c_str());
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
			std::string err_desc;
			if(!remove_job(job->dest_ad,dest_proc_id.cluster,dest_proc_id.proc,"JobRouter orphan",m_schedd2_name,m_schedd2_pool,err_desc)) {
				dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.c_str());
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
		EXCEPT("JobRouter: Failed to parse orphan constraint: '%s'",src_jobs.c_str());
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

		std::string error_details;
		PROC_ID src_proc_id = getProcByString(src_key.c_str());
		if(!yield_job(*src_ad,m_schedd1_name,m_schedd1_pool,false,src_proc_id.cluster,src_proc_id.proc,&error_details,JobRouterName().c_str(),true,m_release_on_hold)) {
			dprintf(D_ALWAYS,"JobRouter (src=%s): failed to yield orphan job: %s\n",
					src_key.c_str(),
					error_details.c_str());
		} else {
			// yield_job() sets the job's status to IDLE. If the job was
			// previously running, we need an evict event.
			int job_status = IDLE;
			src_ad->EvaluateAttrInt( ATTR_JOB_STATUS, job_status );
			if ( job_status == RUNNING || job_status == TRANSFERRING_OUTPUT ) {
				WriteEvictEventToUserLog( *src_ad );
			}
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

	HashTable<std::string,std::string> constraint_list(hashFunction);
	std::string umbrella_constraint;

	std::string dbuf("JobRouter: Checking for candidate jobs. routing table is:\n"
		//123456789012345678901233 entries:001   1234567/1234567 1234567/1234567 
		"Route Name               Source      Submitted/Max        Idle/Max     Throttle");
	if ( ! m_operate_as_tool) {
		dbuf += " Recent: Started Succeeded Failed\n";
	} else {
		dbuf += "\n";
	}
	for (auto it = m_route_order.begin(); it != m_route_order.end(); ++it) {
		route = safe_lookup_route(*it);
		if ( ! route) continue;
		formatstr_cat(dbuf, "%-24s %-11s %7d/%7d %7d/%7d %8s",
		      route->Name(),
		      route->Source(),
		      route->CurrentRoutedJobs(),
		      route->MaxJobs(),
		      route->CurrentIdleJobs(),
		      route->MaxIdleJobs(),
		      route->ThrottleDesc().c_str());
		if ( ! m_operate_as_tool) {
			formatstr_cat(dbuf, "         %7d %9d %6d\n",
			  route->RecentRoutedJobs(),
		      route->RecentSuccesses(),
			  route->RecentFailures()
			);
		} else {
			dbuf += "\n";
		}
	}
	dprintf(D_ALWAYS, "%s", dbuf.c_str());


	if(!AcceptingMoreJobs()) return; //router is full

	// Generate the list of routing constraints.
	// Each route may have its own constraint, but in case many of them
	// are the same, add only unique constraints to the list.
	std::string route_constraints;
	for (auto it = m_routes->begin(); it != m_routes->end(); ++it) {
		route = it->second;
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

	if (!can_switch_ids() && ! (m_operate_as_tool & JOB_ROUTER_TOOL_FLAG_CAN_SWITCH_IDS)) {
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

	if (m_operate_as_tool & JOB_ROUTER_TOOL_FLAG_DEBUG_UMBRELLA) {
		umbrella_constraint.insert(0, "debug(");
		umbrella_constraint += ")";
	}

	dprintf(D_FULLDEBUG,"JobRouter: Umbrella constraint: %s\n",umbrella_constraint.c_str());

	constraint_tree = parser.ParseExpression(umbrella_constraint);
	if(!constraint_tree) {
		EXCEPT("JobRouter: Failed to parse umbrella constraint: %s",umbrella_constraint.c_str());
	}

    query.Bind(ad_collection);
    if(!query.Query("root",constraint_tree)) {
		dprintf(D_ALWAYS,"JobRouter: Error running query: %s\n",umbrella_constraint.c_str());
		delete constraint_tree;
		return;
	}
	delete constraint_tree;

	int cJobsAdded = 0;
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

		if (m_operate_as_tool) { dprintf(D_FULLDEBUG, "JobRouter: Checking Job src=%s against all routes\n", key.c_str()); }

		bool all_routes_full;
		route = ChooseRoute(ad,&all_routes_full);
		if(!route) {
			if(all_routes_full) {
				dprintf(D_FULLDEBUG,"JobRouter: all routes are full (%d managed jobs).  Skipping further searches for candidate jobs.\n",NumManagedJobs());
				break;
			}
			dprintf(D_FULLDEBUG,"JobRouter: no route found for src=%s\n",key.c_str());
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

		dprintf(D_FULLDEBUG,"JobRouter: Found candidate job %s\n",job->JobDesc().c_str());
		AddJob(job);
		++cJobsAdded;

    } while (query.Next(key));

	if (m_operate_as_tool) {
		dprintf(D_ALWAYS, "JobRouter: %d candidate jobs found\n", cJobsAdded);
	}
}

JobRoute *
JobRouter::ChooseRoute(classad::ClassAd *job_ad,bool *all_routes_full) {
	std::vector<JobRoute *> matches;
	JobRoute *route=NULL;
	*all_routes_full = true;
	for (auto it = m_route_order.begin(); it != m_route_order.end(); ++it) {
		route = safe_lookup_route(*it);
		if ( ! route) continue;
		if(!route->AcceptingMoreJobs()) continue;
		*all_routes_full = false;
		if (route->Matches(job_ad)) {
			matches.push_back(route);
			if (m_operate_as_tool) { dprintf(D_FULLDEBUG, "JobRouter: \tRoute Matches: %s\n", route->Name()); }
		}

		if (!m_round_robin_selection && !matches.empty()) {
			break;
		}
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
	for (auto it = m_routes->begin(); it != m_routes->end(); ++it) {
		route = it->second;
		route->ResetCurrentRoutedJobs();
	}

	m_jobs.startIterations();
	while(m_jobs.iterate(job)) {
		if(!job->route_name.empty()) {
			route = safe_lookup_route(job->route_name);
			if (route) {
				route->IncrementCurrentRoutedJobs();
				if(job->IsRunning()) {
					route->IncrementCurrentRunningJobs();
				}
			}
		}
	}

	for (auto it = m_routes->begin(); it != m_routes->end(); ++it) {
		route = it->second;
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

	std::string error_details;
	ClaimJobResult cjr = claim_job(job->src_ad,m_schedd1_name,m_schedd1_pool,job->src_proc_id.cluster, job->src_proc_id.proc, &error_details, JobRouterName().c_str(), job->is_sandboxed);

	switch(cjr) {
	case CJR_ERROR: {
		dprintf(D_ALWAYS,"JobRouter failure (%s): candidate job could not be claimed by JobRouter: %s\n",job->JobDesc().c_str(),error_details.c_str());
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
	        // Retrieve the routing definition
	        JobRoute *route = GetRouteByName(job->route_name.c_str());
        	if(!route) {
        		dprintf(D_FULLDEBUG,"JobRouter (%s): Unable to retrieve route information for translation hook.\n",job->JobDesc().c_str());
        		GracefullyRemoveJob(job);
        		return;
        	}

		std::string route_info = route->RouteDescription();
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

	FinishSubmitJob(job);
}

void
JobRouter::FinishSubmitJob(RoutedJob *job) {

		// If we are not just editing the src job, then we need to do
		// the standard transformations to create a new job now.
	if(!job->edit_job_in_place) {
		VanillaToGrid::vanillaToGrid(&job->dest_ad,job->target_universe,job->grid_resource.c_str(),job->is_sandboxed);
	}

	// Apply any edits to the job ClassAds as defined in the route ad.
	JobRoute *route = GetRouteByName(job->route_name.c_str());
	if(!route) {
		dprintf(D_FULLDEBUG,"JobRouter (%s): route has been removed before job could be submitted.\n",job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	unsigned int xform_flags = 0;
	if (m_operate_as_tool & JOB_ROUTER_TOOL_FLAG_LOG_XFORM_ERRORS) { xform_flags |= XFORM_UTILS_LOG_ERRORS; }
	if (m_operate_as_tool & JOB_ROUTER_TOOL_FLAG_LOG_XFORM_STEPS) { xform_flags |= XFORM_UTILS_LOG_STEPS; }

	// The route ClassAd may change some things in the routed ad.
	if(!route->ApplyRoutingJobEdits(reinterpret_cast<ClassAd*>(&job->dest_ad), m_pre_route_xfms, m_post_route_xfms, xform_flags)) {
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

	std::string idtokens;
	if(!route->EvalSendIDTokens(job,idtokens)) {
		dprintf(D_ALWAYS,
				"JobRouter failure (%s): evaluated SendIDTokens value is invalid!\n",
				job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}
	if (!idtokens.empty()) {
		std::string addfiles;

		StringTokenIterator it(idtokens);
		for (auto str = it.first(); str; str = it.next()) {
			auto idt = m_idtokens.find(str);
			if (idt != m_idtokens.end()) {
				if (!addfiles.empty()) { addfiles += ","; }
				addfiles += idt->second;
			} else {
				dprintf(D_ALWAYS, "JobRouter failure (%s): unknown IDTOKEN %s\n", job->JobDesc().c_str(), str);
				GracefullyRemoveJob(job);
				return;
			}
		}

		if (!addfiles.empty()) {
			std::string xferfiles;
			if (job->dest_ad.LookupString(ATTR_TRANSFER_INPUT_FILES, xferfiles) && ! xferfiles.empty()) {
				addfiles += ",";
				addfiles += xferfiles;
			}
			job->dest_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, addfiles);
		}
	}

	int dest_cluster_id = -1;
	int dest_proc_id = -1;
	bool rc;

	std::string owner, domain;
	if (!job->src_ad.EvaluateAttrString(ATTR_OWNER,  owner)) {
		GracefullyRemoveJob(job);
		return;
	}
	job->src_ad.EvaluateAttrString(ATTR_NT_DOMAIN, domain);

	rc = submit_job(owner, domain, job->dest_ad,m_schedd2_name,m_schedd2_pool,job->is_sandboxed,&dest_cluster_id,&dest_proc_id);

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
	snprintf(buf,sizeof(buf),"%d.%d",dest_cluster_id,dest_proc_id);
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
JobRouter::UpdateRoutedJobStatus(RoutedJob *job, const classad::ClassAd &update) {
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
				"before update finished.  Nothing will be "
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
		dprintf(D_ALWAYS, "JobRouter failure (%s): Failed to update "
				"routed job status.\n", job->JobDesc().c_str());
		GracefullyRemoveJob(job);
		return;
	}

	// Send the updates to the job queue
	if (false == PushUpdatedAttributes(job->dest_ad, true))
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
	// Until we see the job in the job queue mirror, don't invoke the hook.
	// There's nothing new, and our copy of the job ad doesn't have the
	// job id, needed to push any attributes returned by the hook to the
	// schedd.
	if (NULL != m_hook_mgr && job->SawDestJob())
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
		job->is_interrupted = true;
		GracefullyRemoveJob(job);
		return;
	}

	int job_status = 0;
	if( !src_ad->EvaluateAttrInt( ATTR_JOB_STATUS, job_status ) ) {
		dprintf(D_ALWAYS, "JobRouter failure (%s): cannot evaluate JobStatus in src job\n",job->JobDesc().c_str());
		job->is_interrupted = true;
		GracefullyRemoveJob(job);
		return;
	}

	if(job_status == REMOVED) {
		dprintf(D_FULLDEBUG, "JobRouter (%s): found src job marked for removal\n",job->JobDesc().c_str());
		WriteAbortEventToUserLog( *src_ad );
		job->is_interrupted = true;
		GracefullyRemoveJob(job);
		return;
	}

	if(job_status == HELD && !hold_copied_from_target_job(*src_ad) ) {
		dprintf(D_FULLDEBUG, "JobRouter (%s): found src job on hold\n",job->JobDesc().c_str());
		job->is_interrupted = true;
		GracefullyRemoveJob(job);
		return;
	}

	classad::ClassAd *ad = ad_collection2->GetClassAd(job->dest_key);

	// If ad is not found, check if enough time has passed
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
	
	keyword = m_hook_mgr ? m_hook_mgr->getHookKeyword(job->src_ad) : "";
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
	int old_status = IDLE;
	job->src_ad.EvaluateAttrInt(ATTR_JOB_STATUS, old_status);
	if ( old_status != IDLE ) {
		if ( old_status == RUNNING || old_status == TRANSFERRING_OUTPUT ) {
			WriteEvictEventToUserLog( job->src_ad );
		}
		job->src_ad.InsertAttr(ATTR_JOB_STATUS,IDLE);
		if(false == PushUpdatedAttributes(job->src_ad)) {
			dprintf(D_ALWAYS,"JobRouter failure (%s): failed to set src job status back to idle\n",job->JobDesc().c_str());
		}
	}
}

bool
JobRouter::PushUpdatedAttributes(classad::ClassAd& ad, bool routed_job) {
	if(false == push_dirty_attributes(ad,
						routed_job ? m_schedd2_name : m_schedd1_name,
						routed_job ? m_schedd2_pool : m_schedd1_pool))
	{
		return false;
	}
	else
	{
		ad.ClearAllDirtyFlags();
	}
	return true;
}

void
JobRouter::FinishFinalizeJob(RoutedJob *job) {
	std::string owner, domain;
	if (!job->src_ad.EvaluateAttrString(ATTR_OWNER,  owner)) {
		SetJobIdle(job);
		GracefullyRemoveJob(job);
		return;
	}
	job->src_ad.EvaluateAttrString(ATTR_NT_DOMAIN, domain);

	if(!finalize_job(owner, domain, job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,m_schedd2_name,m_schedd2_pool,job->is_sandboxed)) {
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
	bool test_result = route->JobFailureTest(&job->src_ad);
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
	bool test_result = route->JobShouldBeSandboxed(&job->src_ad);
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
	bool test_result = route->EditJobInPlace(&job->src_ad);
	return test_result;
}


bool JobRoute::JobFailureTest(classad::ClassAd * job_ad)
{
	bool test_result = false;
	classad::ExprTree* expr = m_JobFailureTest.Expr();
	if (expr) {
		classad::Value val;
		if ( ! job_ad->EvaluateExpr(expr, val) || ! val.IsBooleanValueEquiv(test_result)) {
			// UNDEFINED is false
			test_result = false;
		}
	}
	return test_result;
}

bool JobRoute::JobShouldBeSandboxed(classad::ClassAd * job_ad)
{
	bool test_result = false;
	classad::ExprTree* expr = m_JobShouldBeSandboxed.Expr();
	if (expr) {
		classad::Value val;
		if ( ! job_ad->EvaluateExpr(expr, val) || ! val.IsBooleanValueEquiv(test_result)) {
			// UNDEFINED is false
			test_result = false;
		}
	}
	return test_result;
}

bool JobRoute::EditJobInPlace(classad::ClassAd * job_ad)
{
	bool test_result = false;
	classad::ExprTree* expr = m_EditJobInPlace.Expr();
	if (expr) {
		classad::Value val;
		if ( ! job_ad->EvaluateExpr(expr, val) || ! val.IsBooleanValueEquiv(test_result)) {
			// UNDEFINED is false
			test_result = false;
		}
	}
	return test_result;
}


bool
JobRoute::EvalUseSharedX509UserProxy(RoutedJob *job)
{
	bool test_result = false;

	classad::ExprTree* expr = m_UseSharedX509UserProxy.Expr();
	if (expr) {
		classad::Value val;
		if ( ! job->src_ad.EvaluateExpr(expr, val) || ! val.IsBooleanValueEquiv(test_result)) {
			// UNDEFINED is false
			test_result = false;
		}
	}
	return test_result;
}

bool
JobRoute::EvalSharedX509UserProxy(RoutedJob *job,std::string &proxy_file)
{
	classad::ExprTree* expr = m_SharedX509UserProxy.Expr();
	if (expr) {
		classad::Value val;
		if (job->src_ad.EvaluateExpr(expr, val) && val.IsStringValue(proxy_file)) {
			return true;
		}
	}
	return false;
}

bool JobRoute::EvalSendIDTokens(RoutedJob *job, std::string &idtokens)
{
	classad::ExprTree* expr = m_SendIDTokens.Expr();
	if (expr) {
		classad::Value val;
		if (job->src_ad.EvaluateExpr(expr, val) && val.IsStringValue(idtokens)) {
			return true;
		}
		// don't treat value==undefined as an error
		if (ExprTreeIsLiteral(expr, val) && val.IsUndefinedValue()) {
			idtokens.clear();
			return true;
		}
		return false;
	} else {
		// if no expression, use the global knob
		param(idtokens, "JOB_ROUTER_SEND_ROUTE_IDTOKENS");
	}
	return true;
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
		std::string err_desc;
		if(!remove_job(job->dest_ad,job->dest_proc_id.cluster,job->dest_proc_id.proc,"JobRouter aborted job",m_schedd2_name,m_schedd2_pool,err_desc)) {
			dprintf(D_ALWAYS,"JobRouter (%s): failed to remove dest job: %s\n",job->JobDesc().c_str(),err_desc.c_str());
		}
		else {
			job->dest_proc_id.cluster = -1;
		}
	}

	if(job->is_claimed) {
		std::string error_details;
		bool keep_trying = true;
		int job_status = IDLE;
		// yield_job() sets the job's status to IDLE. If the job was
		// previously running, we need an evict event.
		job->src_ad.EvaluateAttrInt( ATTR_JOB_STATUS, job_status );
		if ( job_status == RUNNING || job_status == TRANSFERRING_OUTPUT ) {
			WriteEvictEventToUserLog( job->src_ad );
		}
		if(!yield_job(job->src_ad,m_schedd1_name,m_schedd1_pool,job->is_done,job->src_proc_id.cluster,job->src_proc_id.proc,&error_details,JobRouterName().c_str(),job->is_sandboxed,m_release_on_hold,&keep_trying))
		{
			dprintf(D_ALWAYS,"JobRouter (%s): failed to yield job: %s\n",
					job->JobDesc().c_str(),
					error_details.c_str());

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
			else if(!job->is_interrupted) {
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

	// If src_ad cannot be found in the mirror, then the ad has already
	// been retired from the job ad. This is particularly likely to
	// happen if we removed the job (e.g. due to job policy expressions).
	// Consider it synchronized.

	if(!src_ad) {
		src_job_synchronized = true;
	} else {
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
JobRouter::TimerHandler_UpdateCollector( int /* timerID */ ) {
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_public_ad);
}

void
JobRouter::InvalidatePublicAd() {
	ClassAd invalidate_ad;
	std::string line;

	ASSERT( ! m_operate_as_tool);

	SetMyTypeName(invalidate_ad, QUERY_ADTYPE);
	invalidate_ad.Assign(ATTR_TARGET_TYPE, JOB_ROUTER_ADTYPE);

	formatstr(line, "%s == \"%s\"", ATTR_NAME, daemonName.c_str());
	invalidate_ad.AssignExpr(ATTR_REQUIREMENTS, line.c_str());
	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad, NULL, false);
}

JobRoute::JobRoute(const char * source) : m_source(source) {
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
	m_route_from_classad = false;
	m_use_pre_route_transform = false;
}

JobRoute::~JobRoute() {
}
bool JobRoute::AcceptingMoreJobs() const
{
	if( m_throttle > 0 && m_throttle <= m_recent_jobs_routed) {
		return false;
	}
	if( m_max_idle_jobs >= 0 && m_max_idle_jobs <= CurrentIdleJobs() ) {
		return false;
	}
	return m_num_jobs < m_max_jobs;
}
// HTCONDOR-646, for backward compatible logging/hooks print a classad containing the route name
std::string JobRoute::RouteDescription() {
	std::string info;
	classad::ClassAd ad;
	ad.InsertAttr(ATTR_NAME, m_name);
	ad.InsertAttr(JR_ATTR_TARGET_UNIVERSE, m_target_universe);
	if ( ! m_grid_resource.empty()) { ad.InsertAttr(ATTR_GRID_RESOURCE, m_grid_resource); }
	classad::ClassAdUnParser unparser;
	unparser.Unparse(info,&ad);
	return info;
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
		formatstr(desc, "%g jobs/sec", throttle/THROTTLE_UPDATE_INTERVAL);
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
JobRoute::ApplyRoutingJobEdits(
	ClassAd *src_ad,
	std::vector<MacroStreamXFormSource*>& pre_route,
	std::vector<MacroStreamXFormSource*>& post_route,
	unsigned int xform_flags)
{
	XFormHash mset(XFormHash::Flavor::Basic);
	mset.macros().apool.reserve(0x10000); // allocate workspace. TODO: keep track of route workspace size.
	mset.macros().sources.push_back(this->Name());

	std::string errmsg;
	int rval = 0;
	if (m_use_pre_route_transform && !pre_route.empty()) {
		for (MacroStreamXFormSource* xfm: pre_route) {
			if ( ! xfm->matches(src_ad)) {
				dprintf(D_FULLDEBUG, "JobRouter pre-route transform %s: does not match job. skippping it.\n", xfm->getName());
				continue;
			}
			if (xform_flags & XFORM_UTILS_LOG_STEPS) { dprintf(D_ALWAYS, "\tApplying pre-route transform: %s\n", xfm->getName()); }
			rval = TransformClassAd(src_ad, *xfm, mset, errmsg, xform_flags);
			if (rval < 0) {
				// transform failed, errmsg says why.
				dprintf(D_ALWAYS, "JobRouter failure in pre-route transform %s: %s.\n", xfm->getName(), errmsg.c_str());
				return false;
			}
		}
		// the pre-route may leave behind some temp variables in the mset.  we want to sort those now
		// for efficient lookup in the route.  TODO: maybe someday make the macro set sort on insert?
		optimize_macros(mset.macros());
	}

	if (xform_flags & XFORM_UTILS_LOG_STEPS) { dprintf(D_ALWAYS, "\tApplying route: %s\n", m_route.getName()); }
	rval = TransformClassAd(src_ad, m_route, mset, errmsg, xform_flags);
	if (rval < 0) {
		// transform failed, errmsg says why.
		dprintf(D_ALWAYS,"JobRouter failure (route=%s): %s.\n",Name(), errmsg.c_str());
		return false;
	}

	if (m_use_pre_route_transform && ! post_route.empty()) {
		for (MacroStreamXFormSource* xfm: post_route) {
			if ( ! xfm->matches(src_ad)) {
				dprintf(D_FULLDEBUG, "JobRouter post-route transform %s: does not match job. skippping it.\n", xfm->getName());
				continue;
			}
			if (xform_flags & XFORM_UTILS_LOG_STEPS) { dprintf(D_ALWAYS, "\tApplying post-route transform: %s\n", xfm->getName()); }
			rval = TransformClassAd(src_ad, *xfm, mset, errmsg, xform_flags);
			if (rval < 0) {
				// transform failed, errmsg says why.
				dprintf(D_ALWAYS, "JobRouter failure in pre-route transform %s: %s.\n", xfm->getName(), errmsg.c_str());
				return false;
			}
		}
	}
	return true;
}

bool
JobRoute::ParseNext(
	const std::string & routing_string,
	int &offset,
	const classad::ClassAd *router_defaults_ad,
	bool allow_empty_requirements,
	const char * config_name,
	std::string & errmsg)
{
	errmsg.clear();

	// The caller will pass offset 0 and no router_defaults_ad when parsing a knob from JOB_ROUTER_ROUTE_*
	// it should always pass a defaults ad (possibly and empty one) when parsing JOB_ROUTER_ENTRIES*
	// for single route knobs, we require the route name to match the knob tag
	bool single_route_knob = !router_defaults_ad && (offset == 0);
	if (single_route_knob) {
		this->m_route.setName(config_name);
	}

	// skip leading whitespace
	while (offset < (int)routing_string.size() && isspace(routing_string[offset])) ++offset;
	if (offset+1 >= (int)routing_string.size()) return false;

	// if the route starts with [ or with a c-style comment it must be an old-syntax classad route
	if (routing_string[offset] == '[' || (routing_string[offset] == '/' && routing_string[offset+1] == '*')) {
		// parse as new classad, use an empty defaults ad if none was provided
		ClassAd dummy;
		StringList statements;
		std::string route_name(config_name?config_name:"");
		if ( ! router_defaults_ad) router_defaults_ad = &dummy;
		int rval = ConvertClassadJobRouterRouteToXForm(statements, route_name, routing_string, offset, *router_defaults_ad, 0);
		if (rval < 0 || statements.isEmpty()) {
			return false;
		}
		m_route.setName(route_name.c_str()); // probably unncessary because m_route.open will set this also...
		m_route_from_classad = true;
		m_use_pre_route_transform = single_route_knob;
		auto_free_ptr route_str(statements.print_to_delimed_string("\n"));
		int route_offset = 0;
		int nlines = m_route.open(route_str, route_offset, errmsg);
		if (nlines < 0) { // < 0 because routes that don't change the job are permitted
			return false;
		}
	} else {
		m_route_from_classad = false;
		m_use_pre_route_transform = true;
		int nlines = m_route.open(routing_string.c_str(), offset, errmsg);
		if (nlines < 0) { // < 0 because routes that don't change the job are permitted
			return false;
		}
	}
	// for routes that come from a single knob. (i.e. JOB_ROUTER_ROUTE_FOO) that name *must* be the same as the config name
	if (single_route_knob && ! m_name.empty() && (YourStringNoCase(config_name) != m_name.c_str())) {
		dprintf(D_ALWAYS, "WARNING: The Name specified in JOB_ROUTER_ROUTE_%s was \"%s\". using %s instead.", config_name, m_name.c_str(), config_name);
		m_name = config_name;
	}
	const char * name = m_route.getName(); if ( ! name) name = "";

	XFormHash mset;
	mset.init();

	std::string xfm_text;
	dprintf(D_ALWAYS, "JobRouter: route %s converted to :\n%s\n",
			name, m_route.getFormattedText(xfm_text, "\t") );


	// insure that the resulting xform will parse, and also populate the XFormHash with it's params
	// so that we can query/store them for use in the routing process prior to actual job transformation.
	if ( ! ValidateXForm(m_route, mset, nullptr, errmsg)) {
		dprintf(D_ALWAYS, "JobRouter: route %s is not valid: %s\n", name, errmsg.c_str());
		return false;
	}

	// load expressions that need to be evaluated before we actually route.
	// all of these are optional.
	m_JobFailureTest.set(mset.local_param(JR_ATTR_JOB_FAILURE_TEST, m_route.context()));
	m_JobShouldBeSandboxed.set(mset.local_param(JR_ATTR_JOB_SANDBOXED_TEST, m_route.context()));
	m_EditJobInPlace.set(mset.local_param(JR_ATTR_EDIT_JOB_IN_PLACE, m_route.context()));
	m_UseSharedX509UserProxy.set(mset.local_param(JR_ATTR_USE_SHARED_X509_USER_PROXY, m_route.context()));
	m_SharedX509UserProxy.set(mset.local_param(JR_ATTR_SHARED_X509_USER_PROXY, m_route.context()));
	if (mset.lookup(JR_ATTR_SEND_IDTOKENS, m_route.context())) {
		m_SendIDTokens.set(mset.local_param(JR_ATTR_SEND_IDTOKENS, m_route.context()));
		// if SendIDTokens is declared but empty, set it to the empty string so that the route does not use the global default
		if (m_SendIDTokens.empty()) { m_SendIDTokens.set(strdup("\"\"")); }
	}

	bool knob_exists = false;
	m_max_jobs = mset.local_param_int(JR_ATTR_MAX_JOBS, 100, m_route.context(), &knob_exists);
	if ( ! knob_exists) {
		m_max_jobs = param_integer("JOB_ROUTER_DEFAULT_MAX_JOBS_PER_ROUTE", m_max_jobs);
	}
	m_max_idle_jobs = mset.local_param_int(JR_ATTR_MAX_IDLE_JOBS, 50, m_route.context(), &knob_exists);
	if ( ! knob_exists) {
		m_max_idle_jobs = param_integer("JOB_ROUTER_DEFAULT_MAX_IDLE_JOBS_PER_ROUTE", m_max_idle_jobs);
	}
	m_failure_rate_threshold = mset.local_param_double(JR_ATTR_FAILURE_RATE_THRESHOLD, 0.03, m_route.context());
	m_target_universe = m_route.getUniverse();
	if ( ! m_target_universe) {
		m_target_universe = mset.local_param_int(JR_ATTR_TARGET_UNIVERSE, CONDOR_UNIVERSE_GRID, m_route.context());
		m_route.setUniverse(m_target_universe);
	}
	if (m_target_universe == CONDOR_UNIVERSE_GRID) {
		if ( ! mset.local_param_unquoted_string(ATTR_GRID_RESOURCE, m_grid_resource, m_route.context()) ) {
			dprintf(D_FULLDEBUG, "JobRouter: Missing or invalid %s in job route. Jobs will fail if it's not set in routed job ad.\n",ATTR_GRID_RESOURCE);
		}	
	}

	if ( ! m_route.getName()) {
		// If no name is specified, use the GridResource as the name.
		m_route.setName(m_grid_resource.c_str());
		if ( ! m_route.getName()) {
			dprintf(D_ALWAYS, "JobRouter: Missing or invalid %s in job route.\n",ATTR_NAME);
			return false;
		}
	}
	m_name = m_route.getName();

	if ( ! m_route.getRequirements()) {
		if( ! allow_empty_requirements) {
			dprintf(D_ALWAYS, "JobRouter CONFIGURATION ERROR: Missing %s in job route.\n",ATTR_REQUIREMENTS);
			return false;
		}
	}
	bool has_over = false;
	m_override_routing_entry = mset.local_param_bool(JR_ATTR_OVERRIDE_ROUTING_ENTRY, false, m_route.context(), &has_over);
	if ( ! has_over) {
		m_override_routing_entry = -1;
	}

	return true;
}

std::string
RoutedJob::JobDesc() const {
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
	dest_ad.EnableDirtyTracking();
}
