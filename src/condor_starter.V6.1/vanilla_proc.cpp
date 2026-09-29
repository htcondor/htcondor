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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_daemon_client.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "vanilla_proc.h"
#include "condor_uid.h"
#include "starter.h"
#include "condor_config.h"
#include "classad_helpers.h"
#include "filesystem_remap.h"
#include "directory.h"
#include "env.h"
#include "subsystem_info.h"
#include "singularity.h"
#include "has_sysadmin_cap.h"
#include "starter_util.h"
#include "proc_family_direct_cgroup_v2.h"
#include "nvidia_utils.h"
#include <array>

#include <sstream>

#ifdef WIN32
#include "executable_scripts.WINDOWS.h"
#endif

#if defined(HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

extern class Starter *starter;

void StarterStatistics::Clear() {
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
   Pool.Clear();
}

void StarterStatistics::Init() {
    Clear();

    this->RecentWindowQuantum = 1;
    this->RecentWindowMax = this->RecentWindowQuantum;

    STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", BlockReads, IF_BASICPUB);
    STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", BlockWrites, IF_BASICPUB);
    STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", BlockReadBytes, IF_VERBOSEPUB);
    STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "", BlockWriteBytes, IF_VERBOSEPUB);
}

void StarterStatistics::Reconfig() {
    int quantum = param_integer("STATISTICS_WINDOW_QUANTUM_STARTER", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);
    this->RecentWindowQuantum = quantum;

    int window = param_integer("STATISTICS_WINDOW_SECONDS_STARTER", INT_MAX, 1, INT_MAX);
    if (window >= INT_MAX)
        window = param_integer("STATISTICS_WINDOW_SECONDS", 1200, quantum, INT_MAX);
    this->RecentWindowMax = window;

    this->RecentWindowMax = window;
    Pool.SetRecentMax(window, this->RecentWindowQuantum);

    this->PublishFlags = IF_BASICPUB | IF_RECENTPUB;
    char* tmp = param("STATISTICS_TO_PUBLISH");
    if (tmp) {
       this->PublishFlags = generic_stats_ParseConfigString(tmp, "STARTER", "_no_alternate_name_", this->PublishFlags);
       free(tmp);
    }
}

time_t StarterStatistics::Tick(time_t now) {
    if (!now) now = time(NULL);

    int cAdvance = generic_stats_Tick(now,
                                      this->RecentWindowMax,
                                      this->RecentWindowQuantum,
                                      this->InitTime,
                                      this->StatsLastUpdateTime,
                                      this->RecentStatsTickTime,
                                      this->StatsLifetime,
                                      this->RecentStatsLifetime);

    if (cAdvance) Pool.Advance(cAdvance);

    return now;
}

void StarterStatistics::Publish(ClassAd& ad, int flags) const {
    if ((flags & IF_PUBLEVEL) > 0) {
        ad.Assign("StatsLifetime", StatsLifetime);
        if (flags & IF_VERBOSEPUB)
            ad.Assign("StatsLastUpdateTime", StatsLastUpdateTime);
        if (flags & IF_RECENTPUB) {
            ad.Assign("RecentStatsLifetime", RecentStatsLifetime);
            if (flags & IF_VERBOSEPUB) {
                ad.Assign("RecentWindowMax", RecentWindowMax);
                ad.Assign("RecentStatsTickTime", RecentStatsTickTime);
            }
        }
    }

    Pool.Publish(ad, flags);

    if ((flags & IF_PUBLEVEL) > 0) {
        ad.Assign(ATTR_BLOCK_READ_KBYTES, this->BlockReadBytes.value / 1024);
        ad.Assign(ATTR_BLOCK_WRITE_KBYTES, this->BlockWriteBytes.value / 1024);
        if (flags & IF_RECENTPUB) {
            ad.Assign("Recent" ATTR_BLOCK_WRITE_KBYTES, this->BlockWriteBytes.recent / 1024);
            ad.Assign("Recent" ATTR_BLOCK_READ_KBYTES, this->BlockReadBytes.recent / 1024);
        }
    }
}


VanillaProc::VanillaProc(ClassAd* jobAd) : OsProc(jobAd),
	m_memory_limit(-1),
	isCheckpointing(false),
	isSoftKilling(false)
{
    m_statistics.Init();
#if !defined(WIN32)
	m_escalation_tid = -1;
#endif
}

VanillaProc::~VanillaProc() {}

#ifdef LINUX
static bool cgroup_controller_is_writeable(const std::string &controller, std::string relative_cgroup) {

	if (relative_cgroup.length() == 0) {
		return false;
	}

	// Assume cgroup mounted on /sys/fs/cgroup
	std::string cgroup_mount_point = "/sys/fs/cgroup/";

	// In Cgroup v1, need to test each controller separately
	// For cgroup v2, controller will be empty string, but that's OK.
	std::string test_path = cgroup_mount_point;

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
			dprintf(D_ALWAYS, "    Cgroup %s/%s is usable\n", controller.c_str(), relative_cgroup.c_str());
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

static bool cgroup_v2_is_writeable(const std::string &relative_cgroup) {
	bool use_cgroups_without_priv = param_boolean("CREATE_CGROUP_WITHOUT_ROOT", false);
	return (use_cgroups_without_priv || can_switch_ids()) && 
		cgroup_controller_is_writeable("", relative_cgroup);
}

static bool hasCgroupV2() {
	struct stat statbuf{};
	// Should be readable by everyone
	if (stat("/sys/fs/cgroup/cgroup.procs", &statbuf) == 0) {
		// This means we're on cgroups v2
		return true;
	}
	// V1.
	return false;
}

static bool cgroup_is_writeable(const std::string &relative_cgroup) {
	dprintf(D_ALWAYS, "Checking to see if %s is a writeable cgroup\n", relative_cgroup.c_str());
	// Should be readable by everyone
	if (hasCgroupV2()) {
		// This means we're on cgroups v2
		return cgroup_v2_is_writeable(relative_cgroup);
	}
	// V1.
	return cgroup_v1_is_writeable(relative_cgroup);
}
#endif

void Starter::ReportProcessTracking(FamilyInfo & fi, UserProc* up)
{
	if (fi.login) {
		// The following message is documented in the manual as the
		// way to tell whether the dedicated execution account
		// configuration is being used.
		dprintf(D_ALWAYS, "Tracking process family by login \"%s\"\n", fi.login);
	}
	// cgroup_active is set by Create_Process when it actually uses the cgroup
	// we want to capture that, but only for non-side car processes.
	if ( ! up->ThisProcRunsAlongsideMainProc()) {
		main_job_fi.cgroup_active = fi.cgroup_active;
	}
}

int
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// set up a FamilyInfo structure to tell OsProc to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	//
	FamilyInfo fi;
	starter->SetupProcessTracking(fi, this, m_memory_limit);

	dprintf(D_ALWAYS, "ProcessTracking is %s %s\n",
		fi.cgroup ? "cgroup" : "procd",
		fi.login ? fi.login : (fi.cgroup ? fi.cgroup : "")
	);

	// Increase the OOM score of this process; the child will inherit it.
	// This way, the job will be heavily preferred to be killed over a normal process.
	// OOM score is currently exponential - a score of 4 is a factor-16 increase in
	// the OOM score.
	setupOOMScore(4,800);

	// TODO: fix pid namespace code so it does not re-write Cmd and Args of the job!!
#ifdef LINUX
	bool include_pid_namespace = true;
#else
	bool include_pid_namespace = false;
#endif
	FilesystemRemap * remaps = nullptr;
	if ( ! starter->GetFsRemaps(this, remaps, include_pid_namespace)) {
		return FALSE;
	}
	fi.want_pid_namespace = ! m_pid_ns_status_filename.empty();

	// have OsProc start the job
	//
	int retval = OsProc::StartJob(&fi, remaps);

	// Now that the job is started, decrease the likelihood that the starter
	// is killed instead of the job itself.
	setupOOMScore(0,0);

	if (retval) {
		m_statistics.Reconfig();

		if (fi.cgroup_active) {
			int interval = param_integer("CGROUP_POLLING_INTERVAL", 5);
			procFamilyTimerId = daemonCore->Register_Timer( 0, interval,
				(TimerHandlercpp)&VanillaProc::pollFamilyUsage, "cgroup usage poller", this );
		}
	}

	return retval;
}

