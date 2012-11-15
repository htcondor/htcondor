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
#include "condor_holdcodes.h"

bool CondorPrivSepHelper::s_instantiated = false;

CondorPrivSepHelper::CondorPrivSepHelper() :
	m_user_initialized(false),
	m_sandbox_initialized(false),
	m_uid(0),
	m_user_name(NULL),
	m_sandbox_path(NULL),
	m_sandbox_owned_by_user(false)
{
	ASSERT(!s_instantiated);
	s_instantiated = true;
}

CondorPrivSepHelper::~CondorPrivSepHelper()
{
	if (m_sandbox_path != NULL) {
		free(m_sandbox_path);
	}
	if( m_user_name ) {
		free(m_user_name);
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

char const *
CondorPrivSepHelper::get_user_name()
{
	if( !m_user_initialized ) {
		return NULL;
	}
	if( m_user_name ) {
		return m_user_name;
	}
	pcache()->get_user_name(m_uid,m_user_name);
	return m_user_name;
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
	m_sandbox_owned_by_user = false;

	m_sandbox_initialized = true;
}

bool
CondorPrivSepHelper::get_exec_dir_usage(off_t *usage)
{
	ASSERT(m_user_initialized);
	ASSERT(m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "PrivSep: getting dirusage for %s\n", m_sandbox_path);

	if (!privsep_get_dir_usage(m_uid, m_sandbox_path, usage)) {
		dprintf(D_ALWAYS, "ERROR: PrivSep: failed dirusage for %i, %s\n",
				m_uid, m_sandbox_path);
		return false;
	}

	return true;
}

uid_t
CondorPrivSepHelper::get_uid()
{
	ASSERT(m_user_initialized);
	return m_uid;
}

bool
CondorPrivSepHelper::chown_sandbox_to_user(PrivSepError &err)
{
	ASSERT(m_sandbox_initialized);

	if (m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "CondorPrivSepHelper::chown_sandbox_to_user: "
		        	"sandbox already user-owned\n");
		return true;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to the user\n");
	if (!privsep_chown_dir(m_uid, get_condor_uid(), m_sandbox_path)) {
		err.setHoldInfo(
			CONDOR_HOLD_CODE_PrivsepChownSandboxToUser, 0,
			"error changing sandbox ownership to the user" );
		return false;
	}

	m_sandbox_owned_by_user = true;
	return true;
}

bool
CondorPrivSepHelper::chown_sandbox_to_condor(PrivSepError &err)
{
	ASSERT(m_sandbox_initialized);

	if (!m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "CondorPrivSepHelper::chown_sandbox_to_condor: "
		        	"sandbox already condor-owned\n");
		return true;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to condor\n");
	if (!privsep_chown_dir(get_condor_uid(), m_uid, m_sandbox_path)) {
		err.setHoldInfo(
			CONDOR_HOLD_CODE_PrivsepChownSandboxToCondor, 0,
			"error changing sandbox ownership to condor");
		return false;
	}

	m_sandbox_owned_by_user = false;
	return true;
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
                                     FamilyInfo* family_info,
									int *       affinity_mask,
									MyString *  /*error_msg*/)
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
	                               family_info,
								   affinity_mask);
}
