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
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "condor_config.h"
#include "domain_tools.h"
#include "classad_helpers.h"
#include "filesystem_remap.h"
#include "directory.h"
#include "env.h"
#include "subsystem_info.h"
#include "cgroup_limits.h"
#include "selector.h"
#include "singularity.h"
#include "has_sysadmin_cap.h"
#include "starter_util.h"

#include <sstream>

#ifdef WIN32
#include "executable_scripts.WINDOWS.h"
#endif

#if defined(HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

extern Starter *Starter;

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
        ad.Assign("StatsLifetime", (int)StatsLifetime);
        if (flags & IF_VERBOSEPUB)
            ad.Assign("StatsLastUpdateTime", (int)StatsLastUpdateTime);
        if (flags & IF_RECENTPUB) {
            ad.Assign("RecentStatsLifetime", (int)RecentStatsLifetime);
            if (flags & IF_VERBOSEPUB) {
                ad.Assign("RecentWindowMax", (int)RecentWindowMax);
                ad.Assign("RecentStatsTickTime", (int)RecentStatsTickTime);
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
	m_oom_fd(-1),
	m_oom_efd(-1),
	m_oom_efd2(-1),
	isCheckpointing(false),
	isSoftKilling(false)
{
    m_statistics.Init();
#if !defined(WIN32)
	m_escalation_tid = -1;
#endif
}

VanillaProc::~VanillaProc()
{
	cleanupOOM();
}

int
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32

	CHAR		interpreter[MAX_PATH+1],
				systemshell[MAX_PATH+1];    
	const char* jobtmp				= Starter->jic->origJobName();
	size_t		joblen				= strlen(jobtmp);
	const char	*extension			= joblen > 0 ? &(jobtmp[joblen-4]) : NULL;
	bool		binary_executable	= ( extension && 
										( MATCH == strcasecmp ( ".exe", extension ) || 
										  MATCH == strcasecmp ( ".com", extension ) ) ),
				java_universe		= ( CONDOR_UNIVERSE_JAVA == job_universe );
	ArgList		arguments;
	std::string	filename;
	std::string	jobname;
	std::string error;
	
	if ( extension && !java_universe && !binary_executable ) {

		/** since we do not actually know how long the extension of
			the file is, we'll need to hunt down the '.' in the path,
			if it exists */
		extension = strrchr ( jobtmp, '.' );

		if ( !extension ) {

			dprintf ( 
				D_ALWAYS, 
				"VanillaProc::StartJob(): Failed to extract "
				"the file's extension.\n" );

			/** don't fail here, since we want executables to run
				as usual.  That is, some condor jobs submit 
				executables that do not have the '.exe' extension,
				but are, nonetheless, executable binaries.  For
				instance, a submit script may contain:

				executable = executable$(OPSYS) */

		} else {

			/** pull out the path to the executable */
			if ( !JobAd->LookupString ( 
				ATTR_JOB_CMD, 
				jobname ) ) {
				
				/** fall back on Starter->jic->origJobName() */
				jobname = jobtmp;

			}

			/** If we transferred the job, it may have been
				renamed to condor_exec.exe even though it is
				not an executable. Here we rename it back to
				a the correct extension before it will run. */
			if ( MATCH == strcasecmp ( 
					CONDOR_EXEC, 
					condor_basename ( jobname.c_str () ) ) ) {
				formatstr ( filename, "condor_exec%s", extension );
				if (rename(CONDOR_EXEC, filename.c_str()) != 0) {
					dprintf (D_ALWAYS, "VanillaProc::StartJob(): ERROR: "
							"failed to rename executable from %s to %s\n", 
							CONDOR_EXEC, filename.c_str() );
				}
			} else {
				filename = jobname;
			}
			
			/** Since we've renamed our executable, we need to
				update the job ad to reflect this change. */
			if ( !JobAd->Assign ( 
				ATTR_JOB_CMD, 
				filename ) ) {

				dprintf (
					D_ALWAYS,
					"VanillaProc::StartJob(): ERROR: failed to "
					"set new executable name.\n" );

				return FALSE;

			}

			/** We've moved the script to argv[1], so we need to 
				add	the remaining arguments to positions argv[2]..
				argv[/n/]. */
			if ( !arguments.AppendArgsFromClassAd ( JobAd, error ) ||
				 !arguments.InsertArgsIntoClassAd ( JobAd, NULL, error ) ) {

				dprintf (
					D_ALWAYS,
					"VanillaProc::StartJob(): ERROR: failed to "
					"get arguments from job ad: %s\n",
					error.c_str () );

				return FALSE;

			}

			/** Since we know already we don't want this file returned
				to us, we explicitly add it to an exception list which
				will stop the file transfer mechanism from considering
				it for transfer back to its submitter */
			Starter->jic->removeFromOutputFiles (
				filename.c_str () );

		}
			
	}
#endif

	// set up a FamilyInfo structure to tell OsProc to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	//
	FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	//
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	m_dedicated_account = Starter->jic->getExecuteAccountIsDedicated();
	if( ThisProcRunsAlongsideMainProc() ) {
			// If we track a secondary proc's family tree (such as
			// sshd) using the same dedicated account as the job's
			// family tree, we could end up killing the job when we
			// clean up the secondary family.
		m_dedicated_account = NULL;
	}
	if (m_dedicated_account) {
			// using login-based family tracking
		fi.login = m_dedicated_account;
			// The following message is documented in the manual as the
			// way to tell whether the dedicated execution account
			// configuration is being used.
		dprintf(D_ALWAYS,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

	FilesystemRemap * fs_remap = NULL;
#if defined(LINUX)
	// on Linux, we also have the ability to track processes via
	// a phony supplementary group ID
	//
	gid_t tracking_gid = 0;
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		if (!can_switch_ids())
		{
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root");
		}
		fi.group_ptr = &tracking_gid;
	}

	// Increase the OOM score of this process; the child will inherit it.
	// This way, the job will be heavily preferred to be killed over a normal process.
	// OOM score is currently exponential - a score of 4 is a factor-16 increase in
	// the OOM score.
	setupOOMScore(4,800);
#endif

#if defined(HAVE_EXT_LIBCGROUP)
	// Determine the cgroup
	std::string cgroup_base;
	param(cgroup_base, "BASE_CGROUP", "");
	std::string cgroup_str;
	const char *cgroup = NULL;
		/* Note on CONDOR_UNIVERSE_LOCAL - The cgroup setup code below
		 *  requires a unique name for the cgroup. It relies on
		 *  uniqueness of the MachineAd's Name
		 *  attribute. Unfortunately, in the local universe the
		 *  MachineAd (mach_ad elsewhere) is never populated, because
		 *  there is no machine. As a result the ASSERT on
		 *  starter_name fails. This means that the local universe
		 *  will not work on any machine that has BASE_CGROUP
		 *  configured. A potential workaround is to set
		 *  STARTER.BASE_CGROUP on any machine that is also running a
		 *  schedd, but that disables cgroup support from a
		 *  co-resident startd. Instead, I'm disabling cgroup support
		 *  from within the local universe until the intraction of
		 *  local universe and cgroups can be properly worked
		 *  out. -matt 7 nov '12
		 */
	if (CONDOR_UNIVERSE_LOCAL != job_universe && cgroup_base.length() && can_switch_ids() && has_sysadmin_cap()) {
		std::string cgroup_uniq;
		std::string starter_name, execute_str;
		param(execute_str, "EXECUTE", "EXECUTE_UNKNOWN");
			// Note: Starter is a global variable from os_proc.cpp
		Starter->jic->machClassAd()->LookupString(ATTR_NAME, starter_name);
		if (starter_name.size() == 0) {
			char buf[16];
			sprintf(buf, "%d", getpid());
			starter_name = buf;
		}
		//ASSERT (starter_name.size());
		formatstr(cgroup_uniq, "%s_%s", execute_str.c_str(), starter_name.c_str());
		const char dir_delim[2] = {DIR_DELIM_CHAR, '\0'};
		replace_str(cgroup_uniq, dir_delim, "_");
		formatstr(cgroup_str, "%s%ccondor%s", cgroup_base.c_str(), DIR_DELIM_CHAR,
			cgroup_uniq.c_str());
		cgroup_str += this->CgroupSuffix();
		
		cgroup = cgroup_str.c_str();
		ASSERT (cgroup != NULL);
		fi.cgroup = cgroup;
		dprintf(D_FULLDEBUG, "Requesting cgroup %s for job.\n", cgroup);
	}

#endif

// The chroot stuff really only works on linux
#ifdef LINUX
	{
        // Have Condor manage a chroot
       std::string requested_chroot_name;
       JobAd->LookupString("RequestedChroot", requested_chroot_name);
       const char * allowed_root_dirs = param("NAMED_CHROOT");
       if (requested_chroot_name.size()) {
               dprintf(D_FULLDEBUG, "Checking for chroot: %s\n", requested_chroot_name.c_str());
               StringList chroot_list(allowed_root_dirs);
               chroot_list.rewind();
               const char * next_chroot;
               bool acceptable_chroot = false;
               std::string requested_chroot;
               while ( (next_chroot=chroot_list.next()) ) {
                       MyStringWithTokener chroot_spec(next_chroot);
                       chroot_spec.Tokenize();
                       const char * chroot_name = chroot_spec.GetNextToken("=", false);
                       if (chroot_name == NULL) {
                               dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.c_str());
                       }
                       const char * next_dir = chroot_spec.GetNextToken("=", false);
                       if (chroot_name == NULL) {
                               dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.c_str());
                       }
                       dprintf(D_FULLDEBUG, "Considering directory %s for chroot %s.\n", next_dir, chroot_spec.c_str());
                       if (IsDirectory(next_dir) && chroot_name && (strcmp(requested_chroot_name.c_str(), chroot_name) == 0)) {
                               acceptable_chroot = true;
                               requested_chroot = next_dir;
                       }
               }
               // TODO: path to chroot MUST be all root-owned, or we have a nice security exploit.
               // Is this the responsibility of Condor to check, or the sysadmin who set it up?
               if (!acceptable_chroot) {
                       return FALSE;
               }
               dprintf(D_FULLDEBUG, "Will attempt to set the chroot to %s.\n", requested_chroot.c_str());

               std::stringstream ss;
               std::stringstream ss2;
               ss2 << Starter->GetExecuteDir() << DIR_DELIM_CHAR << "dir_" << getpid();
               std::string execute_dir = ss2.str();
               ss << requested_chroot << DIR_DELIM_CHAR << ss2.str();
               std::string full_dir_str = ss.str();
               if (is_trivial_rootdir(requested_chroot)) {
                   dprintf(D_FULLDEBUG, "Requested a trivial chroot %s; this is a no-op.\n", requested_chroot.c_str());
               } else if (IsDirectory(execute_dir.c_str())) {
                       {
                           TemporaryPrivSentry sentry(PRIV_ROOT);
                           if( mkdir(full_dir_str.c_str(), S_IRWXU) < 0 ) {
                               dprintf( D_FAILURE|D_ALWAYS,
                                   "Failed to create sandbox directory in chroot (%s): %s\n",
                                   full_dir_str.c_str(),
                                   strerror(errno) );
                               return FALSE;
                           }
                           if (chown(full_dir_str.c_str(),
                                     get_user_uid(),
                                     get_user_gid()) == -1)
                           {
                               EXCEPT("chown error on %s: %s",
                                      full_dir_str.c_str(),
                                      strerror(errno));
                           }
                       }
                       if (!fs_remap) {
                               fs_remap = new FilesystemRemap();
                       }
                       dprintf(D_FULLDEBUG, "Adding mapping: %s -> %s.\n", execute_dir.c_str(), full_dir_str.c_str());
                       if (fs_remap->AddMapping(execute_dir, full_dir_str)) {
                               // FilesystemRemap object prints out an error message for us.
                               return FALSE;
                       }
                       dprintf(D_FULLDEBUG, "Adding mapping %s -> %s.\n", requested_chroot.c_str(), "/");
                       std::string root_str("/");
                       if (fs_remap->AddMapping(requested_chroot, root_str)) {
                               return FALSE;
                       }
               } else {
                       dprintf(D_ALWAYS, "Unable to do chroot because working dir %s does not exist.\n", execute_dir.c_str());
               }
       } else {
               dprintf(D_FULLDEBUG, "Value of RequestedChroot is unset.\n");
       }
	}
// End of chroot 

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
		const char* working_dir = Starter->GetWorkingDir(0);

		if (IsDirectory(working_dir)) {
			StringList mount_list(mount_under_scratch);

			mount_list.rewind();
			if (!fs_remap) {
				fs_remap = new FilesystemRemap();
			}
			const char * next_dir;
			while ( (next_dir=mount_list.next()) ) {
				if (!*next_dir) {
					// empty string?
					mount_list.deleteCurrent();
					continue;
				}

				// Gah, I wish I could throw an exception to clean up these nested if statements.
				if (IsDirectory(next_dir)) {
					std::string fulldirbuf;
					const char * full_dir = dirscat(working_dir, next_dir, fulldirbuf);

					if (full_dir) {
							// If the execute dir is under any component of MOUNT_UNDER_SCRATCH,
							// bad things happen, so give up.
						if (fulldirbuf.find(next_dir) == 0) {
							dprintf(D_ALWAYS, "Can't bind mount %s under execute dir %s -- skipping MOUNT_UNDER_SCRATCH\n", next_dir, full_dir);
							continue;
						}

						if (!mkdir_and_parents_if_needed( full_dir, S_IRWXU, PRIV_USER )) {
							dprintf(D_ALWAYS, "Failed to create scratch directory %s\n", full_dir);
							delete fs_remap;
							return FALSE;
						}
						dprintf(D_FULLDEBUG, "Adding mapping: %s -> %s.\n", full_dir, next_dir);
						if (fs_remap->AddMapping(full_dir, next_dir)) {
							// FilesystemRemap object prints out an error message for us.
							delete fs_remap;
							return FALSE;
						}
					} else {
						dprintf(D_ALWAYS, "Unable to concatenate %s and %s.\n", working_dir, next_dir);
						delete fs_remap;
						return FALSE;
					}
				} else {
					dprintf(D_ALWAYS, "Unable to add mapping %s -> %s because %s doesn't exist.\n", working_dir, next_dir, next_dir);
				}
			}
		Starter->setTmpDir("/tmp");
		} else {
			dprintf(D_ALWAYS, "Unable to perform mappings because %s doesn't exist.\n", working_dir);
			delete fs_remap;
			return FALSE;
		}
	}
