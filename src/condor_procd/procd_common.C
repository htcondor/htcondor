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
#include "procd_common.h"
#include "condor_debug.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

void
send_signal(pid_t pid, int sig)
{
#if defined(WIN32)
	bool result;
	switch (sig) {
		case SIGTERM:
			result = windows_soft_kill(pid);
			break;
		case SIGKILL:
			result = windows_hard_kill(pid);
			break;
		case SIGSTOP:
			result = windows_suspend(pid);
			break;
		case SIGCONT:
			result = windows_continue(pid);
			break;
		default:
			EXCEPT("invalid signal in send_signal: %d", sig);
	}
	if (!result) {
		dprintf(D_ALWAYS,
		        "send_signal error; pid = %u, sig = %d\n",
		        pid,
		        sig);
	}
#else
	if (kill(pid, sig) == -1) {
		dprintf(D_ALWAYS, "kill(%d, %d) error: %s (%d)\n",
		        pid, sig, strerror(errno), errno);
	}
#endif
}

