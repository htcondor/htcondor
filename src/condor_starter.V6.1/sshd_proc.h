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


#if !defined(_CONDOR_SSHD_PROC_H)
#define _CONDOR_SSHD_PROC_H

#include "vanilla_proc.h"


class SSHDProc : public VanillaProc
{
public:
	SSHDProc(ClassAd* job_ad, const std::string &session_dir, bool delete_ad = false);

	virtual int SshStartJob(int std_fds[],char const *std_fnames[]);

	virtual bool JobExit( void );

	virtual bool JobReaper(int pid, int status);

	virtual bool PublishUpdateAd( ClassAd* ad );

	virtual bool ThisProcRunsAlongsideMainProc();

	virtual char const *getArgv0();

	virtual bool SupportsPIDNamespace() { return false;}
	virtual std::string CgroupSuffix() { return "/sshd";}
	std::string session_dir;
};

#endif
