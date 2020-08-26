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

#ifndef _CONDOR_HOOK_CLIENT_H
#define _CONDOR_HOOK_CLIENT_H

#include "condor_common.h"
#include "enum_utils.h"
#include "condor_daemon_core.h"

class HookClient : public Service
{
public:
	HookClient(HookType hook_type, const char* hook_path, bool wants_output);
	virtual ~HookClient();

		// Functions to retrieve data about this client.
	int getPid() const {return m_pid;};
	const char* path() {return (const char*)m_hook_path;};
	HookType type() {return m_hook_type;};
	bool wantsOutput() const {return m_wants_output;};
	MyString* getStdOut();
	MyString* getStdErr();

		/// Records the pid of this client once spawned.
	void setPid(int pid) {m_pid = pid;};

		/**
		   Called when this hook client has actually exited (if it was
		   spawned expecting output).  Once this function returns, the
		   HookClient is removed from the HookClientMgr's list, and
		   the HookClient object itself is deleted.
		*/
	virtual void hookExited(int exit_status);

protected:
	char* m_hook_path;
	HookType m_hook_type;
	int m_pid;
	MyString m_std_out;
	MyString m_std_err;
	int m_exit_status;
	bool m_has_exited;
	bool m_wants_output;
};


#endif /* _CONDOR_HOOK_CLIENT_H */
