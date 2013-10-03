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
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"
#include "dynuser.h"
#include "condor_config.h"
#include "domain_tools.h"
#include "classad_helpers.h"
#include "filesystem_remap.h"
#include "directory.h"
#include "subsystem_info.h"
#include "cgroup_limits.h"

#ifdef WIN32
#include "executable_scripts.WINDOWS.h"
extern dynuser* myDynuser;
#endif

#if defined(HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

extern CStarter *Starter;
FilesystemRemap * fs_remap = NULL;

VanillaProc::VanillaProc(ClassAd* jobAd) : OsProc(jobAd),
	m_memory_limit(-1),
	m_oom_fd(-1),
	m_oom_efd(-1)
{
#if !defined(WIN32)
	m_escalation_tid = -1;
#endif
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
	int			joblen				= strlen(jobtmp);
	const char	*extension			= joblen > 0 ? &(jobtmp[joblen-4]) : NULL;
	bool		binary_executable	= ( extension && 
										( MATCH == strcasecmp ( ".exe", extension ) || 
										  MATCH == strcasecmp ( ".com", extension ) ) ),
				java_universe		= ( CONDOR_UNIVERSE_JAVA == job_universe );
	ArgList		arguments;
	MyString	filename,
				jobname, 
				error;
	
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
					condor_basename ( jobname.Value () ) ) ) {
				filename.formatstr ( "condor_exec%s", extension );
				if (rename(CONDOR_EXEC, filename.Value()) != 0) {
					dprintf (D_ALWAYS, "VanillaProc::StartJob(): ERROR: "
							"failed to rename executable from %s to %s\n", 
							CONDOR_EXEC, filename.Value() );
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
			if ( !arguments.AppendArgsFromClassAd ( JobAd, &error ) ||
				 !arguments.InsertArgsIntoClassAd ( JobAd, NULL, 
				&error ) ) {

				dprintf (
					D_ALWAYS,
					"VanillaProc::StartJob(): ERROR: failed to "
					"get arguments from job ad: %s\n",
					error.Value () );

				return FALSE;

			}

			/** Since we know already we don't want this file returned
				to us, we explicitly add it to an exception list which
				will stop the file transfer mechanism from considering
				it for transfer back to its submitter */
			Starter->jic->removeFromOutputFiles (
				filename.Value () );

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

	if (fs_remap != NULL) {
        delete fs_remap;
        fs_remap = NULL;
    }
    
#if defined(LINUX)
	// on Linux, we also have the ability to track processes via
	// a phony supplementary group ID
	//
	gid_t tracking_gid = 0;
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		if (!can_switch_ids() &&
		    (Starter->condorPrivSepHelper() == NULL))
		{
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root or using PrivSep");
		}
		fi.group_ptr = &tracking_gid;
	}

	// Increase the OOM score of this process; the child will inherit it.
	// This way, the job will be heavily preferred to be killed over a normal process.
	// OOM score is currently exponential - a score of 4 is a factor-16 increase in
	// the OOM score.
	setupOOMScore(4);
#endif

#if defined(HAVE_EXT_LIBCGROUP)
	// Determine the cgroup
	std::string cgroup_base;
	param(cgroup_base, "BASE_CGROUP", "");
	MyString cgroup_str;
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
	if (CONDOR_UNIVERSE_LOCAL != job_universe && cgroup_base.length()) {
		MyString cgroup_uniq;
		std::string starter_name, execute_str;
		param(execute_str, "EXECUTE", "EXECUTE_UNKNOWN");
			// Note: Starter is a global variable from os_proc.cpp
		Starter->jic->machClassAd()->EvalString(ATTR_NAME, NULL, starter_name);
		if (starter_name.size() == 0) {
			char buf[16];
			sprintf(buf, "%d", getpid());
			starter_name = buf;
		}
		//ASSERT (starter_name.size());
		cgroup_uniq.formatstr("%s_%s", execute_str.c_str(), starter_name.c_str());
		const char dir_delim[2] = {DIR_DELIM_CHAR, '\0'};
		cgroup_uniq.replaceString(dir_delim, "_");
		cgroup_str.formatstr("%s%ccondor%s", cgroup_base.c_str(), DIR_DELIM_CHAR,
			cgroup_uniq.Value());
		cgroup_str += this->CgroupSuffix();
		
		cgroup = cgroup_str.Value();
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
       JobAd->EvalString("RequestedChroot", NULL, requested_chroot_name);
       const char * allowed_root_dirs = param("NAMED_CHROOT");
       if (requested_chroot_name.size()) {
               dprintf(D_FULLDEBUG, "Checking for chroot: %s\n", requested_chroot_name.c_str());
               StringList chroot_list(allowed_root_dirs);
               chroot_list.rewind();
               const char * next_chroot;
               bool acceptable_chroot = false;
               std::string requested_chroot;
               while ( (next_chroot=chroot_list.next()) ) {
                       MyString chroot_spec(next_chroot);
                       chroot_spec.Tokenize();
                       const char * chroot_name = chroot_spec.GetNextToken("=", false);
                       if (chroot_name == NULL) {
                               dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.Value());
                       }
                       const char * next_dir = chroot_spec.GetNextToken("=", false);
                       if (chroot_name == NULL) {
                               dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.Value());
                       }
                       dprintf(D_FULLDEBUG, "Considering directory %s for chroot %s.\n", next_dir, chroot_spec.Value());
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


    // NAMED_MOUNTS
    {
        std::string requested_fuse, fexec, fmnt;
        // RequestedFuse='Name1' Debatable on if it should be a list.
        JobAd->EvalString(ATTR_REQUEST_MNTS, NULL, requested_fuse);
        // NAMED_FUSE= 'Name1=Executable:/sand_box_mountpoint, Name2=Executable:/sand_box_mountpoint'
        const char * allowed_fuse_mounts = param("NAMED_MOUNTS");
        
        if (requested_fuse.size()) 
        {
            dprintf(D_FULLDEBUG, "Checking for fuse: %s\n", requested_fuse.c_str());
            
            StringList fuse_list(allowed_fuse_mounts);
            fuse_list.rewind();
            
            const char * next_fuse;
            bool acceptable_fuse = false;
           
            while ( (next_fuse=fuse_list.next()) ) {
                
                MyString fuse_spec(next_fuse);
                fuse_spec.Tokenize();
                
                const char * fuse_name = fuse_spec.GetNextToken("=", false);
                // e.g 'Name1=Executable:/sand_box_mountpoint'
                if ( fuse_name == NULL ) {
                    dprintf(D_ALWAYS, "Invalid named mount: %s\n", fuse_spec.Value());
                }
                else {
                    
                    dprintf(D_FULLDEBUG, "Considering mount %s.\n", fuse_name);
                    // Check to see if the names are = ?
                    if ( (strcmp(requested_fuse.c_str(), fuse_name) == 0)) 
                    {
                        const char * fuse_exec = fuse_spec.GetNextToken(":", false);
                        //'Executable:/sand_box_mountpoint'
                        if ( fuse_exec == NULL ) {
                            dprintf(D_ALWAYS, "Invalid named mount: %s\n", fuse_spec.Value());
                        }
                        else {
                            
                            const char * fuse_mnt_point = fuse_spec.GetNextToken(":", false);
                            if (fuse_mnt_point) 
                            {
                                fexec=fuse_exec;
                                
                                // 'SCRATCH'/fusemntpt
                                fmnt = Starter->GetWorkingDir();
                                fmnt+=fuse_mnt_point;
                                
                                acceptable_fuse = true;
                            }
                            
                        }
                    }
                    
                }
                
            }
            
            if (!acceptable_fuse) {
                return FALSE;
            }
            
            dprintf(D_FULLDEBUG, "Will attempt to set the Mount to %s.\n", requested_fuse.c_str());
            
            if (!fs_remap) {
                fs_remap = new FilesystemRemap();
            }
            
            // 
            if ( fs_remap->AddNamedMapping( fexec, fmnt ) )
            {
                dprintf(D_FULLDEBUG, "Failed to mount NAMED_MOUNT: %s.\n", requested_fuse.c_str());
                return FALSE;
            }
            
            dprintf(D_FULLDEBUG, "Adding Named Mount Mapping [%s]: %s -> %s.\n", requested_fuse.c_str(),fexec.c_str(),fmnt.c_str());
            
        }
        
    }
    // end autofuse mounting
#endif


	// On Linux kernel 2.4.19 and later, we can give each job its
	// own FS mounts.
	char * mount_under_scratch = param("MOUNT_UNDER_SCRATCH");
	if (mount_under_scratch) {

		std::string working_dir = Starter->GetWorkingDir();

		if (IsDirectory(working_dir.c_str())) {
			StringList mount_list(mount_under_scratch);
			free(mount_under_scratch);

			mount_list.rewind();
			if (!fs_remap) {
				fs_remap = new FilesystemRemap();
			}
			char * next_dir;
			while ( (next_dir=mount_list.next()) ) {
				if (!*next_dir) {
					// empty string?
					mount_list.deleteCurrent();
					continue;
				}
				std::string next_dir_str(next_dir);
				// Gah, I wish I could throw an exception to clean up these nested if statements.
				if (IsDirectory(next_dir)) {
					char * full_dir = dirscat(working_dir, next_dir_str);
					if (full_dir) {
						std::string full_dir_str(full_dir);
						delete [] full_dir; full_dir = NULL;
						if (!mkdir_and_parents_if_needed( full_dir_str.c_str(), S_IRWXU, PRIV_USER )) {
							dprintf(D_ALWAYS, "Failed to create scratch directory %s\n", full_dir_str.c_str());
							return FALSE;
						}
						dprintf(D_FULLDEBUG, "Adding mapping: %s -> %s.\n", full_dir_str.c_str(), next_dir_str.c_str());
						if (fs_remap->AddMapping(full_dir_str, next_dir_str)) {
							// FilesystemRemap object prints out an error message for us.
							return FALSE;
						}
					} else {
						dprintf(D_ALWAYS, "Unable to concatenate %s and %s.\n", working_dir.c_str(), next_dir_str.c_str());
						return FALSE;
					}
				} else {
					dprintf(D_ALWAYS, "Unable to add mapping %s -> %s because %s doesn't exist.\n", working_dir.c_str(), next_dir, next_dir);
				}
			}
		} else {
			dprintf(D_ALWAYS, "Unable to perform mappings because %s doesn't exist.\n", working_dir.c_str());
			return FALSE;
		}
	}

#if defined(LINUX)
	// On Linux kernel 2.6.24 and later, we can give each
	// job its own PID namespace
	if (param_boolean("USE_PID_NAMESPACES", false)) {
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
	}
	dprintf(D_FULLDEBUG, "PID namespace option: %s\n", fi.want_pid_namespace ? "true" : "false");
#endif


	// have OsProc start the job
	//
	int retval = OsProc::StartJob(&fi, fs_remap);


#if defined(HAVE_EXT_LIBCGROUP)

	// Set fairshare limits.  Note that retval == 1 indicates success, 0 is failure.
	// See Note near setup of param(BASE_CGROUP)
	if (CONDOR_UNIVERSE_LOCAL != job_universe && cgroup && retval) {
		std::string mem_limit;
		param(mem_limit, "CGROUP_MEMORY_LIMIT_POLICY", "soft");
		bool mem_is_soft = mem_limit == "soft";
		std::string cgroup_string = cgroup;
		CgroupLimits climits(cgroup_string);
		if (mem_is_soft || (mem_limit == "hard")) {
			ClassAd * MachineAd = Starter->jic->machClassAd();
			int MemMb;
			if (MachineAd->LookupInteger(ATTR_MEMORY, MemMb)) {
				uint64_t MemMb_big = MemMb;
				m_memory_limit = MemMb_big;
				climits.set_memory_limit_bytes(1024*1024*MemMb_big, mem_is_soft);
			} else {
				dprintf(D_ALWAYS, "Not setting memory soft limit in cgroup because "
					"Memory attribute missing in machine ad.\n");
			}
		} else if (mem_limit == "none") {
			dprintf(D_FULLDEBUG, "Not enforcing memory soft limit.\n");
		} else {
			dprintf(D_ALWAYS, "Invalid value of CGROUP_MEMORY_LIMIT_POLICY: %s.  Ignoring.\n", mem_limit.c_str());
		}

		// Now, set the CPU shares
		ClassAd * MachineAd = Starter->jic->machClassAd();
		int numCores = 1;
		if (MachineAd->LookupInteger(ATTR_CPUS, numCores)) {
			climits.set_cpu_shares(numCores*100);
		} else {
			dprintf(D_FULLDEBUG, "Invalid value of Cpus in machine ClassAd; ignoring.\n");
		}
		setupOOMEvent(cgroup);
	}

	// Now that the job is started, decrease the likelihood that the starter
	// is killed instead of the job itself.
	if (retval)
	{
		setupOOMScore(-4);
	}

#endif

	return retval;
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	ProcFamilyUsage* usage;
	ProcFamilyUsage cur_usage;
	if (m_proc_exited) {
		usage = &m_final_usage;
	}
	else {
		if (daemonCore->Get_Family_Usage(JobPid, cur_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage in "
					"VanillaProc::PublishUpdateAd() for pid %d\n", JobPid);
			return false;
		}
		usage = &cur_usage;
	}

		// Publish the info we care about into the ad.
	ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, (double)usage->sys_cpu_time);
	ad->Assign(ATTR_JOB_REMOTE_USER_CPU, (double)usage->user_cpu_time);

	ad->Assign(ATTR_IMAGE_SIZE, usage->max_image_size);
	ad->Assign(ATTR_RESIDENT_SET_SIZE, usage->total_resident_set_size);

	std::string memory_usage;
	if (param(memory_usage, "MEMORY_USAGE_METRIC", "((ResidentSetSize+1023)/1024)")) {
		ad->AssignExpr(ATTR_MEMORY_USAGE, memory_usage.c_str());
	}

#if HAVE_PSS
	if( usage->total_proportional_set_size_available ) {
		ad->Assign( ATTR_PROPORTIONAL_SET_SIZE, usage->total_proportional_set_size );
	}
#endif

	if (usage->block_read_bytes >= 0) {
		ad->Assign(ATTR_BLOCK_READ_KBYTES, usage->block_read_bytes/1024);
	}
	if (usage->block_write_bytes >= 0) {
		ad->Assign(ATTR_BLOCK_WRITE_KBYTES, usage->block_write_bytes/1024);
	}

		// Update our knowledge of how many processes the job has
	num_pids = usage->num_procs;

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


bool
VanillaProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"Inside VanillaProc::JobReaper()\n");

	if (pid == JobPid) {

			// To make sure that no job processes are still lingering
			// on the machine, call Kill_Family().
			//
			// HOWEVER, iff we are tracking process families via a
			// dedicated execute account, we want to delay killing until
			// there is only one job under the starter.  We do this to
			// prevent the starter from killing the SSH daemon early, and
			// therefore effectively killing any ssh_to_job session as soon
			// as the job exits on machine configured with a dedicated
			// execute account.
		if ( !m_dedicated_account || Starter->numberOfJobs() == 1 )
		{
			dprintf(D_PROCFAMILY,"About to call Kill_Family()\n");
			daemonCore->Kill_Family(JobPid);
		} else {
			dprintf(D_PROCFAMILY,
				"Postponing call to Kill_Family() (perhaps due to ssh_to_job)\n");
		}

			// Record final usage stats for this process family, since
			// once the reaper returns, the family is no longer
			// registered with DaemonCore and we'll never be able to
			// get this information again.
		if (daemonCore->Get_Family_Usage(JobPid, m_final_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage for pid %d in "
					"VanillaProc::JobReaper()\n", JobPid);
		}
	}

	if (fs_remap != NULL) {
        fs_remap->cleanup();
        delete fs_remap;
        fs_remap = NULL;
    }
	
	
		// This will reset num_pids for us, too.
	return OsProc::JobReaper( pid, status );
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

	// WE USED TO.....
	//
	// take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	//
	// TODO: should we make an explicit call to the procd here to tell
	// it to take a snapshot???

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	//
	OsProc::ShutdownGraceful();
	return false; // shutdown is pending (same as OsProc::ShutdownGraceful()
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

	if (m_oom_efd >= 0) {
		dprintf(D_FULLDEBUG, "Closing event FD pipe in shutdown %d.\n", m_oom_efd);
		daemonCore->Close_Pipe(m_oom_efd);
		m_oom_efd = -1;
	}
	if (m_oom_fd >= 0) {
		close(m_oom_fd);
		m_oom_fd = -1;
	}

	return false;	// shutdown is pending, so return false
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
		daemonCore->Close_Pipe(m_oom_efd);
		close(m_oom_fd);
		m_oom_efd = -1;
		m_oom_fd = -1;

		return 0;
	}

	std::stringstream ss;
	if (m_memory_limit >= 0) {
		ss << "Job has gone over memory limit of " << m_memory_limit << " megabytes.";
	} else {
		ss << "Job has encountered an out-of-memory event.";
	}
	Starter->jic->holdJob(ss.str().c_str(), CONDOR_HOLD_CODE_JobOutOfResources, 0);

	// this will actually clean up the job
	if ( Starter->Hold( ) ) {
		dprintf( D_FULLDEBUG, "All jobs were removed due to OOM event.\n" );
		Starter->allJobsDone();
	}

	dprintf(D_FULLDEBUG, "Closing event FD pipe %d.\n", m_oom_efd);
	daemonCore->Close_Pipe(m_oom_efd);
	close(m_oom_fd);
	m_oom_efd = -1;
	m_oom_fd = -1;

	Starter->ShutdownFast();

	return 0;
}

