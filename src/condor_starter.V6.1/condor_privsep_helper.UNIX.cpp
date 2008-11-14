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
#include "condor_privsep_helper.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_arglist.h"
#include "condor_daemon_core.h"

bool CondorPrivSepHelper::s_instantiated = false;

CondorPrivSepHelper::CondorPrivSepHelper() :
	m_user_initialized(false),
	m_sandbox_initialized(false),
	m_uid(0),
	m_sandbox_path(NULL)
{
	ASSERT(!s_instantiated);
	s_instantiated = true;
}

CondorPrivSepHelper::~CondorPrivSepHelper()
{
	if (m_sandbox_path != NULL) {
		free(m_sandbox_path);
	}
}

void
CondorPrivSepHelper::initialize_user(uid_t uid)
{
	ASSERT(!m_user_initialized);

	dprintf(D_FULLDEBUG,
	        "PrivSep: initializing UID %u\n",
	        (unsigned)uid);

	m_uid = uid;

	m_user_initialized = true;
}

void
CondorPrivSepHelper::initialize_user(const char* name)
{
	uid_t uid;
	if (!pcache()->get_user_uid(name, uid)) {
		EXCEPT("CondorPrivSepHelper::initialize_user: "
		           "%s not found in passwd cache",
		       name);
	}
	initialize_user(uid);
}

void
CondorPrivSepHelper::initialize_sandbox(const char* path)
{
	ASSERT(m_user_initialized);
	ASSERT(!m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "PrivSep: initializing sandbox: %s\n", path);

	if (!privsep_create_dir(get_condor_uid(), path)) {
		EXCEPT("CondorPrivSepHelper::initialize_sandbox: "
		           "privsep_create_dir error on %s",
		       path);
	}

	m_sandbox_path = strdup(path);
	ASSERT(m_sandbox_path);

	m_sandbox_initialized = true;
}

uid_t
CondorPrivSepHelper::get_uid()
{
	ASSERT(m_user_initialized);
	return m_uid;
}

void
CondorPrivSepHelper::chown_sandbox_to_user()
{
	ASSERT(m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "changing sandbox ownership to the user\n");
	if (!privsep_chown_dir(m_uid, get_condor_uid(), m_sandbox_path)) {
		EXCEPT("error changing sandbox ownership to the user");
	}
}

void
CondorPrivSepHelper::chown_sandbox_to_condor()
{
	ASSERT(m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "changing sandbox ownership to condor\n");
	if (!privsep_chown_dir(get_condor_uid(), m_uid, m_sandbox_path)) {
		EXCEPT("error changing sandbox ownership to condor");
	}
}

int
CondorPrivSepHelper::create_process(const char* path,
                                     ArgList&    args,
                                     Env&        env,
                                     const char* iwd,
                                     int         std_fds[3],
                                     const char* std_file_names[3],
                                     int         nice_inc,
                                     size_t*     core_size_ptr,
                                     int         reaper_id,
                                     int         dc_job_opts,
                                     FamilyInfo* family_info)
{
	ASSERT(m_user_initialized);
	return privsep_launch_user_job(m_uid,
	                               path,
	                               args,
	                               env,
	                               iwd,
	                               std_fds,
	                               std_file_names,
	                               nice_inc,
	                               core_size_ptr,
	                               reaper_id,
	                               dc_job_opts,
	                               family_info);
}