// This function really belongs in Starter.cpp, Leaving it in this file
// to minimize diffs for now.
void Starter::SetupProcessTracking(FamilyInfo & fiOut, UserProc* up, int64_t & mem_limit)
{
	FamilyInfo & fi = main_job_fi;
	int job_universe = up->Universe();
	if ( ! job_universe) { // if we have not set the universe for the Proc, look in the job classad.
		jic->jobClassAd()->LookupInteger(ATTR_JOB_UNIVERSE, job_universe);
	}
	VanillaProc * vanilla = dynamic_cast<VanillaProc*>(up);

	// take snapshots at no more than 15 seconds in between, by default
	//
	bool first_time = main_job_fi.max_snapshot_interval < 0;
	if (first_time) {
		fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

		fi.login = jic->getExecuteAccountIsDedicated();

	#if defined(LINUX)
		// on Linux, we also have the ability to track processes via
		// a phony supplementary group ID
		//
		if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
			if (!can_switch_ids())
			{
				EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
						   "the group list of our children unless running as "
						   "root");
			}
			fi.group_ptr = &m_tracking_gid;
		}

		if (param_boolean("NO_JOB_NETWORKING", false)) {
			fi.want_net_namespace = true;
		}
	#endif

		// choose the main cgroup for the job and set FamilyInfo limits
		if (vanilla) {
			fi.cgroup = ChooseCGroup(fi, job_universe, "");
		}
	}

	// oom memory limit is set when we setup the main cgroup
	mem_limit = m_oom_memory_limit;

	fiOut = fi;
	if (up->ThisProcRunsAlongsideMainProc()) {
		// If we track a secondary proc's family tree (such as
		// sshd) using the same dedicated account as the job's
		// family tree, we could end up killing the job when we
		// clean up the secondary family.
		fiOut.login = nullptr;

		// if tracking by cgroup, we want to get a sub-cgroup name for the sidecar proc
		// this can probably be simplified to just constructing the sub-cgroup name.
		// we expect all of the other things set in fiOut to be the same as main_job_fi
		if (vanilla && fi.cgroup) {
			std::string cgroup_suffix = vanilla->CgroupSuffix();
			fiOut.cgroup = ChooseCGroup(fiOut, job_universe, cgroup_suffix);
		}
	}
}

