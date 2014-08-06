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
#include "../condor_privsep/condor_privsep.h"
#include "procd_config.h"

// enable PROCAPI profileing code.
#define PROFILE_PROCAPI

// this class is just used to forward reap events to the real
// ProcFamilyProxy object; we do this in a separate class to
// avoid multiple inheritance (would that even work?) since DC
// C++ callbacks need to be in a class derived from Service
//
class ProcFamilyProxyReaperHelper : public Service {

public:
	ProcFamilyProxyReaperHelper(ProcFamilyProxy* ptr) : m_ptr(ptr) { }

	int procd_reaper(int pid, int status)
	{
		return m_ptr->procd_reaper(pid, status);
	}

	ProcFamilyProxy* m_ptr;
};

int ProcFamilyProxy::s_instantiated = false;

ProcFamilyProxy::ProcFamilyProxy(const char* address_suffix) :
	m_procd_pid(-1),
	m_reaper_id(FALSE)
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
	MyString procd_addr_base = m_procd_addr;
	if (address_suffix != NULL) {
		m_procd_addr.formatstr_cat(".%s", address_suffix);
	}

	// see what log file (if any) the ProcD will be using if we
	// need to start one (use the address_suffix here as well to
	// avoid collisions)
	//
	char* procd_log = param("PROCD_LOG");
	if (procd_log != NULL) {
		m_procd_log = procd_log;
		free(procd_log);
		if (address_suffix != NULL) {
			m_procd_log.formatstr_cat(".%s", address_suffix);
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
		SetEnv("CONDOR_PROCD_ADDRESS_BASE", procd_addr_base.Value());
		SetEnv("CONDOR_PROCD_ADDRESS", m_procd_addr.Value());
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
	if (!m_client->initialize(m_procd_addr.Value())) {
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
ProcFamilyProxy::track_family_via_cgroup(pid_t pid, const char* cgroup)
{
	bool response;
	dprintf(D_FULLDEBUG, "track_family_via_cgroup: Tracking PID %u via cgroup %s.\n",
		pid, cgroup);
	if (!m_client->track_family_via_cgroup(pid, cgroup, response)) {
		dprintf(D_ALWAYS,
			"track_family_via_cgroup: ProcD communication error\n");
		return false;
	}
	return response;
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
	bool response;
	if (!m_client->unregister_family(pid, response)) {
		dprintf(D_ALWAYS, "unregister_subfamily: ProcD communication error\n");
		recover_from_procd_error();
	}
	return response;
}

bool
ProcFamilyProxy::use_glexec_for_family(pid_t pid, const char* proxy)
{
	// see "HACK" comment in register_subfamily for why we don't try
	// to recover from errors here
	//
	bool response;
	if (!m_client->use_glexec_for_family(pid, proxy, response)) {
		dprintf(D_ALWAYS, "use_glexec_for_family: ProcD communication error\n");
		return false;
	}
	return response;
}

bool
ProcFamilyProxy::start_procd()
{
	// we'll only start one ProcD
	//
	ASSERT(m_procd_pid == -1);

	// now, we build up an ArgList for the procd
	//
	MyString exe;
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

	// the (optional) procd log file
	//
	if (m_procd_log.Length() > 0) {
		args.AppendArg("-L");
		args.AppendArg(m_procd_log);
	}
	
	// the (optional) procd log file size
	//
	char *procd_log_size = param("MAX_PROCD_LOG");
	if (procd_log_size != NULL) {
		args.AppendArg("-R");
		args.AppendArg(procd_log_size);
		free(procd_log_size);
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
	args.AppendArg(get_condor_uid());
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
		if (!can_switch_ids() && !privsep_enabled()) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root or using PrivSep");
		}
		int min_tracking_gid = param_integer("MIN_TRACKING_GID", 0);
		if (min_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, "
			           "but MIN_TRACKING_GID is %d\n",
			       min_tracking_gid);
		}
		int max_tracking_gid = param_integer("MAX_TRACKING_GID", 0);
		if (max_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, "
			           "but MAX_TRACKING_GID is %d\n",
			       max_tracking_gid);
		}
		if (min_tracking_gid > max_tracking_gid) {
			EXCEPT("invalid tracking gid range: %d - %d\n",
			       min_tracking_gid,
			       max_tracking_gid);
		}
		args.AppendArg("-G");
		args.AppendArg(min_tracking_gid);
		args.AppendArg(max_tracking_gid);
	}
#endif

	// for the GLEXEC_JOB feature, we'll need to pass the ProcD paths
	// to glexec and the condor_glexec_kill script
	//
	if (param_boolean("GLEXEC_JOB", false)) {
		args.AppendArg("-I");
		char* libexec = param("LIBEXEC");
		if (libexec == NULL) {
			EXCEPT("GLEXEC_JOB is defined, but LIBEXEC not configured");
		}
		MyString glexec_kill;
		glexec_kill.formatstr("%s/condor_glexec_kill", libexec);
		free(libexec);
		args.AppendArg(glexec_kill.Value());
		char* glexec = param("GLEXEC");
		if (glexec == NULL) {
			EXCEPT("GLEXEC_JOB is defined, but GLEXEC not configured");
		}
		args.AppendArg(glexec);
		free(glexec);
		int glexec_retries = param_integer("GLEXEC_RETRIES",3,0);
		int glexec_retry_delay = param_integer("GLEXEC_RETRY_DELAY",5,0);
		args.AppendArg(glexec_retries);
		args.AppendArg(glexec_retry_delay);
	}

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
	if (privsep_enabled()) {
		m_procd_pid = privsep_spawn_procd(exe.Value(),
		                                  args,
		                                  std_io,
		                                  m_reaper_id);
	}
	else {
		m_procd_pid = daemonCore->Create_Process(exe.Value(),
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
	}
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

void
ProcFamilyProxy::stop_procd()
{
	bool response;
	if (!m_client->quit(response)) {
		dprintf(D_ALWAYS, "error telling ProcD to exit\n");
	}

	// set m_procd_pid back to -1 so the reaper expects to see
	// the ProcD exit
	//
	m_procd_pid = -1;
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

	while (ntries > 0 && m_client == NULL) {
	
		ntries--;

		// the ProcD has failed. we know this either because communication
		// has failed or the ProcD's reaper has fired
		//
		if (m_procd_pid != -1) {

			// we were the one to bring up the ProcD, so we'll be the one
			// to restart it
			//
			dprintf(D_ALWAYS, "attempting to restart the Procd\n");
			m_procd_pid = -1;
			if (!start_procd()) {
				EXCEPT("unable to start the ProcD");
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
		if (!m_client->initialize(m_procd_addr.Value())) {
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

	return 0;
}
