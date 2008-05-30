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

#ifndef _GLEXEC_STARTER_H
#define _GLEXEC_STARTER_H

// This module provides helper functions for launching a Starter via GLExec.
// It is expected that the caller will call glexec_starter_prepare(), then
// Create_Process, then glexec_starter_handle_env(). Failure to call all three
// may create a mess.

// This beast does all the work to prepare us for launching a Starter via
// GLExec. It takes as input the args, env, and std FDs that would be used
// to launch the Starter without GLExec. It returns modifies versions of
// these that should then be handed to Create_Process().
//
bool glexec_starter_prepare(const char* starter_path,
                            const char* proxy_file,
                            const ArgList& orig_args,
                            const Env* orig_env,
                            const int orig_std_fds[3],
                            ArgList& glexec_args,
                            Env& glexec_env,
                            int glexec_std_fds[3]);

// After calling Create_Process(), the condor_glexec_wrapper program will
// be waiting to receive a set of environment variables to use by the
// Starter. This function handles sending the environment to the wrapper.
// This function must be called after Create_Process() and be given its
// return value (in order to determine whether it succeeded).
//
bool glexec_starter_handle_env(pid_t pid);

#endif
