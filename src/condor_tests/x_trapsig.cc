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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "../config.h"

volatile int signalcaught = 0;

void catchsig(int sig);

struct sigaction sa;
struct sigaction old;

void catchsig( int sig )
{
	printf("Generate lots of output.................\n");
	printf("%d\n",sig);
	fflush(stdout);
	printf("%d\n",sig);
	fflush(stdout);
	signalcaught = sig;
	/*exit(sig);*/
}

int main(int argc, char **argv)
{
	int res = 0;

	/* do not assign sa_handler and sa_sigaction at the same time since on some
		architectures those two are a union */
	sa.sa_handler = catchsig;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER;

	sigset_t	set;
	int 		caughtsig;
	int 		n;

	sigemptyset(&set);
	for(n=1;n<15;n++) {
		if(n != 9) {
			sigaddset(&set,n);
		}
	}



	/*signal( 1, (sighandler_t)&catchsig);*/
	res = sigaction( 1, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 1\n");
	}
	res = sigaction( 2, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 2\n");
	}
	res = sigaction( 3, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 3\n");
	}
	res = sigaction( 4, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 4\n");
	}
	res = sigaction( 5, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 5\n");
	}
	res = sigaction( 6, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 6\n");
	}
	res = sigaction( 7, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 7\n");
	}
	res = sigaction( 8, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 8\n");
	}
	/*
	**res = sigaction( 9, &sa, &old);
	**if(res != 0 )
	**
		**printf("failed to replace handler intr 9\n");
	**
	*/
	res = sigaction( 10, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 10\n");
	}
	res = sigaction( 11, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 11\n");
	}
	res = sigaction( 12, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 12\n");
	}
	res = sigaction( 13, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 13\n");
	}
	res = sigaction( 14, &sa, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 14\n");
	}

# if (SIGWAIT_ARGS == 2)
	sigwait( &set, &caughtsig );
# elif (STATFS_ARGS == 1)
	caughtsig = sigwait( &set );
# else
#  error "Unknown sigwait() implemenation"
# endif
	sleep(1);
	printf("%d\n",caughtsig);
	fflush(stdout);

	return 0;
}