#endif

#if defined(LINUX)
	// On Linux kernel 2.6.24 and later, we can give each
	// job its own PID namespace.
	static bool previously_setup_for_pid_namespace = false;

	if ( (previously_setup_for_pid_namespace || param_boolean("USE_PID_NAMESPACES", false))
			&& !htcondor::Singularity::job_enabled(*Starter->jic->machClassAd(), *JobAd) ) 
	{
		if (!can_switch_ids()) {
			EXCEPT("USE_PID_NAMESPACES enabled, but can't perform this "
				"call in Linux unless running as root.");
		}
		fi.want_pid_namespace = this->SupportsPIDNamespace();
		if (fi.want_pid_namespace) {
			if (!fs_remap) {
				fs_remap = new FilesystemRemap();
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

			filename = Starter->GetWorkingDir(0);
			filename += "/.condor_pid_ns_status";

			if (!env.MergeFrom(JobAd,  env_errors)) {
				dprintf(D_ALWAYS, "Cannot merge environ from classad so cannot run condor_pid_ns_init\n");
				delete fs_remap;
				return 0;
			}
			env.SetEnv("_CONDOR_PID_NS_INIT_STATUS_FILENAME", filename);

			if (!env.InsertEnvIntoClassAd(JobAd,  env_errors)) {
				dprintf(D_ALWAYS, "Cannot Insert environ from classad so cannot run condor_pid_ns_init\n");
				delete fs_remap;
				return 0;
			}

			Starter->jic->removeFromOutputFiles(condor_basename(filename.c_str()));
			this->m_pid_ns_status_filename = filename;
			
			// Now, set the job's CMD to the wrapper, and shift
			// over the arguments by one

			ArgList args;
			std::string cmd;

			JobAd->LookupString(ATTR_JOB_CMD, cmd);
			args.AppendArg(cmd);
			if (!args.AppendArgsFromClassAd(JobAd, arg_errors)) {
				dprintf(D_ALWAYS, "Cannot Append args from classad so cannot run condor_pid_ns_init\n");
				delete fs_remap;
				return 0;
			}

			if (!args.InsertArgsIntoClassAd(JobAd, NULL, arg_errors)) {
				dprintf(D_ALWAYS, "Cannot Insert args into classad so cannot run condor_pid_ns_init\n");
				delete fs_remap;
				return 0;
			}
	
			std::string libexec;
			if( !param(libexec,"LIBEXEC") ) {
				dprintf(D_ALWAYS, "Cannot find LIBEXEC so can not run condor_pid_ns_init\n");
				delete fs_remap;
				return 0;
			}
			std::string c_p_n_i = libexec + "/condor_pid_ns_init";
			JobAd->Assign(ATTR_JOB_CMD, c_p_n_i);
			previously_setup_for_pid_namespace = true;
		}
	}
	dprintf(D_FULLDEBUG, "PID namespace option: %s\n", fi.want_pid_namespace ? "true" : "false");
#endif


	// have OsProc start the job
	//
	int retval = OsProc::StartJob(&fi, fs_remap);

	if (fs_remap != NULL) {
		delete fs_remap;
	}

#if defined(HAVE_EXT_LIBCGROUP)

	// Set fairshare limits.  Note that retval == 1 indicates success, 0 is failure.
	// See Note near setup of param(BASE_CGROUP)
	if (CONDOR_UNIVERSE_LOCAL != job_universe && cgroup && retval) {
#ifdef LINUX
		setCgroupMemoryLimits(cgroup);
#endif
		setupOOMEvent(cgroup);
	}

    m_statistics.Reconfig();

	// Now that the job is started, decrease the likelihood that the starter
	// is killed instead of the job itself.
	if (retval)
	{
		setupOOMScore(0,0);
	}

#endif

	return retval;
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );
	static unsigned int max_rss = 0;
