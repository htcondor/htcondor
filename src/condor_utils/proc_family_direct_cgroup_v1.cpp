/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "directory.h"
#include "proc_family_direct_cgroup_v1.h"
#include <numeric>
#include <array>

#include <filesystem>
#include <sys/eventfd.h>
#include <sys/sysmacros.h> // for device major/minor

namespace stdfs = std::filesystem;

static std::map<pid_t, std::string> cgroup_map;
static std::map<pid_t, int> cgroup_eventfd_map;

static stdfs::path cgroup_mount_point() {
	return "/sys/fs/cgroup";
}

// The controllers we use.  We manipulate and measure all
// of these.
static std::array<const std::string, 4> controllers {
		"memory", "cpu,cpuacct", "freezer", "devices"};

// Note that even root can't remove control group *files*,
// only directories.
void fullyRemoveCgroup(const stdfs::path &absCgroup) {

	// Common case, it shouldn't still exist, we're done
	if (!stdfs::exists(absCgroup)) {
		return;
	}

	// Otherwise, we need to depth-firstly remove any subdirs
	std::error_code ec;
	for (auto const& entry: std::filesystem::directory_iterator{absCgroup, ec}) {
		if (entry.is_directory()) {
			// Must go depth-first
			fullyRemoveCgroup(absCgroup / entry);
			int r = rmdir((absCgroup / entry).c_str());
			if ((r < 0) && (errno != ENOENT))  {
				dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1 error removing cgroup %s: %s\n", (absCgroup / entry).c_str(), strerror(errno));
			} else {
				dprintf(D_FULLDEBUG, "ProcFamilyDirect removed old cgroup %s\n", (absCgroup / entry).c_str());
			}
		}
	}
	// Finally, remove "."
	int r = rmdir(absCgroup.c_str());
	if ((r < 0) && (errno != ENOENT))  {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1 error removing cgroup %s: %s\n", absCgroup.c_str(), strerror(errno));
	} else {
		dprintf(D_FULLDEBUG, "ProcFamilyDirect removed old cgroup %s\n", absCgroup.c_str());
	}
}

// Make the cgroup.  We call this before the fork, so if we fail, we can try
// to manage processes via some other means.
static bool
makeCgroupV1(const std::string &cgroup_name) {
	dprintf(D_FULLDEBUG, "Creating cgroup %s\n", cgroup_name.c_str());
	TemporaryPrivSentry sentry( PRIV_ROOT );

	// Start from the root of the cgroup mount point
	stdfs::path cgroup_root_dir = cgroup_mount_point();

	for (const std::string &controller: controllers) {
		stdfs::path absolute_cgroup = cgroup_root_dir / stdfs::path(controller) / cgroup_name;

		// If the full cgroup exists, remove it to clear the various
		// peak statistics and any existing memory
		fullyRemoveCgroup(absolute_cgroup);

		bool can_make_cgroup_dir = mkdir_and_parents_if_needed(absolute_cgroup.c_str(), 0755, 0755, PRIV_ROOT);
		if (!can_make_cgroup_dir) {
			dprintf(D_ALWAYS, "Cannot mkdir %s, failing to use cgroups\n", absolute_cgroup.c_str());
			return false;
		}
	}
	return true;
}

void 
ProcFamilyDirectCgroupV1::assign_cgroup_for_pid(pid_t pid, const std::string &cgroup_name) {
	auto [it, success] = cgroup_map.emplace(pid, cgroup_name);
	if (!success) {
		EXCEPT("Couldn't insert into cgroup map, duplicate?");
	}
}

bool 
ProcFamilyDirectCgroupV1::register_subfamily_before_fork(FamilyInfo *fi) {

	bool success = false;

	if (fi->cgroup) {
		// Hopefully, if we can make the cgroup, we will be able to use it
		// in the child process
		success = makeCgroupV1(fi->cgroup);
	}

	return success;
}

//

