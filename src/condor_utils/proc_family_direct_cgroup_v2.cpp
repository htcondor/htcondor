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
#include "directory.h"
#include "proc_family_direct_cgroup_v2.h"
#include "condor_config.h"
#include <numeric>
#include <algorithm>

#include <filesystem>
#include <charconv>

// For bpf device hiding
#include <linux/bpf.h>
#include <sys/syscall.h>

// for major/minor
#include <sys/sysmacros.h>

// Only for V23.0
#undef HAS_CGROUP_DEVICE
#ifdef HAS_CGROUP_DEVICE
// The missing bpf syscall wrapper
static int bpf(enum bpf_cmd cmd, union bpf_attr *attr, unsigned int size)
{
    return syscall(__NR_bpf, cmd, attr, size);
}
#endif

namespace stdfs = std::filesystem;

static std::map<pid_t, std::string> cgroup_map;
static std::vector<pid_t> lifetime_extended_pids;

static stdfs::path cgroup_mount_point() {
	return "/sys/fs/cgroup";
}

// given a relative cgroup name, send a signal
// to every process in exactly that cgroup (but 
// not sub-cgroups thereof)
static bool
signal_cgroup(const std::string &cgroup_name, int sig) {

	pid_t me = getpid();
	stdfs::path procs = cgroup_mount_point() / stdfs::path(cgroup_name) / stdfs::path("cgroup.procs");

	TemporaryPrivSentry sentry(PRIV_ROOT);
	FILE *f = fopen(procs.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::signal_process cannot open %s: %d %s\n", procs.c_str(), errno, strerror(errno));
		return false;
	}
	pid_t victim_pid;
	while (fscanf(f, "%d", &victim_pid) != EOF) {
		if (victim_pid != me) { // just in case
			dprintf(D_FULLDEBUG, "cgroupv2 killing with signal %d to pid %d in cgroup %s\n", sig, victim_pid, cgroup_name.c_str());
			kill(victim_pid, sig);
		}
	}
	fclose(f);
	return true;
}

// Return a vector of the absolute paths to all the
// cgroup directories rooted here.
static std::vector<stdfs::path> getTree(std::string cgroup_name) {
	std::vector<stdfs::path> dirs;

	// First add the root of the tree, if it exists
	std::error_code ec;
	if (!stdfs::exists(cgroup_mount_point() / cgroup_name, ec)) {
		// even the root of our tree doesn't exit, return empty vector
		return std::vector<stdfs::path>();
	}

	// Now add the root, as the iterator below doesn't include it
	dirs.emplace_back(cgroup_mount_point() / cgroup_name);

	// append all directories from here on down
	for (const auto& entry: stdfs::recursive_directory_iterator{cgroup_mount_point() / cgroup_name, ec}) {
		if (stdfs::is_directory(entry)) {
			dirs.emplace_back(entry);
		}	
	}

	auto deepest_first = [](const stdfs::path &p1, const stdfs::path &p2) {
		// Sort by length first, so deepest path sorts first
		if (p1.string().length() != p2.string().length()) {
			return p1.string().length() > p2.string().length();
		}

		// otherwise we don't care, just lexi
		return p1.string() > p2.string();
	};

	// Sort them so deeper (longer) dirs come first
	std::sort(dirs.begin(), dirs.end(), deepest_first);

	return dirs;
}

// given a relative cgroup name, return the 
// number of processes in the cgroup
static int
processesInCgroup(const std::string &cgroup_name) {
	int count = 0;
	stdfs::path procs = cgroup_mount_point() / stdfs::path(cgroup_name) / stdfs::path("cgroup.procs");

	TemporaryPrivSentry sentry(PRIV_ROOT);
	FILE *f = fopen(procs.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::processesInCgroup cannot open %s: %d %s\n", procs.c_str(), errno, strerror(errno));
		return -1;
	}
	pid_t some_pid;
	while (fscanf(f, "%d", &some_pid) != EOF) {
		count++;
	}
	fclose(f);
	return count;
}

