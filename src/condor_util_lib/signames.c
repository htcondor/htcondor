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

char	*SigNames[] = {
"(0)",
"SIGHUP",		/* 1 */
"SIGINT",		/* 2 */
"SIGQUIT",		/* 3 */
"SIGILL",		/* 4 */
"SIGTRAP",		/* 5 */
"SIGIOT",		/* 6 */
"SIGEMT",		/* 7 */
"SIGFPE",		/* 8 */
"SIGKILL",		/* 9 */
"SIGBUS",		/* 10 */
"SIGSEGV",		/* 11 */
"SIGSYS",		/* 12 */
"SIGPIPE",		/* 13 */
"SIGALRM",		/* 14 */
"SIGTERM",		/* 15 */
"SIGURG",		/* 16 */
"SIGSTOP",		/* 17 */
"SIGTSTP",		/* 18 */
"SIGCONT",		/* 19 */
"SIGCHLD",		/* 20 */
"SIGTTIN",		/* 21 */
"SIGTTOU",		/* 22 */
"SIGIO",		/* 23 */
"SIGXCPU",		/* 24 */
"SIGXFSZ",		/* 25 */
"SIGVTALRM",	/* 26 */
"SIGPROF",		/* 27 */
"SIGWINCH",		/* 28 */
"(29)",			/* 29 */
"SIGUSR1",		/* 30 */
"SIGUSR2",		/* 31 */
};
