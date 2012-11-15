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


#ifndef _CONDOR_PRIVSEP_H
#define _CONDOR_PRIVSEP_H

#include "condor_uid.h"

class MyString;
class ArgList;
class Env;
struct FamilyInfo;

// are we running in PrivSep mode?
//
bool privsep_enabled();

// get pipes for communicating with a privsep switchboard
//
bool privsep_create_pipes(FILE*& in_fp,
                          int&   in_fd_for_child,
                          FILE*& err_fp,
                          int&   err_fd_for_child);

// get the path and arguments needed to exec a privsep switchboard
// for the given command string
//
void privsep_get_switchboard_command(const char* op,
                                     int         in_fd_for_child,
                                     int         err_fd_for_child,
                                     MyString&   cmd_path,
                                     ArgList&    args);

// use the error pipe to a switchboard process to get its response
//
bool privsep_get_switchboard_response(FILE* err_fp, MyString *response = NULL);

// the privsep_exec_set_* group of functions are for piping
// information to a switchboard that has been spawned with the
// "exec" command
//
void privsep_exec_set_uid(FILE* fp, uid_t uid);
void privsep_exec_set_path(FILE* fp, const char* path);
void privsep_exec_set_args(FILE* fp, ArgList& args);
void privsep_exec_set_env(FILE* fp, Env& env);
void privsep_exec_set_iwd(FILE* fp, const char* iwd);
void privsep_exec_set_inherit_fd(FILE* fp, int fd);
void privsep_exec_set_std_file(FILE* fp, int target_fd, const char* path);
void privsep_exec_set_is_std_univ(FILE* fp);
#if defined(LINUX)
void privsep_exec_set_tracking_group(FILE* fp, gid_t tracking_group);
#endif

// spawn a ProcD - our pid and birthday are passed in as a convenience
//
int privsep_spawn_procd(const char* path,
                        ArgList&    args,
                        int         std_fds[3],
                        int         reaper_id);

// launch a job as the given user
//
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
							int  	    *affinity_mask = 0);

// make a directory owned by the given user
//
bool privsep_create_dir(uid_t uid, const char* pathname);

// report disk usage for a specific directory and user
//
bool privsep_get_dir_usage(uid_t uid, const char* pathname, off_t *usage);

// remove a directory tree
//
bool privsep_remove_dir(const char* pathname);

// change the ownership of a directory tree to the given user.
// this operation will fail if anything it tries to chown is
// not owned by the given source UID. this prevents us from
// getting hacked via tricks like hard links
//
bool privsep_chown_dir(uid_t target_uid,
                       uid_t source_uid,
                       const char* pathname);

#endif