static bool killCgroupTree(const std::string &cgroup_name) {
	TemporaryPrivSentry sentry(PRIV_ROOT);

	//
	// cgroup.kill is a new addition to cgroupv2, and doesn't exist on el9.  It is very useful, though:
	// writing a '1' to it sends a kill -9 to all processes in this cgroup, and all processes in all
	// sub-cgroups.  Let's try to use it, and also try the old-fashioned way.

	stdfs::path kill_path = cgroup_mount_point() / stdfs::path(cgroup_name) / stdfs::path("cgroup.kill");
	FILE *f = fopen(kill_path.c_str(), "w");
	if (!f) {
		// Could be it just doesn't exist
		if (errno != ENOENT) {
			dprintf(D_ALWAYS, "trimCgroupTree: cannot open %s: %d %s\n", kill_path.c_str(), errno, strerror(errno));
		}
	} else {
		fprintf(f, "%c", '1');
		fclose(f);
	}

	// kill -9 any processes in any cgroup in us or under us
	for (const auto& dir: getTree(cgroup_name)) {
		// getTree returns absolute paths, but signal_cgroup needs relative -- rip off the mount_point
		std::string relative_cgroup = dir.string().substr(cgroup_mount_point().string().length() + 1);
		signal_cgroup(relative_cgroup, SIGKILL);
	}

	time_t startTime = time(nullptr);

	// Repeat for up to five seconds to let zombies get reape
	while ((time(nullptr) - startTime) < 5) {
		if (processesInCgroup(cgroup_name) == 0) {
			return true;
		}
		sleep(1);
	}

	return true;
}

// given a relative cgroup name, which may or may not exist,
// remove all child cgroups.  rm -fr (or equivalent) can't
// work because the files can't be rm'd, even by root, only
// the directories.  And the directories can't be removed
// if there is an active process in that cgroup, or if there
// is a sub-cgroup.  So, first atomically kill all the
// procs in cgroups rooted at this tree, then bottom-up
// remove their directories

static bool trimCgroupTree(const std::string &cgroup_name) {

	// Kill all processes in the whole cgroup tree
	killCgroupTree(cgroup_name);

	TemporaryPrivSentry sentry(PRIV_ROOT);
	
	// Remove all the subcgroups, bottom up
	for (const auto& dir: getTree(cgroup_name)) {
		int r = rmdir(dir.c_str());
		if ((r < 0) && (errno != ENOENT)) {
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::trimCgroupTree error removing cgroup %s: %s\n", cgroup_name.c_str(), strerror(errno));
		}
	}
	return true;
}

static bool makeCgroup(const std::string &cgroup_name) {
	TemporaryPrivSentry sentry( PRIV_ROOT );

	// Start from the root of the cgroup mount point
	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path cgroup_relative_to_root_dir(cgroup_name);

	// If the full cgroup exists, remove it to clear the various
	// peak statistics and any existing memory
	trimCgroupTree(cgroup_name);

	// Accumulate along path components
	auto interior_dir_maker = [](const stdfs::path &fulldir, const stdfs::path &next_part) {
		stdfs::path interior = fulldir / next_part;
		mkdir_and_parents_if_needed(interior.c_str(), 0755, 0755, PRIV_ROOT);

		// Now that we've made our interior node
		// we need to tell it which cgroup controllers *it's* child has
		stdfs::path controller_filename = interior / "cgroup.subtree_control";
		int fd = open(controller_filename.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			// TODO: write these individually
			const char *child_controllers = "+cpu +io +memory +pids";
			int r = write(fd, child_controllers, strlen(child_controllers));
			if (r < 0) {
				dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::track_family_via_cgroup error writing to %s: %s\n", controller_filename.c_str(), strerror(errno));
			}
			close(fd);
		}
		return interior;
	};

	// cdr down the path, making all the interior nodes, skipping the last (leaf) one
	std::accumulate(cgroup_relative_to_root_dir.begin(), --cgroup_relative_to_root_dir.end(),
			cgroup_root_dir, interior_dir_maker);

	// Now the leaf cgroup
	stdfs::path leaf = cgroup_root_dir / cgroup_relative_to_root_dir;

	bool can_make_cgroup_dir = mkdir_and_parents_if_needed(leaf.c_str(), 0755, 0755, PRIV_ROOT);
	if (!can_make_cgroup_dir) {
		dprintf(D_ALWAYS, "Cannot mkdir %s, failing to use cgroups\n", leaf.c_str());
		return false;
	}

	return true;
}

