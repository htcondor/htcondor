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
#include "glexec_privsep_helper.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_arglist.h"
#include "condor_daemon_core.h"
#include "my_popen.h"

GLExecPrivSepHelper::GLExecPrivSepHelper() :
	m_initialized(false)
{
}

GLExecPrivSepHelper::~GLExecPrivSepHelper()
{
	if (m_initialized) {
		free(m_glexec);
		free(m_sandbox);
		free(m_proxy);
	}
}

int
GLExecPrivSepHelper::run_script(ArgList& args)
{
	FILE* fp = my_popen(args, "r", TRUE);
	if (fp == NULL) {
		dprintf(D_ALWAYS,
		        "GLExecPrivSepHelper::run_script: "
		            "my_popen failure on %s\n",
		        args.GetArg(0));
		return -1;
	}
	MyString str;
	while (str.readLine(fp, true));
	int ret = my_pclose(fp);
	if (ret != 0) {
		dprintf(D_ALWAYS,
		        "GLExecPrivSepHelper::run_script: %s exited "
		            "with status %d and following output:\n%s\n",
		        args.GetArg(0),
		        ret,
		        str.Value());
	}
	return ret;
}

void
GLExecPrivSepHelper::initialize(const char* proxy, const char* sandbox)
{
	ASSERT(!m_initialized);

	ASSERT(proxy != NULL);
	ASSERT(sandbox != NULL);

	dprintf(D_FULLDEBUG,
	        "GLEXEC: initializing with proxy %s and sandbox %s\n",
	        proxy,
	        sandbox);

	m_proxy = strdup(proxy);
	ASSERT(m_proxy != NULL);

	m_sandbox = strdup(sandbox);
	ASSERT(m_sandbox != NULL);

	m_glexec = param("GLEXEC");
	if (m_glexec == NULL) {
		EXCEPT("GLEXEC_JOB specified but GLEXEC not defined");
	}

	char* libexec = param("LIBEXEC");
	if (libexec == NULL) {
		EXCEPT("GLExec: LIBEXEC not defined");
	}
	m_setup_script.sprintf("%s/condor_glexec_setup", libexec);
	m_run_script.sprintf("%s/condor_glexec_run", libexec);
	m_cleanup_script.sprintf("%s/condor_glexec_cleanup", libexec);
	free(libexec);

	m_sandbox_owned_by_user = false;

	m_initialized = true;
}

void
GLExecPrivSepHelper::chown_sandbox_to_user()
{
	ASSERT(m_initialized);

	if (m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "GLExecPrivSepHelper::chown_sandbox_to_user: "
		            "sandbox already user-owned\n");
		return;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to the user\n");

	ArgList args;
	args.AppendArg(m_setup_script);
	args.AppendArg(m_glexec);
	args.AppendArg(m_proxy);
	args.AppendArg(m_sandbox);
	if (run_script(args) != 0) {
		EXCEPT("error changing sandbox ownership to the user");
	}

	m_sandbox_owned_by_user = true;
}

void
GLExecPrivSepHelper::chown_sandbox_to_condor()
{
	ASSERT(m_initialized);

	if (!m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "GLExecPrivSepHelper::chown_sandbox_to_condor: "
		            "sandbox already condor-owned\n");
		return;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to condor\n");

	ArgList args;
	args.AppendArg(m_cleanup_script);
	args.AppendArg(m_glexec);
	args.AppendArg(m_proxy);
	args.AppendArg(m_sandbox);
	if (run_script(args) != 0) {
		EXCEPT("error changing sandbox ownership to condor");
	}

	m_sandbox_owned_by_user = false;
}

int
GLExecPrivSepHelper::create_process(const char* path,
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
	ASSERT(m_initialized);

	ArgList modified_args;
	modified_args.AppendArg(m_run_script);
	modified_args.AppendArg(m_glexec);
	modified_args.AppendArg(m_proxy);
	modified_args.AppendArg(m_sandbox);
	if (std_fds != NULL) {
		for (int i = 0; i < 3; i++) {
			modified_args.AppendArg((std_fds[i] == -1) ?
			                            std_file_names[i] : "-");
		}
	}
	else {
		modified_args.AppendArg("-");
		modified_args.AppendArg("-");
		modified_args.AppendArg("-");
	}
	modified_args.AppendArg(path);
	for (int i = 1; i < args.Count(); i++) {
		modified_args.AppendArg(args.GetArg(i));
	}

	FamilyInfo fi;
	FamilyInfo* fi_ptr = (family_info != NULL) ? family_info : &fi;
	MyString proxy_path;
	proxy_path.sprintf("%s.condor/%s", m_sandbox, m_proxy);
	fi_ptr->glexec_proxy = proxy_path.Value();

	return daemonCore->Create_Process(m_run_script.Value(),
	                                  modified_args,
	                                  PRIV_USER_FINAL,
	                                  reaper_id,
	                                  FALSE,
	                                  &env,
	                                  iwd,
	                                  fi_ptr,
	                                  NULL,
	                                  std_fds,
	                                  NULL,
	                                  nice_inc,
	                                  NULL,
	                                  dc_job_opts,
	                                  core_size_ptr);
}
