/***************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_version.h"
#include "condor_distribution.h"
#include "match_prefix.h"
#include "shared_port_scm_rights.h"
#include "fdpass.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] [command ...]\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help              Display options\n");
	fprintf(stderr,"\nIf command is given, run it inside the container.\n");
	fprintf(stderr,"Otherwise, start an interactive shell.\n");
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	int i;

	set_priv_initialize(); // allow uid switching if root
	config();

	if (argc > 1 && is_arg_prefix(argv[1],"-help")) {
		usage(argv[0]);
		exit(0);
	}

	int uds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uds < 0) {
		fprintf(stderr, "Cannot create unix domain socket: errno %d\n", errno);
		exit(1);
	}

	// Connect to .docker_sock in the current directory (the job's scratch
	// dir, which is the cwd when condor_docker_enter is launched inside
	// the container).
	//
	// AF_UNIX sun_path is only 108 bytes on Linux, but the absolute path
	// to the scratch directory may be much longer.  We could rely on
	// kernel cwd-relative lookup of a literal ".docker_sock" -- and the
	// older code did -- but to mirror the bind() side in the starter
	// (see docker_proc.cpp::SetupDockerSsh) we use the same Linux-only
	// /proc/self/fd/<N>/<name> trick: open the directory as a dirfd and
	// let the kernel resolve the magic /proc symlink back to it.  The
	// resulting sun_path string is bounded by the fixed prefix plus an
	// int and the filename, well under 108 bytes regardless of the real
	// directory depth.
	int dirfd = open(".", O_PATH | O_DIRECTORY | O_CLOEXEC);
	if (dirfd < 0) {
		fprintf(stderr, "Cannot open current directory for docker ssh_to_job: errno %d\n", errno);
		return -1;
	}

	struct sockaddr_un pipe_addr;
	memset(&pipe_addr, 0, sizeof(pipe_addr));
	pipe_addr.sun_family = AF_UNIX;
	unsigned pipe_addr_len;

	int n = snprintf(pipe_addr.sun_path, sizeof(pipe_addr.sun_path),
		"/proc/self/fd/%d/.docker_sock", dirfd);
	if (n < 0 || (size_t)n >= sizeof(pipe_addr.sun_path)) {
		// Unreachable in practice: the formatted path is ~30 bytes.
		fprintf(stderr, "Cannot format /proc/self/fd path for docker ssh_to_job\n");
		close(dirfd);
		return -1;
	}
	pipe_addr_len = SUN_LEN(&pipe_addr);

	int rc = connect(uds, (struct sockaddr *)&pipe_addr, pipe_addr_len);
	close(dirfd);

	if (rc < 0) {
		dprintf(D_ALWAYS, "Cannot connect to unix domain socket .docker_sock for docker ssh_to_job: %d\n", errno);
		return -1;
	}
	fdpass_send(uds, 0);
	fdpass_send(uds, 1);
	fdpass_send(uds, 2);

	// Build command string from remaining arguments (if any)
	std::string command;
	for (i = 1; i < argc; i++) {
		if (i > 1) command += ' ';
		command += argv[i];
	}

	// Send length-prefixed command string so the starter knows
	// what to exec inside the container.  Length 0 means interactive shell.
	uint32_t len = htonl(command.size());
	int wrc = write(uds, &len, sizeof(len));
	if (wrc != (int)sizeof(len)) {
		fprintf(stderr, "Failed to send command length: errno %d\n", errno);
		exit(1);
	}
	if (!command.empty()) {
		wrc = write(uds, command.data(), command.size());
		if (wrc != (int)command.size()) {
			fprintf(stderr, "Failed to send command: errno %d\n", errno);
			exit(1);
		}
	}

	char buf[1];
	rc = read(uds, buf, 1); // wait until other side hangs up

	
	return 0;
}