// mkdir the cgroup, and all required interior cgroups.  Note that the leaf
// cgroup in v2 cannot have anything in ../cgroup_subtree_control, or else
// we can't put a process in it.  Interior nodes *must* have the controllers
// put in that file, or else we won't have any controllers to query in
// our interior nodes.
//
// Note this is called after the fork, but before the exec of the process to
// be monitored/controlled.

bool 
ProcFamilyDirectCgroupV2::cgroupify_myself(const std::string &cgroup_name) {
	pid_t pid = getpid();
	dprintf(D_FULLDEBUG, "Creating cgroup %s for pid %d\n", cgroup_name.c_str(), pid);

	TemporaryPrivSentry sentry( PRIV_ROOT );

	// Move pid to the leaf of the newly-created tree
	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path leaf = cgroup_root_dir / cgroup_name;
	stdfs::path procs_filename = leaf / "cgroup.procs";

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
			dprintf(D_ALWAYS, "Successfully moved procid %d to cgroup %s\n", pid, procs_filename.c_str());
		}
		close(fd);
	}

	// Set memory limits, if any
	if (cgroup_memory_limit > 0) {
		// write memory limits
		stdfs::path memory_limits_path = leaf / "memory.max";
		int fd = open(memory_limits_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			formatstr(buf, "%lu", cgroup_memory_limit);
			int r = write(fd, buf.data(), buf.size());
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup memory limit of %s in cgroup %s: %s\n", buf.c_str(), leaf.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup memory limit of %lu in cgroup %s: %s\n", cgroup_memory_limit, leaf.c_str(), strerror(errno));
		}
	}

	if (cgroup_memory_limit_low > 0) {
		// write memory limits
		stdfs::path memory_limits_path = leaf / "memory.low";
		int fd = open(memory_limits_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			formatstr(buf, "%lu", cgroup_memory_limit_low);
			int r = write(fd, buf.data(), buf.size());
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup low memory limit of %s in cgroup %s: %s\n", buf.c_str(), leaf.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup memory low limit of %lu in cgroup %s: %s\n", cgroup_memory_limit_low, leaf.c_str(), strerror(errno));
		}
	}

	// Set swap limits, if any
	if (cgroup_memory_and_swap_limit > 0) {
		// write memory limits
		stdfs::path memory_swap_limits_path = leaf / "memory.swap.max";
		int fd = open(memory_swap_limits_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			std::string buf;
			// cgroup.v2 memory.swap.max is swap EXclusive of ram usage, our interface
			// is INclusive
			uint64_t swap_limit;
			if (cgroup_memory_and_swap_limit < cgroup_memory_limit) {
				swap_limit = 0;
			} else {
				swap_limit = cgroup_memory_and_swap_limit - cgroup_memory_limit;
			}
			formatstr(buf, "%lu", swap_limit);
			int r = write(fd, buf.data(), buf.size());
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup swap limit of %s in cgroup %s: %s\n", buf.c_str(), leaf.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup swap limit of %lu in cgroup %s: %s\n", cgroup_memory_and_swap_limit, leaf.c_str(), strerror(errno));
		}
	}

	// Set cpu limits, if any
	if (cgroup_cpu_shares > 0) {
		// write memory limits
		stdfs::path cpu_shares_path = leaf / "cpu.weight";
		int fd = open(cpu_shares_path.c_str(), O_WRONLY, 0666);
		if (fd >= 0) {
			char buf[16];
			{ auto [p, ec] = std::to_chars(buf, buf + sizeof(buf) - 1, cgroup_cpu_shares); *p = '\0';}
			int r = write(fd, buf, strlen(buf));
			if (r < 0) {
				dprintf(D_ALWAYS, "Error setting cgroup cpu weight of %d in cgroup %s: %s\n", cgroup_cpu_shares, leaf.c_str(), strerror(errno));
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "Error setting cgroup cpu weight of %d in cgroup %s: %s\n", cgroup_cpu_shares, leaf.c_str(), strerror(errno));
		}
	}

	// If this fails, we will run without oom killing, which is unfortunate, but
	// we made it decades without this support
	stdfs::path oom_group = cgroup_mount_point() / stdfs::path(cgroup_name) / "memory.oom.group";
	fd = open(oom_group.c_str(), O_WRONLY, 0666);
	if (fd >= 0) {
		char one[1] = {'1'};
		ssize_t result = write(fd, one, 1);
		if (result < 0) {
			dprintf(D_ALWAYS, "Error enabling per-cgroup oom killing: %d (%s)\n", errno, strerror(errno));
		}
		close(fd);
	} else {
		dprintf(D_ALWAYS, "Error enabling per-cgroup oom killing: %d (%s)\n", errno, strerror(errno));
	}

	// Enable delgation.  That is, allow processes in this cgroup to make sub-cgroups within this one.
	// So, e.g. if the job is a glidein, the glidein can create sub-cgroups for each of its slots, and
	// divide up the memory, etc. resources here and apply limits.
	// Delegation requires three things.
	// 1.) cgroup directory writeable (unix permission-wise) by the user
	// 2.) cgroup.procs file writeable by the user (so they can move processes out of the interior
	//                  node and into the leaf.  Cgroupv2 requires all processes to live in the leaf nodes.
	// 3.) cgroup.subtree_control

	// none of this works if we aren't root, so avoid confusing warnings when we aren't
	if (can_switch_ids()) {
		uid_t uid = get_user_uid();
		gid_t gid = get_user_gid();

		if ( (uid != (uid_t) -1) && (gid != (gid_t) -1)) {
			int r = chown((cgroup_mount_point() / stdfs::path(cgroup_name)).c_str(), uid, gid);
			if (r < 0) {
				dprintf(D_ALWAYS, "Error chown'ing cgroup directory to user %u and group %u: %s\n", uid, gid, strerror(errno));
			}

			r = chown((cgroup_mount_point() / stdfs::path(cgroup_name) / "cgroup.procs").c_str(), uid, gid);
			if (r < 0) {
				dprintf(D_ALWAYS, "Error chown'ing cgroup.procs file to user %u and group %u: %s\n", uid, gid, strerror(errno));
			}

			r = chown((cgroup_mount_point() / stdfs::path(cgroup_name) / "cgroup.subtree_control").c_str(), uid, gid);
			if (r < 0) {
				dprintf(D_ALWAYS, "Error chown'ing cgroup.subtree_control file to user %u and group %u: %s\n", uid, gid, strerror(errno));
			}
		}
	}

	return true;
}
		
