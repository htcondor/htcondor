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

#define _CONDOR_ALLOW_OPEN

#include "condor_common.h"
#include "MyString.h"
#include "env.h"
#include "condor_blkng_full_disk_io.h"
#include "fdpass.h"

static char* read_env(int);
static int read_fd(int);

int
main(int, char* argv[])
{
	std::string err;

	// dup FD 0 since well will later replace FD 0 with the job's stdin
	//
	int sock_fd = dup(0);
	if (sock_fd == -1) {
		formatstr(err, "dup error on FD 0: %s", strerror(errno));
		full_write(0, err.c_str(), err.length() + 1);
		exit(1);
	}

	// set up an Env object that we'll use for the job. we'll initialize
	// it with the environment that Condor sends us then merge on top of
	// that the environment that glexec prepared for us
	//
	Env env;
	char* env_buf = read_env(sock_fd);
	MyString merge_err;
	if (!env.MergeFromV2Raw(env_buf, &merge_err)) {
		formatstr(err, "Env::MergeFromV2Raw error: %s", merge_err.Value());
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	env.MergeFrom(environ);
	delete[] env_buf;

	// now receive an FD on our stdin (which is a UNIX domain socket)
	// that we'll use as the job's stdin
	//
	int job_fd = read_fd(sock_fd);
	if (dup2(job_fd, 0) == -1) {
		formatstr(err, "dup2 to FD 0 error: %s", strerror(errno));
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	close(job_fd);
	if (fcntl(sock_fd, F_SETFD, FD_CLOEXEC) == -1) {
		formatstr(err, "fcntl error setting close-on-exec: %s",
		            strerror(errno));
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}

	// now we can exec the job. for arguments, we shift this wrappers
	// arguments by one. similarly, the job's executable path is taken
	// as our argv[1]
	//
	char** envp = env.getStringArray();
	execve(argv[1], &argv[1], envp);
	formatstr(err, "execve error: %s", strerror(errno));
	full_write(sock_fd, err.c_str(), err.length() + 1);
	exit(1);
}

static char*
read_env(int sock_fd)
{
	std::string err;
	int bytes;
	int env_len;
	bytes = full_read(0, &env_len, sizeof(env_len));
	if (bytes != sizeof(env_len)) {
		if (bytes == -1) {
			formatstr(err, "read error getting env size: %s",
			            strerror(errno));
		}
		else {
			formatstr(err, "short read of env size: %d of %lu bytes",
			            bytes,
			            sizeof(env_len));
		}
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	if (env_len <= 0) {
		formatstr(err, "invalid env size %d read from stdin", env_len);
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	char* env_buf = new char[env_len];
	if (env_buf == NULL) {
		formatstr(err, "failure to allocate %d bytes", env_len);
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
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
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	return env_buf;
}

static int
read_fd(int sock_fd)
{
	std::string err;
	int bytes;
	int flag;
	bytes = full_read(0, &flag, sizeof(flag));
	if (bytes != sizeof(flag)) {
		if (bytes == -1) {
			formatstr(err, "read error getting flag: %s",
			            strerror(errno));
		}
		else {
			formatstr(err, "short read of flag: %d of %lu bytes",
			            bytes,
			            sizeof(flag));
		}
		full_write(sock_fd, err.c_str(), err.length() + 1);
		exit(1);
	}
	int fd;
	if (flag) {
		fd = fdpass_recv(sock_fd);
		if (fd == -1) {
			formatstr(err, "fdpass_recv failed\n");
			full_write(sock_fd, err.c_str(), err.length() + 1);
			exit(1);
		}
	}
	else {
		fd = open("/dev/null", O_RDONLY);
		if (fd == -1) {
			formatstr(err, "error opening /dev/null: %s",
			            strerror(errno));
			full_write(sock_fd, err.c_str(), err.length() + 1);
			exit(1);
		}
	}
	return fd;
}