// mkdir the cgroup, and all required interior cgroups.
bool 
ProcFamilyDirectCgroupV1::cgroupify_process(const std::string &cgroup_name, pid_t pid) {

	TemporaryPrivSentry sentry( PRIV_ROOT );

	// Start from the root of the cgroup mount point
	stdfs::path cgroup_root_dir = cgroup_mount_point();

	for (const std::string &controller: controllers) {
		stdfs::path absolute_cgroup = cgroup_root_dir / stdfs::path(controller) / cgroup_name;

		// Move pid to the leaf of the newly-created tree
		stdfs::path procs_filename = absolute_cgroup / "cgroup.procs";
		int fd = open(procs_filename.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			formatstr(buf, "%u", pid);
			int r = write(fd, buf.c_str(), strlen(buf.c_str()));
			if (r < 0) {
				dprintf(D_ALWAYS, "Error writing procid %d to %s: %s\n", pid, procs_filename.c_str(), strerror(errno));
				close(fd);
				return false;
			} else {
				dprintf(D_ALWAYS, "Moved process %d to cgroup %s\n", pid, absolute_cgroup.c_str());
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error opening %s: %s\n", procs_filename.c_str(), strerror(errno));
			return false;
		}
	}


	// Set limits, if any
	if (cgroup_memory_limit > 0) {
		// write memory limits
		stdfs::path memory_limits_path = cgroup_root_dir / "memory" / cgroup_name / "memory.limit_in_bytes";
		int fd = open(memory_limits_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			formatstr(buf, "%lu", cgroup_memory_limit);
			int r = write(fd, buf.c_str(), strlen(buf.c_str()));
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup memory limit of %s in cgroup %s: %s\n", buf.c_str(), memory_limits_path.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup memory limit of %lu in cgroup %s: %s\n", cgroup_memory_limit, memory_limits_path.c_str(), strerror(errno));
		}
	} else {
		dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1 not setting any cgroup memory limits\n");
	}

	// Set cpu limits, if any
	if (cgroup_cpu_shares > 0) {
		// write CPU limits
		stdfs::path cpu_shares_path = cgroup_root_dir / "cpu,cpuacct" / cgroup_name / "cpu.shares";
		int fd = open(cpu_shares_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			formatstr(buf, "%d", cgroup_cpu_shares);
			int r = write(fd, buf.c_str(), buf.length());
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup cpu weight of %d in cgroup %s: %s\n", cgroup_cpu_shares, cpu_shares_path.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup cpu weight of %d in cgroup %s: %s\n", cgroup_cpu_shares, cpu_shares_path.c_str(), strerror(errno));
		}
	}

	// Chown the leaf cgroup dirs to the non-root owner.  The control files wlil
	// still be owned by root, so the job can't change them, but they can
	// create sub-dirs, so that a glidein can manage the resources given in this
	// cgroup
	pid_t uid = get_user_uid();
	pid_t gid = get_user_gid();

	// in a personal condor, uid/gid aren't returned properly.
	if ((uid > 0) && (gid > 0)) {
		for (auto &controller: controllers) {
			int r = chown((cgroup_root_dir / controller / cgroup_name).c_str(), uid, gid);
			if (r < 0) {
				dprintf(D_FULLDEBUG, "Error chowning cgroup directory: %s to (%d.%d)\n", strerror(errno), uid, gid);
			}
		}
	}

	// Make an eventfd than we can read on job exit to see if an OOM fired
	// EFD_NONBLOCK means don't block reading it, if there are no OOMs
	int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (efd < 0) {
		dprintf(D_ALWAYS, "Cannot create eventfd for monitoring OOM: %s\n", strerror(errno));
		return false;
	}

	// get the fd to memory.oom_control to put into event_control
	stdfs::path oom_control_path = cgroup_root_dir / "memory" / cgroup_name / "memory.oom_control";
	int oomc = open(oom_control_path.c_str(), O_WRONLY, 0666);
	if (oomc < 0) {
		dprintf(D_ALWAYS, "Cannot open memory.oom_control for monitoring OOM: %s\n", strerror(errno));
		close(efd);
		return false;
	}

	// get the fd to cgroup.event_control to tell the kernel to increment oom event count in the eventfd
	stdfs::path cgroup_control_path = cgroup_root_dir / "memory" / cgroup_name / "cgroup.event_control";
	int ccp = open(cgroup_control_path.c_str(), O_WRONLY, 0666);
	if (ccp < 0) {
		dprintf(D_ALWAYS, "Cannot open memory.oom_control for monitoring OOM: %s\n", strerror(errno));
		close(efd);
		close(oomc);
		return false;
	}

	// tell the cgroup_event control the eventfd and the oom_control
	std::string two_fds;
	formatstr(two_fds, "%d %d", efd, oomc);

	int r = write(ccp, two_fds.c_str(), strlen(two_fds.c_str()));
	if (r < 0) {
		dprintf(D_ALWAYS, "Cannot write %s to  cgroup.event_control for monitoring OOM: %s\n", two_fds.c_str(), strerror(errno));
		close(efd);
		close(ccp);
		close(oomc);
		return false;
	}

	// Close the ones we don't need, keep the one eventfd we will use later
	close(ccp);
	close(oomc);

	// and save the eventfd, so we can read from it when the job exits to 
	// check for ooms
	cgroup_eventfd_map[pid] = efd;


	// Remove devices we want to make unreadable.  Note they will still appear
	// in /dev, but any access will fail, even if permission bits allow for it.
	for (dev_t dev: this->cgroup_hide_devices) {
		stdfs::path cgroup_device_deny_path = cgroup_root_dir / "devices" / cgroup_name / "devices.deny";
		int devd = open(cgroup_device_deny_path.c_str(), O_WRONLY, 0666);
		if (devd > 0) {
			std::string deny_command;
			formatstr(deny_command, "c %d:%d rwm", major(dev), minor(dev));
			dprintf(D_ALWAYS, "Cgroupv1 hiding device with %s\n", deny_command.c_str());
			int r = write(devd, deny_command.c_str(), deny_command.length());
			if (r < 0) {
				dprintf(D_ALWAYS, "Cgroupv1 hiding device write failed with %d\n",errno);
			}

			close(devd);
		}
	}

	return true;
}

