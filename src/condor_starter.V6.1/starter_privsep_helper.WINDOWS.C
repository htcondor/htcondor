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

// the methods in this file are all stubbed out for now, until we
// have support for privilege separation on Windows
//

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
StarterPrivSepHelper::initialize_user(const char* name)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
}

void
StarterPrivSepHelper::initialize_sandbox(const char* path)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
}

int
StarterPrivSepHelper::open_user_file(const char* path,
                                     int flags,
                                     mode_t mode)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return -1;
}

int
StarterPrivSepHelper::open_sandbox_file(const char* path,
                                        int flags,
                                        mode_t mode)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return -1;
}

FILE*
StarterPrivSepHelper::fopen_sandbox_file(const char* path, const char* mode)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return NULL;
}

void
StarterPrivSepHelper::chown_sandbox_to_user()
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
}

void
StarterPrivSepHelper::chown_sandbox_to_condor()
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
}

void
StarterPrivSepHelper::setup_exec_as_user(ArgList& args)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
}

bool
StarterPrivSepHelper::rename(const char* old_path, const char* new_path)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return false;
}

bool
StarterPrivSepHelper::chmod(const char* path, mode_t mode)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return false;
}