#if HAVE_PSS
	static unsigned int max_pss = 0;
#endif

	ProcFamilyUsage current_usage;
	if( m_proc_exited ) {
		current_usage = m_final_usage;
	} else {
		if (daemonCore->Get_Family_Usage(JobPid, current_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage in "
					"VanillaProc::PublishUpdateAd() for pid %d\n", JobPid);
			return false;
		}
	}

	ProcFamilyUsage reported_usage = m_checkpoint_usage;
	reported_usage += current_usage;
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
void VanillaProc::notifySuccessfulPeriodicCheckpoint() { /* FIXME (#4969) */ }

void VanillaProc::recordFinalUsage() {
	if( daemonCore->Get_Family_Usage(JobPid, m_final_usage) == FALSE ) {
		dprintf( D_ALWAYS, "error getting family usage for pid %d in "
			"VanillaProc::JobReaper()\n", JobPid );
	}
}

void VanillaProc::killFamilyIfWarranted() {
	// Kill_Family() will (incorrectly?) kill the SSH-to-job daemon
	// if we're using dedicated accounts, so don't unless we know
	// we're the only job.
	if ( ! m_dedicated_account || Starter->numberOfJobs() == 1 ) {
		dprintf( D_PROCFAMILY, "About to call Kill_Family()\n" );
		daemonCore->Kill_Family( JobPid );
	} else {
		dprintf( D_PROCFAMILY, "Postponing call to Kill_Family() "
			"(perhaps due to ssh_to_job)\n" );
	}
}