void 
ProcFamilyDirectCgroupV2::assign_cgroup_for_pid(pid_t pid, const std::string &cgroup_name) {
	auto [it, success] = cgroup_map.emplace(pid, cgroup_name);
	if (!success) {
		EXCEPT("Couldn't insert into cgroup map, duplicate?");
	}
}

bool 
ProcFamilyDirectCgroupV2::track_family_via_cgroup(pid_t pid, FamilyInfo *fi) {

	ASSERT(fi->cgroup);

	std::string cgroup_name = fi->cgroup;
	this->cgroup_memory_limit = fi->cgroup_memory_limit;
	// 23.0 this->cgroup_memory_limit_low = fi->cgroup_memory_limit_low;
	this->cgroup_memory_and_swap_limit = fi->cgroup_memory_and_swap_limit;
	this->cgroup_cpu_shares = fi->cgroup_cpu_shares;
	// 23.0 this->cgroup_hide_devices = fi->cgroup_hide_devices;

	assign_cgroup_for_pid(pid, cgroup_name);

	fi->cgroup_active = cgroupify_myself(cgroup_name);
	return fi->cgroup_active;
}

static bool
get_user_sys_cpu(const std::string &cgroup_name, uint64_t &user_usec, uint64_t &sys_usec) {

	// Just to be sure
	user_usec = 0;
	sys_usec = 0;

	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path leaf            = cgroup_root_dir / cgroup_name;
	stdfs::path cpu_stat        = leaf / "cpu.stat";

	// Get cpu statistics from cpu.stat  Format is
	//
	// cpu.stat:
	// usage_usec 8691663872
	// user_usec 1445107847
	// system_usec 7246556025

	FILE *f = fopen(cpu_stat.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot open %s: %d %s\n", cpu_stat.c_str(), errno, strerror(errno));
		return false;
	}

	char word[128]; // max size of a word in cpu.stat
	while (fscanf(f, "%127s", word) != EOF) {
		// if word is usage_usec
		if (strcmp(word, "user_usec") == 0) {
			// next word is the user time in microseconds
			if (fscanf(f, "%ld", &user_usec) != 1) {
				dprintf(D_ALWAYS, "Error reading user_usec field out of cpu.stat\n");
				fclose(f);
				return false;
			}
		}
		if (strcmp(word, "system_usec") == 0) {
			// next word is the system time in microseconds
			if (fscanf(f, "%ld", &sys_usec) != 1) {
				dprintf(D_ALWAYS, "Error reading system_usec field out of cpu.stat\n");
				fclose(f);
				return false;
			}
		}
	}
	fclose(f);
	return true;
}