bool 
ProcFamilyDirectCgroupV1::track_family_via_cgroup(pid_t pid, FamilyInfo *fi) {

	ASSERT(fi->cgroup);

	std::string cgroup_name   = fi->cgroup;
	this->cgroup_memory_limit = fi->cgroup_memory_limit;
	this->cgroup_cpu_shares   = fi->cgroup_cpu_shares;
	this->cgroup_hide_devices = fi->cgroup_hide_devices;

	auto [it, success] = cgroup_map.insert(std::make_pair(pid, cgroup_name));
	if (!success) {
		ASSERT("Couldn't insert into cgroup map, duplicate?");
	}

	fi->cgroup_active = cgroupify_process(cgroup_name, pid);
	return fi->cgroup_active;
}

#ifndef USER_HZ
#define USER_HZ (100)
#endif

bool
ProcFamilyDirectCgroupV1::get_usage(pid_t pid, ProcFamilyUsage& usage, bool /*full*/)
{
	// DaemonCore uses "get_usage(getpid())" to test the procd, ignoring the usage
	// even if we haven't registered that pid as a subfamily.  Or even if there
	// is a procd.

	if (pid == getpid()) {
		return true;
	}

	std::string cgroup_name = cgroup_map[pid];

	// Initialize the ones we don't set to -1 to mean "don't know".
	usage.block_reads = usage.block_writes = usage.block_read_bytes = usage.block_write_bytes = usage.m_instructions = -1;
	usage.io_wait = -1.0;
	usage.total_proportional_set_size_available = false;
	usage.total_proportional_set_size = 0;

	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path leaf            = cgroup_root_dir / "cpu,cpuacct" / cgroup_name;
	stdfs::path cpu_stat        = leaf / "cpuacct.stat";

	// Get cpu statistics from cpuacct.stat  Format is
	//
	// cpuacct.stat:
	// user 8691663872
	// system 7246556025

	FILE *f = fopen(cpu_stat.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::get_usage cannot open %s: %d %s\n", cpu_stat.c_str(), errno, strerror(errno));
		return false;
	}

	// These are in USER_HZ, which is always (?) 100 hz
	uint64_t user_hz = 0;
	uint64_t sys_hz  = 0;

	char word[128]; // max size of a word in cpu.stat
	while (fscanf(f, "%s", word) != EOF) {
		// if word is usage_usec
		if (strcmp(word, "user") == 0) {
			// next word is the user time in microseconds
			if (fscanf(f, "%ld", &user_hz) != 1) {
				dprintf(D_ALWAYS, "Error reading user_usec field out of cpu.stat\n");
				fclose(f);
				return false;
			}
		}
		if (strcmp(word, "system") == 0) {
			// next word is the system time in microseconds
			if (fscanf(f, "%ld", &sys_hz) != 1) {
				dprintf(D_ALWAYS, "Error reading system_usec field out of cpu.stat\n");
				fclose(f);
				return false;
			}
		}
	}
	fclose(f);

	time_t wall_time = time(nullptr) - start_time;
	usage.percent_cpu = double(user_hz + sys_hz) / double((wall_time * USER_HZ));

	usage.user_cpu_time = user_hz / USER_HZ; // usage.user_cpu_times in seconds, ugh
	usage.sys_cpu_time  =  sys_hz / USER_HZ; //  usage.sys_cpu_times in seconds, ugh

	stdfs::path memory_current = cgroup_root_dir / "memory" / cgroup_name / "memory.usage_in_bytes";
	stdfs::path memory_peak   = cgroup_root_dir / "memory" / cgroup_name / "memory.max_usage_in_bytes";

	f = fopen(memory_current.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::get_usage cannot open %s: %d %s\n", memory_current.c_str(), errno, strerror(errno));
		return false;
	}

	uint64_t memory_current_value = 0;
	if (fscanf(f, "%ld", &memory_current_value) != 1) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::get_usage cannot read %s: %d %s\n", memory_current.c_str(), errno, strerror(errno));
		fclose(f);
		return false;
	}
	fclose(f);

	uint64_t memory_peak_value = 0;

	f = fopen(memory_peak.c_str(), "r");
	if (!f) {
		// Some cgroup v1 versions don't have this file
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::get_usage cannot open %s: %d %s\n", memory_peak.c_str(), errno, strerror(errno));
	} else {
		if (fscanf(f, "%ld", &memory_peak_value) != 1) {

			// But this error should never happen
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::get_usage cannot read %s: %d %s\n", memory_peak.c_str(), errno, strerror(errno));
			fclose(f);
			return false;
		}
		fclose(f);
	}

	// usage is in kbytes.  cgroups reports in bytes
	usage.total_image_size = usage.total_resident_set_size = (memory_current_value / 1024);

	// Sanity check if system is missing memory.peak file
	if (memory_current_value > memory_peak_value) {
		memory_peak_value = memory_current_value;
	}

	// More sanity checking to latch memory high water mark
	if ((memory_peak_value / 1024) > usage.max_image_size) {
		usage.max_image_size = memory_peak_value / 1024;
	}

	return true;
}

