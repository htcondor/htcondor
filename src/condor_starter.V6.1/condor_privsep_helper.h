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


#ifndef _CONDOR_PRIVSEP_HELPER_H
#define _CONDOR_PRIVSEP_HELPER_H

#include "privsep_helper.h"
#include "../condor_privsep/condor_privsep.h"

// helper class for allowing the Starter to run in PrivSep mode. mainly,
// this takes care of two things:
//   1) storing the user's UID for passing to the PrivSep Switchboard
//   2) keeping track of who owns the sandbox. this will normally be the user,
//      but the FileTransfer object will chown the sandbox to condor so it can
//      do its thing

class CondorPrivSepHelper : public PrivSepHelper {

public:

	CondorPrivSepHelper();
	~CondorPrivSepHelper();

	// initialize the UID
	//
#if !defined(WIN32)
	void initialize_user(uid_t);
#endif
	void initialize_user(const char* name);

	// initialize the sandbox
	//
	void initialize_sandbox(const char* path);

	// report disk space used by sandbox dir
	//
	bool get_exec_dir_usage(off_t *usage);

#if !defined(WIN32)
	// get the initialized UID
	//
	uid_t get_uid();
#endif

	// get the name of the user (NULL if not set)
	//
	char const *get_user_name();

	// change ownership of the sandbox to the user
	//
	bool chown_sandbox_to_user(PrivSepError &err);

	// change our state to "sandbox is owned by user"
	void set_sandbox_owned_by_user() { m_sandbox_owned_by_user=true; }

	// change ownership of the sandbox to condor
	//
	bool chown_sandbox_to_condor(PrivSepError &err);

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
					   int *       affinity_mask = 0,
					   MyString *  error_msg = NULL);

private:

	// we only want one of these instantiated, so keep a static flag
	//
	static bool s_instantiated;

	// set true once we've been initialized the user info
	//
	bool m_user_initialized;

	// set true once we've initialized the sandbox info
	//
	bool m_sandbox_initialized;

#if !defined(WIN32)
	// the "user priv" UID
	//
	uid_t m_uid;
#endif
	char* m_user_name;

	// the sandbox directory name
	//
	char* m_sandbox_path;

	// true when the sandbox is owned by the user, not condor
	//
	bool m_sandbox_owned_by_user;
};

#endif