bool
ProcFamilyDirectCgroupV2::get_usage(pid_t pid, ProcFamilyUsage& usage, bool /*full*/)
{
	// DaemonCore uses "get_usage(getpid())" to test the procd, ignoring the usage
	// even if we haven't registered that pid as a subfamily.  Or even if there
	// is a procd.

	if (pid == getpid()) {
		return true;
	}

	const std::string cgroup_name = cgroup_map[pid];

	// Initialize the ones we don't set to -1 to mean "don't know".
	usage.block_reads = usage.block_writes = usage.block_read_bytes = usage.block_write_bytes = usage.m_instructions = -1;
	usage.io_wait = -1.0;
	usage.total_proportional_set_size_available = false;
	usage.total_proportional_set_size = 0;

	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path leaf            = cgroup_root_dir / cgroup_name;

	uint64_t user_usec = 0;
	uint64_t sys_usec  = 0;

	bool cpu_result = get_user_sys_cpu(cgroup_name, user_usec, sys_usec);

	if (cpu_result) {
		// Bias for starting values
		user_usec -= starting_user_usec;
		sys_usec  -= starting_sys_usec;

		time_t wall_time = time(nullptr) - start_time;
		usage.percent_cpu = double(user_usec + sys_usec) / double((wall_time * 1'000'000));

		usage.user_cpu_time = user_usec / 1'000'000; // usage.user_cpu_times in seconds, ugh
		usage.sys_cpu_time  =  sys_usec / 1'000'000; //  usage.sys_cpu_times in seconds, ugh
	} else {
		usage.percent_cpu = 0.0;
		usage.user_cpu_time = 0;
		usage.sys_cpu_time  = 0;
	}

	stdfs::path cgroup_procs   = leaf / "cgroup.procs";

	FILE *f = fopen(cgroup_procs.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot open %s: %d %s\n", cgroup_procs.c_str(), errno, strerror(errno));
		return false;
	}
	char pidstr[64]; // Far beyond max size of a pid
	usage.num_procs = 0;
	while (fscanf(f, "%s\n", pidstr) == 1) {
		usage.num_procs++;
	}
	fclose(f);

	// Memory reading follows.

	// "memory.current" and "memory.peak" include many caches and kernel data structures, which we usually don't
	// want to report in the condor.  This means that we need to read the broken down fields out
	// of "memory.stat", which unfortunately, is not clamped by the kernel, so we need to poll.

	stdfs::path memory_current = leaf / "memory.current";
	stdfs::path memory_peak    = leaf / "memory.peak";
	stdfs::path memory_stat    = leaf / "memory.stat";

	f = fopen(memory_stat.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot open %s: %d %s\n", memory_stat.c_str(), errno, strerror(errno));
		return false;
	}

	uint64_t memory_stat_anon_value = 0;
	uint64_t memory_stat_shmem_value = 0;
	uint64_t memory_stat_file_mapped_value = 0;
	char line[256];
	size_t total_read = 0;
	while (fgets(line, 256, f)) {

		// "anon": Amount of memory used in anonymous mappings such as brk(), sbrk(), and mmap(MAP_ANONYMOUS)
		total_read += sscanf(line, "anon %ld", &memory_stat_anon_value);

		// "shmem": Amount of cached filesystem data that is swap-backed, such as tmpfs, shm segments, shared anonymous mmap()s
		total_read += sscanf(line, "shmem %ld", &memory_stat_shmem_value);
		if (total_read == 2) {
			break;
		}
	}
	fclose(f);

	if (total_read != 2) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot read anon and shmem from memory.stat\n");
		return false;
	}

	uint64_t memory_current_value = memory_stat_anon_value + memory_stat_shmem_value;

	uint64_t memory_peak_value = 0;

	// Peak value is incorrect if we reuse a cgroup from job to job.  This can happen when
	// a process in a job is unkillable, in which case we reuse the cgroup from the previous
	// job, and inherit whatever the peak memory was. But when we don't reuse, it is more precise
	if (param_boolean("CGROUP_USE_PEAK_MEMORY", false)) {
		f = fopen(memory_peak.c_str(), "r");
		if (!f) {
			// Some cgroup v2 versions don't have this file
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot open %s: %d %s\n", memory_peak.c_str(), errno, strerror(errno));
		} else {
			if (fscanf(f, "%ld", &memory_peak_value) != 1) {

				// But this error should never happen
				dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot read %s: %d %s\n", memory_peak.c_str(), errno, strerror(errno));
				fclose(f);
				return false;
			}
			fclose(f);
		}

		// If we read the peak memory, that include disk caches, etc. which we don't want to count
		// subtract those here.
		if (param_boolean("CGROUP_IGNORE_CACHE_MEMORY", true)) {
			f = fopen(memory_stat.c_str(), "r");
			if (!f) {
				dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot open %s: %d %s\n", memory_stat.c_str(), errno, strerror(errno));
				return false;
			}

			uint64_t memory_file_value = 0;
			uint64_t memory_inactive_anon_value = 0;
			char line[256];
			size_t total_read = 0;
			while (fgets(line, 256, f)) {
				total_read += sscanf(line, "file %ld", &memory_file_value);
				total_read += sscanf(line, "inactive_anon %ld", &memory_inactive_anon_value);
				if (total_read == 2) {
					break;
				}
			}
			fclose(f);
			if (total_read != 2) {
				dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::get_usage cannot read inactive_file or inactive_anon from %s: %d %s\n", memory_stat.c_str(), errno, strerror(errno));
				return false;
			}
			memory_current_value -= (memory_inactive_anon_value + memory_file_value);
			if (memory_current_value < 0) { 
				memory_current_value = 0; // sanity check
			}
		}
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

// Note that in cgroup v2, cgroup.procs contains only those processes in
// this direct cgroup, and does not contain processes in any descendent
// cgroup (except the / croup, which does).
	bool
ProcFamilyDirectCgroupV2::signal_process(pid_t pid, int sig)
{
	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::signal_process for %u sig %d\n", pid, sig);

	std::string cgroup_name = cgroup_map[pid];
	return signal_cgroup(cgroup_name, sig);
}

// Writing a '1' to cgroup.freeze freezes all the processes in this cgroup
// and also freezes all descendent cgroups.  Might need to poll
// cgroup.freeze until it reads '1' to verify that everyone is frozen.
	bool
ProcFamilyDirectCgroupV2::suspend_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::suspend for pid %u for root pid %u in cgroup %s\n", 
			pid, family_root_pid, cgroup_name.c_str());

	stdfs::path freezer = cgroup_mount_point() / stdfs::path(cgroup_name) / "cgroup.freeze";
	bool success = true;
	TemporaryPrivSentry sentry( PRIV_ROOT );
	int fd = open(freezer.c_str(), O_WRONLY, 0666);
	if (fd >= 0) {

		char one[1] = {'1'};
		ssize_t result = write(fd, one, 1);
		if (result < 0) {
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::suspend_family error %d (%s) writing to cgroup.freeze\n", errno, strerror(errno));
			success = false;
		}
		close(fd);
	} else {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::suspend_family error %d (%s) opening cgroup.freeze\n", errno, strerror(errno));
		success = false;
	}
	return success;
}

	bool
