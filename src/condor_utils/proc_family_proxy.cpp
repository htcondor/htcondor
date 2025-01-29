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
#include "condor_config.h"
#include "proc_family_proxy.h"
#include "condor_daemon_core.h"
#include "../condor_procd/proc_family_client.h"
#include "setenv.h"
#include "directory.h"
#include "basename.h"
#include "procd_config.h"

// enable PROCAPI profileing code.
// #define PROFILE_PROCAPI

// this class is just used to forward reap events to the real
// ProcFamilyProxy object; we do this in a separate class to
// avoid multiple inheritance (would that even work?) since DC
// C++ callbacks need to be in a class derived from Service
//
class ProcFamilyProxyReaperHelper : public Service {

public:
	ProcFamilyProxyReaperHelper(ProcFamilyProxy* ptr) : m_ptr(ptr) { }

	int procd_reaper(int pid, int status) const
	{
		return m_ptr->procd_reaper(pid, status);
	}

	ProcFamilyProxy* m_ptr;
};

int ProcFamilyProxy::s_instantiated = false;

ProcFamilyProxy::ProcFamilyProxy(const char* address_suffix)
	: m_procd_pid(-1)
	, m_former_procd_pid(-1)
	, m_reaper_id(FALSE)
	, m_reaper_notify(NULL)
	, m_reaper_notify_me(NULL)
{
	// only one of these should be instantiated
	//
	if (s_instantiated) {
		EXCEPT("ProcFamilyProxy: multiple instantiations");
	}
	s_instantiated = true;

	// get the address that we'll use to contact the ProcD
	//
	m_procd_addr = get_procd_address();

	// if we were handed a non-NULL address_suffix argument, tack
	// it on. this is meant so that if we are in a situation where
	// multiple daemons want to start ProcDs and they have the same
	// setting for PROCD_ADDRESS, the ProcDs won't attempt to use
	// the same "command pipe" (which would cause one of them to
	// fail)
	//
	std::string procd_addr_base = m_procd_addr;
	if (address_suffix != NULL) {
		formatstr_cat(m_procd_addr, ".%s", address_suffix);
	}

	// see what log file (if any) the ProcD will be using if we
	// need to start one (use the address_suffix here as well to
	// avoid collisions)
	//
	if (param_boolean("LOG_TO_SYSLOG", false)) {
		m_procd_log = "SYSLOG";
	} else {
		char* procd_log = param("PROCD_LOG");
		if (procd_log != NULL) {
			m_procd_log = procd_log;
			free(procd_log);
			if (address_suffix != NULL) {
				formatstr_cat(m_procd_log, ".%s", address_suffix);
			}
		}
	}
	
	// create our "reaper helper" before we think about starting a ProcD
	//
	m_reaper_helper = new ProcFamilyProxyReaperHelper(this);
	ASSERT(m_reaper_helper != NULL);

	// determine if we need to launch a ProcD
	//
	// if a parent daemon created a ProcD that we can use, it will
	// have handed us an environment variable indicating its address.
	// if this address matches the ProcD address that we are configured
	// to use, we don't need to create a ProcD. if the env var isn't
	// there or they don't match, we need to create a ProcD and also set
	// the environment variable so any DC children can share this ProcD
	//
	const char* base_addr_from_env = GetEnv("CONDOR_PROCD_ADDRESS_BASE");
	if ((base_addr_from_env == NULL) || (procd_addr_base != base_addr_from_env)) {
		if (!start_procd()) {
			EXCEPT("unable to spawn the ProcD");
		}
		SetEnv("CONDOR_PROCD_ADDRESS_BASE", procd_addr_base.c_str());
		SetEnv("CONDOR_PROCD_ADDRESS", m_procd_addr.c_str());
	}
	else {
		const char* addr_from_env = GetEnv("CONDOR_PROCD_ADDRESS");
		if (addr_from_env == NULL) {
			EXCEPT("CONDOR_PROCD_ADDRESS_BASE in environment "
			           "but not CONDOR_PROCD_ADDRESS");
		}
		m_procd_addr = addr_from_env;
	}

	// create the ProcFamilyClient object for communicating with the ProcD
	//
	m_client = new ProcFamilyClient;
	ASSERT(m_client != NULL);
	if (!m_client->initialize(m_procd_addr.c_str())) {
		dprintf(D_ALWAYS,
		        "ProcFamilyProxy: error initializing ProcFamilyClient\n");
		recover_from_procd_error();
	}
}