// Construct a cgroup name for the given universe and suffix and store that name
// in the Starter's table of cgroup names by suffix. If cgroups are not enabled,
// or not available, the table of cgroup names by suffix will end up with entries
// that map the suffix to nullptr. This indicates that cgroups are not being used, and is not an error.
const char * Starter::ChooseCGroup(FamilyInfo & fi, int job_universe, const std::string & suffix)
{
	auto [itr, inserted] = m_cgroup_names.emplace(suffix,nullptr);
	auto & cgroup_cstr = itr->second;
	if ( ! inserted) {
		// if we have already inserted the name into the map, then just return the name
		// even if it is null, since that indicates that we are not using cgroups.
		fi.cgroup = cgroup_cstr.get();
		return fi.cgroup;
	}

#ifdef LINUX

	ClassAd* JobAd = jic->jobClassAd();
	ClassAd * mad = jic->machClassAd();

	// This works for both v1 and v2 cgroups
	// Determine the cgroup
	std::string cgroup_base;
	param(cgroup_base, "BASE_CGROUP", "");
	std::string cgroup_str;
	const char *cgroup = nullptr;

	bool create_cgroup = true;
	if ((CONDOR_UNIVERSE_LOCAL == job_universe) &&
				! param_boolean("USE_CGROUPS_FOR_LOCAL_UNIVERSE", true)) {
		create_cgroup = false;
	}

	if (cgroup_base.empty()) {
		create_cgroup = false;
	}

	// For v2, let's put the job into the current cgroup hierarchy
	// Because of the "no process in interior cgroups" rule, this means
	// we create a new child of our parent. (a sibling, if you will).

	if (hasCgroupV2()) {
		cgroup_base = ProcFamilyDirectCgroupV2::make_full_cgroup_name(cgroup_base);
	}

	if (create_cgroup && cgroup_is_writeable(cgroup_base)) {
		std::string cgroup_uniq;
		std::string starter_name;
		std::string execute_str;
		param(execute_str, "EXECUTE", "EXECUTE_UNKNOWN");
			// Note: Starter is a global variable from os_proc.cpp
		mad->LookupString(ATTR_NAME, starter_name);
		if (starter_name.size() == 0) {
			starter_name = std::to_string(getpid());
		}
		formatstr(cgroup_uniq, "%s_%s", execute_str.c_str(), starter_name.c_str());
		const char dir_delim[2] = {DIR_DELIM_CHAR, '\0'};
		replace_str(cgroup_uniq, dir_delim, "_");
		formatstr(cgroup_str, "%s%c%s", cgroup_base.c_str(), DIR_DELIM_CHAR,
			cgroup_uniq.c_str());
		cgroup_str += suffix;
		
		cgroup_cstr.reset(strdup(cgroup_str.c_str()));
		cgroup = cgroup_cstr.get();
		ASSERT (cgroup != NULL);

		int numCores = 1;
		if (!mad->LookupInteger(ATTR_CPUS, numCores)) {
			dprintf(D_ALWAYS, "Invalid value of Cpus in machine ClassAd.\n");
			if (param_boolean("LOCAL_UNIVERSE_CGROUP_ENFORCEMENT", false)) {
				if (!JobAd->LookupInteger(ATTR_REQUEST_CPUS, numCores)) {
					dprintf(D_ALWAYS, "   Job does not have RequestCpus either, falling back to 1 cpu\n");
				}
			}
		}

		fi.cgroup_cpu_shares = 100 * numCores;

		if (param_boolean("STARTER_HIDE_GPU_DEVICES", true)) {
			// Potentially disable GPU devices from job
			std::string available_gpus;
			const char *gpu_expr = "join(\",\",evalInEachContext(strcat(\"GPU-\",DeviceUuid),AvailableGPUs))";
			classad::Value v;
			// if the expression does not evaluate to a string, available_gpus
			// remains empty, which means hide all (see below)
			std::ignore = mad->EvaluateExpr(gpu_expr, v);
			std::ignore = v.IsStringValue(available_gpus);

			// will remain empty if not set, meaning hide all
			fi.cgroup_hide_devices = nvidia_env_var_to_exclude_list(available_gpus);
		}
		int64_t memory = 0;
		if (!mad->LookupInteger(ATTR_MEMORY, memory)) {
			dprintf(D_ALWAYS, "Invalid value of memory in machine ClassAd.\n");
			if (param_boolean("LOCAL_UNIVERSE_CGROUP_ENFORCEMENT", false)) {
				if (!JobAd->LookupInteger(ATTR_REQUEST_MEMORY, memory)) {
					dprintf(D_ALWAYS, "   Job does not have RequestMemory either, falling back to no memory limit\n");
					memory = 0; // just to be sure
				} else {
					dprintf(D_ALWAYS, "   Using RequestMemory from job at of %ld Mb\n", memory);
				}
			}
		}
		fi.cgroup_memory_limit = 0; // meaning no limit

		// Need to set this in the unlikely case that we get OOM killed without
		// setting cgroup memory limits
		m_oom_memory_limit = memory * 1024 * 1024;

		std::string policy;
		param(policy, "CGROUP_MEMORY_LIMIT_POLICY", "hard");
		if (policy == "hard") {
			fi.cgroup_memory_limit = (uint64_t) memory * 1024 * 1024;
		}

		long long low_value = 0; // meaning do not set low limit
		if (param_longlong("CGROUP_LOW_MEMORY_LIMIT", low_value,
					false, // use_default,
					0,     // default_value
					false, // check_ranges
					0,     // min_value
					std::numeric_limits<long long>::max(), // max_value
					mad, // my ad
					JobAd)) {
			fi.cgroup_memory_limit_low = (uint64_t) low_value * 1024 * 1024;
		}

		if (policy == "custom") {
			long long hard_value = 0;
			const bool use_default = false;
			const long long default_value = 0;
			const bool check_ranges = false;
			const long long min_value = std::numeric_limits<long long>::min();
			const long long max_value = std::numeric_limits<long long>::max();

			if  (param_longlong( "CGROUP_HARD_MEMORY_LIMIT_EXPR", hard_value,
					use_default, 
					default_value,
					check_ranges,
					min_value,
					max_value,
					mad,
					JobAd)) {
				fi.cgroup_memory_limit = (uint64_t) hard_value * 1024 * 1024;
			} else {
				dprintf(D_ALWAYS, "CGROUP_HARD_MEMORY_LIMIT_EXPR did not evalute to numeric, ignoring\n");
			}
		}

		// If the admin has set CGROUP_ZSWAP_MAX_EXPR, and it evaluates to a
		// non-negative number (of megabytes) in the context of the slot and job
		// ads, set the job's memory.zswap.max.  If unset, or it doesn't evaluate
		// to such a number, we leave the control file alone.  Note that
		// param_longlong returns true whenever the knob is *defined*, even when
		// the expression fails to parse or evaluate, in which case it hands back
		// the default value -- so we pass a negative sentinel as the default and
		// reject anything negative below.  Zero is meaningful (it disables zswap
		// for the job), so we must not confuse it with a failed evaluation.
		const long long zswap_max_megabytes = std::numeric_limits<long long>::max() / (1024 * 1024);
		long long zswap_value = -1;
		if (param_longlong("CGROUP_ZSWAP_MAX_EXPR", zswap_value,
					true,  // use_default
					-1,    // default_value (sentinel for failed evaluation)
					false, // check_ranges
					0,     // min_value
					std::numeric_limits<long long>::max(), // max_value
					starter->jic->machClassAd(), // my ad
					JobAd)) {
			if (zswap_value < 0) {
				dprintf(D_ALWAYS, "CGROUP_ZSWAP_MAX_EXPR did not evaluate to a non-negative number, ignoring\n");
			} else if (zswap_value > zswap_max_megabytes) {
				dprintf(D_ALWAYS, "CGROUP_ZSWAP_MAX_EXPR evaluated to %lld megabytes, which overflows, ignoring\n", zswap_value);
			} else {
				fi.cgroup_zswap_max = (uint64_t) zswap_value * 1024 * 1024;
			}
		}

		// if DISABLE_SWAP_FOR_JOB is true, set swap limit to memory (meaning no swap) 
		bool disable_swap = param_boolean("DISABLE_SWAP_FOR_JOB", true);
		if (disable_swap && fi.cgroup_memory_limit > 0) {
			fi.cgroup_memory_and_swap_limit = fi.cgroup_memory_limit;
		}

		dprintf(D_ALWAYS, "Requesting cgroup %s for job with %d cpu weight and memory limit of %lu (slot memory is %ld).\n",
			cgroup, fi.cgroup_cpu_shares, fi.cgroup_memory_limit, memory);
	}
#endif // LINUX

	fi.cgroup = cgroup_cstr.get();
	return fi.cgroup;
}

