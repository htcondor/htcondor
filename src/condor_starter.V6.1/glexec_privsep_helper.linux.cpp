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
#include "glexec_privsep_helper.linux.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_arglist.h"
#include "condor_daemon_core.h"
#include "condor_blkng_full_disk_io.h"
#include "fdpass.h"
#include "my_popen.h"
#include "globus_utils.h"
#include "condor_holdcodes.h"
#include "basename.h"

#define INVALID_PROXY_RC -10000

GLExecPrivSepHelper::GLExecPrivSepHelper() :
	m_initialized(false),m_glexec(0),  m_sandbox(0), m_proxy(0),m_sandbox_owned_by_user(false),
	m_glexec_retries(0),m_glexec_retry_delay(0)

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


/* proxy_valid_right_now()

   this function is used in this object to determine if glexec should actually
   be invoked.  glexec will always fail with an expired proxy, and there is
   overhead in invoking it.
*/
int
GLExecPrivSepHelper::proxy_valid_right_now()
{

	int result = TRUE;
		/* Note that set_user_priv is a no-op if condor is running as
		   non-root (the "usual" mode for invoking glexec) */
	priv_state priv_saved = set_user_priv();
	if (!m_proxy) {
		dprintf(D_FULLDEBUG, "GLExecPrivSepHelper::proxy_valid_right_now: no proxy defined\n");
		result = FALSE;
	} else {

		time_t expiration_time = x509_proxy_expiration_time(m_proxy);
		time_t now = time(NULL);

		if (expiration_time == -1) {
			dprintf(D_FULLDEBUG, "GLExecPrivSepHelper::proxy_valid_right_now: Globus error when getting proxy %s expiration: %s.\n", m_proxy, x509_error_string());
			result = FALSE;
		} else if (expiration_time < now) {
			dprintf(D_FULLDEBUG, "GLExecPrivSepHelper::proxy_valid_right_now: proxy %s expired %ld seconds ago!\n", m_proxy, now - expiration_time);
			result = FALSE;
		}
	}

	set_priv(priv_saved);

	return result;
}


