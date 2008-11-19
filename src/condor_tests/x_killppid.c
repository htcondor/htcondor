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
#include <unistd.h>
#include <signal.h>

main(int argc, char ** argv)
{
	char * killsig;
	int mykillsig;
	pid_t parentpid;

	printf("args in for x_killppid is %d\n",argc);
	printf("Kill signal is %s\n",argv[1]);

	if(argc < 2) {
		printf("Usage: %s signal\n",argv[0]);
		exit(1);
	} 

	killsig = argv[1];
	if(!strcasecmp(killsig,"KILL")) {
		mykillsig = SIGKILL;
	} else if(!strcasecmp(killsig,"TERM")) {
		mykillsig = SIGTERM;
	} else if(!strcasecmp(killsig,"SEGV")) {
		mykillsig = SIGSEGV;
	} else if(!strcasecmp(killsig,"ABRT")) {
		mykillsig = SIGABRT;
	}

	parentpid = getppid();

	printf("Parent pid is %d\n",parentpid);
	kill(parentpid, mykillsig);
	return(0);
}
