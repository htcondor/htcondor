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
#include "condor_privsep.h"
#include "privsep_fork_exec.h"

PrivSepForkExec::PrivSepForkExec() :
	m_in_fp(NULL),
	m_err_fp(NULL),
	m_child_in_fd(-1),
	m_child_err_fd(-1)
{
}

PrivSepForkExec::~PrivSepForkExec()
{
	if (m_in_fp != NULL) {
		fclose(m_in_fp);
	}
	if (m_err_fp != NULL) {
		fclose(m_err_fp);
	}
	if (m_child_in_fd != -1) {
		close(m_child_in_fd);
	}
	if (m_child_err_fd != -1) {
		close(m_child_err_fd);
	}
}

bool
PrivSepForkExec::init()
{
	return privsep_create_pipes(m_in_fp,
	                            m_child_in_fd,
	                            m_err_fp,
	                            m_child_err_fd);
}

FILE*
PrivSepForkExec::parent_begin()
{
	close(m_child_in_fd);
	close(m_child_err_fd);
	m_child_in_fd = m_child_err_fd = -1;
	return m_in_fp;
}

bool
PrivSepForkExec::parent_end()
{
	fclose(m_in_fp);
	m_in_fp = NULL;
	bool ret = privsep_get_switchboard_response(m_err_fp);
	m_err_fp = NULL;
	return ret;
}

void
PrivSepForkExec::in_child(MyString& cmd, ArgList& args)
{
	close(fileno(m_in_fp));
	close(fileno(m_err_fp));
	m_in_fp = m_err_fp = NULL;
	privsep_get_switchboard_command("exec",
	                                m_child_in_fd,
	                                m_child_err_fd,
	                                cmd,
	                                args);
}
