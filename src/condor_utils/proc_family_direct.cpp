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
#include "proc_family_direct.h"
#include "killfamily.h"
#include "condor_daemon_core.h"

struct ProcFamilyDirectContainer {
	ProcFamilyDirectContainer(KillFamily *_family, int _tid) : family(_family), timer_id(_tid) {}

	std::unique_ptr<KillFamily> family;
	int         timer_id;
};

ProcFamilyDirect::ProcFamilyDirect()
{
}

ProcFamilyDirect::~ProcFamilyDirect() {
	// We don't cancel the timers we've registered. We shouldn't be
	// destructed before DaemonCore is being torn down.
}

bool
ProcFamilyDirect::register_subfamily(pid_t pid,
                                     pid_t,
                                     int snapshot_interval)
{
    DC_AUTO_RUNTIME_PROBE(__FUNCTION__,dummy);

	// create a KillFamily object according to our arguments
	//
	KillFamily* family = new KillFamily(pid, PRIV_ROOT);
	ASSERT(family != NULL);

	// register a timer with DaemonCore to take snapshots of this family
	//
	int timer_id = daemonCore->Register_Timer(2,
	                                          snapshot_interval,
	                                          (TimerHandlercpp)&KillFamily::takesnapshot,
	                                          "KillFamily::takesnapshot",
	                                          family);
	if (timer_id == -1) {
		dprintf(D_ALWAYS,
		        "failed to register snapshot timer for family of pid %u\n",
		        pid);
		delete family;
		return false;
	}

	// insert the KillFamily and timer ID into the hash table
	//
	auto [it, success] = m_table.insert({pid, ProcFamilyDirectContainer(family, timer_id)});
	if (!success) {
		dprintf(D_ALWAYS,
		        "error inserting KillFamily for pid %u into table\n",
		        pid);
		daemonCore->Cancel_Timer(timer_id);
		return false;
	}

	return true;
}

bool
ProcFamilyDirect::track_family_via_environment(pid_t pid, PidEnvID& penvid)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->setFamilyEnvironmentID(&penvid);
	return true;
}

bool
ProcFamilyDirect::track_family_via_login(pid_t pid, const char* login)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->setFamilyLogin(login);
	return true;
}

bool
ProcFamilyDirect::get_usage(pid_t pid, ProcFamilyUsage& usage, bool full)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}

	family->get_cpu_usage(usage.sys_cpu_time, usage.user_cpu_time);
	family->get_max_imagesize(usage.max_image_size);
	usage.num_procs = family->size();

	usage.percent_cpu = 0.0;
	usage.total_image_size = 0;
    usage.total_resident_set_size = 0;
#if HAVE_PSS
    usage.total_proportional_set_size = 0;
    usage.total_proportional_set_size_available = false;
#endif
	if (full) {
		pid_t* family_array;
		int family_size = family->currentfamily(family_array);
		procInfo proc_info;
		procInfo* proc_info_ptr = &proc_info;
		int status;
		int ret = ProcAPI::getProcSetInfo(family_array,
		                                  family_size,
                                          proc_info_ptr,
		                                  status);
		delete[] family_array;
		if (ret != PROCAPI_FAILURE) {
			usage.percent_cpu = proc_info.cpuusage;
			usage.total_image_size = proc_info.imgsize;
            usage.total_resident_set_size = proc_info.rssize;
#if HAVE_PSS
            usage.total_proportional_set_size = proc_info.pssize;
            usage.total_proportional_set_size_available = proc_info.pssize_available;
#endif
		}
		else {
			dprintf(D_ALWAYS,
			        "error getting full usage info for family: %u\n",
			        pid);
		}	        
	}

	return true;
}

bool
ProcFamilyDirect::signal_process(pid_t pid, int sig)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->softkill(sig);
	return true;
}

bool
ProcFamilyDirect::suspend_family(pid_t pid)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->suspend();
	return true;
}

bool
ProcFamilyDirect::continue_family(pid_t pid)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->resume();
	return true;
}

bool
ProcFamilyDirect::kill_family(pid_t pid)
{
	KillFamily* family = lookup(pid);
	if (family == NULL) {
		return false;
	}
	family->hardkill();
	return true;
}

bool
ProcFamilyDirect::unregister_family(pid_t pid)
{
	auto it = m_table.find(pid);
	if (it == m_table.end()) {
		dprintf(D_ALWAYS,
		        "ProcFamilyDirect: no family registered for pid %u\n",
		        pid);
		return false;
	}

	// cancel the family's snapshot timer
	//
	daemonCore->Cancel_Timer(it->second.timer_id);

	// delete the family object and the container
	//
	m_table.erase(it);

	return true;
}

KillFamily*
ProcFamilyDirect::lookup(pid_t pid)
{
	auto it = m_table.find(pid);
	if (it == m_table.end()) {
		dprintf(D_ALWAYS, "ProcFamilyDirect: no family for pid %u\n", pid);
		return NULL;
	}
	return it->second.family.get();
}
