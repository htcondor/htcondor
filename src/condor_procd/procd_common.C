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

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

birthday_t
procd_atob(char* str)
{
#if defined(WIN32)
	return _atoi64(str);
#else
	return atol(str);
#endif
}

void
send_signal(procInfo* pi, int sig)
{
#if defined(WIN32)
	bool result;
	switch (sig) {
		case SIGTERM:
			result = windows_soft_kill(pi->pid);
			break;
		case SIGKILL:
			result = windows_hard_kill(pi->pid);
			break;
		case SIGSTOP:
			result = windows_suspend(pi->pid);
			break;
		case SIGCONT:
			result = windows_continue(pi->pid);
			break;
		default:
			EXCEPT("invalid signal in send_signal: %d", sig);
	}
	if (!result) {
		dprintf(D_ALWAYS,
		        "send_signal error; pid = %u, sig = %d\n",
		        pi->pid,
		        sig);
	}
#else
	if (kill(pi->pid, sig) == -1) {
		dprintf(D_ALWAYS, "kill(%d, %d) error: %s (%d)\n",
		        pi->pid, sig, strerror(errno), errno);
	}
#endif
}