ProcFamilyDirectCgroupV2::continue_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];
	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::continue for pid %u for root pid %u in cgroup %s\n", 
			pid, family_root_pid, cgroup_name.c_str());

	stdfs::path freezer = cgroup_mount_point() / stdfs::path(cgroup_name) / "cgroup.freeze";
	bool success = true;
	TemporaryPrivSentry sentry( PRIV_ROOT );
	int fd = open(freezer.c_str(), O_WRONLY, 0666);
	if (fd >= 0) {

		char zero[1] = {'0'};
		ssize_t result = write(fd, zero, 1);
		if (result < 0) {
			dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::continue_family error %d (%s) writing to cgroup.freeze\n", errno, strerror(errno));
			success = false;
		}
		close(fd);
	} else {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::continue_family error %d (%s) opening cgroup.freeze\n", errno, strerror(errno));
		success = false;
	}

	return success;
}

bool
ProcFamilyDirectCgroupV2::kill_family(pid_t pid)
{
	std::string cgroup_name = cgroup_map[pid];

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::kill_family for pid %u\n", pid);

	// Suspend the whole cgroup first, so that all processes are atomically
	// killed
	this->suspend_family(pid);

	// send SIGKILL or use cgroup.kill to SIGKILL the whole tree
	killCgroupTree(cgroup_name);

	this->continue_family(pid);

	return true;
}

