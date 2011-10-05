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


// this file contains PrivSep functions that use DeamonCore
// facilities

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_privsep.h"
#include "setenv.h"

static int
privsep_create_process(const char* cmd,
                       const char* path,
                       ArgList&    args,
                       Env*        env,
                       const char* iwd,
                       int         std_fds[3],
                       const char* std_file_names[3],
                       int         nice_inc,
                       size_t*     core_size_ptr,
                       int         reaper_id,
                       int         dc_job_opts,
                       FamilyInfo* family_info,
                       uid_t       uid,
					   int * 	   affinity_mask = 0)
{
	// if we're using privilege separation, we have to do a bit
	// of extra work to do to launch processes as other UIDs
	// since we need to use the PrivSep Swtichboard. the procedure
	// will be:
	//
	//   1) create two pipes over which we'll communicate with
	//      the switchboard
	//   2) call create process on the switchboard
	//   3) use the input pipe to give the switchboard info about
	//      what binary to run, what files to use for i/o, etc.
	//   4) use the error pipe to see if everything went ok

	// create the pipes
	//
	FILE* in_fp;
	int child_in_fd;
	FILE* err_fp;
	int child_err_fd;
	bool ok;
	ok = privsep_create_pipes(in_fp,
	                          child_in_fd,
	                          err_fp,
	                          child_err_fd);
	if (!ok) {
		dprintf(D_ALWAYS,
		        "privsep_create_process: privsep_create_pipes failure\n");
		errno = 0;
		return FALSE;
	}

	// fire up the switchboard
	//
	MyString sb_path;
	ArgList sb_args;
	privsep_get_switchboard_command(cmd,
	                                child_in_fd,
	                                child_err_fd,
	                                sb_path,
	                                sb_args);
	int pipe_fds[] = {child_in_fd, child_err_fd, 0};
	int pid = daemonCore->Create_Process(
		sb_path.Value(), // switchboard path
		sb_args,         // switchboard args
		PRIV_USER_FINAL, // priv states are ignored in priv sep mode
		reaper_id,       // reaper id
		FALSE,           // no command port
		NULL,            // we'll pass the job's real env via pipe
		NULL,            // we'll pass the job's real iwd via pipe
		family_info,     // tracking info for the ProcD
		NULL,            // no sockets to inherit
		std_fds,         // FDs that will become std{in,out,err}
		pipe_fds,        // tell DC to pass down the in/err pipe FDs
		nice_inc,        // niceness (TODO: need switchboard?)
		NULL,            // don't mess with the signal mask
		dc_job_opts,     // DC job options
		core_size_ptr,  // core size limit (TODO: need switchboard?)
		affinity_mask);  // Processor affinity mask
	close(child_in_fd);
	close(child_err_fd);
	if (pid == FALSE) {
		dprintf(D_ALWAYS, "privsep_create_process: DC::Create_Process error\n");
		fclose(in_fp);
		fclose(err_fp);
		return FALSE;
	}

	// feed the switchboard information on how to create the new
	// process
	//
	privsep_exec_set_uid(in_fp, uid);
	privsep_exec_set_path(in_fp, path);
	privsep_exec_set_args(in_fp, args);
	Env tmp_env;
	if (HAS_DCJOBOPT_ENV_INHERIT(dc_job_opts)) {
		tmp_env.MergeFrom(GetEnviron());
		if (env != NULL) {
			tmp_env.MergeFrom(*env);
		}
		env = &tmp_env;
	}
	if (env != NULL) {
		privsep_exec_set_env(in_fp, *env);
	}
	if (iwd != NULL) {
		privsep_exec_set_iwd(in_fp, iwd);
	}
	for (int i = 0; i < 3; i++) {
		if ((std_fds != NULL) && (std_fds[i] != -1)) {
			privsep_exec_set_inherit_fd(in_fp, i);
		}
		else if (std_file_names) {
			privsep_exec_set_std_file(in_fp, i, std_file_names[i]);
		}
	}
#if defined(LINUX)
	if ((family_info != NULL) && (family_info->group_ptr != NULL)) {
		privsep_exec_set_tracking_group(in_fp, *family_info->group_ptr);
	}
#endif
	fclose(in_fp);

	// check the switchboard's error pipe for any problems
	// (privsep_get_switchboard_response will fclose the error
	// pipe for us)
	//
	if (!privsep_get_switchboard_response(err_fp)) {
		dprintf(D_ALWAYS,
		        "privsep_create_process: "
		            "privsep_get_switchboard_response failure\n");
		errno = 0;
		return FALSE;
	}

	// if we've gotten here, everything worked
	//
	return pid;
}

int
privsep_spawn_procd(const char* path,
                    ArgList& args,
                    int std_fds[3],
                    int reaper_id)
{
	return privsep_create_process("pd",
	                              path,
	                              args,
	                              NULL,
	                              NULL,
	                              std_fds,
	                              NULL,
	                              0,
	                              NULL,
	                              reaper_id,
	                              0,
	                              NULL,
	                              0);
}

int privsep_launch_user_job(uid_t       uid,
                            const char* path,
                            ArgList&    args,
                            Env&        env,
                            const char* iwd,
                            int         std_fds[3],
                            const char* std_file_names[3],
                            int         nice_inc,
                            size_t*     core_size_ptr,
                            int         reaper_id,
                            int         dc_job_opts,
                            FamilyInfo* family_info,
							int *       affinity_mask)
{
	return privsep_create_process("exec",
	                              path,
	                              args,
	                              &env,
	                              iwd,
	                              std_fds,
	                              std_file_names,
	                              nice_inc,
	                              core_size_ptr,
	                              reaper_id,
	                              dc_job_opts,
	                              family_info,
	                              uid,
								  affinity_mask);
}