int
VanillaProc::setupOOMScore(int new_score)
{

#if !(defined(HAVE_EVENTFD))
	if (new_score) // Done to suppress compiler warnings.
		return 0;
	return 0;
#else 
	TemporaryPrivSentry sentry(PRIV_ROOT);
	// oom_adj is deprecated on modern kernels and causes a deprecation warning when used.
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
		// oom_score_adj is linear; oom_adj was exponential.
		if (new_score > 0)
			new_score = 1 << new_score;
		else
			new_score = -(1 << -new_score);
	}

	std::stringstream ss;
	ss << new_score;
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
	m_oom_efd = eventfd(0, EFD_CLOEXEC);
	if (m_oom_efd == -1) {
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
		cgroup_get_controller_next(&handle, &mount_info);
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
		dprintf(D_ALWAYS,
			"Unable to set OOM control to %s for starter: %u %s\n",
				limits, errno, strerror(errno));
		close(event_ctrl_fd);
		close(oom_fd2);
		return 1;
	}
	close(oom_fd2);

	// Create the subscription string:
	std::stringstream sub_ss;
	sub_ss << m_oom_efd << " " << m_oom_fd;
	std::string sub_str = sub_ss.str();

	if ((nwritten = full_write(event_ctrl_fd, sub_str.c_str(), sub_str.size())) < 0) {
		dprintf(D_ALWAYS,
			"Unable to write into event control file for starter: %u %s\n",
			errno, strerror(errno));
		close(event_ctrl_fd);
		return 1;
	}
	close(event_ctrl_fd);

	// Fool DC into talking to the eventfd
	int pipes[2]; pipes[0] = -1; pipes[1] = -1;
	int fd_to_replace = -1;
	if (daemonCore->Create_Pipe(pipes, true) == -1 || pipes[0] == -1) {
		dprintf(D_ALWAYS, "Unable to create a DC pipe\n");
		close(m_oom_efd);
		m_oom_efd = -1;
		close(m_oom_fd);
		m_oom_fd = -1;
		return 1;
	}
	if ( daemonCore->Get_Pipe_FD(pipes[0], &fd_to_replace) == -1 || fd_to_replace == -1) {
		dprintf(D_ALWAYS, "Unable to lookup pipe's FD\n");
		close(m_oom_efd); m_oom_efd = -1;
		close(m_oom_fd); m_oom_fd = -1;
		daemonCore->Close_Pipe(pipes[0]);
		daemonCore->Close_Pipe(pipes[1]);
		return 1;
	}
	dup3(m_oom_efd, fd_to_replace, O_CLOEXEC);
	close(m_oom_efd);
	m_oom_efd = pipes[0];

	// Inform DC we want to recieve notifications from this FD.
	daemonCore->Register_Pipe(pipes[0],"OOM event fd", static_cast<PipeHandlercpp>(&VanillaProc::outOfMemoryEvent),"OOM Event Handler",this,HANDLE_READ);
	return 0;
#endif
}

