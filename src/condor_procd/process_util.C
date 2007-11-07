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
#include "process_util.h"

#if defined(WIN32)

pid_t
get_process_id()
{
	return GetCurrentProcessId();
}

#else

pid_t
get_process_id()
{
	return getpid();
}

#endif

#if defined(WIN32)

pid_t
create_detached_child(char* argv[])
{
	// for now, we simply space-delimit the argument list
	//
	int cmdline_len = 0;
	for (char** ptr = argv; *ptr != NULL; ptr++) {
		cmdline_len += strlen(*ptr) + 1;
	}
	ASSERT(cmdline_len > 0);
	char* cmdline = (char*)malloc(cmdline_len);
	ASSERT(cmdline != NULL);
	char* dst = cmdline;
	char** src = argv;
	while (true) {
		int len = strlen(*src);
		memcpy(dst, *src, len);
		dst += len;
		src++;
		if (*src == NULL) {
			*dst = '\0';
			break;
		}
		*dst = ' ';
		dst++;
	}

	// make the call to CreateProcess
	//
	printf("about to execute: %s\n", cmdline);
	STARTUPINFO si;
	memset(&si, 0, sizeof(STARTUPINFO));
	PROCESS_INFORMATION pi;
	if (CreateProcess(NULL,
	                  cmdline,
					  NULL,
	                  NULL,
	                  FALSE,
	                  0,
	                  NULL,
	                  NULL,
	                  &si,
					  &pi) == FALSE)
	{
		fprintf(stderr, "CreateProcess error: %u\n", GetLastError());
		exit(1);
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return pi.dwProcessId;
}

#else

static void
enable_auto_reap()
{
	static bool already_done = false;

	if (!already_done) {
		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_handler = SIG_IGN;
		sa.sa_flags = SA_NOCLDWAIT;
		if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			perror("sigaction");
			exit(1);
		}
		already_done = true;
	}
}

pid_t
create_detached_child(char* argv[])
{
	// make sure we aren't leaving zombies around
	enable_auto_reap();

	pid_t child_pid = fork();
	if (child_pid == -1) {
		perror("fork");
		exit(1);
	}
	
	if (child_pid == 0) {

		// TODO: make sure all FDs that we don't want
		// to pass on are close-on-exec

		// just exec the child
		if (execv(argv[0], argv) == -1) {
			perror("execv");
			exit(1);
		}

		// should never get here
		assert(0);
	}

	return child_pid;
}

#endif

#if defined(WIN32)

void
init_signal_handler(void (*)(int))
{
}

#else

void
init_signal_handler(void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handler;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

#endif
