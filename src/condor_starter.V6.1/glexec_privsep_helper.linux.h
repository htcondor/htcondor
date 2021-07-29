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


#ifndef _GLEXEC_PRIVSEP_HELPER_H
#define _GLEXEC_PRIVSEP_HELPER_H

#include "privsep_helper.h"
#include "MyString.h"

class GLExecPrivSepHelper : public PrivSepHelper {

public:

	GLExecPrivSepHelper();
	~GLExecPrivSepHelper();

	// initialize the location of the sandbox and the
	// location of the user's proxy within
	//
	void initialize(const char* proxy, const char* sandbox);

	// change ownership of the sandbox to the user
	//
	bool chown_sandbox_to_user(PrivSepError &err);

	// change our state to "sandbox is owned by user"
	void set_sandbox_owned_by_user() { m_sandbox_owned_by_user=true; }

	// change ownership of the sandbox to condor
	//
	bool chown_sandbox_to_condor(PrivSepError &err);

	// drop an updated proxy into the job sandbox
	//
	bool update_proxy(const char* tmp_proxy);

	// launch the job as the user
	//
	int create_process(const char* path,
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
	                   int *       affinity_mask /* = 0 */,
	                   std::string & error_msg);

	// check if the proxy is currently valid
	//
	int proxy_valid_right_now();

private:

	// helper for calling out to scripts
	//
	int run_script(ArgList&,MyString &error_desc);

	// helper for interfacing with condor_glexec_wrapper
	int feed_wrapper(int pid,
	                 int sock_fds[2],
	                 Env& env,
	                 int dc_job_opts,
	                 int job_std_fds[3],
					 int &glexec_err_fd,
					 MyString *error_msg,
					 int *glexec_rc);

	// set once we're initialized
	//
	bool m_initialized;

	// path to the glexec binary
	//
	char* m_glexec;

	// path to the sandbox
	// 
	char* m_sandbox;

	// file name with the user's proxy
	//
	char* m_proxy;

	// paths to scripts used to aid with GLExec support
	//
	MyString m_setup_script;
	MyString m_run_script;
	MyString m_wrapper_script;
	MyString m_proxy_update_script;
	MyString m_cleanup_script;

	// tracks current ownership of the sandbox
	//
	bool m_sandbox_owned_by_user;

	int m_glexec_retries;
	int m_glexec_retry_delay;
};

#endif
