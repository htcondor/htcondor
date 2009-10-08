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
#include "condor_blkng_full_disk_io.h"
#include "fdpass.h"
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
	m_wrapper_script.sprintf("%s/condor_glexec_job_wrapper", libexec);
	m_proxy_update_script.sprintf("%s/condor_glexec_update_proxy", libexec);
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

bool
GLExecPrivSepHelper::update_proxy(const char* new_proxy)
{
	ASSERT(m_initialized);

	MyString glexec_arg = m_glexec;
	if (!m_sandbox_owned_by_user) {
		glexec_arg = "-";
	}

	ArgList args;
	args.AppendArg(m_proxy_update_script);
	args.AppendArg(glexec_arg);
	args.AppendArg(new_proxy);
	args.AppendArg(m_proxy);
	args.AppendArg(m_sandbox);

	return (run_script(args) == 0);
}

int
GLExecPrivSepHelper::create_process(const char* path,
                                    ArgList&    args,
                                    Env&        env,
                                    const char* iwd,
                                    int         orig_std_fds[3],
                                    const char* std_file_names[3],
                                    int         nice_inc,
                                    size_t*     core_size_ptr,
                                    int         reaper_id,
                                    int         dc_job_opts,
                                    FamilyInfo* family_info)
{
	ASSERT(m_initialized);

	// make a copy of std FDs so we're not messing w/ our caller's
	// memory
	int std_fds[3] = {-1, -1, -1};
	if (orig_std_fds != NULL) {
		memcpy(std_fds, orig_std_fds, 3 * sizeof(int));
	}

	ArgList modified_args;
	modified_args.AppendArg(m_run_script);
	modified_args.AppendArg(m_glexec);
	modified_args.AppendArg(m_proxy);
	modified_args.AppendArg(m_sandbox);
	modified_args.AppendArg(m_wrapper_script);
	for (int i = 0; i < 3; i++) {
		modified_args.AppendArg((std_fds[i] == -1) ?
		                            std_file_names[i] : "-");
	}
	modified_args.AppendArg(path);
	for (int i = 1; i < args.Count(); i++) {
		modified_args.AppendArg(args.GetArg(i));
	}

	// setup a UNIX domain socket for communicating with
	// condor_glexec_wrapper (see comment above feed_wrapper()
	// for details
	//
	int sock_fds[2];
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sock_fds) == -1)
	{
		dprintf(D_ALWAYS,
		        "GLEXEC: socketpair error: %s\n",
		        strerror(errno));
		return false;
	}
	int std_in = std_fds[0];
	std_fds[0] = sock_fds[1];

	FamilyInfo fi;
	FamilyInfo* fi_ptr = (family_info != NULL) ? family_info : &fi;
	MyString proxy_path;
	proxy_path.sprintf("%s.condor/%s", m_sandbox, m_proxy);
	fi_ptr->glexec_proxy = proxy_path.Value();

	int pid = daemonCore->Create_Process(m_run_script.Value(),
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

	return feed_wrapper(pid, sock_fds, env, dc_job_opts, std_in);
}

// we launch the job via a wrapper. the full chain of exec() calls looks
// like:
//
// (condor_starter)->(condor_glexec_run)->(glexec)->(condor_glexec_wrapper)->(job)
//
// glexec_wrapper serves two purposes:
//   - it allows us to pass environment variables to the job, which glexec
//     (as of 08/2008) does not support
//   - it allows us to distinguish between failures in the job and failures
//     in condor_glexec_run or glexec or condor_glexec_wrapper
//
// this function:
//   - sends the job's environment over to the wrapper's stdin (which is a
//     UNIX domain socket we set up to communicate with the wrapper)
//   - sends the job's stdin FD to the wrapper
//   - waits for an error message from the wrapper; if EOF is read first, the
//     job was successfully exec()'d
//
int
GLExecPrivSepHelper::feed_wrapper(int pid,
                                  int sock_fds[2],
                                  Env& env,
                                  int dc_job_opts,
                                  int std_in)
{
	// we can now close the end of the socket that we handed down
	// to the wrapper; the other end we'll use to send stuff over
	//
	close(sock_fds[1]);

	// if pid is 0, Create_Process failed; just close the socket
	// and return
	//
	if (pid == FALSE) {
		close(sock_fds[0]);
		return pid;
	}

	// now send over the environment
	//
	Env env_to_send;
	if (HAS_DCJOBOPT_ENV_INHERIT(dc_job_opts)) {
		env_to_send.MergeFrom(environ);
	}
	env_to_send.MergeFrom(env);
	MyString env_str;
	MyString merge_err;
	if (!env_to_send.getDelimitedStringV2Raw(&env_str, &merge_err)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: Env::getDelimitedStringV2Raw error: %s\n",
		        merge_err.Value());
		close(sock_fds[0]);
		return FALSE;
	}
	const char* env_buf = env_str.Value();
	int env_len = env_str.Length() + 1;
	errno = 0;
	if (full_write(sock_fds[0], &env_len, sizeof(env_len)) != sizeof(env_len)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error sending env size to wrapper: %s\n",
		        strerror(errno));
		close(sock_fds[0]);
		return FALSE;
	}
	errno = 0;
	if (full_write(sock_fds[0], env_buf, env_len) != env_len) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error sending env to wrapper: %s\n",
		        strerror(errno));
		close(sock_fds[0]);
		return FALSE;
	}

	// now send over the FD that the Starter should use for stdin, if any
	//
	if (std_in != -1) {
		int pipe_fd;
		if (daemonCore->Get_Pipe_FD(std_in, &pipe_fd) == TRUE) {
			std_in = pipe_fd;
		}
		if (fdpass_send(sock_fds[0], std_in) == -1) {
			dprintf(D_ALWAYS, "GLEXEC: fdpass_send failed\n");
			close(sock_fds[0]);
			return FALSE;
		}
	}

	// now read any error messages produced by the wrapper
	//
	char err[256];
	ssize_t bytes = full_read(sock_fds[0], err, sizeof(err) - 1);
	if (bytes == -1) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error reading message from wrapper: %s\n",
		        strerror(errno));
		close(sock_fds[0]);
		return FALSE;
	}
	if (bytes > 0) {
		err[bytes] = '\0';
		dprintf(D_ALWAYS, "GLEXEC: error from wrapper: %s\n", err);
		return FALSE;
	}

	// if we're here, it all worked
	//
	close(sock_fds[0]);
	return pid;
}
