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

int
StarterPrivSepHelper::create_process(const char*,
                                     ArgList&,
                                     Env&,
                                     const char*,
                                     int[3],
                                     const char*[3],
                                     int,
                                     size_t*,
                                     int,
                                     int,
                                     FamilyInfo*)
{
	EXCEPT("StarterPrivSepHelper: Windows support not implemented");
	return 0;
}