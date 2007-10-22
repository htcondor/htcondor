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
bool privsep_get_switchboard_response(FILE* err_fp);

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
                            FamilyInfo* family_info);

// make a directory owned by the given user
//
bool privsep_create_dir(uid_t uid, const char* pathname);

// remove a directory tree
//
bool privsep_remove_dir(uid_t uid, const char* pathname);

// change the ownership of a directory tree to the given user
//
bool privsep_chown_dir(uid_t uid, const char* pathname);

#endif