// This function really belongs in Starter.cpp, Leaving it in this file
// to minimize diffs for now.
bool Starter::GetFsRemaps(OsProc * up, FilesystemRemap * & remaps, bool include_pid_namespace)
{
	remaps = nullptr;

#ifdef LINUX
	ClassAd* JobAd = jic->jobClassAd();
	ClassAd * mad = jic->machClassAd();
	int job_universe = up->Universe();

	// On Linux kernel 2.4.19 and later, we can give each job its
	// own FS mounts.
	std::string mount_under_scratch;
	param(mount_under_scratch, "MOUNT_UNDER_SCRATCH");
	if (! mount_under_scratch.empty()) {
		// try evaluating mount_under_scratch as a classad expression, if it is
		// an expression it must return a string. if it's not an expression, just
		// use it as a string (as we did before 8.3.6)
		classad::Value value;
		if (JobAd->EvaluateExpr(mount_under_scratch.c_str(), value)) {
			const char * pval = NULL;
			if (value.IsStringValue(pval)) {
				mount_under_scratch = pval;
			} else {
				// was an expression, but not a string, so report and error and fail.
				dprintf(D_ALWAYS | D_ERROR,
					"ERROR: MOUNT_UNDER_SCRATCH does not evaluate to a string, it is : %s\n",
					ClassAdValueToString(value));
				return FALSE;
			}
		}
	}

	// if execute dir is encrypted, add /tmp and /var/tmp to mount_under_scratch
	bool encrypt_execdir = false;
	JobAd->LookupBool(ATTR_ENCRYPT_EXECUTE_DIRECTORY,encrypt_execdir);
	if (encrypt_execdir || param_boolean_crufty("ENCRYPT_EXECUTE_DIRECTORY",false)) {
		// prepend /tmp, /var/tmp to whatever admin wanted. don't worry
		// if admin already listed /tmp etc - subdirs can appear twice
		// in this list because AddMapping() ok w/ duplicate entries
		std::string buf("/tmp,/var/tmp,");
		buf += mount_under_scratch;
		mount_under_scratch = buf;
	}

	// mount_under_scratch only works with rootly powers
	if (! mount_under_scratch.empty() && can_switch_ids() && has_sysadmin_cap() && (job_universe != CONDOR_UNIVERSE_LOCAL)) {
		const char* working_dir = GetWorkingDir(WD::OUTER);

		if (IsDirectory(working_dir)) {
			if (!fs_remap) {
				fs_remap.reset(new FilesystemRemap());
			}
			for (const auto& next_dir: StringTokenIterator(mount_under_scratch)) {
				// Gah, I wish I could throw an exception to clean up these nested if statements.
				if (IsDirectory(next_dir.c_str())) {
					std::string fulldirbuf;
					const char * full_dir = dirscat(working_dir, next_dir.c_str(), fulldirbuf);

					if (full_dir) {
							// If the execute dir is under any component of MOUNT_UNDER_SCRATCH,
							// bad things happen, so give up.
						if (fulldirbuf.find(next_dir.c_str()) == 0) {
							dprintf(D_ALWAYS, "Can't bind mount %s under execute dir %s -- skipping MOUNT_UNDER_SCRATCH\n", next_dir.c_str(), full_dir);
							continue;
						}

						if (!mkdir_and_parents_if_needed( full_dir, S_IRWXU, PRIV_USER )) {
							dprintf(D_ALWAYS, "Failed to create scratch directory %s\n", full_dir);
							return FALSE;
						}
						dprintf(D_FULLDEBUG, "Adding mapping: %s -> %s.\n", full_dir, next_dir.c_str());
						if (fs_remap->AddMapping(full_dir, next_dir.c_str())) {
							// FilesystemRemap object prints out an error message for us.
							return FALSE;
						}
					} else {
						dprintf(D_ALWAYS, "Unable to concatenate %s and %s.\n", working_dir, next_dir.c_str());
						return FALSE;
					}
				} else {
					dprintf(D_ALWAYS, "Unable to add mapping %s -> %s because %s doesn't exist.\n", working_dir, next_dir.c_str(), next_dir.c_str());
				}
			}
			setTmpDir("/tmp");
		} else {
			dprintf(D_ALWAYS, "Unable to perform mappings because %s doesn't exist.\n", working_dir);
			return FALSE;
		}
	}
#endif

	// when the caller is the vanilla proc, we also want to consider pid namespaces
	// TODO: fix the pid namespace code so that it does not mutate the cmd and args of the job ad.
	VanillaProc * vanilla = dynamic_cast<VanillaProc*>(up);
	if ( ! vanilla || ! include_pid_namespace) {
		remaps = fs_remap.get();
		return TRUE;
	}

#if defined(LINUX)
	// On Linux kernel 2.6.24 and later, we can give each
	// job its own PID namespace.
	static bool previously_setup_for_pid_namespace = false;
	bool want_pid_namespace = false;
	if ( (previously_setup_for_pid_namespace || param_boolean("USE_PID_NAMESPACES", false))
			&& !htcondor::Singularity::job_enabled(*mad, *JobAd)
			&& can_switch_ids() )
	{
		want_pid_namespace = up->SupportsPIDNamespace();
		if (want_pid_namespace) {
			if (!fs_remap) {
				fs_remap.reset(new FilesystemRemap());
			}
			fs_remap->RemapProc();
		}

		// When PID Namespaces are enabled, need to run the job
		// under the condor_pid_ns_init program, so that signals
		// propagate through to the child.
		// Be aware that StartJob() can be called repeatedly in the
		// case of a self-checkpointing job, so be careful to only make
		// modifications to the job classad once.

		// First tell the program where to log output status
		// via an environment variable
		if (!previously_setup_for_pid_namespace && param_boolean("USE_PID_NAMESPACE_INIT", true)) {
			Env env;
			std::string env_errors;
			std::string arg_errors;
			std::string filename;

			filename = GetWorkingDir(WD::OUTER);
			filename += "/.condor_pid_ns_status";

			if (!env.MergeFrom(JobAd,  env_errors)) {
				dprintf(D_ALWAYS, "Cannot merge environ from classad so cannot run condor_pid_ns_init\n");
				return 0;
			}
			env.SetEnv("_CONDOR_PID_NS_INIT_STATUS_FILENAME", filename);

			if (!env.InsertEnvIntoClassAd(*JobAd,  env_errors)) {
				dprintf(D_ALWAYS, "Cannot Insert environ from classad so cannot run condor_pid_ns_init\n");
				return 0;
			}

			jic->removeFromOutputFiles(condor_basename(filename.c_str()));
			
			// Now, set the job's CMD to the wrapper, and shift
			// over the arguments by one

			ArgList args;
			std::string cmd;

			JobAd->LookupString(ATTR_JOB_CMD, cmd);

			up->canonicalizeJobPath(cmd, jic->jobRemoteIWD());

			// Must set this *after* calling canonicalizeJobPath!
			vanilla->set_pid_ns_status_filename(filename);

			args.AppendArg(cmd);
			if (!args.AppendArgsFromClassAd(JobAd, arg_errors)) {
				dprintf(D_ALWAYS, "Cannot Append args from classad so cannot run condor_pid_ns_init\n");
				return 0;
			}

			if (!args.InsertArgsIntoClassAd(JobAd, NULL, arg_errors)) {
				dprintf(D_ALWAYS, "Cannot Insert args into classad so cannot run condor_pid_ns_init\n");
				return 0;
			}
	
			std::string libexec;
			if( !param(libexec,"LIBEXEC") ) {
				dprintf(D_ALWAYS, "Cannot find LIBEXEC so can not run condor_pid_ns_init\n");
				return 0;
			}
			std::string c_p_n_i = libexec + "/condor_pid_ns_init";
			JobAd->Assign(ATTR_JOB_CMD, c_p_n_i);
			previously_setup_for_pid_namespace = true;
		}
	}
	dprintf(D_FULLDEBUG, "PID namespace option: %s\n", want_pid_namespace ? "true" : "false");

#endif

	remaps = fs_remap.get();
	return TRUE;
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	static unsigned int max_rss = 0;
#if HAVE_PSS
	static unsigned int max_pss = 0;
#endif

	if (!m_proc_exited) {
		if (daemonCore->Get_Family_Usage(JobPid, m_current_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage in "
					"VanillaProc::PublishUpdateAd() for pid %d\n", JobPid);
			return false;
		}
	}

	ProcFamilyUsage reported_usage = m_checkpoint_usage;
	reported_usage += m_current_usage;
	ProcFamilyUsage * usage = & reported_usage;

        // prepare for updating "generic_stats" stats, call Tick() to update current time
    m_statistics.Tick();

		// Publish the info we care about into the ad.
	ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, (double)usage->sys_cpu_time);
	ad->Assign(ATTR_JOB_REMOTE_USER_CPU, (double)usage->user_cpu_time);

	ad->Assign(ATTR_IMAGE_SIZE, usage->max_image_size);

	if (usage->total_resident_set_size > max_rss) {
		max_rss = usage->total_resident_set_size;
	}
	ad->Assign(ATTR_RESIDENT_SET_SIZE, max_rss);

	std::string memory_usage;
	if (param(memory_usage, "MEMORY_USAGE_METRIC", "((ResidentSetSize+1023)/1024)")) {
		ad->AssignExpr(ATTR_MEMORY_USAGE, memory_usage.c_str());
	}

