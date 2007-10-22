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
#include "condor_debug.h"
#include "condor_config.h"
#include "proc_family_proxy.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "../condor_procd/proc_family_client.h"
#include "setenv.h"
#include "directory.h"
#include "basename.h"
#include "../condor_privsep/condor_privsep.h"

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

ProcFamilyProxy::ProcFamilyProxy(char* address_suffix) :
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
	char* procd_addr = param("PROCD_ADDRESS");
	if (procd_addr != NULL) {
		m_procd_addr = procd_addr;
		free(procd_addr);
	}
	else {
		// setup a good default for PROCD_ADDRESS.
#ifdef WIN32
		// Win32
		//
		m_procd_addr = "//./pipe/procd_pipe";
#else
		// Unix - default to $(LOCK)/procd_pipe
		//
		char *lockdir = param("LOCK");
		if (!lockdir) {
			lockdir = param("LOG");
		}
		if (!lockdir) {
			EXCEPT("PROCD_ADDRESS not defined in configuration");
		}
		char *temp = dircat(lockdir,"procd_pipe");
		ASSERT(temp);
		m_procd_addr = temp;
		free(lockdir);
		delete [] temp;
#endif
	}

	// if we were handed a non-NULL address_suffix argument, tack
	// it on. this is meant so that if we are in a situation where
	// multiple daemons want to start ProcDs and they have the same
	// setting for PROCD_ADDRESS, the ProcDs won't attempt to use
	// the same "command pipe" (which would cause one of them to
	// fail)
	//
	MyString procd_addr_base = m_procd_addr;
	if (address_suffix != NULL) {
		m_procd_addr.sprintf_cat(".%s", address_suffix);
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
			m_procd_log.sprintf_cat(".%s", address_suffix);
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
ProcFamilyProxy::register_subfamily_child(pid_t root_pid,
                                          pid_t watcher_pid,
                                          int max_snapshot_interval,
                                          PidEnvID* penvid,
                                          const char* login)
{
	// HACK: we treat this call specially, since it is only called
	// from forked children on UNIX. this means that if we were to
	// try to restart the ProcD we would update the ProcD-related
	// information in the child, and the parent wouldn't see the
	// changes. therefore, we consider it an error if communication
	// with the ProcD fails

	bool response;
	if (!m_client->register_subfamily(root_pid,
	                                  watcher_pid,
	                                  max_snapshot_interval,
	                                  penvid,
	                                  login,
	                                  response))
	{
		dprintf(D_ALWAYS, "register_subfamily: ProcD communication error\n");
		return false;
	}
	return response;
}

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
ProcFamilyProxy::start_procd()
{
	// we'll only start one ProcD
	//
	ASSERT(m_procd_pid == -1);

	// the ProcD's "root pid" (a.k.a. the daddy pid) is going to be us
	//
	pid_t root_pid = daemonCore->getpid();

	// now, we build up an ArgList for the procd
	//
	MyString exe;
	ArgList args;

	// path to the executable
	//
	char* path = param("PROCD");
	if (path == NULL) {
		// Setup a default of PROCD=$(SBIN)/condor_procd
		char *binpath = param("SBIN");
		if (!binpath) {
			binpath = param("BIN");
		}
		if ( binpath ) {
#if defined(WIN32)
			char *temp = dircat(binpath,"condor_procd.exe");
#else
			char *temp = dircat(binpath,"condor_procd");
#endif
			ASSERT(temp);
				// Note: temp allocated with new char[]; we want
				// path to be allocated with malloc.
			path = strdup(temp);
			free(binpath);
			delete [] temp;
		}
		if ( path == NULL ) {
			dprintf(D_ALWAYS, "start_procd: PROCD not defined in configuration\n");
			return false;
		}
	}
	exe = path;
	args.AppendArg(condor_basename(path));
	free(path);

	// the procd's address
	//
	args.AppendArg("-A");
	args.AppendArg(m_procd_addr);

	// PID and birthday of the procd's "root process" (which is us)
	//
	MyString root_pid_arg;
	root_pid_arg.sprintf("%u", root_pid);
	int ignored;
	procInfo* pi = NULL;
	priv_state priv = set_root_priv();
	if (ProcAPI::getProcInfo(root_pid, pi, ignored) != PROCAPI_SUCCESS) {
		dprintf(D_ALWAYS,
		        "start_procd: unable to get own process information\n");
		set_priv(priv);
		return false;
	}
	set_priv(priv);
	MyString root_birthday_arg;
	root_birthday_arg.sprintf(PROCAPI_BIRTHDAY_FORMAT, pi->birthday);
	delete pi;
	args.AppendArg("-P");
	args.AppendArg(root_pid_arg.Value());
	args.AppendArg(root_birthday_arg.Value());

	// the (optional) procd log file
	//
	if (m_procd_log.Length() > 0) {
		args.AppendArg("-L");
		args.AppendArg(m_procd_log);
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
	if (softkill_path == NULL) {
		// Setup a default of $(SBIN)/condor_softkill.exe
		char *binpath = param("SBIN");
		if (!binpath) {
			binpath = param("BIN");
		}
		if ( binpath ) {
			char *temp = dircat(binpath,"condor_softkill.exe");
			ASSERT(temp);
				// Note: temp allocated with new char[]; we want
				// path to be allocated with malloc.
			softkill_path = strdup(temp);
			free(binpath);
			delete [] temp;
		}
	}

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
		                                         NULL,
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
	if (!param_boolean("RESTART_PROCD_ON_ERROR", false)) {
		EXCEPT("ProcD has failed");
	}

	// ditch our ProcFamilyClient object, since its broken
	//
	delete m_client;
	m_client = NULL;

	while (m_client == NULL) {
	
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
