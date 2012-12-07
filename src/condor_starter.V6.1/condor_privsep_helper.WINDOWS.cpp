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

// the methods in this file are all stubbed out for now, until we
// have support for privilege separation on Windows

bool CondorPrivSepHelper::s_instantiated = false;

CondorPrivSepHelper::CondorPrivSepHelper() :
	m_user_initialized(false),
	m_user_name(NULL),
	m_sandbox_initialized(false),
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
	if( m_user_name ) {
		free(m_user_name);
	}
}

void
CondorPrivSepHelper::initialize_user(const char* name)
{
	m_user_name = strdup(name);

	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
}

char const *
CondorPrivSepHelper::get_user_name()
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
	return m_user_name;
}

void
CondorPrivSepHelper::initialize_sandbox(const char* path)
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
}

bool
CondorPrivSepHelper::get_exec_dir_usage(off_t *usage)
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
}


bool
CondorPrivSepHelper::chown_sandbox_to_user(PrivSepError &)
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
	return false;
}

bool
CondorPrivSepHelper::chown_sandbox_to_condor(PrivSepError &)
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
	return false;
}

int
CondorPrivSepHelper::create_process(const char*,
                                     ArgList&,
                                     Env&,
                                     const char*,
                                     int[3],
                                     const char*[3],
                                     int,
                                     size_t*,
                                     int,
                                     int,
                                     FamilyInfo*,
                                     int *affinity_mask,
                                     MyString *  error_msg /*= NULL*/)
{
	EXCEPT("CondorPrivSepHelper: Windows support not implemented");
	return 0;
}