#if HAVE_PSS
	if( usage->total_proportional_set_size_available ) {
		if (usage->total_proportional_set_size > max_pss) {
			max_pss = usage->total_proportional_set_size;
		}
		ad->Assign( ATTR_PROPORTIONAL_SET_SIZE, max_pss );
	}
#endif

	if (usage->block_read_bytes >= 0) {
		m_statistics.BlockReadBytes = usage->block_read_bytes;
		ad->Assign(ATTR_BLOCK_READ_KBYTES, usage->block_read_bytes / 1024l);
	}
	if (usage->block_write_bytes >= 0) {
		m_statistics.BlockWriteBytes = usage->block_write_bytes;
		ad->Assign(ATTR_BLOCK_WRITE_KBYTES, usage->block_write_bytes / 1024l);
	}

	if (usage->block_reads >= 0) {
		m_statistics.BlockReads = usage->block_reads;
		ad->Assign(ATTR_BLOCK_READS, usage->block_reads);
	}
	if (usage->block_writes >= 0) {
		m_statistics.BlockWrites = usage->block_writes;
		ad->Assign(ATTR_BLOCK_WRITES, usage->block_writes);
	}

	if (usage->io_wait >= 0.0) {
		ad->Assign(ATTR_IO_WAIT, usage->io_wait);
	}

#ifdef LINUX
	if (usage->m_instructions > 0) {
		ad->Assign(ATTR_JOB_CPU_INSTRUCTIONS, usage->m_instructions);
	}
#endif


		// Update our knowledge of how many processes the job has
	num_pids = usage->num_procs;

        // publish standardized "generic_stats" statistics
    m_statistics.Publish(*ad);

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


int VanillaProc::pidNameSpaceReaper( int status ) {
	if (requested_exit) {
		return 0;
	}

	TemporaryPrivSentry sentry(PRIV_ROOT);
	FILE *f = safe_fopen_wrapper_follow(m_pid_ns_status_filename.c_str(), "r");
	if (f == NULL) {
		// Probably couldn't exec the wrapper.  Badness
		dprintf(D_ALWAYS, "JobReaper: condor_pid_ns_init didn't drop filename %s (%d)\n", m_pid_ns_status_filename.c_str(), errno);
		EXCEPT("Starter configured to use PID NAMESPACES, but libexec/condor_pid_ns_init did not run properly");
	}
	if (fscanf(f, "ExecFailed") > 0) {
		EXCEPT("Starter configured to use PID NAMESPACES, but execing the job failed");
	}
	if (fseek(f, 0, SEEK_SET) < 0) {
		dprintf(D_ALWAYS, "JobReaper: condor_pid_ns_init couldn't seek back to beginning of file\n");
	}

	if (fscanf(f, "Exited: %d", &status) > 0) {
		dprintf(D_FULLDEBUG, "Real job exit code of %d read from wrapper output file\n", status);
	}
	fclose(f);

	return status;
}

void VanillaProc::notifySuccessfulEvictionCheckpoint() { /* FIXME (#4969) */ }

