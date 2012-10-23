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
#include "condor_config.h"
#include "sshd_proc.h"
#include "starter.h"

extern CStarter *Starter;

int
SSHDProc::SshStartJob(int std_fds[],char const *std_fnames[])
{
	dprintf(D_FULLDEBUG,"in SSHDProc::StartJob()\n");

	SetStdFiles(std_fds,std_fnames);

	return VanillaProc::StartJob();
}

bool
SSHDProc::JobExit( void )
{
	dprintf( D_FULLDEBUG, "Inside SSHDProc::JobExit()\n" );
	return true;
}

bool
SSHDProc::PublishUpdateAd( ClassAd* )
{
	dprintf( D_FULLDEBUG, "In SSHDProc::PublishUpdateAd()\n" );

		// do not call VanillaProc's handler or it will overwrite
		// attributes from the real job (like ImageSize)
	return true;
}


bool
SSHDProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in SSHDProc::JobReaper()\n");

	if (pid == JobPid) {
			// Could remove sshd session directory, but for now, we leave
			// it there for easier debugging.
	}

	return VanillaProc::JobReaper( pid, status );
}

bool
SSHDProc::ThisProcRunsAlongsideMainProc()
{
		// this indicates that we are running concurrently with the job
		// which changes some behaviors in the lower level proc handlers
	return true;
}

char const *
SSHDProc::getArgv0()
{
		// It would be confusing to run sshd as condor_exec, since the job
		// also runs like that.  It might be nice to run it as something
		// other than sshd in order to avoid alarming admins.  However,
		// on some systems, I have observed failure to log in due to
		// PAM complaints when sshd runs with a different argv[0].
		// Probably these problems can be worked around in sshd_config,
		// but that makes it harder to support a wide range of sshd versions.
		// Therefore, just leave argv[0] alone.
	return NULL;
}
