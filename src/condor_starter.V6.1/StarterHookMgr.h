/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_STARTER_HOOK_MGR_H
#define _CONDOR_STARTER_HOOK_MGR_H

#include "condor_common.h"
#include "HookClientMgr.h"
#include "HookClient.h"
#include "enum_utils.h"

class HookPrepareJobClient;
class HookPrepareJobPreTransferClient;
class HookJobExitClient;


/**
   The StarterHookMgr manages all the hooks that the starter invokes.
*/
class StarterHookMgr final : public JobHookClientMgr
{
public:
	StarterHookMgr();
	~StarterHookMgr();

	virtual bool reconfig() override;

	virtual bool useProcd() const override {return true;}

	virtual const std::string paramPrefix() const override {return "STARTER";}


		/**
		   See if we're configured to use HOOK_PREPARE_JOB and spawn it.
		   @return 1 if we spawned the hook, 0 if we're not configured
		   to use this hook for the job's hook keyword, or -1 on error.
		*/
	int tryHookPrepareJob();

		/**
		   See if we're configured to use HOOK_PREPARE_JOB_BEFORE_TRANSFER and spawn it.
		   @return 1 if we spawned the hook, 0 if we're not configured
			to use this hook for the job's hook keyword, or -1 on error.
		*/
	int tryHookPrepareJobPreTransfer();

		/**
		   Invoke HOOK_UPDATE_JOB_INFO to provide a periodic update
		   for job information such as image size, CPU time, etc.
		   Also called on temporary state changes like suspend/resume.
		   @param job_info ClassAd of job info for the update.
		   @return True if a hook is spawned, otherwise false.
		*/
	bool hookUpdateJobInfo(ClassAd* job_info);

		/**
		   Invoke HOOK_JOB_EXIT to tell the outside world the
		   final status of a given job, including the exit status,
		   total CPU time, final image size, etc.  The starter will
		   wait until this hook returns before exiting, although all
		   output from the hook is ignored, including the exit status.

		   @param job_info ClassAd of final info about the job.
		   @param exit_reason String explaining why the job exited.
		     Possible values: "exit" (on its own), "hold", "remove",
			 or "evict" (PREEMPT, condor_vacate, condor_off, etc).
			 This string is passed as argv[1] for the hook.

		   @return 1 if we spawned the hook or it's already running,
		     0 if we're not configured to use this hook for the job's
		     hook keyword, or -1 on error.
		 */
	int tryHookJobExit(ClassAd* job_info, const char* exit_reason);

	int getExitHookTimeout() const { return m_hook_job_exit_timeout; };

private:
		/// The path to HOOK_PREPARE_JOB, if defined.
	std::string m_hook_prepare_job;
		/// The path to HOOK_PREPARE_JOB_BEFORE_TRANSFER, if defined.
	std::string m_hook_prepare_job_before_transfer;
		/// The path to HOOK_UPDATE_JOB_INFO, if defined.
	std::string m_hook_update_job_info;
		/// The path to HOOK_JOB_EXIT, if defined.
	std::string m_hook_job_exit;

		// Arguments to pass to the HOOK_PREPARE_JOB
	ArgList m_hook_prepare_jobs_args;

	int m_hook_job_exit_timeout;

		/// Clears out all the hook paths we've validated and saved.
	void clearHookPaths( void );

	int tryHookPrepareJob_implementation(const char *hook_path, bool after_transfer);
};


/**
   Manages an invocation of HOOK_PREPARE_JOB.
*/
class HookPrepareJobClient : public HookClient
{
public:
	friend class StarterHookMgr;

	HookPrepareJobClient(const char* hook_path, bool after_filetransfer);

		/**
		   HOOK_PREPARE_JOB has exited.  If the hook exited with a
		   non-zero status, the job environment couldn't be prepared,
		   so the job is not spawned and the starter exits immediately.
		   Otherwise, we let the Starter know the job is ready to run.
		*/
	virtual void hookExited(int exit_status);
};


/**
   Manages an invocation of HOOK_JOB_EXIT.
*/
class HookJobExitClient : public HookClient
{
public:
	friend class StarterHookMgr;

	HookJobExitClient(const char* hook_path);
	virtual void hookExited(int exit_status);
};


#endif /* _CONDOR_STARTER_HOOK_MGR_H */
