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
StarterPrivSepHelper::initialize_user(uid_t uid, gid_t gid)
{
	ASSERT(!m_user_initialized);

	dprintf(D_FULLDEBUG,
	        "PrivSep: initializing UID %u and GID %u\n",
	        (unsigned)uid,
	        (unsigned)gid);

	m_uid = uid;
	m_gid = gid;

	m_user_initialized = true;
}

void
StarterPrivSepHelper::initialize_user(const char* name)
{
	uid_t uid;
	gid_t gid;
	if (!pcache()->get_user_ids(name, uid, gid)) {
		EXCEPT("StarterPrivSepHelper::initialize_user: "
		           "%s not found in passwd cache",
		       name);
	}
	initialize_user(uid, gid);
}

void
StarterPrivSepHelper::initialize_sandbox(const char* path)
{
	ASSERT(m_user_initialized);
	ASSERT(!m_sandbox_initialized);

	dprintf(D_FULLDEBUG, "PrivSep: initializing sandbox: %s\n", path);

	if (!privsep_create_dir(m_uid, m_gid, path)) {
		EXCEPT("StarterPrivSepHelper::initialize_sandbox: "
		           "privsep_create_dir error on %s",
		       path);
	}

	m_sandbox_path = strdup(path);
	ASSERT(m_sandbox_path);
	m_sandbox_owned_by_user = true;

	m_sandbox_initialized = true;
}

void
StarterPrivSepHelper::get_user_ids(uid_t& uid, gid_t& gid)
{
	ASSERT(m_user_initialized);
	uid = m_uid;
	gid = m_gid;
}

int
StarterPrivSepHelper::open_user_file(const char* path,
                                     int flags,
                                     mode_t mode)
{
	ASSERT(m_user_initialized);
	dprintf(D_FULLDEBUG,
	        "PrivSep: using Switchboard to open user file '%s'\n",
	        path);
	return privsep_open(m_uid, m_gid, path, flags, mode);
}

int
StarterPrivSepHelper::open_sandbox_file(const char* path,
                                        int flags,
                                        mode_t mode)
{
	ASSERT(m_sandbox_initialized);

	if (m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "PrivSep: using Switchboard to open sandbox file '%s'\n",
		        path);
		return privsep_open(m_uid, m_gid, path, flags, mode);
	}
	else {
		dprintf(D_FULLDEBUG,
		        "PrivSep: directly opening sandbox file '%s'\n",
		        path);
		return safe_open_wrapper(path, flags, mode);
	}
}

FILE*
StarterPrivSepHelper::fopen_sandbox_file(const char* path, const char* mode)
{
	int flags;
	if (mode[0] == 'r') {
		flags = O_RDONLY;
	}
	else if (mode[0] == 'w') {
		flags = O_WRONLY;
	}
	else {
		EXCEPT("StarterPrivSepHelper::fopen_sandbox_file: "
		           "unable to handle mode: %s",
		       mode);
	}

	int fd = open_sandbox_file(path, flags, 0644);
	if (fd == -1) {
		return NULL;
	}
	FILE* fp = fdopen(fd, mode);
	if (fp == NULL) {
		dprintf(D_ALWAYS,
		        "PrivSepStarterHelper::fopen_sandbox_file: "
		            "fdopen error: %s (%d)\n",
		        strerror(errno),
		        errno);
		close(fd);
		return NULL;
	}

	return fp;
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
	if (!privsep_chown_dir(m_uid, m_gid, m_sandbox_path)) {
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
	if (!privsep_chown_dir(get_condor_uid(),
	                       get_condor_gid(),
	                       m_sandbox_path)) {
		EXCEPT("error changing sandbox ownership to condor");
	}

	m_sandbox_owned_by_user = false;
}

void
StarterPrivSepHelper::setup_exec_as_user(MyString& exe, ArgList& args)
{
	ASSERT(m_user_initialized);
	if (!privsep_setup_exec_as_user(m_uid, m_gid, exe, args)) {
		EXCEPT("StarterPrivSepHelper::setup_exec_as_user: "
		           "privsep_setup_exec_as_user error");
	}
}

bool
StarterPrivSepHelper::rename(const char* old_path, const char* new_path)
{
	ASSERT(m_user_initialized);
	dprintf(D_FULLDEBUG,
	        "PrivSep: renaming %s to %s as the user\n",
	        old_path,
	        new_path);
	return privsep_rename(m_uid, m_gid, old_path, new_path);
}

bool
StarterPrivSepHelper::chmod(const char* path, mode_t mode)
{
	ASSERT(m_user_initialized);
	dprintf(D_FULLDEBUG,
	        "PrivSep: about to chmod %s to 0%o as the user\n",
	        path,
	        (unsigned)mode);
	return privsep_chmod(m_uid, m_gid, path, mode);
}

