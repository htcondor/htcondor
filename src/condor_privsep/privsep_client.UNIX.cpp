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
#include "condor_config.h"
#include "condor_uid.h"
#include "basename.h"
#include "MyString.h"
#include "condor_arglist.h"
#include "env.h"
#include "condor_privsep.h"

static const char* switchboard_path = NULL;
static const char* switchboard_file = NULL;

bool
privsep_enabled()
{
	static bool first_time = true;
	static bool answer;

	if (first_time) {

		first_time = false;

		if (is_root()) {
			answer = false;
		}
		else {
			answer = param_boolean("PRIVSEP_ENABLED", false);
		}

		if (answer) {
			switchboard_path = param("PRIVSEP_SWITCHBOARD");
			if (switchboard_path == NULL) {
				EXCEPT("PRIVSEP_ENABLED is true, but "
				           "PRIVSEP_SWITCHBOARD is undefined");
			}
			switchboard_file = condor_basename(switchboard_path);
		}
	}

	return answer;
}

bool
privsep_create_pipes(FILE*& in_writer,
                     int& in_reader,
                     FILE*& err_reader,
                     int& err_writer)
{
	int in_pipe[2] = {-1, -1};
	int err_pipe[2] = {-1, -1};
	FILE* in_fp = NULL;
	FILE* err_fp = NULL;

	// make the pipes at the OS-level
	//
	if (pipe(in_pipe) == -1) {
		dprintf(D_ALWAYS,
		        "privsep_create_pipes: pipe error: %s (%d)\n",
		        strerror(errno),
		        errno);
		goto PRIVSEP_CREATE_PIPES_FAILURE;
	}
	if (pipe(err_pipe) == -1) {
		dprintf(D_ALWAYS,
		        "privsep_create_pipes: pipe error: %s (%d)\n",
		        strerror(errno),
		        errno);
		goto PRIVSEP_CREATE_PIPES_FAILURE;
	}

	// make FILE*'s for convenience
	//
	in_fp = fdopen(in_pipe[1], "w");
	if (in_fp == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_create_pipes: pipe error: %s (%d)\n",
		        strerror(errno),
		        errno);
		goto PRIVSEP_CREATE_PIPES_FAILURE;
	}
	err_fp = fdopen(err_pipe[0], "r");
	if (err_fp == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_create_pipes: pipe error: %s (%d)\n",
		        strerror(errno),
		        errno);
		goto PRIVSEP_CREATE_PIPES_FAILURE;
	}

	// set the return arguments and return success
	//
	in_writer = in_fp;
	in_reader = in_pipe[0];
	err_reader = err_fp;
	err_writer = err_pipe[1];
	return true;

PRIVSEP_CREATE_PIPES_FAILURE:
	if (in_fp != NULL) {
		fclose(in_fp);
		in_pipe[1] = -1;
	}
	if (err_fp != NULL) {
		fclose(err_fp);
		err_pipe[0] = -1;
	}
	if (in_pipe[0] != -1) {
		close(in_pipe[0]);
	}
	if (in_pipe[1] != -1) {
		close(in_pipe[1]);
	}
	if (err_pipe[0] != -1) {
		close(err_pipe[0]);
	}
	if (err_pipe[1] != -1) {
		close(err_pipe[1]);
	}
	return false;
}

void
privsep_get_switchboard_command(const char* op,
                                int in_fd,
                                int err_fd,
                                MyString& cmd,
                                ArgList& arg_list)
{
	cmd = switchboard_path;
	arg_list.Clear();
	arg_list.AppendArg(switchboard_file);
	arg_list.AppendArg(op);
	arg_list.AppendArg(in_fd);
	arg_list.AppendArg(err_fd);
}

bool
privsep_get_switchboard_response(FILE* err_fp, MyString *response)
{
	// first read everything off the error pipe and close
	// the error pipe
	//
	MyString err;
	while (err.readLine(err_fp, true)) { }
	fclose(err_fp);
	
	// if this is passed in, assume the caller will handle any
	// error propagation, and we just succeed.
	if (response) {
		*response = err;
		return true;
	}

	// if there was something there, print it out here (since no one captured
	// the error message) and return false to indicate something went wrong.
	if (err.Length() != 0) {
		dprintf(D_ALWAYS,
		        "privsep_get_switchboard_response: error received: %s",
			err.Value());
		return false;
	}

	// otherwise, indicate that everything's fine
	//
	return true;
}

static int write_error_code;