ProcFamilyProxy::~ProcFamilyProxy()
{
	// if we started a ProcD, shut it down and remove the environment
	// variable marker
	//
	if (m_procd_pid != -1) {
		stop_procd();
		UnsetEnv("CONDOR_PROCD_ADDRESS_BASE");
		UnsetEnv("CONDOR_PROCD_ADDRESS");
	}

	// clean up allocated memory
	//
	delete m_client;
	delete m_reaper_helper;

	// update instantiated flag
	//
	s_instantiated = false;

}

bool
ProcFamilyProxy::register_subfamily(pid_t root_pid,
                                    pid_t watcher_pid,
                                    int max_snapshot_interval)
{
    #ifdef PROFILE_PROCAPI
    DC_AUTO_RUNTIME_PROBE(__FUNCTION__,auto0);
    #endif

	// HACK: we treat this call specially, since it is only called
	// from forked children on UNIX. this means that if we were to
	// try to restart the ProcD we would update the ProcD-related
	// information in the child, and the parent wouldn't see the
	// changes. therefore, we consider it an error if communication
	// with the ProcD fails
	//
	bool response;
	if (!m_client->register_subfamily(root_pid,
	                                  watcher_pid,
	                                  max_snapshot_interval,
	                                  response))
	{
		dprintf(D_ALWAYS, "register_subfamily: ProcD communication error\n");
		return false;
	}

    // suck statistics out of global variables, this is ugly, but the procd doesn't have
    // access to daemonCore, so I can't store the values until I get back to here..
    #ifdef PROFILE_PROCAPI
    {
       extern double pfc_lc_rt_start_connection;
       extern double pfc_lc_rt_open_pipe;
       extern double pfc_lc_rt_wait_pipe;
       extern double pfc_lc_rt_write_pipe;
       extern double pfc_lc_rt_read_data;
       extern double pfc_lc_rt_end_connection;

       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily_0start_connection", IF_VERBOSEPUB, pfc_lc_rt_start_connection);
       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily__0open_pipe", IF_VERBOSEPUB, pfc_lc_rt_open_pipe);
       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily__1wait_pipe", IF_VERBOSEPUB, pfc_lc_rt_wait_pipe);
       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily__2write_pipe", IF_VERBOSEPUB, pfc_lc_rt_write_pipe);
       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily_1read_data", IF_VERBOSEPUB, pfc_lc_rt_read_data);
       daemonCore->dc_stats.AddSample("DCFuncProcFamilyProxy::register_subfamily_2end_connection", IF_VERBOSEPUB, pfc_lc_rt_end_connection);
    }
    #endif

	return response;
}

bool
ProcFamilyProxy::track_family_via_environment(pid_t pid, PidEnvID& penvid)
{
	// see "HACK" comment in register_subfamily for why we don't try
	// to recover from errors here
	//
	bool response;
	if (!m_client->track_family_via_environment(pid, penvid, response)) {
		dprintf(D_ALWAYS,
		        "track_family_via_environment: "
		            "ProcD communication error\n");
		return false;
	}
	return response;
}

bool
ProcFamilyProxy::track_family_via_login(pid_t pid, const char* login)
{
	// see "HACK" comment in register_subfamily for why we don't try
	// to recover from errors here
	//
	bool response;
	if (!m_client->track_family_via_login(pid, login, response)) {
		dprintf(D_ALWAYS,
		        "track_family_via_login: "
		            "ProcD communication error\n");
		return false;
	}
	return response;
}