void VanillaProc::restartCheckpointedJob() {
	ProcFamilyUsage last_usage;
	if( daemonCore->Get_Family_Usage( JobPid, last_usage ) == FALSE ) {
		dprintf( D_ALWAYS, "error getting family usage for pid %d in "
			"VanillaProc::restartCheckpointedJob()\n", JobPid );
	}
	m_checkpoint_usage += last_usage;

	if( Starter->jic->uploadCheckpointFiles() ) {
			notifySuccessfulPeriodicCheckpoint();
	} else {
			// We assume this is a transient failure and will try
			// to transfer again after the next periodic checkpoint.
			dprintf( D_ALWAYS, "Failed to transfer checkpoint.\n" );
	}

	// While it's arguably sensible to kill the process family
	// before we restart the job, that would mean that checkpointing
	// would behave differently during ssh-to-job, which seems bad.
	// killFamilyIfWarranted();

	m_proc_exited = false;
	StartJob();
}

bool
VanillaProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"Inside VanillaProc::JobReaper()\n");

	//
	// Run all the reapers first, since some of them change the exit status.
	//
	if( m_pid_ns_status_filename.length() > 0 ) {
		status = pidNameSpaceReaper( status );
	}
	bool jobExited = OsProc::JobReaper( pid, status );
	if( pid != JobPid ) { return jobExited; }

