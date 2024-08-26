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

#ifndef _CONDOR_HOOK_CLIENT_MGR_H
#define _CONDOR_HOOK_CLIENT_MGR_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "HookClient.h"
#include <vector>


namespace classad {

class ClassAd;

}

class HookClientMgr : public Service
{
public:
	HookClientMgr();
	~HookClientMgr();

	bool initialize();

	bool spawn(HookClient* client, ArgList* args, const std::string & hook_stdin, priv_state priv = PRIV_CONDOR_FINAL, Env *env = NULL);
	bool remove(HookClient* client);

		/**
		   Reaper that saves all output written to std(out|err) and
		   records the exit status of the hook.
		*/
	int reaperOutput(int exit_pid, int exit_status);

		/**
		   Reaper that just ignores the reaped child. Used for hooks
		   that the caller expects no output from.
		*/
	int reaperIgnore(int exit_pid, int exit_status);

protected:
		/**
		   List of HookClient objects we've spawned and are waiting
		   for output from.
		*/
	std::vector<HookClient*> m_client_list;
	virtual bool useProcd() const = 0;

private:
		/// DC reaper ID. @see reaperIgnore()
	int m_reaper_ignore_id;

		/// DC reaper ID. @see reaperOutput()
	int m_reaper_output_id;

};


// Hook manager for the case where a job ad is available (shadow, starter)
class JobHookClientMgr : public HookClientMgr
{
public:
	JobHookClientMgr() : HookClientMgr() {}

	virtual bool initialize(classad::ClassAd *ad);

	int getHookTimeout(HookType hook_type, int def_value);

protected:
	virtual bool useProcd() const = 0;
	virtual bool reconfig() = 0;
	virtual const std::string paramPrefix() const = 0;
	bool getHookPath(HookType hook_type, std::string &path);
	bool getHookArgs(HookType hook_type, ArgList &args, CondorError &err);

	std::string m_hook_keyword;
};

#endif /* _CONDOR_HOOK_CLIENT_MGR_H */