void
VanillaProc::notifySuccessfulPeriodicCheckpoint( int checkpointNumber ) {
	//
	// The checkpoint number in the job ad in the shadow / schedd is
	// ADVISORY.  On start-up, the shadow will examine the job's SPOOL
	// directory and determine which MANIFEST file(s) actually exist and
	// have correct checksums, and use that information to determine
	// from which checkpoint number to restart.
	//
	// However, this information could be useful for debugging, so we
	// might as well send it along.
	//
	ClassAd updateAd;

	starter->publishUpdateAd( & updateAd );
	updateAd.Assign( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );

	// UserProc::PublishUpdateAd() truncates, so we will too.
	time_t lastCheckpointTime = job_exit_time.tv_sec;
	updateAd.Assign( ATTR_JOB_LAST_CHECKPOINT_TIME, lastCheckpointTime );

	// UserProc::PublishUpdateAd() truncates, so we will too.
	int newlyCommittedTime = (int)timersub_double(job_exit_time, job_start_time);
	updateAd.Assign( ATTR_JOB_NEWLY_COMMITTED_TIME, newlyCommittedTime );

	starter->jic->periodicJobUpdate( & updateAd );

	// Let's not try to be subtle and confusing (by reacting to the above
	// update in the shadow instead of to a specific event).
	ClassAd eventAd;
	int ignored = -1;
	eventAd.InsertAttr( "EventType", "SuccessfulCheckpoint" );
	eventAd.InsertAttr( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
	starter->jic->notifyGenericEvent( eventAd, ignored );
}

void
VanillaProc::notifyFailedPeriodicCheckpoint( int checkpointNumber ) {
    ClassAd ad;
    int ignored = -1;
    ad.InsertAttr( "EventType", "FailedCheckpoint" );
    ad.InsertAttr( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
    starter->jic->notifyGenericEvent( ad, ignored );
}

void VanillaProc::recordFinalUsage() {
	if( daemonCore->Get_Family_Usage(JobPid, m_current_usage) == FALSE ) {
		dprintf( D_ALWAYS, "error getting family usage for pid %d in "
			"VanillaProc::JobReaper()\n", JobPid );
	}
}
 
void VanillaProc::pollFamilyUsage(int /*timerid*/) {
	if (JobPid > 0) {
		if( daemonCore && daemonCore->Get_Family_Usage(JobPid, m_current_usage) == FALSE ) {
			dprintf( D_ALWAYS, "error polling family usage\n");
		}
	}
}

void VanillaProc::killFamilyIfWarranted() {
	// Kill_Family() will (incorrectly?) kill the SSH-to-job daemon
	// if we're using dedicated accounts or cgroups, so don't unless we know
	// we're the only job.
	if (starter->numberOfJobs() == 1 ) {
		dprintf( D_PROCFAMILY, "About to call Kill_Family()\n" );
		daemonCore->Kill_Family(JobPid);
	} else {
		dprintf( D_PROCFAMILY, "Postponing call to Kill_Family() "
			"(perhaps due to ssh_to_job)\n" );
		// Tell DC not to kill this process tree on exit, As
		// it might kill child cgroups
		// This is a hack until we have proper nested cgroup v2s
		daemonCore->Extend_Family_Lifetime(JobPid);
	}
}

bool VanillaProc::restartCheckpointedJob() {

	// daemoncore unregisters the family
	// after it calls the reaper, if it was registered.
	// on cgroup systems, this kills everything in the cgroup.
	//
	// We call restartCheckpointedJob from the reaper, so the
	// unregistration hasn't happened yet. We start a new job here In
	// the reaper, but when we return, daemon core will unregister
	// and kill all the processes in this cgroup, including the ones 
	// we just starts.  Extend_Family_Lifetime turns off the unregistration

	daemonCore->Extend_Family_Lifetime(JobPid);

	ProcFamilyUsage last_usage;
	if( daemonCore->Get_Family_Usage( JobPid, last_usage ) == FALSE ) {
		dprintf( D_ALWAYS, "error getting family usage for pid %d in "
			"VanillaProc::restartCheckpointedJob()\n", JobPid );
	}
	m_checkpoint_usage += last_usage;

	static int checkpointNumber = -1;
	if( checkpointNumber == -1 ) {
		JobAd->LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
	}

	// Because not all upload attempts fail without writing any data, we
	// need to clean up after failed attempts, which implies numbering them.
	++checkpointNumber;
	if( starter->jic->uploadCheckpointFiles(checkpointNumber) ) {
			notifySuccessfulPeriodicCheckpoint(checkpointNumber);
	} else {
			// We assume this is a transient failure and will try
			// to transfer again after the next periodic checkpoint.
			dprintf( D_ALWAYS, "Failed to transfer checkpoint.\n" );
			notifyFailedPeriodicCheckpoint(checkpointNumber);
	}

	//
	// New semantic: restarting a checkpointed job is now considered
	// equivalent to reactivating a claim, so ask the startd if a claim
	// reactivation would succeed.
	//
	if( param_boolean( "CHECK_REACTIVATE_AFTER_CHECKPOINT", true ) )
	{
		DCStartd startd((const char *)nullptr);
		if(! startd.locate()) {
			dprintf( D_ERROR, "Unable to locate startd while attempting to determine if the checkpointed job should restart: %s\n", startd.error() );

			// Fall back to the semantics from before this check was added.
			goto restart_job;
		}
		std::string claimID;
		if(! starter->getJobClaimId(claimID)) {
			dprintf( D_ERROR, "Unable to get my job's claim ID.\n" );

			// Fall back to the semantics from before this check was added.
			goto restart_job;
		}
		startd.setClaimId(claimID);

		bool claim_is_closing = false;
		bool OK = startd.reactivateClaimCheck(claim_is_closing);
		if(! OK) {
			dprintf( D_ERROR, "Attempt to check if this checkpointed job should restart failed: %s\n", startd.error() );

			// Fall back to the semantics from before this check was added.
			goto restart_job;
		}


		// Ask the AP what it thinks, given what the EP said.
		ClassAd context;
		context.InsertAttr( ATTR_EP_CHECKPOINT_RESTART, claim_is_closing );
		auto guidance = Starter::requestGuidanceCheckpointTaken(
			starter, context
		);
		if( guidance ) {
			claim_is_closing = * guidance;
		}

		if( claim_is_closing ) {
			dprintf( D_ALWAYS, "This checkpointed job should NOT restart.\n" );

			// We didn't restart the job (and didn't want to).
			return false;
		}
    }

    restart_job:;

	// While it's arguably sensible to kill the process family
	// before we restart the job, that would mean that checkpointing
	// would behave differently during ssh-to-job, which seems bad.
	// killFamilyIfWarranted();

	m_proc_exited = false;
	StartJob();

	// We started the job.
	return true;
}


/*
 * This will be called when DC tells us the process exited to due a OOM event.
 */
int
VanillaProc::outOfMemoryEvent() {

	/* The cgroup API generates this notification whenever the OOM fires OR
	 * the cgroup is removed. If the cgroups are not pre-created, the kernel will
	 * remove the cgroup when the job completes. So if we land here and there are
	 * no more job pids, we assume the cgroup was removed and just ignore the event.
	 * However, if we land here and we still have job pids, we assume the OOM fired
	 * and thus we place the job on hold. See gt#3824.
	 */

	// The OOM killer has fired, and the process is frozen.  However,
	// the cgroup still has accurate memory usage.  Let's grab that
	// and make a final update, so the user can see exactly how much
	// memory they used.
	ClassAd updateAd;
	PublishUpdateAd( &updateAd );
	starter->jic->periodicJobUpdate( &updateAd );
	int64_t usageKB = 0;
	// This is the peak
	updateAd.LookupInteger(ATTR_IMAGE_SIZE, usageKB);
	int64_t usageMB = usageKB / 1024;

	// But sometimes the job dies before we can poll for memory on systems 
	// that don't have a memory.peak, and we get 0 MB. Assume job hit
	// memory limit, and report that number, not the confusing 0 bytes used.
	bool should_hold = param_boolean("STARTER_ALWAYS_HOLD_ON_OOM", true);
	if ((usageMB == 0) || (should_hold)) {
		usageMB = m_memory_limit / (1024 * 1024);
	}

	//
	//  Cgroup memory limits are limits, not reservations.
	//  For many reasons, a job could be below the memory limit,
	//  but still get an OOM notification.  Commonly, this happens
	//  when other processes on the system are using large amounts
	//  of memory.  
	//
	//  Check to see if this is the case, and if so, it 
	//  isn't our job's fault, but we need to 
	//  evict the job, so it might run more successfully somewhere
	//  else.  The situation is pretty dire, so we likely can't
	//  checkpoint or otherwise exit gracefully, but at least let's
	//  try to get a message a back to the shadow.

	// Why not 100%?  We have seen cases where our last cgroup poll was a bit 
	// lower than the limit when the OOM killer fired.
	// So have some slop, just in case.

	// Now that we are polling cgroup, and not getting peaks from cgroup
	// a quickly-growing job can have a last-reported memory significantly
	// lower than the limit.  In this case we want to always hold the job
	// and report and out-of-memory condition

	if (!should_hold) {
		if (usageMB < (0.9 * (m_memory_limit / (1024 * 1024)))) {
			dprintf(D_ALWAYS, "Evicting job because system is out of memory, even though the job is below requested memory: Usage is %lld Mb limit is %lld\n", (long long)usageMB, (long long)m_memory_limit);
			starter->jic->notifyStarterError("Worker node is out of memory", true, 0, 0);
			starter->jic->allJobsGone(); // and exit to clean up more memory
			return 0;
		}
	}

	std::string ss;
	if (m_memory_limit >= 0) {
		formatstr(ss, "Job has gone over cgroup memory limit of %lld megabytes. Last measured usage: %lld megabytes.  Consider resubmitting with a higher request_memory.", 
				(long long)(m_memory_limit / (1024 * 1024)), (long long)usageMB);
	} else {
		ss = "Job has encountered an out-of-memory event.";
	}
	if( isCheckpointing ) {
		ss += "  This occurred while the job was checkpointing.";
	}

	dprintf( D_ALWAYS, "Job was held due to OOM event: %s\n", ss.c_str());

	// This ulogs the hold event and KILLS the shadow
	starter->jic->holdJob(ss.c_str(), CONDOR_HOLD_CODE::JobOutOfResources, OUT_OF_RESOURCES_SUB_CODE::Memory);

	return 0;
}

ReapResult
VanillaProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"Inside VanillaProc::JobReaper()\n");

	if (procFamilyTimerId > 0) {
		daemonCore->Cancel_Timer(procFamilyTimerId);
		procFamilyTimerId = -1;
	}
	// If cgroup v2 is enabled, we'll get this high bit set in exit_status
#ifdef LINUX
	if (!isSoftKilling && (status & DC_STATUS_OOM_KILLED)) {
		// Will put the job on hold
		this->outOfMemoryEvent();
		status &= ~DC_STATUS_OOM_KILLED;
	} 
#endif
	
	//
	//
	// Run all the reapers first, since some of them change the exit status.
	//
	if( m_pid_ns_status_filename.length() > 0 ) {
		status = pidNameSpaceReaper( status );
	}
	ReapResult result = OsProc::JobReaper( pid, status );
	if( pid != JobPid ) { return result; }

	//
	// We have three cases to consider:
	//   * if we're checkpointing; or
	//   * if we see a special checkpoint exit code; or
	//   * there's no special case to consider.
	//

	bool wantsFileTransferOnCheckpointExit = false;
	JobAd->LookupBool( ATTR_WANT_FT_ON_CHECKPOINT, wantsFileTransferOnCheckpointExit );

	int successfulCheckpointStatus = computeDesiredExitStatus( "Checkpoint", JobAd );

	if( isCheckpointing ) {
		dprintf( D_FULLDEBUG, "Inside VanillaProc::JobReaper() during a checkpoint\n" );

		if( exit_status == successfulCheckpointStatus ) {
			if( isSoftKilling ) {
				notifySuccessfulEvictionCheckpoint();
				return ReapResult::JobDone;
			}

			if( restartCheckpointedJob() ) {
				isCheckpointing = false;
				return ReapResult::JobShouldReExec;
			} else {
				// We need to prevent (final) output transfer from happening
				// as well as ensure that the job is requeued.  The latter
				// should happen automatically as a result of the former,
				// but doesn't.
				starter->jic->setOutputTransfer(true);
				starter->SetVacateReason(
					"Rescheduling self-checkpoint job after checkpoint upload because reactivating the claim would have failed.",
					CONDOR_HOLD_CODE::SuccessfulCheckpoint, 0
				);
				starter->jicNotifyStarterError( true );
				requested_exit = true;

				// At this point, the shadow will ask the startd to deactivate
				// the claim, but the startd will block waiting for the starter
				// to exit, so we'll just exit first.  (Arguably, we should
				// schedule a zero-second timer here to exit so that we can
				// exit through the event loop.)
				starter->StarterExit( JOB_SHOULD_REQUEUE );

				// This job is done, but we want to avoid reporting a job
				// exit here, because it will be misinterpreted as a job
				// termination, and we need the job rescheduled.
				return ReapResult::JobShouldReExec;
			}
		} else {
			// The job exited without taking a checkpoint.  If we don't do
			// anything, it will be reported as if the error code or signal
			// had happened naturally (and the job will usually exit the
			// queue).  This could confuse the users.
			//
			// Instead, we'll put the job on hold, figuring that if the job
			// requested that we (periodically) send it a signal, and we
			// did, that it's not our fault that the job failed.  This has
			// the convenient side-effect of not overwriting the job's
			// previous checkpoint(s), if any (since file transfer doesn't
			// occur when the job goes on hold).
			killFamilyIfWarranted();
			recordFinalUsage();

			int checkpointExitCode = 0;
			JobAd->LookupInteger( ATTR_CHECKPOINT_EXIT_CODE, checkpointExitCode );
			int checkpointExitSignal = 0;
			JobAd->LookupInteger( ATTR_CHECKPOINT_EXIT_SIGNAL, checkpointExitSignal );
			bool checkpointExitBySignal = 0;
			JobAd->LookupBool( ATTR_CHECKPOINT_EXIT_BY_SIGNAL, checkpointExitBySignal );

			std::string holdMessage;
			formatstr( holdMessage, "Job did not exit as promised when sent its checkpoint signal.  "
				"Promised exit was %s %u, actual exit status was %s %u.",
				checkpointExitBySignal ? "on signal" : "with exit code",
				checkpointExitBySignal ? checkpointExitSignal : checkpointExitCode,
				WIFSIGNALED( exit_status ) ? "on signal" : "with exit code",
				WIFSIGNALED( exit_status ) ? WTERMSIG( exit_status ) : WEXITSTATUS( exit_status ) );
			starter->jic->holdJob( holdMessage.c_str(), CONDOR_HOLD_CODE::FailedToCheckpoint, exit_status );
			starter->Hold();
			return ReapResult::JobDone;
		}
	} else if( wantsFileTransferOnCheckpointExit && exit_status == successfulCheckpointStatus ) {
		dprintf( D_FULLDEBUG, "Inside VanillaProc::JobReaper() and the job self-checkpointed.\n" );

		if( isSoftKilling ) {
			notifySuccessfulEvictionCheckpoint();
			return ReapResult::JobDone;
		} else {
			if( restartCheckpointedJob() ) {
				return ReapResult::JobShouldReExec;
			} else {
				// We need to prevent (final) output transfer from happening
				// as well as ensure that the job is requeued.  The latter
				// should happen automatically as a result of the former,
				// but doesn't.
				starter->jic->setOutputTransfer(true);
				starter->SetVacateReason(
					"Rescheduling self-checkpoint job after checkpoint upload because reactivating the claim would have failed.",
					CONDOR_HOLD_CODE::SuccessfulCheckpoint, 0
				);
				starter->jicNotifyStarterError( true );
				requested_exit = true;

				// At this point, the shadow will ask the startd to deactivate
				// the claim, but the startd will block waiting for the starter
				// to exit, so we'll just exit first.  (Arguably, we should
				// schedule a zero-second timer here to exit so that we can
				// exit through the event loop.)
				starter->StarterExit( JOB_SHOULD_REQUEUE );

				// This job is done, but we want to avoid reporting a job
				// exit here, because it will be misinterpreted as a job
				// termination, and we need the job rescheduled.
				return ReapResult::JobShouldReExec;
			}
		}
	} else {
		// If the parent job process died, clean up all of the job's processes.
		killFamilyIfWarranted();

		// Record final usage stats for this process family, since
		// once the reaper returns, the family is no longer
		// registered with DaemonCore and we'll never be able to
		// get this information again.
		recordFinalUsage();

		// We're going to exit, so daemon core won't get a chance to unregister our subfamily
		// force that no
		daemonCore->Unregister_subfamily(pid);

		return result;
	}
}