#if defined(LINUX)
	// On newer kernels if memory.use_hierarchy==1, then we cannot disable
	// the OOM killer.  Hence, we have to be ready for a SIGKILL to be delivered
	// by the kernel at the same time we get the notification.  Hence, if we
	// see an exit signal, we must also check the event file descriptor.
	//
	// outOfMemoryEvent() is aware of checkpointing and will mention that
	// the OOM event happened during a checkpoint.
	int efd = -1;
	if( (m_oom_efd >= 0) && daemonCore->Get_Pipe_FD(m_oom_efd, &efd) && (efd != -1) ) {
		Selector selector;
		selector.add_fd(efd, Selector::IO_READ);
		selector.set_timeout(0);
		selector.execute();
		if( !selector.failed() && !selector.timed_out() && selector.has_ready() && selector.fd_ready(efd, Selector::IO_READ) ) {
			outOfMemoryEvent( m_oom_efd );
		}
	}
#endif

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
				return true;
			}

			restartCheckpointedJob();
			isCheckpointing = false;
			return false;
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
			Starter->jic->holdJob( holdMessage.c_str(), CONDOR_HOLD_CODE_FailedToCheckpoint, exit_status );
			Starter->Hold();
			return true;
		}
	} else if( wantsFileTransferOnCheckpointExit && exit_status == successfulCheckpointStatus ) {
		dprintf( D_FULLDEBUG, "Inside VanillaProc::JobReaper() and the job self-checkpointed.\n" );

		if( isSoftKilling ) {
			notifySuccessfulEvictionCheckpoint();
			return true;
		} else {
			restartCheckpointedJob();
			return false;
		}
	} else {
		// If the parent job process died, clean up all of the job's processes.
		killFamilyIfWarranted();

		// Record final usage stats for this process family, since
		// once the reaper returns, the family is no longer
		// registered with DaemonCore and we'll never be able to
		// get this information again.
		recordFinalUsage();

		return jobExited;
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

	if (m_oom_efd != -1) {dprintf(D_FULLDEBUG, "Closing event FD pipe in shutdown %d.\n", m_oom_efd);}
	cleanupOOM();

	return false;	// shutdown is pending, so return false
}

/*
 * Clean up any file descriptors associated with the OOM control.
 */
void
VanillaProc::cleanupOOM()
{
	if (m_oom_efd != -1)
	{
		daemonCore->Close_Pipe(m_oom_efd);
		daemonCore->Close_Pipe(m_oom_efd2);
		m_oom_efd = -1;
		m_oom_efd2 = -1;
	}
	if (m_oom_fd != -1)
	{
		close(m_oom_fd);
		m_oom_fd = -1;
	}
}

/*
 * This will be called when the event fd fires, indicating an OOM event.
 */