#if defined(LINUX)
bool
ProcFamilyProxy::track_family_via_allocated_supplementary_group(pid_t pid, gid_t& gid)
{ 
	// see "HACK" comment in register_subfamily for why we don't try
	// to recover from errors here
	//
	bool response;
	if (!m_client->track_family_via_allocated_supplementary_group(pid,
	                                                       response,
	                                                       gid)) {
		dprintf(D_ALWAYS,
		        "track_family_via_allocated_supplementary_group: "
		            "ProcD communication error\n");
		return false;
	}
	return response;
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
bool
ProcFamilyProxy::track_family_via_cgroup(pid_t pid, const FamilyInfo *fi)
{
	bool response;
	dprintf(D_FULLDEBUG, "track_family_via_cgroup: Tracking PID %u via cgroup %s.\n",
		pid, fi->cgroup);
	if (!m_client->track_family_via_cgroup(pid, fi->cgroup, response)) {
		dprintf(D_ALWAYS,
			"track_family_via_cgroup: ProcD communication error\n");
		return false;
	}
	return response;
}
#else
bool
ProcFamilyProxy::track_family_via_cgroup(pid_t , FamilyInfo *)
{
	// We can hit this path when the first DaemonCore::Create_Process doesn't request
	// a cgroup, but a subsequent one does.  Currently, this only happens when a docker
	// universe job (which doesn't request a cgroup for the docker command) subsequently
	// runs ssh-to-job (which does (although it doesn't need to)).
	//
	// For now fix this by just ignoring this, it doesn't cause any real problems, 
	// but we should have a more robust solution in the future
	dprintf(D_ALWAYS, "Cgroup based family tracking requested, but we have a proc family that can't, skipping.\n");
	return true;
}
#endif

bool
ProcFamilyProxy::get_usage(pid_t pid, ProcFamilyUsage& usage, bool)
{
	bool response;
	while (!m_client->get_usage(pid, usage, response)) {
		dprintf(D_ALWAYS, "get_usage: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::signal_process(pid_t pid, int sig)
{
	bool response;
	while (!m_client->signal_process(pid, sig, response)) {
		dprintf(D_ALWAYS, "signal_process: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::kill_family(pid_t pid)
{
	bool response;
	while (!m_client->kill_family(pid, response)) {
		dprintf(D_ALWAYS, "kill_family: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::suspend_family(pid_t pid)
{
	bool response;
	while (!m_client->suspend_family(pid, response)) {
		dprintf(D_ALWAYS, "suspend_family: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::continue_family(pid_t pid)
{
	bool response;
	if (!m_client->continue_family(pid, response)) {
		dprintf(D_ALWAYS, "continue_family: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::unregister_family(pid_t pid)
{
	//dprintf(D_ALWAYS, "ProcFamilyProxy::unregister_family(%d)\n", pid);

	// if we just stopped the procd, quietly ignore a request to unregister a pid
	// since stopping the procd unregisters everthing. In normal operation, we hit
	// this code for shared-port and for the procd itself during master shutdown and restart.
	if ((m_former_procd_pid != -1) && (m_procd_pid == -1)) {
		// procd is gone, so unregister is successful by definition. ;)
		return true;
	}

	bool response;
	if (!m_client->unregister_family(pid, response)) {
		dprintf(D_ALWAYS, "unregister_subfamily: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::snapshot()
{
	bool response;
	if (!m_client->snapshot(response)) {
		dprintf(D_ALWAYS, "snapshot: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}


bool
ProcFamilyProxy::quit(void(*notify)(void*me, int pid, int status), void* me)
{
	if (m_procd_pid != -1) {
		m_reaper_notify = notify;
		m_reaper_notify_me = me;
		bool stopping = stop_procd();
		UnsetEnv("CONDOR_PROCD_ADDRESS_BASE");
		UnsetEnv("CONDOR_PROCD_ADDRESS");
		return stopping;
	}
	return false;
}

bool
ProcFamilyProxy::start_procd()
{
	// we'll only start one ProcD
	//
	ASSERT(m_procd_pid == -1);

	// now, we build up an ArgList for the procd
	//
	std::string exe;
	ArgList args;

	// path to the executable
	//
	char* path = param("PROCD");
	if (path == NULL) {
		dprintf(D_ALWAYS, "start_procd: PROCD not defined in configuration\n");
		return false;
	}
	exe = path;
	args.AppendArg(condor_basename(path));
	free(path);

	// the procd's address
	//
	args.AppendArg("-A");
	args.AppendArg(m_procd_addr);

	// parse the (optional) procd log file size
	// we do this here so that we can have a log size of 0 disable logging.
	//
	int log_size = -1; // -1 means infinite
	char *max_procd_log = param("MAX_PROCD_LOG");
	if (max_procd_log) {
		long long maxlog = 0;
		bool unit_is_time = false;
		bool r = dprintf_parse_log_size(max_procd_log, maxlog, unit_is_time);
		if (!r) {
			dprintf(D_ALWAYS, "Invalid config! MAX_PROCD_LOG = %s: must be an integer literal and may be followed by a units value\n", max_procd_log);
			maxlog = 1*1000*1000; // use the default of 1Mb
		}
		if (unit_is_time) {
			dprintf(D_ALWAYS, "Invalid config! MAX_PROCD_LOG must be a size, not a time in this version of HTCondor.\n");
			maxlog = 1*1000*1000; // use the default of 1Mb
		}

		// the procd takes it's maxlog as an integer, so only pass the param if it is 0, or a positive value that is in range
		// the procd treats -1 as INFINITE, and that is the default, so we don't need to pass large values on
		// we will also treat log size of 0 as a special case that just disables the log entirely.
		// 
		if (maxlog >= 0 && maxlog < INT_MAX) { log_size = (int)maxlog; }
		free(max_procd_log);
	}

	// the (optional) procd log file
	//
	if (m_procd_log.length() > 0 && log_size != 0) {
		args.AppendArg("-L");
		args.AppendArg(m_procd_log);
	
		// pass a log size arg if it is > 0 (-1 is the internal default, and 0 means to disable the log)
		if (log_size > 0) {
			args.AppendArg("-R");
			args.AppendArg(std::to_string(log_size));
		}
	}


	Env env;
	// The procd can't param, so pass this via the environment
	if (param_boolean("USE_PSS", false)) {
		env.SetEnv("_condor_USE_PSS=TRUE");
	}

	// the (optional) maximum snapshot interval
	// (the procd will default to every minute)
	//
	char* max_snapshot_interval = param("PROCD_MAX_SNAPSHOT_INTERVAL");
	if (max_snapshot_interval != NULL) {
		args.AppendArg("-S");
		args.AppendArg(max_snapshot_interval);
		free(max_snapshot_interval);
	}

	// (optional) make the procd sleep on startup so a
	// debugger can attach
	//
	bool debug = param_boolean("PROCD_DEBUG", false);
	if (debug) {
		args.AppendArg("-D");
	}

#if !defined(WIN32)
	// On UNIX, we need to tell the procd to allow connections from the
	// condor user
	//
	args.AppendArg("-C");
	args.AppendArg(std::to_string(get_condor_uid()));
#endif

#if defined(WIN32)
	// on Windows, we need to tell the procd what program to use to send
	// softkills
	//
	char* softkill_path = param("WINDOWS_SOFTKILL");
	if ( softkill_path == NULL ) {
		dprintf(D_ALWAYS,
		        "WINDOWS_SOFTKILL undefined; "
		        	"ProcD won't be able to send WM_CLOSE to jobs\n");
	}
	else {
		args.AppendArg("-K");
		args.AppendArg(softkill_path);
		free(softkill_path);
	}
#endif

#if defined(LINUX)
	// enable group-based tracking if a group ID range is given in the
	// config file
	//
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		if (!can_switch_ids()) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root");
		}
		int min_tracking_gid = param_integer("MIN_TRACKING_GID", 0);
		if (min_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, "
			           "but MIN_TRACKING_GID is %d",
			       min_tracking_gid);
		}
		int max_tracking_gid = param_integer("MAX_TRACKING_GID", 0);
		if (max_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, "
			           "but MAX_TRACKING_GID is %d",
			       max_tracking_gid);
		}
		if (min_tracking_gid > max_tracking_gid) {
			EXCEPT("invalid tracking gid range: %d - %d",
			       min_tracking_gid,
			       max_tracking_gid);
		}
		args.AppendArg("-G");
		args.AppendArg(std::to_string(min_tracking_gid));
		args.AppendArg(std::to_string(max_tracking_gid));
	}
#endif

	// done constructing the argument list; now register a reaper for
	// notification when the procd exits
	//
	if (m_reaper_id == FALSE) {
		m_reaper_id = daemonCore->Register_Reaper(
			"condor_procd reaper",
			(ReaperHandlercpp)&ProcFamilyProxyReaperHelper::procd_reaper,
			"condor_procd reaper",
			m_reaper_helper
		);
	}
	if (m_reaper_id == FALSE) {
		dprintf(D_ALWAYS,
		        "start_procd: unable to register a reaper for the procd\n");
		return false;
	}

	// we start the procd with a pipe coming back to us on its stderr.
	// the procd will close this pipe after it starts listening for
	// commands.
	//
	int pipe_ends[2];
	if (daemonCore->Create_Pipe(pipe_ends) == FALSE) {
		dprintf(D_ALWAYS, "start_procd: error creating pipe for the procd\n");
		return false;
	}
	int std_io[3];
	std_io[0] = -1;
	std_io[1] = -1;
	std_io[2] = pipe_ends[1];

	// use Create_Process to start the procd
	//
	m_procd_pid = daemonCore->Create_Process(exe.c_str(),
	                                         args,
	                                         PRIV_ROOT,
	                                         m_reaper_id,
	                                         FALSE,
	                                         FALSE,
	                                         &env,
	                                         NULL,
	                                         NULL,
	                                         NULL,
	                                         std_io);
	if (m_procd_pid == FALSE) {
		dprintf(D_ALWAYS, "start_procd: unable to execute the procd\n");
		daemonCore->Close_Pipe(pipe_ends[0]);
		daemonCore->Close_Pipe(pipe_ends[1]);
		m_procd_pid = -1;
		return false;
	}

	// now close the pipe end we handed to the child and then block on the
	// pipe until it closes (which tells us the procd is listening for
	// commands)
	//
	if (daemonCore->Close_Pipe(pipe_ends[1]) == FALSE) {
		dprintf(D_ALWAYS, "error closing procd's pipe end\n");
		daemonCore->Shutdown_Graceful(m_procd_pid);
		daemonCore->Close_Pipe(pipe_ends[0]);
		m_procd_pid = -1;
		return false;
	}
	const int MAX_PROCD_ERR_LEN = 80;
	char err_msg[MAX_PROCD_ERR_LEN + 1];
	int ret = daemonCore->Read_Pipe(pipe_ends[0], err_msg, MAX_PROCD_ERR_LEN);
	if (ret != 0) {
		daemonCore->Shutdown_Graceful(m_procd_pid);
		daemonCore->Close_Pipe(pipe_ends[0]);
		m_procd_pid = -1;
		if (ret == -1) {
			dprintf(D_ALWAYS, "start_procd: error reading pipe from procd\n");
			return false;
		}
		err_msg[ret] = '\0';
		dprintf(D_ALWAYS,
		        "start_procd: error received from procd: %s\n",
		        err_msg);
		return false;
	}
	if (daemonCore->Close_Pipe(pipe_ends[0]) == FALSE) {
		dprintf(D_ALWAYS, "start_procd: error closing pipe to procd\n");
		daemonCore->Shutdown_Graceful(m_procd_pid);
		m_procd_pid = -1;
		return false;
	}

	// OK, the ProcD's up and running!
	//
	return true;
}

bool
ProcFamilyProxy::stop_procd()
{
	//dprintf(D_ALWAYS, "stop_procd(). pid = %d, former_pid=%d\n", m_procd_pid, m_former_procd_pid);

	bool response=false;
	if (!m_client->quit(response)) {
		dprintf(D_ALWAYS, "error telling ProcD to exit\n");
	}

	// remember the procd pid before we overwrite it
	// so that we can quietly ignore a subsequent request to unregister it from daemoncore
	if (m_procd_pid != -1) {
		m_former_procd_pid = m_procd_pid;
	}

	// set m_procd_pid back to -1 so the reaper expects to see
	// the ProcD exit
	//
	m_procd_pid = -1;
	//dprintf(D_ALWAYS, "return %d from stop_procd() pid = %d, former_pid=%d\n", response, m_procd_pid, m_former_procd_pid);
	return response;
}

void
ProcFamilyProxy::recover_from_procd_error()
{
	if (!param_boolean("RESTART_PROCD_ON_ERROR", true)) {
		EXCEPT("ProcD has failed");
	}

	// ditch our ProcFamilyClient object, since its broken
	//
	delete m_client;
	m_client = NULL;
	int ntries = 5;

	// if we were the one to bring up the ProcD, then we'll be the one
	// to restart it
	bool try_restart = m_procd_pid != -1;

	while (ntries > 0 && m_client == NULL) {
	
		ntries--;

		// the ProcD has failed. we know this either because communication
		// has failed or the ProcD's reaper has fired
		//
		if (try_restart) {

			dprintf(D_ALWAYS, "attempting to restart the Procd\n");
			m_procd_pid = -1;
			if (!start_procd()) {
				dprintf(D_ALWAYS, "restarting the Procd failed\n");
				// We failed to restart the procd, don't bother trying
				// to create a ProcFamilyClient
				continue;
			}
		}
		else {

			// an ancestor of ours will be restarting the ProcD shortly. just
			// sleep a second and return
			//
			// TODO: should we do this?
			//
			dprintf(D_ALWAYS,
			        "waiting a second to allow the ProcD to be restarted\n");
			sleep(1);
		}

		m_client = new ProcFamilyClient;
		ASSERT(m_client != NULL);
		if (!m_client->initialize(m_procd_addr.c_str())) {
			dprintf(D_ALWAYS,
			        "recover_from_procd_error: "
			            "error initializing ProcFamilyClient\n");
			delete m_client;
			m_client = NULL;
		}
	}

	if ( m_client == NULL ) {
		// Ran out of attempts to restart procd
		EXCEPT("unable to restart the ProcD after several tries");
	}
}

int
ProcFamilyProxy::procd_reaper(int pid, int status)
{
	if ((m_procd_pid == -1) || (pid != m_procd_pid)) {
		dprintf(D_ALWAYS,
		        "procd (pid = %d) exited with status %d\n",
		        pid,
		        status);
	}
	else {
		dprintf(D_ALWAYS,
		        "procd (pid = %d) exited unexpectedly with status %d\n",
		        pid,
		        status);
		recover_from_procd_error();
	}

	// do one-time reaping callback if one was requested.
	if (m_reaper_notify) {
		m_reaper_notify(m_reaper_notify_me, pid, status);
	}
	m_reaper_notify = NULL;

	return 0;
}
