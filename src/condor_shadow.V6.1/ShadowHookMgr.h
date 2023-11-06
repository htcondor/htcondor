/***************************************************************
 *
 * Copyright (C) 2022, Condor Team, Computer Sciences Department,
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

#pragma once

#include "condor_common.h"
#include "HookClientMgr.h"
#include "HookClient.h"

class ShadowHookMgr final : public JobHookClientMgr
{
public:
	ShadowHookMgr();
	virtual ~ShadowHookMgr();

	virtual bool reconfig() override;

	virtual bool useProcd() const override {return false;}

	virtual const std::string paramPrefix() const override {return "SHADOW";}

	/**
	 * Invoke the HOOK_PREPARE_JOB as applicable and spawn it.
	 * @return 1 if we spawned the hook, 0 if we're not configured
	 *         to use this hook for the job's hook keyword, or -1 on error.
	 */
	int tryHookPrepareJob();
private:

	std::string m_hook_keyword;

	std::string m_hook_prepare_job;

	ArgList m_args;
};

/**
 * Manages an invoccation of HOOK_PREPARE_JOB
 */
class HookShadowPrepareJobClient : public HookClient
{
	friend class ShadowHookMgr;
public:

	HookShadowPrepareJobClient(const std::string &hook_path);

	/**
	 * Hook has exited.
	 */
	virtual void hookExited(int exit_status) override;
};
