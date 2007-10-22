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

#include "condor_common.h"
#include "condor_debug.h"
#include "starter_privsep_helper.h"
#include "condor_uid.h"
#include "condor_arglist.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

StarterPrivSepHelper privsep_helper;

bool StarterPrivSepHelper::s_instantiated = false;

StarterPrivSepHelper::StarterPrivSepHelper() :
	m_user_initialized(false),
	m_sandbox_initialized(false),
	m_sandbox_path(NULL)
{
	ASSERT(!s_instantiated);
	s_instantiated = true;
}

StarterPrivSepHelper::~StarterPrivSepHelper()
{
	if (m_sandbox_path != NULL) {
		free(m_sandbox_path);
	}
}

void
StarterPrivSepHelper::initialize_user(uid_t uid)
{
	ASSERT(!m_user_initialized);

	dprintf(D_FULLDEBUG,
	        "PrivSep: initializing UID %u\n",
	        (unsigned)uid);

	m_uid = uid;

	m_user_initialized = true;
}

void
StarterPrivSepHelper::initialize_user(const char* name)
{
	uid_t uid;
	if (!pcache()->get_user_uid(name, uid)) {
		EXCEPT("StarterPrivSepHelper::initialize_user: "
		           "%s not found in passwd cache",
		       name);
	}
	initialize_user(uid);
}

void
StarterPrivSepHelper::initialize_sandbox(const char* path)
{
	ASSERT(m_user_initialized);
	ASSERT(!m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "PrivSep: initializing sandbox: %s\n", path);

	if (!privsep_create_dir(m_uid, path)) {
		EXCEPT("StarterPrivSepHelper::initialize_sandbox: "
		           "privsep_create_dir error on %s",
		       path);
	}

	m_sandbox_path = strdup(path);
	ASSERT(m_sandbox_path);
	m_sandbox_owned_by_user = true;

	m_sandbox_initialized = true;
}

uid_t
StarterPrivSepHelper::get_uid()
{
	ASSERT(m_user_initialized);
	return m_uid;
}

void
StarterPrivSepHelper::chown_sandbox_to_user()
{
	ASSERT(m_sandbox_initialized);

	if (m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "StarterPrivSepHelper::chown_sandbox_to_user: "
		        	"sandbox already user-owned\n");
		return;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to the user\n");
	if (!privsep_chown_dir(m_uid, m_sandbox_path)) {
		EXCEPT("error changing sandbox ownership to the user");
	}

	m_sandbox_owned_by_user = true;
}

void
StarterPrivSepHelper::chown_sandbox_to_condor()
{
	ASSERT(m_sandbox_initialized);

	if (!m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "StarterPrivSepHelper::chown_sandbox_to_condor: "
		        	"sandbox already condor-owned\n");
		return;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to condor\n");
	if (!privsep_chown_dir(get_condor_uid(), m_sandbox_path)) {
		EXCEPT("error changing sandbox ownership to condor");
	}

	m_sandbox_owned_by_user = false;
}

int
StarterPrivSepHelper::create_process(const char* path,
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