void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");
	
	// suspend the user job
	if (JobPid != -1) {
		if (daemonCore->Suspend_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error suspending family in VanillaProc::Suspend()\n");
		}
	}
	
	// set our flag
	is_suspended = true;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");
	
	// resume user job
	if (JobPid != -1 && is_suspended ) {
		if (daemonCore->Continue_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error continuing family in VanillaProc::Continue()\n");
		}
	}

	// set our flag
	is_suspended = false;
}

bool
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");

	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	isSoftKilling = true;
	// Because we allow the user to specify different signals for periodic
	// checkpoint and for soft kills, don't suppress the soft kill signal
	// if we're checkpointing when we're vacated.  (This also simplifies
	// keeping signal semantics consistent with removing or holding jobs.)
	return OsProc::ShutdownGraceful();
}

bool
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	requested_exit = true;

	return finishShutdownFast();
}

bool
VanillaProc::finishShutdownFast()
{
	// this used to be the only place where we would clean up the process
	// family. this, however, wouldn't properly clean up local universe jobs
	// so a call to Kill_Family has been added to JobReaper(). i'm not sure
	// that this call is still needed, but am unwilling to remove it on the
	// eve of Condor 7
	//   -gquinn, 2007-11-14
	daemonCore->Kill_Family(JobPid);

	return false;	// shutdown is pending, so return false
}

