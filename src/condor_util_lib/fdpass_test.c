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
#include "fdpass.h"

int
main(void)
{
	int fds[2];
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, fds) == -1) {
		perror("socketpair");
		exit(1);
	}

	printf("fd[0] = %d\n", fds[0]);
	printf("fd[1] = %d\n", fds[1]);

	if (fdpass_send(fds[0], 0) == -1) {
		fprintf(stderr, "fdpass_send failed\n");
		exit(1);
	}

	int fd = fdpass_recv(fds[1]);
	if (fd == -1) {
		fprintf(stderr, "fdpass_recv failed\n");
		exit(1);
	}

	printf("recvied fd %d\n", fd);

	return 0;
}
