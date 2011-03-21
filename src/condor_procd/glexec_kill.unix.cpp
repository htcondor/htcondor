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
#include "glexec_kill.unix.h"

static char* glexec_kill_path = NULL;
static char* glexec_path = NULL;

void
glexec_kill_init(char* glexec_kill, char* glexec)
{
	glexec_kill_path = strdup(glexec_kill);
	ASSERT(glexec_kill_path != NULL);
	glexec_path = strdup(glexec);
	ASSERT(glexec_path != NULL);
}

bool
glexec_kill_check()
{
	return (glexec_kill_path != NULL);
}

bool
glexec_kill(char* proxy, pid_t target_pid, int sig)
{
	// TODO: once the ProcD is running as the user (and thus able to
	// link in the various Condor util libraries), get rid of this home-
	// rolled fork/exec and use my_popen or whatever

	int ret;

	if (glexec_kill_path == NULL) {
		dprintf(D_ALWAYS,
		        "glexec_kill: module not initialized\n");
		return false;
	}

	char target_pid_str[10];
	ret = snprintf(target_pid_str,
	               sizeof(target_pid_str),
	               "%u",
	               (unsigned)target_pid);
	if ((ret < 1) || (ret >= (int)sizeof(target_pid_str))) {
		dprintf(D_ALWAYS,
		        "glexec_kill: snprintf error on PID %u\n",
		        (unsigned)target_pid);
		return false;
	}

	char sig_str[10];
	ret = snprintf(sig_str,
	               sizeof(sig_str),
	               "%d",
	               sig);
	if ((ret < 1) || (ret >= (int)sizeof(sig_str))) {
		dprintf(D_ALWAYS,
		        "glexec_kill: snprintf error on signal %d\n",
		        sig);
		return false;
	}

	pid_t child_pid = fork();
	if (child_pid == -1) {
		dprintf(D_ALWAYS,
		        "glexec_kill: fork error: %s\n",
		        strerror(errno));
		return false;
	}

	if (child_pid == 0) {
		for (int i = 0; i < 255; i++) {
			close(i);
		}
		char* argv[] = {glexec_kill_path,
		                glexec_path,
		                proxy,
		                target_pid_str,
		                sig_str,
		                NULL};
		execv(glexec_kill_path, argv);
		_exit(1);
	}

	int status;
	if (waitpid(child_pid, &status, 0) == -1) {
		dprintf(D_ALWAYS,
		        "glexec_kill: waitpid error: %s\n",
		        strerror(errno));
		return false;
	}
	if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
		dprintf(D_ALWAYS,
		        "glexec_kill: unexpected status from %s: %d\n",
		        glexec_kill_path,
		        status);
		return false;
	}

	return true;
}