bool
ProcFamilyDirectCgroupV2::extend_family_lifetime(pid_t pid)
{
	lifetime_extended_pids.emplace_back(pid);
	return true;
}
		
bool 
ProcFamilyDirectCgroupV2::register_subfamily_before_fork(FamilyInfo *fi) {

	bool success = false;

	if (fi->cgroup) {
		// Hopefully, if we can make the cgroup, we will be able to use it
		// in the child process
		success = makeCgroup(fi->cgroup);
		get_user_sys_cpu(fi->cgroup, starting_user_usec, starting_sys_usec);
	}

	return success;
}

//
// Note: DaemonCore doesn't call this from the starter, because
// the starter exits from the JobReaper, and dc call this after
// calling the reaper.
	bool
ProcFamilyDirectCgroupV2::unregister_family(pid_t pid)
{
	if (std::count(lifetime_extended_pids.begin(), lifetime_extended_pids.end(), (pid)) > 0) {
		dprintf(D_FULLDEBUG, "Unregistering process with living sshds, not killing it\n");
		return true;
	}

	std::string cgroup_name = cgroup_map[pid];

	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::unregister_family for pid %u\n", pid);
	// Remove this cgroup, so that we clear the various peak statistics it holds

	trimCgroupTree(cgroup_name);

	return true;
}

