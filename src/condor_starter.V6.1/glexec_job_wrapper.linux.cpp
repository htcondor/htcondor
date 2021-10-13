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

// This is a wrapper program that we use for things we launch via glexec,
// in order to get around glexec's inability to pass environment variables.
// The basic procedure is:
//   1) Condor sets up a UNIX domain socket pair
//   2) Condor invokes this wrapper via glexec, passing one of the sockets
//      to the wrapper as it's standard input stream.
//   3) A stream of environment variables is sent over the socket pair and
//      merged into the environment that will be used for the job.
//   4) The job's "real" standard input FD is sent over the socket pair. The
//      wrapper's socket end is dup'ed and FD 0 is then set to use the
//      received FD.
//   5) The wrapper's socket end is set close-on-exec, and the wrapper then
//      exec's the job.

#include "condor_common.h"
#include "MyString.h"
#include "env.h"
#include "condor_blkng_full_disk_io.h"
#include "fdpass.h"

static void fatal_error();
static char* read_env();
static void get_std_fd(int, char*);

static std::string err;
static int sock_fd;

int
main(int argc, char* argv[])
{
	// dup FD 0 since we will later replace FD 0 with the job's stdin
	// Make sure we dup it to something >= 3 so it doesn't get
	// clobbered when we set up stdin, stdout, and stderr.
	do {
		sock_fd = dup(0);
		if (sock_fd == -1) {
			formatstr(err, "dup error on FD 0: %s", strerror(errno));
			sock_fd = 0;
			fatal_error();
		}
	} while( sock_fd < 3 );

	// deal with our arguments
	//
	if (argc < 5) {
		formatstr(err, "usage: %s <in> <out> <err> <cmd> [<arg> ...]",
		            argv[0]);
		fatal_error();
	}
	char* job_stdin = argv[1];
	char* job_stdout = argv[2];
	char* job_stderr = argv[3];
	char** job_argv = &argv[4];

	unsigned int hello = 0xdeadbeef;
	if( write(sock_fd,&hello,sizeof(int)) != sizeof(int) ) {
		err = "Failed to send hello";
		fatal_error();
	}

	// set up an Env object that we'll use for the job. We'll
	// merge the environment that Condor sends us on top of that
	// the environment that glexec prepared for us.  Therefore,
	// settings from Condor win in case of conflict.  This is
	// important for X509_USER_PROXY and for PATH.

	Env env;

	env.MergeFrom(environ); // glexec environment

	char* env_buf = read_env();
	MyString merge_err;
	if (!env.MergeFromV2Raw(env_buf, &merge_err)) {
		formatstr(err, "Env::MergeFromV2Raw error: %s", merge_err.Value());
		fatal_error();
	}
	delete[] env_buf;

	// now prepare the job's standard FDs
	//
	get_std_fd(0, job_stdin);
	get_std_fd(1, job_stdout);
	get_std_fd(2, job_stderr);

		// Now we do a little dance to replace the socketpair that we
		// have been using to communicate with the starter with a new
		// one.  Why?  Because, as of glexec 0.8, when glexec is
		// configured with linger=on, some persistent process
		// (glexec?, procd?) is keeping a handle to the wrapper's end
		// of the socket open, so the starter hangs waiting for the
		// socket to close when the job is executed.

	int new_sock_fd = fdpass_recv(sock_fd);
	if (new_sock_fd == -1) {
		err = "fdpass_recv error on new_sock_fd";
		fatal_error();
	}
	if (dup2(new_sock_fd,sock_fd) == -1 ) {
		err = "dup2 error on new_sock_fd";
		fatal_error();
	}
	close(new_sock_fd);

	// set our UNIX domain socket end close-on-exec; if the Starter
	// sees it close without seeing an error message first, it assumes
	// that the job has begun execution
	//
	if (fcntl(sock_fd, F_SETFD, FD_CLOEXEC) == -1) {
		formatstr(err, "fcntl error setting close-on-exec: %s",
		            strerror(errno));
		fatal_error();
	}

	// now we can exec the job. for arguments, we shift this wrapper's
	// arguments by one. similarly, the job's executable path is taken
	// as our argv[1]
	//
	char** envp = env.getStringArray();
	execve(job_argv[0], &job_argv[0], envp);
	formatstr(err, "execve error: %s", strerror(errno));
	fatal_error();
}

static int error_return;

static void
fatal_error()
{
	error_return = write(sock_fd, err.c_str(), err.length() + 1);
	exit(1);
}

static char*
read_env()
{
	int bytes;
	int env_len;
	bytes = full_read(0, &env_len, sizeof(env_len));
	if (bytes != sizeof(env_len)) {
		if (bytes == -1) {
			formatstr(err, "read error getting env size: %s",
			            strerror(errno));
		}
		else {
			formatstr(err, "short read of env size: %d of %u bytes",
			            bytes,
			            (unsigned)sizeof(env_len));
		}
		fatal_error();
	}
	if (env_len <= 0) {
		formatstr(err, "invalid env size %d read from stdin", env_len);
		fatal_error();
	}
	char* env_buf = new char[env_len];
	if (env_buf == NULL) {
		formatstr(err, "failure to allocate %d bytes", env_len);
		fatal_error();
	}
	bytes = full_read(0, env_buf, env_len);
	if (bytes != env_len) {
		if (bytes == -1) {
			formatstr(err, "read error getting env: %s",
			            strerror(errno));
		}
		else {
			formatstr(err, "short read of env: %d of %d bytes",
			            bytes,
			            env_len);
		}
		fatal_error();
	}
	return env_buf;
}

static void
get_std_fd(int std_fd, char* name)
{
	int new_fd = std_fd;
	if (strcmp(name, "-") == 0) {
			// if we were passed "-" on the command line,
			// get the FD to use from the Starter
			//
		new_fd = fdpass_recv(sock_fd);
		if (new_fd == -1) {
			err = "fdpass_recv error";
			fatal_error();
		}
	}
	else {
		int flags;
		if (std_fd == 0) {
			flags = O_RDONLY;
		}
		else {
			flags = O_WRONLY | O_CREAT | O_TRUNC;
		}
		new_fd = safe_open_wrapper_follow(name, flags, 0600);
		if (new_fd == -1) {
			formatstr(err, "safe_open_wrapper_follow error on %s: %s",
			            name,
			            strerror(errno));
			fatal_error();
		}
	}
	if (new_fd != std_fd) {
		if (dup2(new_fd, std_fd) == -1) {
			formatstr(err, "dup2 error (%d to %d): %s",
			            new_fd,
			            std_fd,
			            strerror(errno));
			fatal_error();
		}
		close(new_fd);
	}
}