static pid_t
privsep_launch_switchboard(const char* op, FILE*& in_fp, FILE*& err_fp)
{
	ASSERT(switchboard_path != NULL);
	ASSERT(switchboard_file != NULL);

	// create the pipes for communication with the switchboard
	//
	int child_in_fd;
	int child_err_fd;
	if (!privsep_create_pipes(in_fp, child_in_fd, err_fp, child_err_fd)) {
		return 0;
	}

	pid_t switchboard_pid = fork();
	if (switchboard_pid == -1) {
		dprintf(D_ALWAYS,
		        "privsep_launch_switchboard: fork error: %s (%d)\n",
		        strerror(errno),
		        errno);
		return 0;
	}

	// in the parent, we just return back to the caller so they can
	// start sending commands to the switchboard's input pipe; but
	// make sure to close the clients sides of our pipes first
	//
	if (switchboard_pid != 0) {
		close(child_in_fd);
		close(child_err_fd);
		return switchboard_pid;
	}

	// in the child, we need to exec the switchboard binary with the
	// appropriate arguments
	//
	close(fileno(in_fp));
	close(fileno(err_fp));
	MyString cmd;
	ArgList arg_list;
	privsep_get_switchboard_command(op,
	                                child_in_fd,
	                                child_err_fd,
	                                cmd,
	                                arg_list);
	execv(cmd.Value(), arg_list.GetStringArray());

	// exec failed; tell our parent using the error pipe that something
	// went wrong before exiting
	//
	MyString err;
	err.formatstr("exec error on %s: %s (%d)\n",
	            cmd.Value(),
	            strerror(errno),
	            errno);
	write_error_code = write(child_err_fd, err.Value(), err.Length());
	_exit(1);
}

static bool
privsep_reap_switchboard(pid_t switchboard_pid, FILE* err_fp, MyString *response)
{
	// we always capture the response, so we can propagate or log it if needed
	MyString real_response;

	// read any data from the error pipe
	privsep_get_switchboard_response(err_fp, &real_response);
		
	// now call waitpid on the switchboard pid
	//
	int status;
	if (waitpid(switchboard_pid, &status, 0) == -1) {
		dprintf(D_ALWAYS,
		        "privsep_reap_switchboard: waitpid error: %s (%d)\n",
		        strerror(errno),
		        errno);
		return false;
	}
	
	// now we have both the exit status and result from the error pipe.  there
	// are several possibilities here, so please listen closely as our options
	// have recently changed.

	// if the exit code was non-zero, or a signal, it is an error and we log it
	// as such with the response.
	bool exited_with_zero = (WIFEXITED(status) && (WEXITSTATUS(status) == 0));
	if (!exited_with_zero) {
		MyString err_msg;
		if(WIFSIGNALED(status)) {
			err_msg.formatstr("error received: exited with signal (%i) "
					"and message (%s)", WTERMSIG(status), real_response.Value());
		} else {
			err_msg.formatstr("error received: exited with non-zero status (%i) "
					"and message (%s)", WEXITSTATUS(status), real_response.Value());
		}
		dprintf (D_ALWAYS, "privsep_reap_switchboard: %s\n", err_msg.Value());
		if(response) {
			*response = err_msg;
		}
		return false;
	}

	// ALL CASES BELOW HERE ASSUME EXIT CODE WAS ZERO
	ASSERT(exited_with_zero);
	

	// caller will handle response
	if (response) {
		// propagate the response to the caller and succeed
		*response = real_response;
		return true;
	}

	// nothing to propagate
	if (real_response.Length() == 0) {
		return true;
	}

	// there was a response to propagate but nothing to receive it.  log it and
	// fail.
	dprintf (D_ALWAYS, "privsep_reap_switchboard: unhandled message (%s)\n", real_response.Value());

	return false;
}

void
privsep_exec_set_uid(FILE* fp, uid_t uid)
{
	fprintf(fp, "user-uid=%u\n", (unsigned)uid);
}

void
privsep_exec_set_path(FILE* fp, const char* path)
{
	fprintf(fp, "exec-path=%s\n", path);
}

void
privsep_exec_set_args(FILE* fp, ArgList& args)
{
	int num_args = args.Count();
	for (int i = 0; i < num_args; i++) {
		fprintf(fp, "exec-arg<%lu>\n", (unsigned long)strlen(args.GetArg(i)));
		fprintf(fp, "%s\n", args.GetArg(i));
	}
}

void
privsep_exec_set_env(FILE* fp, Env& env)
{
	char** env_array = env.getStringArray();
	for (char** ptr = env_array; *ptr != NULL; ptr++) {
		fprintf(fp, "exec-env<%lu>\n", (unsigned long)strlen(*ptr));
		fprintf(fp, "%s\n", *ptr);
	}
	deleteStringArray(env_array);
}

void
privsep_exec_set_iwd(FILE* fp, const char* iwd)
{
	fprintf(fp, "exec-init-dir=%s\n", iwd);
}

void
privsep_exec_set_inherit_fd(FILE* fp, int fd)
{
	fprintf(fp, "exec-keep-open-fd=%d\n", fd);
}