bool 
ProcFamilyDirectCgroupV2::has_been_oom_killed(pid_t pid) {
	bool killed = false;

	std::string cgroup_name = cgroup_map[pid];

	stdfs::path cgroup_root_dir = cgroup_mount_point();
	stdfs::path leaf            = cgroup_root_dir / cgroup_name;
	stdfs::path memory_events   = leaf / "memory.events"; // includes children, if any

	FILE *f = fopen(memory_events.c_str(), "r");
	if (!f) {
		dprintf(D_ALWAYS, "ProcFamilyDirectCgroupV2::has_been_oom_killed cannot open %s: %d %s\n", memory_events.c_str(), errno, strerror(errno));
		return false;
	}

	uint64_t oom_count = 0;

	char word[128]; // max size of a word in memory_events
	while (fscanf(f, "%s", word) != EOF) {
		// if word is oom_killed
		uint64_t oom_kill_value = 0;
		if ((strcmp(word, "oom_group_kill") == 0) ||
			(strcmp(word, "oom_kill") == 0))	{
			// next word is the count
			if (fscanf(f, "%ld", &oom_kill_value) != 1) {
				dprintf(D_ALWAYS, "Error reading oom_count field out of memory.events\n");
				fclose(f);
				return false;
			}
			// Take the higher of "oom_group_kill" or "oom_kill"
			if (oom_kill_value > oom_count) {
				oom_count = oom_kill_value;
			}
		}
	}
	fclose(f);
	dprintf(D_FULLDEBUG, "ProcFamilyDirectCgroupV2::checking if pid %d was oom killed... oom_count was %zu\n", pid, oom_count);

	killed = oom_count > 0;

	return killed;
}

// Returns true if cgroup v2 is mounted
bool 
ProcFamilyDirectCgroupV2::has_cgroup_v2() {
	bool found = false;
	stdfs::path mount_point = cgroup_mount_point();

	// if cgroup.procs exists in the root, then we are
	// a pure cgroup v2 system

	// Don't bother to check with elevated privileges
	stdfs::path cgroup_root_procs = mount_point / "cgroup.procs";
	std::error_code ec;
	if (stdfs::exists(cgroup_root_procs, ec)) {
		found = true;
	}
	return found;
}

static std::string current_parent_cgroup() {
	TemporaryPrivSentry sentry(PRIV_ROOT);
	std::string cgroup;

	int fd = open("/proc/self/cgroup", O_RDONLY);
	if (fd < 0) {
		dprintf(D_ALWAYS, "Cannot open /proc/self/cgroup: %s\n", strerror(errno));
		return cgroup; // empty cgroup is invalid
	}

	char buf[2048];
	int r = read(fd, buf, sizeof(buf)-1);
	if (r < 0) {
		dprintf(D_ALWAYS, "Cannot read /proc/self/cgroup: %s\n", strerror(errno));
		close(fd);
		return cgroup;
	}

	buf[r] = '\0';
	cgroup = buf;
	close(fd);

	if (cgroup.starts_with("0::")) {
		cgroup = cgroup.substr(3,cgroup.size() - 4); // 4 because of newline at end
	} else {
		dprintf(D_ALWAYS, "Unknown prefix for /proc/self/cgroup: %s\n", cgroup.c_str());
		cgroup = "";
	}

	size_t lastSlash = cgroup.rfind('/');
	if (lastSlash == std::string::npos) {
		// This can happen if you are at the root of a cgroup namespace.  Not sure how to handle
		dprintf(D_ALWAYS, "Cgroup %s has no internal directory to chdir .. to...\n", cgroup.c_str());
		cgroup = "";
	} else {
		cgroup.erase(lastSlash); // Remove trailing slash
	}
	return cgroup;
}

bool 
ProcFamilyDirectCgroupV2::can_create_cgroup_v2() {
	bool success = false;

	if (!has_cgroup_v2()) {
		return false;
	}

	TemporaryPrivSentry sentry(PRIV_ROOT);
	std::string parent = cgroup_mount_point().string() + current_parent_cgroup();
	if (access(parent.c_str(), R_OK | W_OK) == 0) {
		success = true;
	}
	return success;
}