int
VanillaProc::outOfMemoryEvent(int /* fd */)
{

	/* The cgroup API generates this notification whenever the OOM fires OR
	 * the cgroup is removed. If the cgroups are not pre-created, the kernel will
	 * remove the cgroup when the job completes. So if we land here and there are
	 * no more job pids, we assume the cgroup was removed and just ignore the event.
	 * However, if we land here and we still have job pids, we assume the OOM fired
	 * and thus we place the job on hold. See gt#3824.
	 */

	// If we have no jobs left, prolly just cgroup removed, so do nothing and return
	
	if (num_pids == 0) {
		dprintf(D_FULLDEBUG, "Closing event FD pipe %d.\n", m_oom_efd);
		cleanupOOM();
		return 0;
	}

	// The OOM killer has fired, and the process is frozen.  However,
	// the cgroup still has accurate memory usage.  Let's grab that
	// and make a final update, so the user can see exactly how much
	// memory they used.
	ClassAd updateAd;
	PublishUpdateAd( &updateAd );
	Starter->jic->periodicJobUpdate( &updateAd, true );
	int usage = 0;
	updateAd.LookupInteger(ATTR_MEMORY_USAGE, usage);

		//
#ifdef LINUX
	if (param_boolean("IGNORE_LEAF_OOM", true)) {
		// if memory.use_hierarchy is 1, then hitting the limit at
		// the parent notifies all children, even if those children
		// are below their usage.  If we are below our usage, ignore
		// the OOM, and continue running.  Hopefully, some process
		// will be killed, and when it does, this job will get unfrozen
		// and continue running.

		if (usage < (0.9 * m_memory_limit)) {
			long long oomData = 0xdeadbeef;
			int efd = -1;
			ASSERT( daemonCore->Get_Pipe_FD(m_oom_efd, &efd) );
				// need to drain notification fd, or it will still
				// be hot, and we'll come right back here again
			int r = read(efd, &oomData, 8);

			dprintf(D_ALWAYS, "Spurious OOM event, usage is %d, slot size is %d megabytes, ignoring OOM (read %d bytes)\n", usage, m_memory_limit, r);
			return 0;
		}
	}
#endif

	std::stringstream ss;
	if (m_memory_limit >= 0) {
		ss << "Job has gone over memory limit of " << m_memory_limit << " megabytes. Peak usage: " << usage << " megabytes.";
	} else {
		ss << "Job has encountered an out-of-memory event.";
	}
	if( isCheckpointing ) {
		ss << "  This occurred while the job was checkpointing.";
	}

	dprintf( D_ALWAYS, "Job was held due to OOM event: %s\n", ss.str().c_str());

	dprintf(D_FULLDEBUG, "Closing event FD pipe %d.\n", m_oom_efd);
	cleanupOOM();

	// This ulogs the hold event and KILLS the shadow
	Starter->jic->holdJob(ss.str().c_str(), CONDOR_HOLD_CODE_JobOutOfResources, 0);

	return 0;
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

int
VanillaProc::setupOOMEvent(const std::string &cgroup_string)
{
#if !(defined(HAVE_EVENTFD) && defined(HAVE_EXT_LIBCGROUP))
	// Shut the compiler up.
	cgroup_string.size();
	return 0;
#else
	// Initialize the event descriptor
	int tmp_efd = eventfd(0, EFD_CLOEXEC);
	if (tmp_efd == -1) {
		dprintf(D_ALWAYS,
			"Unable to create new event FD for starter: %u %s\n",
			errno, strerror(errno));
		return 1;
	}

	// Find the memcg location on disk
	void * handle = NULL;
	struct cgroup_mount_point mount_info;
	int ret = cgroup_get_controller_begin(&handle, &mount_info);
	std::stringstream oom_control;
	std::stringstream event_control;
	bool found_memcg = false;
	while (ret == 0) {
		if (strcmp(mount_info.name, MEMORY_CONTROLLER_STR) == 0) {
			found_memcg = true;
			oom_control << mount_info.path << "/";
			event_control << mount_info.path << "/";
			break;
		}
		ret = cgroup_get_controller_next(&handle, &mount_info);
	}
	if (!found_memcg && (ret != ECGEOF)) {
		dprintf(D_ALWAYS,
			"Error while locating memcg controller for starter: %u %s\n",
			ret, cgroup_strerror(ret));
		return 1;
	}
	cgroup_get_controller_end(&handle);
	if (found_memcg == false) {
		dprintf(D_ALWAYS,
			"Memcg is not available; OOM notification disabled for starter.\n");
		return 1;
	}

	// Finish constructing the location of the control files
	oom_control << cgroup_string << "/memory.oom_control";
	std::string oom_control_str = oom_control.str();
	event_control << cgroup_string << "/cgroup.event_control";
	std::string event_control_str = event_control.str();

	// Open the oom_control and event control files
	TemporaryPrivSentry sentry(PRIV_ROOT);
	m_oom_fd = open(oom_control_str.c_str(), O_RDONLY | O_CLOEXEC);
	if (m_oom_fd == -1) {
		dprintf(D_ALWAYS,
			"Unable to open the OOM control file for starter: %u %s\n",
			errno, strerror(errno));
		return 1;
	}
	int event_ctrl_fd = open(event_control_str.c_str(), O_WRONLY | O_CLOEXEC);
	if (event_ctrl_fd == -1) {
		dprintf(D_ALWAYS,
			"Unable to open event control for starter: %u %s\n",
			errno, strerror(errno));
		return 1;
	}

	// Inform Linux we will be handling the OOM events for this container.
	int oom_fd2 = open(oom_control_str.c_str(), O_WRONLY | O_CLOEXEC);
	if (oom_fd2 == -1) {
		dprintf(D_ALWAYS,
			"Unable to open the OOM control file for writing for starter: %u %s\n",
			errno, strerror(errno));
		close(event_ctrl_fd);
		return 1;
	}
	const char limits [] = "1";
        ssize_t nwritten = full_write(oom_fd2, &limits, 1);
	if (nwritten < 0) {
		/* Newer kernels return EINVAL if you attempt to enable OOM management
		 * on a cgroup where use_hierarchy is set to 1 and it is not the parent
		 * cgroup.
		 *
		 * This is a common setup, so we log and move along.
		 *
		 * See also #4435.
		 */
		if (errno == EINVAL)
		{
			dprintf(D_FULLDEBUG, "Unable to setup OOM killer management because"
				" memory.use_hierarchy is enabled for this cgroup; consider"
				" disabling it for this host or set BASE_CGROUP=/.  The hold"
				" message for an OOM event may not be reliably set.\n");
		}
		else
		{
			dprintf(D_ALWAYS, "Failure when attempting to enable OOM killer "
				" management for this job (errno=%d, %s).\n", errno, strerror(errno));
			close(event_ctrl_fd);
			close(oom_fd2);
			close(tmp_efd);
			return 1;
		}

	}
	close(oom_fd2);

	// Create the subscription string:
	std::stringstream sub_ss;
	sub_ss << tmp_efd << " " << m_oom_fd;
	std::string sub_str = sub_ss.str();

	if ((nwritten = full_write(event_ctrl_fd, sub_str.c_str(), sub_str.size())) < 0) {
		dprintf(D_ALWAYS,
			"Unable to write into event control file for starter: %u %s\n",
			errno, strerror(errno));
		close(event_ctrl_fd);
		close(tmp_efd);
		return 1;
	}
	close(event_ctrl_fd);

	// Fool DC into talking to the eventfd
	int pipes[2]; pipes[0] = -1; pipes[1] = -1;
	int fd_to_replace = -1;
	if (!daemonCore->Create_Pipe(pipes, true) || pipes[0] == -1) {
		dprintf(D_ALWAYS, "Unable to create a DC pipe\n");
		close(tmp_efd);
		close(m_oom_fd);
		m_oom_fd = -1;
		return 1;
	}
	if (!daemonCore->Get_Pipe_FD(pipes[0], &fd_to_replace) || fd_to_replace == -1) {
		dprintf(D_ALWAYS, "Unable to lookup pipe's FD\n");
		close(tmp_efd);
		close(m_oom_fd); m_oom_fd = -1;
		daemonCore->Close_Pipe(pipes[0]);
		daemonCore->Close_Pipe(pipes[1]);
		return 1;
	}
	dup3(tmp_efd, fd_to_replace, O_CLOEXEC);
	close(tmp_efd);
	m_oom_efd = pipes[0];
	m_oom_efd2 = pipes[1];

	// Inform DC we want to receive notifications from this FD.
	if (-1 == daemonCore->Register_Pipe(pipes[0],"OOM event fd", static_cast<PipeHandlercpp>(&VanillaProc::outOfMemoryEvent),"OOM Event Handler",this,HANDLE_READ))
	{
		dprintf(D_ALWAYS, "Failed to register OOM event FD pipe.\n");
		daemonCore->Close_Pipe(pipes[0]);
		daemonCore->Close_Pipe(pipes[1]);
		m_oom_fd = -1;
		m_oom_efd = -1;
		m_oom_efd2 = -1;
	}
	dprintf(D_FULLDEBUG, "Subscribed the starter to OOM notification for this cgroup; jobs triggering an OOM will be put on hold.\n");
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

#if defined(HAVE_EXT_LIBCGROUP)
#ifdef LINUX
void 
VanillaProc::setCgroupMemoryLimits(const char *cgroup) {

		ClassAd * MachineAd = Starter->jic->machClassAd();
		std::string cgroup_string = cgroup;
		CgroupLimits climits(cgroup_string);

		// First, set the CPU shares
		int numCores = 1;
		if (MachineAd->LookupInteger(ATTR_CPUS, numCores)) {
			climits.set_cpu_shares(numCores*100);
		} else {
			dprintf(D_FULLDEBUG, "Invalid value of Cpus in machine ClassAd; ignoring.\n");
		}

		// Now, set the memory limits
		std::string mem_limit;
		param(mem_limit, "CGROUP_MEMORY_LIMIT_POLICY", "none");
		if (mem_limit == "none") {
			dprintf(D_ALWAYS, "Not enforcing cgroup memory limit because CGROUP_MEMORY_LIMIT_POLICY is \"none\".\n");
			return;
		}

		if ((mem_limit != "hard") && (mem_limit != "soft") && (mem_limit != "custom")) {
			dprintf(D_ALWAYS, "Not enforcing cgroup memory limit because CGROUP_MEMORY_LIMIT_POLICY is an unknown value: %s.\n", mem_limit.c_str());
			return;
		}

		// The default hard memory limit -- the amount of memory in the slot
		std::string hard_memory_limit_expr = "My.Memory";

		// The default soft memory limit -- 90% of the amount of memory in the slot
		std::string soft_memory_limit_expr = "0.9 * My.Memory";

		if (mem_limit == "soft") {
				// If the policy is soft, make the hard limit the total memory for this startd
				hard_memory_limit_expr = "My.TotalMemory";
				// If the policy is soft, make the soft limit the slot size
				soft_memory_limit_expr = "My.Memory";
		} else if (mem_limit == "custom") {
				param(hard_memory_limit_expr, "CGROUP_HARD_MEMORY_LIMIT_EXPR", "");
				if (hard_memory_limit_expr.empty()) {
					dprintf(D_ALWAYS, "Missing CGROUP_HARD_MEMORY_LIMIT_EXPR, this must be set when CGROUP_MEMORY_LIMIT_POLICY is custom\n");
					return;
				}
				param(soft_memory_limit_expr, "CGROUP_SOFT_MEMORY_LIMIT_EXPR", "");
				if (soft_memory_limit_expr.empty()) {
					dprintf(D_ALWAYS, "Missing CGROUP_SOFT_MEMORY_LIMIT_EXPR, this must be set when CGROUP_MEMORY_LIMIT_POLICY is custom\n");
					return;
				}
		}

		int64_t hard_limit = 0;
		int64_t soft_limit = 0;

		ExprTree *expr = nullptr;
		::ParseClassAdRvalExpr(hard_memory_limit_expr.c_str(), expr);
		if (expr == nullptr) {
			dprintf(D_ALWAYS, "Can't parse CGROUP_HARD_MEMORY_LIMIT_EXPR: %s, ignoring\n", hard_memory_limit_expr.c_str());
			return;
		}

		classad::Value value;
		int evalRet = EvalExprTree(expr, MachineAd, JobAd, value);
		if ((!evalRet) || (!value.IsNumber(hard_limit))) {
			dprintf(D_ALWAYS, "Can't evaluate CGROUP_HARD_MEMORY_LIMIT_EXPR: %s, ignoring\n", hard_memory_limit_expr.c_str());
			delete expr;
			return;
		}

		delete expr;
		expr = nullptr;

		::ParseClassAdRvalExpr(soft_memory_limit_expr.c_str(), expr);
		if (expr == nullptr) {
			dprintf(D_ALWAYS, "Can't parse CGROUP_SOFT_MEMORY_LIMIT_EXPR: %s, ignoring\n", soft_memory_limit_expr.c_str());
			return;
		}

		evalRet = EvalExprTree(expr, MachineAd, JobAd, value);
		if ((!evalRet) || (!value.IsNumber(soft_limit))) {
			dprintf(D_ALWAYS, "Can't evaluate CGROUP_SOFT_MEMORY_LIMIT_EXPR: %s, ignoring\n", soft_memory_limit_expr.c_str());
			delete expr;
			return;
		}
		delete expr;

		// cgroups prevents us from setting hard limits above
		// the memsw limit.  If we are reusing this cgroup,
		// we don't know what the previous values were
		// So, set mem to 0, memsw to +inf, so that the real
		// values can be set without interference

		climits.set_memory_limit_bytes(0, true);
		climits.set_memsw_limit_bytes(LONG_MAX);

		// If we get an OOM, check against this value to see if it our fault, or a global OOM
		m_memory_limit = soft_limit;
		climits.set_memory_limit_bytes(1024 * 1024 * hard_limit, false /* == hard */);
		climits.set_memory_limit_bytes(1024 * 1024 * soft_limit, true /* == soft */);

		// if DISABLE_SWAP_FOR_JOB is true, set swap to hard memory limit
		// otherwise, leave at infinity

		if (param_boolean("DISABLE_SWAP_FOR_JOB", false)) {
			climits.set_memsw_limit_bytes(1024 * 1024 * hard_limit);
		}
}
#endif
#endif