// Note that in cgroup v1, cgroup.procs contains only those processes in
// this direct cgroup, and does not contain processes in any descendent
// cgroup (except the / croup, which does).
bool
ProcFamilyDirectCgroupV1::signal_process(pid_t pid, int sig)
{
	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1::signal_process for %u sig %d\n", pid, sig);

	std::string cgroup_name = cgroup_map[pid];

	pid_t me = getpid();
	stdfs::path procs = cgroup_mount_point() / "memory" / stdfs::path(cgroup_name) / stdfs::path("cgroup.procs");

	TemporaryPrivSentry sentry(PRIV_ROOT);
	FILE *f = fopen(procs.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::signal_process cannot open %s: %d %s\n", procs.c_str(), errno, strerror(errno));
		return false;
	}
	pid_t victim_pid;
	while (fscanf(f, "%d", &victim_pid) != EOF) {
		if (pid != me) { // just in case
			kill(victim_pid, sig);
		}
	}
	fclose(f);
	return true;
}

// Writing FREEZE to freeze freezes all the processes in this cgroup
// and also freezes all descendent cgroups. 
bool
ProcFamilyDirectCgroupV1::suspend_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1::suspend for pid %u for root pid %u in cgroup %s\n", 
			pid, family_root_pid, cgroup_name.c_str());

	stdfs::path freezer = cgroup_mount_point() / "freezer" / stdfs::path(cgroup_name) / "freezer.state";
	bool success = true;
	TemporaryPrivSentry sentry( PRIV_ROOT );
	int fd = open(freezer.c_str(), O_WRONLY, 0666);
	if (fd >= 0) {

		const static char *frozen = "FROZEN"; // Let it go...
		ssize_t result = write(fd, frozen, strlen(frozen));
		if (result < 0) {
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::suspend_family error %d (%s) writing to cgroup.freeze\n", errno, strerror(errno));
			success = false;
		}
		close(fd);
	} else {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::suspend_family error %d (%s) opening cgroup.freeze\n", errno, strerror(errno));
		success = false;
	}
	return success;
}

bool
ProcFamilyDirectCgroupV1::continue_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];
	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1::continue for pid %u for root pid %u in cgroup %s\n", 
			pid, family_root_pid, cgroup_name.c_str());

	stdfs::path freezer = cgroup_mount_point() / "freezer" / stdfs::path(cgroup_name) / "freezer.state";
	bool success = true;
	TemporaryPrivSentry sentry( PRIV_ROOT );
	int fd = open(freezer.c_str(), O_WRONLY, 0666);
	if (fd >= 0) {

		const static char *thawed = "THAWED";
		ssize_t result = write(fd, thawed, strlen(thawed));
		if (result < 0) {
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::continue_family error %d (%s) writing to cgroup.freeze\n", errno, strerror(errno));
			success = false;
		}
		close(fd);
	} else {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV1::continue_family error %d (%s) opening cgroup.freeze\n", errno, strerror(errno));
		success = false;
	}

	return success;
}

