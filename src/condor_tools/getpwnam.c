/***************************************************************
 *
 * Copyright (C) 1990-2019, Condor Team, Computer Sciences Department,
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


// When condor_ssh_to_job runs, the condor_starter spawns the system
// sshd with the non-root uid and euid of the job.  On some systems
// the password entry for this id doesn't exist, doesn't have a valid
// shell, or whose shell isn't in /etc/shells.  In that case, the sshd
// fails.  To prevent this, HTCondor can force the following implementation
// of getpwnam into the sshd process that defines a valid shell and user name
// 
// Not this doesn't allow a privilege escalation, as the whole thing runs
// as the user, and condor allows the job to run any binary it wants to
// begin with.
//
// The function is only forced into the sshd binary, not the job itself.
// One could argue this is a bug.

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

static struct passwd fakeEntry; 
static dirbuf[512];

struct passwd *getpwnam(const char *name) {
	fakeEntry.pw_name = name;
	fakeEntry.pw_passwd = "password";
	fakeEntry.pw_uid = getuid();
	fakeEntry.pw_gid = getgid();
	fakeEntry.pw_gecos = "HTCondor ssh_to_job";
	fakeEntry.pw_dir = getcwd(dirbuf, 511);
	fakeEntry.pw_shell = "/bin/bash";
	return &fakeEntry;
}
