/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

unsigned long get_image_size(procInfo* pi)
{
#if defined(WIN32)
	return pi->rssize;
#else
	return pi->imgsize;
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