int
GLExecPrivSepHelper::run_script(ArgList& args,MyString &error_desc)
{
	if (!proxy_valid_right_now()) {
		dprintf(D_ALWAYS, "GLExecPrivSepHelper::run_script: not invoking glexec since the proxy is not valid!\n");
		error_desc += "The job proxy is not valid.";
		return INVALID_PROXY_RC;
	}

		/* Note that set_user_priv is a no-op if condor is running as
		   non-root (the "usual" mode for invoking glexec) */
	priv_state priv_saved = set_user_priv();
	FILE* fp = my_popen(args, "r", TRUE);
	set_priv(priv_saved);
	if (fp == NULL) {
		dprintf(D_ALWAYS,
		        "GLExecPrivSepHelper::run_script: "
		            "my_popen failure on %s: errno=%d (%s)\n",
		        args.GetArg(0),
			errno,
			strerror(errno));
		return -1;
	}
	MyString str;
	while (str.readLine(fp, true));

	priv_saved = set_user_priv();
	int ret = my_pclose(fp);
	set_priv(priv_saved);

	if (ret != 0) {
		str.trim();
		dprintf(D_ALWAYS,
		        "GLExecPrivSepHelper::run_script: %s exited "
		            "with status %d and following output:\n%s\n",
		        args.GetArg(0),
		        ret,
		        str.c_str());
		error_desc.formatstr_cat("%s exited with status %d and the following output: %s",
				       condor_basename(args.GetArg(0)),
				       ret,
				       str.c_str());
		error_desc.replaceString("\n","; ");
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
	m_setup_script.formatstr("%s/condor_glexec_setup", libexec);
	m_run_script.formatstr("%s/condor_glexec_run", libexec);
	m_wrapper_script.formatstr("%s/condor_glexec_job_wrapper", libexec);
	m_proxy_update_script.formatstr("%s/condor_glexec_update_proxy", libexec);
	m_cleanup_script.formatstr("%s/condor_glexec_cleanup", libexec);
	free(libexec);

	m_sandbox_owned_by_user = false;

	m_glexec_retries = param_integer("GLEXEC_RETRIES",3,0);
	m_glexec_retry_delay = param_integer("GLEXEC_RETRY_DELAY",5,0);

	m_initialized = true;
}

bool
GLExecPrivSepHelper::chown_sandbox_to_user(PrivSepError &err)
{
	ASSERT(m_initialized);

	if (m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "GLExecPrivSepHelper::chown_sandbox_to_user: "
		            "sandbox already user-owned\n");
		return true;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to the user\n");

	ArgList args;
	args.AppendArg(m_setup_script);
	args.AppendArg(m_glexec);
	args.AppendArg(m_proxy);
	args.AppendArg(m_sandbox);
	args.AppendArg(m_glexec_retries);
	args.AppendArg(m_glexec_retry_delay);
	MyString error_desc = "error changing sandbox ownership to the user: ";
	int rc = run_script(args,error_desc);
	if( rc != 0) {
		int hold_code = CONDOR_HOLD_CODE_GlexecChownSandboxToUser;
		if( rc != INVALID_PROXY_RC && !param_boolean("GLEXEC_HOLD_ON_INITIAL_FAILURE",true) ) {
			// Do not put the job on hold due to glexec failure.
			// It will simply return to idle status and try again.
			hold_code = 0;
		}

		err.setHoldInfo( hold_code, rc, error_desc.c_str());
		return false;
	}

	m_sandbox_owned_by_user = true;
	return true;
}

bool
GLExecPrivSepHelper::chown_sandbox_to_condor(PrivSepError &err)
{
	ASSERT(m_initialized);

	if (!m_sandbox_owned_by_user) {
		dprintf(D_FULLDEBUG,
		        "GLExecPrivSepHelper::chown_sandbox_to_condor: "
		            "sandbox already condor-owned\n");
		return true;
	}

	dprintf(D_FULLDEBUG, "changing sandbox ownership to condor\n");

	ArgList args;
	args.AppendArg(m_cleanup_script);
	args.AppendArg(m_glexec);
	args.AppendArg(m_proxy);
	args.AppendArg(m_sandbox);
	args.AppendArg(m_glexec_retries);
	args.AppendArg(m_glexec_retry_delay);
	MyString error_desc = "error changing sandbox ownership to condor: ";
	int rc = run_script(args,error_desc);
	if( rc != 0) {
		err.setHoldInfo(
			CONDOR_HOLD_CODE_GlexecChownSandboxToCondor, rc,
			error_desc.c_str());
		return false;
	}

	m_sandbox_owned_by_user = false;
	return true;
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

	MyString error_desc;
	return (run_script(args,error_desc) == 0);
}

int
GLExecPrivSepHelper::create_process(const char* path,
                                    ArgList&    args,
                                    Env&        env,
                                    const char* iwd,
                                    int         job_std_fds[3],
                                    const char* std_file_names[3],
                                    int         nice_inc,
                                    size_t*     core_size_ptr,
                                    int         reaper_id,
                                    int         dc_job_opts,
                                    FamilyInfo* family_info,
                                    int *,
                                    std::string & error_msg)
{
	ASSERT(m_initialized);

	if (!proxy_valid_right_now()) {
		dprintf(D_ALWAYS, "GLExecPrivSepHelper::create_process: not invoking glexec since the proxy is not valid!\n");
		formatstr_cat(error_msg, "The job proxy is invalid.");
		return -1;
	}

	// make a copy of std FDs so we're not messing w/ our caller's
	// memory
	int std_fds[3] = {-1, -1, -1};

	ArgList modified_args;
	modified_args.AppendArg(m_run_script);
	modified_args.AppendArg(m_glexec);
	modified_args.AppendArg(m_proxy);
	modified_args.AppendArg(m_sandbox);
	modified_args.AppendArg(m_wrapper_script);
	for (int i = 0; i < 3; i++) {
		modified_args.AppendArg((job_std_fds == NULL || job_std_fds[i] == -1) ?
		                            std_file_names[i] : "-");
	}
	modified_args.AppendArg(path);
	for (int i = 1; i < args.Count(); i++) {
		modified_args.AppendArg(args.GetArg(i));
	}

	int glexec_errors = 0;
	while(1) {
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
		std_fds[0] = sock_fds[1];

			// now create a pipe for receiving diagnostic stdout/stderr from glexec
		int glexec_out_fds[2];
		if (pipe(glexec_out_fds) < 0) {
			dprintf(D_ALWAYS,
					"GLEXEC: pipe() error: %s\n",
					strerror(errno));
			close(sock_fds[0]);
			close(sock_fds[1]);
			return false;
		}
		std_fds[1] = glexec_out_fds[1];
		std_fds[2] = std_fds[1]; // collect glexec stderr and stdout together

		FamilyInfo fi;
		FamilyInfo* fi_ptr = (family_info != NULL) ? family_info : &fi;
		MyString proxy_path;
		proxy_path.formatstr("%s.condor/%s", m_sandbox, m_proxy);
		fi_ptr->glexec_proxy = proxy_path.c_str();

			// At the very least, we need to pass the condor daemon's
			// X509_USER_PROXY to condor_glexec_run.  Currently, we just
			// pass all daemon environment.  We do _not_ run
			// condor_glexec_run in the job environment, because that
			// would be a security risk and would serve no purpose, since
			// glexec cleanses the environment anyway.
		dc_job_opts &= ~(DCJOBOPT_NO_ENV_INHERIT);

		int pid = daemonCore->Create_Process(m_run_script.c_str(),
		                                     modified_args,
		                                     PRIV_USER_FINAL,
		                                     reaper_id,
		                                     FALSE,
		                                     FALSE,
		                                     NULL,
		                                     iwd,
		                                     fi_ptr,
		                                     NULL,
		                                     std_fds,
		                                     NULL,
		                                     nice_inc,
		                                     NULL,
		                                     dc_job_opts,
		                                     core_size_ptr,
											 NULL,
											 NULL,
											 error_msg);

			// close our handle to glexec's end of the diagnostic output pipe
		close(glexec_out_fds[1]);

		MyString glexec_error_msg;
		int glexec_rc = 0;
		int ret_val = feed_wrapper(pid, sock_fds, env, dc_job_opts, job_std_fds, glexec_out_fds[0], &glexec_error_msg, &glexec_rc);

			// if not closed in feed_wrapper, close the glexec error pipe now
		if( glexec_out_fds[0] != -1 ) {
			close(glexec_out_fds[0]);
		}

			// Unlike the other glexec operations where we handle
			// glexec retry inside the helper script,
			// condor_glexec_run cannot handle retry for us, because
			// it must exec glexec rather than spawning it off in a
			// new process.  Therefore, we handle here retries in case
			// of transient errors.

		if( ret_val != 0 ) {
			return ret_val; // success
		}
		bool retry = true;
		if( glexec_rc != 202 && glexec_rc != 203 ) {
				// Either a non-transient glexec error, or some other
				// non-glexec error.
			retry = false;
		}
		else {
			// This _could_ be a transient glexec issue, such as a
			// communication error with GUMS, so retry up to some
			// limit.
			glexec_errors += 1;
			if( glexec_errors > m_glexec_retries ) {
				retry = false;
			}
		}

		if( !retry ) {
				// return the most recent glexec error output
			formatstr_cat(error_msg,"%s",glexec_error_msg.c_str());
			return 0;
		}
			// truncated exponential backoff
		int delay_rand = 1 + (get_random_int_insecure() % glexec_errors) % 100;
		int delay = m_glexec_retry_delay * delay_rand;
		dprintf(D_ALWAYS,"Glexec exited with status %d; retrying in %d seconds.\n",
				glexec_rc, delay );
		sleep(delay);
			// now try again ...
	}

		// should never get here
	return 0;
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
                                  int job_std_fds[3],
								  int &glexec_err_fd,
								  MyString *error_msg,
								  int *glexec_rc)
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

	unsigned int hello = 0;
	ssize_t bytes = full_read(sock_fds[0], &hello, sizeof(int));
	if (bytes != sizeof(int)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error reading hello from glexec_job_wrapper\n");
		close(sock_fds[0]);

		if( bytes <= 0 ) {
				// Since we failed to read the expected hello bytes
				// from the wrapper, this likely indicates that glexec
				// failed to execute the wrapper.  Attempt to read an
				// error message from glexec.
			MyString glexec_stderr;
			FILE *fp = fdopen(glexec_err_fd,"r");
			if( fp ) {
				while( glexec_stderr.readLine(fp,true) );
				fclose(fp);
				glexec_err_fd = -1; // fd is closed now
			}

				// Collect the exit status from glexec.  Since we
				// created this process via
				// DaemonCore::Create_Process() we could/should wait
				// for DaemonCore to reap the process.  However, given
				// the way this function is used in the starter, that
				// happens too late.
			int glexec_status = 0;
			if (waitpid(pid,&glexec_status,0)==pid) {
				if (WIFEXITED(glexec_status)) {
					int status = WEXITSTATUS(glexec_status);
					ASSERT( glexec_rc );
					*glexec_rc = status;
					dprintf(D_ALWAYS,
							"GLEXEC: glexec call exited with status %d\n",
							status);
					if( error_msg ) {
						error_msg->formatstr_cat(
							" glexec call exited with status %d",
							status);
					}
				}
				else if (WIFSIGNALED(glexec_status)) {
					int sig = WTERMSIG(glexec_status);
					dprintf(D_ALWAYS,
							"GLEXEC: glexec call exited via signal %d\n",
							sig);
					if( error_msg ) {
						error_msg->formatstr_cat(
							" glexec call exited via signal %d",
							sig);
					}
				}
			}

			if( !glexec_stderr.empty() ) {
				glexec_stderr.trim();
				StringList lines(glexec_stderr.c_str(),"\n");
				lines.rewind();
				char const *line;

				if( error_msg ) {
					*error_msg += " and with error output (";
				}
				int line_count=0;
				while( (line=lines.next()) ) {
						// strip out the annoying line about pthread_mutex_init
					if( strstr(line,"It appears that the value of pthread_mutex_init") ) {
						continue;
					}
					line_count++;

					if( !glexec_stderr.empty() ) {
						glexec_stderr += "; ";
					}
					dprintf(D_ALWAYS,
							"GLEXEC: error output: %s\n",line);

					if( error_msg ) {
						if( line_count>1 ) {
							*error_msg += "; ";
						}
						*error_msg += line;
					}
				}
				if( error_msg ) {
					*error_msg += ")";
				}
			}

		}
		errno = 0; // avoid higher-level code thinking there was a syscall error
		return FALSE;
	}
	if( hello != 0xdeadbeef ) {
		dprintf(D_ALWAYS,
				"GLEXEC: did not receive expected hello from wrapper: %x\n",
				hello);
		close(sock_fds[0]);
		return FALSE;
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
		        merge_err.c_str());
		close(sock_fds[0]);
		return FALSE;
	}
	const char* env_buf = env_str.c_str();
	int env_len = env_str.length() + 1;
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

	// now send over the FDs that the Starter should use for stdin/out/err
	//
	int i;
	for(i=0;i<3;i++) {
		int std_fd = job_std_fds[i];
		if (std_fd != -1) {
			int pipe_fd;
			if (daemonCore->Get_Pipe_FD(std_fd, &pipe_fd) == TRUE) {
				std_fd = pipe_fd;
			}
			if (fdpass_send(sock_fds[0], std_fd) == -1) {
				dprintf(D_ALWAYS, "GLEXEC: fdpass_send failed\n");
				close(sock_fds[0]);
				return FALSE;
			}
		}
	}

		// Now we do a little dance to replace the socketpair that we
		// have been using to communicate with the wrapper with a new
		// one.  Why?  Because, as of glexec 0.8, when glexec is
		// configured with linger=on, some persistent process
		// (glexec?, procd?) is keeping a handle to the wrapper's end
		// of the socket open, so the starter hangs waiting for the
		// socket to close when the job is executed.

	int old_sock_fd = sock_fds[0];
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sock_fds) == -1)
	{
		dprintf(D_ALWAYS,
		        "GLEXEC: socketpair error: %s\n",
		        strerror(errno));
		close(old_sock_fd);
		return FALSE;
	}
	if (fdpass_send(old_sock_fd, sock_fds[1]) == -1) {
		dprintf(D_ALWAYS, "GLEXEC: fdpass_send failed on new sock fd\n");
		close(old_sock_fd);
		close(sock_fds[0]);
		close(sock_fds[1]);
		return FALSE;
	}
		// close our handle to the wrapper's end of the socket
	close(sock_fds[1]);
		// close our old socket
	close(old_sock_fd);

	// now read any error messages produced by the wrapper
	//
	char err[256];
	bytes = full_read(sock_fds[0], err, sizeof(err) - 1);
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
		if( error_msg ) {
			error_msg->formatstr_cat("glexec_job_wrapper error: %s", err);
		}
			// prevent higher-level code from thinking this was a syscall error
		errno = 0;
		return FALSE;
	}

	// if we're here, it all worked
	//
	close(sock_fds[0]);
	return pid;
}