int
VanillaProc::setupOOMScore(int oom_adj, int oom_score_adj)
{

#if !(defined(HAVE_EVENTFD))
	if (oom_adj + oom_score_adj) // Done to suppress compiler warnings.
		return 0;
	return 0;
#else 
	TemporaryPrivSentry sentry(PRIV_ROOT);
	// oom_adj is deprecated on modern kernels and causes a deprecation warning when used.

	int oom_score = oom_adj; // assume the old way

	int oom_score_fd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);
	if (oom_score_fd == -1) {
		if (errno != ENOENT) {
			dprintf(D_ALWAYS,
				"Unable to open oom_score_adj for the starter: (errno=%u, %s)\n",
				errno, strerror(errno));
			return 1;
		} else {
			oom_score_fd = open("/proc/self/oom_adj", O_WRONLY | O_CLOEXEC);
			if (oom_score_fd == -1) {
				dprintf(D_ALWAYS,
					"Unable to open oom_adj for the starter: (errno=%u, %s)\n",
					errno, strerror(errno));
				return 1;
			}
		}
	} else {
		// oops, we've got the new kind.  Use that.
		oom_score = oom_score_adj;
	}

	std::stringstream ss;
	ss << oom_score;
	std::string new_score_str = ss.str();
        ssize_t nwritten = full_write(oom_score_fd, new_score_str.c_str(), new_score_str.length());
	if (nwritten < 0) {
		dprintf(D_ALWAYS,
			"Unable to write into oom_adj file for the starter: (errno=%u, %s)\n",
			errno, strerror(errno));
		close(oom_score_fd);
		return 1;
	}
	close(oom_score_fd);
	return 0;
#endif
}

bool VanillaProc::Ckpt() {
	dprintf( D_FULLDEBUG, "Entering VanillaProc::Ckpt()\n" );

	if( isSoftKilling ) { return false; }

	bool wantCheckpointSignal = false;
	JobAd->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpointSignal );
	if( wantCheckpointSignal && ! isCheckpointing ) {
		int periodicCheckpointSignal = findCheckpointSig( JobAd );
		if( periodicCheckpointSignal == -1 ) {
			periodicCheckpointSignal = soft_kill_sig;
		}
		daemonCore->Send_Signal( JobPid, periodicCheckpointSignal );
		isCheckpointing = true;

		// Do not do intermediate file transfer, since we're not blocking.
		// Instead, do intermediate file transfer in the reaper.
		return false;
	}

	return OsProc::Ckpt();
}

int VanillaProc::outputOpenFlags() {
	bool wantCheckpoint = false;
	JobAd->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpoint );
	bool wantsFileTransferOnCheckpointExit = false;
	JobAd->LookupBool( ATTR_WANT_FT_ON_CHECKPOINT, wantsFileTransferOnCheckpointExit );
	bool dontAppend = true;
	JobAd->LookupBool( ATTR_DONT_APPEND, dontAppend );
	if( wantCheckpoint || wantsFileTransferOnCheckpointExit || (!dontAppend) ) {
		return O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE;
	} else {
		return this->OsProc::outputOpenFlags();
	}
}

int VanillaProc::streamingOpenFlags( bool isOutput ) {
	bool wantCheckpoint = false;
	JobAd->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpoint );
	bool wantsFileTransferOnCheckpointExit = false;
	JobAd->LookupBool( ATTR_WANT_FT_ON_CHECKPOINT, wantsFileTransferOnCheckpointExit );
	bool dontAppend = true;
	JobAd->LookupBool( ATTR_DONT_APPEND, dontAppend );
	if( wantCheckpoint || wantsFileTransferOnCheckpointExit || (!dontAppend) ) {
		return isOutput ? O_CREAT | O_APPEND | O_WRONLY : O_RDONLY;
	} else {
		return this->OsProc::streamingOpenFlags( isOutput );
	}
}