void
privsep_exec_set_std_file(FILE* fp, int target_fd, const char* path)
{
	ASSERT((target_fd >= 0) && (target_fd <= 2));
	static const char* handle_name_array[3] = {"stdin", "stdout", "stderr"};
	fprintf(fp, "exec-%s=%s\n", handle_name_array[target_fd], path);
}

void
privsep_exec_set_is_std_univ(FILE* fp)
{
	fprintf(fp, "exec-is-std-univ\n");
}

#if defined(LINUX)
void
privsep_exec_set_tracking_group(FILE* fp, gid_t tracking_group)
{
	ASSERT( tracking_group != 0 ); // tracking group should never be group 0
	fprintf(fp, "exec-tracking-group=%u\n", tracking_group);
}
#endif

bool
privsep_create_dir(uid_t uid, const char* pathname)
{
	// launch the privsep switchboard with the "mkdir" operation
	//
	FILE* in_fp = 0;
	FILE* err_fp = 0;
	pid_t switchboard_pid = privsep_launch_switchboard("mkdir",
	                                                   in_fp,
	                                                   err_fp);
	if (switchboard_pid == 0) {
		dprintf(D_ALWAYS, "privsep_create_dir: "
		                      "error launching switchboard\n");
		if(in_fp) fclose(in_fp);
		if(err_fp) fclose(err_fp);
		return false;
	}

	// feed it the uid and pathname via its input pipe
	//
	fprintf(in_fp, "user-uid = %u\n", (unsigned)uid);
	fprintf(in_fp, "user-dir = %s\n", pathname);
	fclose(in_fp);

	// now reap it and return
	//
	return privsep_reap_switchboard(switchboard_pid, err_fp, NULL);
}

bool
privsep_remove_dir(const char* pathname)
{
	// launch the privsep switchboard with the "rmdir" operation
	//
	FILE* in_fp = 0;
	FILE* err_fp = 0;
	pid_t switchboard_pid = privsep_launch_switchboard("rmdir",
	                                                   in_fp,
	                                                   err_fp);
	if (switchboard_pid == 0) {
		dprintf(D_ALWAYS, "privsep_remove_dir: "
		                      "error launching switchboard\n");
		if(in_fp) fclose(in_fp);
		if(err_fp) fclose(err_fp);
		return false;
	}

	// feed it the pathname via its input pipe
	//
	fprintf(in_fp, "user-dir = %s\n", pathname);
	fclose(in_fp);

	// now reap it and return
	//
	return privsep_reap_switchboard(switchboard_pid, err_fp, NULL);
}

bool
privsep_get_dir_usage(uid_t uid, const char* pathname, off_t *total_size)
{
	// launch the privsep switchboard with the "dirusage" operation
	//
	FILE* in_fp = 0;
	FILE* err_fp = 0;
	pid_t switchboard_pid = privsep_launch_switchboard("dirusage",
	                                                   in_fp,
	                                                   err_fp);
	if (switchboard_pid == 0) {
		dprintf(D_ALWAYS, "privsep_get_dir_usage: "
		                      "error launching switchboard\n");
		if(in_fp) fclose(in_fp);
		if(err_fp) fclose(err_fp);
		return false;
	}

	// feed it the pathname via its input pipe
	//
	fprintf(in_fp, "user-uid = %i\n", uid);
	fprintf(in_fp, "user-dir = %s\n", pathname);
	fclose(in_fp);

	// now reap it and return
	//
	MyString total_usage_str;
	bool rval = privsep_reap_switchboard(switchboard_pid, err_fp, &total_usage_str);
	if (rval) {
		// the call succeeded -- there should be a response for us.
		intmax_t tval;
		int r = sscanf(total_usage_str.Value(), "%ju", &tval);
		if (r) {
			*total_size = *((off_t*)&tval);
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool
privsep_chown_dir(uid_t target_uid, uid_t source_uid, const char* pathname)
{
	// launch the privsep switchboard with the "chowndir" operation
	//
	FILE* in_fp;
	FILE* err_fp;
	pid_t switchboard_pid = privsep_launch_switchboard("chowndir",
	                                                   in_fp,
	                                                   err_fp);
	if (switchboard_pid == 0) {
		dprintf(D_ALWAYS, "privsep_chown_dir: "
		                      "error launching switchboard\n");
		fclose(in_fp);
		fclose(err_fp);
		return false;
	}

	// feed it the uid and pathname via its input pipe
	//
	fprintf(in_fp, "user-uid = %u\n", (unsigned)target_uid);
	fprintf(in_fp, "user-dir = %s\n", pathname);
	fprintf(in_fp, "chown-source-uid=%u\n", (unsigned)source_uid);
	fclose(in_fp);

	// now reap it and return
	//
	return privsep_reap_switchboard(switchboard_pid, err_fp, NULL);
}
