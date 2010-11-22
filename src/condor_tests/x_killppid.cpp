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


#include <sys/types.h>
#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#include "ntsysinfo.WINDOWS.h"
#else
#include <unistd.h>
#include <strings.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
	char * killsig;
	int mykillsig = 0;
#ifdef WIN32
	CSysinfo cSysInfo;
	DWORD parentpid;
	HANDLE parentprocess;
	BOOL killparent;
#else
	pid_t parentpid;
#endif

	if(argc < 2) {
		printf("Usage: %s signal\n",argv[0]);
		exit(1);
	} 

	killsig = argv[1];

	printf("args in for x_killppid is %d\n",argc);
	printf("Kill signal is %s\n",argv[1]);

	if(!strcasecmp(killsig,"KILL")) {
		mykillsig = SIGKILL;
	} else if(!strcasecmp(killsig,"TERM")) {
		mykillsig = SIGTERM;
	} else if(!strcasecmp(killsig,"SEGV")) {
		mykillsig = SIGSEGV;
	} else if(!strcasecmp(killsig,"ABRT")) {
		mykillsig = SIGABRT;
	}
#ifdef WIN32
	parentpid = cSysInfo.GetParentPID(GetCurrentProcessId());
#else
	parentpid = getppid();
#endif

	printf("Parent pid is %d\n",parentpid);
#ifdef WIN32

	parentprocess = OpenProcess(PROCESS_TERMINATE, FALSE, parentpid);
	if(!parentprocess)
	{
		return -1;
	}

	killparent = TerminateProcess(parentprocess, mykillsig);
#else
	kill(parentpid, mykillsig);
#endif
	return(0);
}