bool
ProcFamilyDirectCgroupV1::kill_family(pid_t pid)
{

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1::kill_family for pid %u\n", pid);

	// Suspend the whole cgroup first, so that all processes are atomically
	// killed
	this->suspend_family(pid);
	this->signal_process(pid, SIGKILL);
	this->continue_family(pid);
	return true;
}

// Note: DaemonCore doesn't call this from the starter, because
// the starter exits from the JobReaper, and dc call this after
// calling the reaper.
	bool
ProcFamilyDirectCgroupV1::unregister_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV1::unregister_family for pid %u\n", pid);
	// Remove this cgroup, so that we clear the various peak statistics it holds

	TemporaryPrivSentry sentry(PRIV_ROOT);
	for (auto &controller : controllers) {
		fullyRemoveCgroup(cgroup_mount_point() / controller / cgroup_name);
	}
	return true;
}

bool 
ProcFamilyDirectCgroupV1::has_been_oom_killed(pid_t pid) {
	bool killed = false;

	// reading 8 bytes from the eventfd will result in a non-zero
	// oom count, meaning we got oom killed, or -1 with EAGAIN
	// if it is zero
	if (cgroup_eventfd_map.contains(pid)) {
		int efd = cgroup_eventfd_map[pid];
		int64_t oom_count = 0;
		int r = read(efd, &oom_count, sizeof(oom_count));
		if (r < 0) {
		dprintf(D_FULLDEBUG, "reading from eventfd oom returns -1: %s\n", strerror(errno));
		}
		killed = oom_count > 0;

		cgroup_eventfd_map.erase(efd);
		close(efd);
	}

	return killed;
}

// Returns true if cgroup v1 is mounted
bool 
ProcFamilyDirectCgroupV1::has_cgroup_v1() {
	bool found = false;
	stdfs::path mount_point = cgroup_mount_point();

	// if memory exists in the root, then we are
	// have at least some cgroup v1

	// Don't bother to check with elevated privileges
	stdfs::path cgroup_root_procs = mount_point / "memory";
	std::error_code ec;
	if (stdfs::exists(cgroup_root_procs, ec)) {
		found = true;
	}
	return found;
}

static bool cgroup_controller_is_writeable(const std::string &controller, std::string relative_cgroup) {

	if (relative_cgroup.length() == 0) {
		return false;
	}

	// In Cgroup v1, need to test each controller separately
	// For cgroup v2, controller will be empty string, but that's OK.
	std::string test_path = cgroup_mount_point();
	test_path += '/';

	if (!controller.empty()) {
		// cgroup v1 with controller at root
		test_path += controller + '/';
	} 

	// Regardless of v1 or v2, the relative cgroup at the end
	test_path += relative_cgroup;

	// The relative path given might not completly exist.  We can write
	// to it if we can write to the fully given path (the usual case)
	// OR, if we have write power to the parent of the top-most non-existing
	// directory.

	{
		TemporaryPrivSentry sentry(PRIV_ROOT); // Test with all our powers

		if (access(test_path.c_str(), R_OK | W_OK) == 0) {
			dprintf(D_ALWAYS, "    Cgroup %s/%s is useable\n", controller.c_str(), relative_cgroup.c_str());
			return true;
		}
	}

	// The directory doesn't exist.  See if we can write to the parent.
	if ((errno == ENOENT) && (relative_cgroup.length() > 1))  {
		size_t trailing_slash = relative_cgroup.find_last_of('/');
		if (trailing_slash == std::string::npos) {
			relative_cgroup = '/'; // last try from the root of the mount point
		} else {
			relative_cgroup.resize(trailing_slash); // Retry one directory up
		}
		return cgroup_controller_is_writeable(controller, relative_cgroup);

	}
	
	dprintf(D_ALWAYS, "    Cgroup %s/%s is not writeable, cannot use cgroups\n", controller.c_str(), relative_cgroup.c_str());
	return false;
}

static bool cgroup_v1_is_writeable(const std::string &relative_cgroup) {
	return 
		// These should be synchronized to the required_controllers in the procd
		cgroup_controller_is_writeable("memory", relative_cgroup)     &&
		cgroup_controller_is_writeable("cpu,cpuacct", relative_cgroup) &&
		cgroup_controller_is_writeable("freezer", relative_cgroup);
}

bool 
ProcFamilyDirectCgroupV1::can_create_cgroup_v1(std::string &cgroup) {
	if (!has_cgroup_v1()) {
		return false;
	}

	// see if cgroup is writeable
	return cgroup_v1_is_writeable(cgroup);
}
