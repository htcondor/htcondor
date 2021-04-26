/***************************************************************
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
#include "condor_config.h"
#include "hook_utils.h"
#include "JobRouterHookMgr.h"
#include "status_string.h"
#include "JobRouter.h"
#include "set_user_priv_from_ad.h"
#include "Scheduler.h"
#include "submit_job.h"

extern JobRouter* job_router;

SimpleList<HOOK_RUN_INFO*> JobRouterHookMgr::m_job_hook_list;


// // // // // // // // // // // // 
// JobRouterHookMgr
// // // // // // // // // // // //

JobRouterHookMgr::JobRouterHookMgr()
	: HookClientMgr(),
	  m_warn_cleanup(true),
	  m_warn_update(true),
	  m_warn_translate(true),
	  m_warn_exit(true),
	  NUM_HOOKS(5),
	  UNDEFINED("UNDEFINED"),
	  m_hook_paths(hashFunction)
{
	// JOB_EXIT is an old name for JOB_FINALIZE.
	// Keep both for anyone still using the old name.
	m_hook_maps[HOOK_TRANSLATE_JOB] = 1;
	m_hook_maps[HOOK_UPDATE_JOB_INFO] = 2;
	m_hook_maps[HOOK_JOB_EXIT] = 3;
	m_hook_maps[HOOK_JOB_CLEANUP] = 4;
	m_hook_maps[HOOK_JOB_FINALIZE] = 5;
	m_default_hook_keyword = NULL;

	dprintf(D_FULLDEBUG, "Instantiating a JobRouterHookMgr\n");
}


JobRouterHookMgr::~JobRouterHookMgr()
{
	dprintf(D_FULLDEBUG, "Deleting the JobRouterHookMgr\n");

	// Delete our copies of the paths for each hook.
	clearHookPaths();

	JobRouterHookMgr::removeAllKnownHooks();

	if (NULL != m_default_hook_keyword)
	{
		free(m_default_hook_keyword);
	}
}


void
JobRouterHookMgr::clearHookPaths()
{
	std::string key;
	char** paths;
	if (0 < m_hook_paths.getNumElements())
	{
		m_hook_paths.startIterations();
		while (m_hook_paths.iterate(key, paths))
		{
			for (int i = 0; i < NUM_HOOKS; i++)
			{
				if (NULL != paths[i] && UNDEFINED != paths[i])
				{
					free(paths[i]);
				}
			}
			delete[] paths;
		}
		m_hook_paths.clear();
	}
}


bool
JobRouterHookMgr::initialize()
{
	reconfig();
	return HookClientMgr::initialize();
}


bool
JobRouterHookMgr::reconfig()
{
	// Clear out old copies of each hook's path.
	clearHookPaths();

	// Get the default hook keyword if it's defined
	if (NULL != m_default_hook_keyword)
	{
		free(m_default_hook_keyword);
	}
	m_default_hook_keyword = param("JOB_ROUTER_HOOK_KEYWORD");
	m_warn_translate = true;
	m_warn_exit = true;
	m_warn_update = true;
	m_warn_cleanup = true;
	return true;
}


char*
JobRouterHookMgr::getHookPath(HookType hook_type, const classad::ClassAd &ad)
{
	std::string keyword;
	char** paths;
	char* hook_path;

	if (0 == m_hook_maps[hook_type])
	{
		dprintf(D_ALWAYS, "JobRouterHookMgr failure: Unable to get hook path for unknown hook type '%s'\n", getHookTypeString(hook_type));
		return NULL;
	}

	keyword = getHookKeyword(ad);
	if (0 == keyword.length())
	{
		return NULL;
	}

	if (0 > m_hook_paths.lookup(keyword, paths))
	{
		// Initialize the hook paths for this keyword
		paths = new char*[NUM_HOOKS];
		for (int i = 0; i < NUM_HOOKS; i++)
		{
			paths[i] = NULL;
		}
		m_hook_paths.insert(keyword, paths);
	}

	hook_path = paths[m_hook_maps[hook_type]-1];
	if (NULL == hook_path)
	{
		std::string _param;
		formatstr(_param, "%s_HOOK_%s", keyword.c_str(), getHookTypeString(hook_type));
        // Here the distinction between undefined hook and a hook path error 
        // is being collapsed
		validateHookPath(_param.c_str(), hook_path);
		dprintf(D_FULLDEBUG, "Hook %s: %s\n", _param.c_str(),
			hook_path ? hook_path : "UNDEFINED");
	}
	else if (UNDEFINED == hook_path)
	{
		hook_path = NULL;
	}
	return hook_path;
}


std::string
JobRouterHookMgr::getHookKeyword(const classad::ClassAd &ad)
{
	std::string hook_keyword;

	if (false == ad.EvaluateAttrString(ATTR_HOOK_KEYWORD, hook_keyword))
	{
		if ( m_default_hook_keyword ) {
			hook_keyword = m_default_hook_keyword;
		}
	}
	return hook_keyword;
}


int
JobRouterHookMgr::hookTranslateJob(RoutedJob* r_job, std::string &route_info)
{
	ClassAd temp_ad;
	char* hook_translate = getHookPath(HOOK_TRANSLATE_JOB, r_job->src_ad);

	if (NULL == hook_translate)
	{
		// hook not defined, which is ok
		if (m_warn_translate) {
			dprintf(D_FULLDEBUG, "HOOK_TRANSLATE_JOB not configured.\n");
			m_warn_translate = false;
		}
		return 0;
	}

	// Verify the translate hook hasn't already been spawned and that
	// we're not waiting for it to return.
	std::string key = r_job->src_key;
	if (true == JobRouterHookMgr::checkHookKnown(key.c_str(), HOOK_TRANSLATE_JOB))
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookTranslateJob "
			"retried while still waiting for translate hook to "
			"return for job with key %s - ignoring\n", key.c_str());
		return 1;
	}

	temp_ad = r_job->src_ad;

	std::string hook_stdin;
	hook_stdin = route_info.c_str();
	hook_stdin += "\n------\n";
	sPrintAd(hook_stdin, temp_ad);

	TranslateClient* translate_client = new TranslateClient(hook_translate, r_job);
	if (NULL == translate_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookTranslateJob: "
			"failed to create translation client\n");
		return -1;
	}

	TemporaryPrivSentry sentry(true);
	if (init_user_ids_from_ad(r_job->src_ad) == false) {
		delete translate_client;
		return -1;
	}
	if (0 == spawn(translate_client, NULL, hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookTranslateJob: "
				"failed to spawn HOOK_TRANSLATE_JOB (%s)\n", hook_translate);
		delete translate_client;
		return -1;
	}
	
	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_TRANSLATE_JOB))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookTranslateJob: "
				"failed to add HOOK_TRANSLATE_JOB to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_TRANSLATE_JOB (%s) invoked.\n",
			hook_translate);
	return 1;
}


int
JobRouterHookMgr::hookUpdateJobInfo(RoutedJob* r_job)
{
	ClassAd temp_ad;
	char* hook_update_job_info = getHookPath(HOOK_UPDATE_JOB_INFO, r_job->src_ad);

	if (NULL == hook_update_job_info)
	{
		// hook not defined
		if (m_warn_update) {
			dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO not configured.\n");
			m_warn_update = false;
		}
		return 0;
	}

	// Verify the status hook hasn't already been spawned and that
	// we're not waiting for it to return.
	std::string key = r_job->dest_key;
	if (true == JobRouterHookMgr::checkHookKnown(key.c_str(), HOOK_UPDATE_JOB_INFO))
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookUpdateJobInfo "
			"retried while still waiting for status hook to return "
			"for job with key %s - ignoring\n", key.c_str());
		return 1;
	}


	temp_ad = r_job->dest_ad;

	std::string hook_stdin;
	sPrintAd(hook_stdin, temp_ad);

	StatusClient* status_client = new StatusClient(hook_update_job_info, r_job);
	if (NULL == status_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to create status update client\n");
		return -1;
	}

	TemporaryPrivSentry sentry(true);
	if (init_user_ids_from_ad(r_job->src_ad) == false) {
		delete status_client;
		return -1;
	}
	if (0 == spawn(status_client, NULL, hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to spawn HOOK_UPDATE_JOB_INFO (%s)\n", hook_update_job_info);
		delete status_client;
		return -1;

	}

	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_UPDATE_JOB_INFO))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to add HOOK_UPDATE_JOB_INFO to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			hook_update_job_info);
	return 1;
}


int
JobRouterHookMgr::hookJobExit(RoutedJob* r_job)
{
	ClassAd temp_ad;
	char* hook_job_exit = getHookPath(HOOK_JOB_FINALIZE, r_job->src_ad);

	// JOB_EXIT is an old name for the JOB_FINALIZE hook.
	// CRUFT Remove this check of the old name in a future release.
	if ( NULL == hook_job_exit ) {
		hook_job_exit = getHookPath(HOOK_JOB_EXIT, r_job->src_ad);
	}
	if (NULL == hook_job_exit)
	{
		// hook not defined
		if (m_warn_exit) {
			dprintf(D_FULLDEBUG, "HOOK_JOB_FINALIZE not configured.\n");
			m_warn_exit = false;
		}
		return 0;
	}

	// Verify the exit hook hasn't already been spawned and that
	// we're not waiting for it to return.
	std::string key = r_job->dest_key;
	if (true == JobRouterHookMgr::checkHookKnown(key.c_str(),HOOK_JOB_FINALIZE))
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookJobExit "
			"retried while still waiting for exit hook to return "
			"for job with key %s - ignoring\n", key.c_str());
		return 1;
	}

	temp_ad = r_job->src_ad;

	std::string hook_stdin;
	sPrintAd(hook_stdin, temp_ad);
	hook_stdin += "\n------\n";

	temp_ad = r_job->dest_ad;
	sPrintAd(hook_stdin, temp_ad);

	ExitClient *exit_client = new ExitClient(hook_job_exit, r_job);
	if (NULL == exit_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to create exit client\n");
		return -1;
	}

	TemporaryPrivSentry sentry(true);
	if (init_user_ids_from_ad(r_job->src_ad) == false) {
		delete exit_client;
		return -1;
	}
	if (0 == spawn(exit_client, NULL, hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to spawn HOOK_JOB_FINALIZE (%s)\n", hook_job_exit);
		delete exit_client;
		return -1;

	}
	
	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_JOB_FINALIZE))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to add HOOK_JOB_FINALIZE to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_JOB_FINALIZE (%s) invoked.\n", hook_job_exit);
	return 1;
}


int
JobRouterHookMgr::hookJobCleanup(RoutedJob* r_job)
{
	ClassAd temp_ad;
	char* hook_cleanup = getHookPath(HOOK_JOB_CLEANUP, r_job->src_ad);

	if (NULL == hook_cleanup)
	{
		// hook not defined
		if (m_warn_cleanup) {
			dprintf(D_FULLDEBUG, "HOOK_JOB_CLEANUP not configured.\n");
			m_warn_cleanup = false;
		}
		return 0;
	}

	if (0 >= r_job->dest_ad.size())
	{
		return 0;
	}

	// Verify the cleanup hook hasn't already been spawned and that
	// we're not waiting for it to return.
	std::string key = r_job->dest_key;
	if (true == JobRouterHookMgr::checkHookKnown(key.c_str(), HOOK_JOB_CLEANUP))
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookJobCleanup "
			"retried while still waiting for cleanup hook to "
			"return for job with key %s - ignoring\n", key.c_str());
		return 1;
	}


	temp_ad = r_job->dest_ad;

	std::string hook_stdin;
	sPrintAd(hook_stdin, temp_ad);

	CleanupClient* cleanup_client = new CleanupClient(hook_cleanup, r_job);
	if (NULL == cleanup_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookJobCleanup: "
			"failed to create status update client\n");
		return -1;
	}

	TemporaryPrivSentry sentry(true);
	if (init_user_ids_from_ad(r_job->src_ad) == false) {
		delete cleanup_client;
		return -1;
	}
	if (0 == spawn(cleanup_client, NULL, hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::JobCleanup: "
				"failed to spawn HOOK_JOB_CLEANUP (%s)\n", hook_cleanup);
		delete cleanup_client;
		return -1;

	}

	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_JOB_CLEANUP))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookJobCleanup: "
				"failed to add HOOK_JOB_CLEANUP to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_JOB_CLEANUP (%s) invoked.\n",
			hook_cleanup);
	return 1;
}


bool
JobRouterHookMgr::addKnownHook(const char* key, HookType hook)
{
	if (NULL == key)
	{
		return false;
	}
	HOOK_RUN_INFO *hook_info = (HOOK_RUN_INFO*) malloc(sizeof(HOOK_RUN_INFO));
	ASSERT( hook_info != NULL );
	hook_info->hook_type = hook;
	hook_info->key = strdup(key);
	ASSERT( hook_info->key );
	return(m_job_hook_list.Append(hook_info));
}


bool
JobRouterHookMgr::checkHookKnown(const char* key, HookType hook)
{
	HOOK_RUN_INFO *hook_info;
	m_job_hook_list.Rewind();
	while (m_job_hook_list.Next(hook_info))
	{
		if ((0 == strncmp(hook_info->key,key,strlen(hook_info->key))) &&
			 (hook_info->hook_type == hook))
		{
			return true;
		}
	}
	return false;
}


bool
JobRouterHookMgr::removeKnownHook(const char* key, HookType hook)
{
	bool found = false;
	HOOK_RUN_INFO *hook_info;
	m_job_hook_list.Rewind();
	while (m_job_hook_list.Next(hook_info))
	{
		if ((0 == strncmp(hook_info->key,key,strlen(hook_info->key))) &&
			 (hook_info->hook_type == hook))
		{
			found = true;
			break;
		}
	}

	if (true == found)
	{
		// Delete the information about this hook process
		m_job_hook_list.DeleteCurrent();
		free(hook_info->key);
		free(hook_info);
	}
	else
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::removeKnownHook "
			"called for unknown job key %s.\n", key);
	}

	return found;
}


void
JobRouterHookMgr::removeAllKnownHooks()
{
	HOOK_RUN_INFO *hook_info;
	m_job_hook_list.Rewind();
	while (m_job_hook_list.Next(hook_info))
	{
		m_job_hook_list.DeleteCurrent();
		free(hook_info->key);
		free(hook_info);
	}
}


// // // // // // // // // // // // 
// TranslateClient class
// // // // // // // // // // // // 

TranslateClient::TranslateClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_TRANSLATE_JOB, hook_path, true)
{
	m_routed_job = r_job;
}


void
TranslateClient::hookExited(int exit_status)
{
	std::string key = m_routed_job->src_key;
	if (false == JobRouterHookMgr::removeKnownHook(key.c_str(), HOOK_TRANSLATE_JOB))
	{
		dprintf(D_ALWAYS|D_FAILURE, "TranslateClient::hookExited (%s): "
			"Failed to remove hook info for job key %s.\n", 
			m_routed_job->JobDesc().c_str(), key.c_str());
		EXCEPT("TranslateClient::hookExited: Received exit "
			"notification for job with key %s, which isn't a key "
			"for a job known to have a translate hook running.",
			 key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.length())
	{
		dprintf(D_ALWAYS, "TranslateClient::hookExited (%s): "
				"Warning, hook %s (pid %d) printed to stderr: "
				"%s\n", m_routed_job->JobDesc().c_str(), 
				m_hook_path, (int)m_pid, m_std_err.c_str());
	}
	if (m_std_out.length() && 0 == WEXITSTATUS(exit_status))
	{
		ClassAd job_ad;
		const char* hook_line = NULL;

		MyStringTokener tok;
		tok.Tokenize(m_std_out.c_str());
		while ((hook_line = tok.GetNextToken("\n", true)))
		{
			if (!job_ad.Insert(hook_line))
			{
				dprintf(D_ALWAYS, "TranslateClient::hookExited "
						"(%s): Failed to insert \"%s\" "
						"into ClassAd, ignoring "
						"invalid hook output\n", 
						m_routed_job->JobDesc().c_str(),
						hook_line);
				job_router->GracefullyRemoveJob(m_routed_job);
				return;
			}
		}
		m_routed_job->dest_ad = job_ad;
	}
	else
	{
		if (0 == WEXITSTATUS(exit_status))
		{
			dprintf(D_ALWAYS, "TranslateClient::hookExited (%s): "
					"Hook %s (pid %d) returned no data.\n",
					m_routed_job->JobDesc().c_str(), 
					m_hook_path, (int)m_pid);
		}
		else
		{
			dprintf(D_ALWAYS, "TranslateClient::hookExited (%s): "
					"Hook %s (pid %d) exited with return "
					"status %d.  Ignoring output.\n", 
					m_routed_job->JobDesc().c_str(),
					m_hook_path, (int)m_pid,
					(int)WEXITSTATUS(exit_status));
			job_router->GracefullyRemoveJob(m_routed_job);
		}
		return;
	}

	job_router->FinishSubmitJob(m_routed_job);
}


// // // // // // // // // // // // 
// StatusClient class
// // // // // // // // // // // // 

StatusClient::StatusClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_UPDATE_JOB_INFO, hook_path, true)
{
	m_routed_job = r_job;
}


void
StatusClient::hookExited(int exit_status)
{
	std::string key = m_routed_job->dest_key;
	if (false == JobRouterHookMgr::removeKnownHook(key.c_str(), HOOK_UPDATE_JOB_INFO))
	{
		dprintf(D_ALWAYS|D_FAILURE, "StatusClient::hookExited (%s): "
			"Failed to remove hook info for job key %s.\n", 
			m_routed_job->JobDesc().c_str(), key.c_str());
		EXCEPT("StatusClient::hookExited: Received exit notification "
			"for job with key %s, which isn't a key for a job "
			"known to have a status hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.length())
	{
		dprintf(D_ALWAYS, "StatusClient::hookExited (%s): Warning, "
				"hook %s (pid %d) printed to stderr: %s\n",
				m_routed_job->JobDesc().c_str(), m_hook_path,
				(int)m_pid, m_std_err.c_str());
	}
	if (m_std_out.length() && 0 == WEXITSTATUS(exit_status))
	{
		ClassAd job_ad;
		const char* hook_line = NULL;
		const char* attrs_to_delete[] = {
			ATTR_MY_TYPE,
			ATTR_TARGET_TYPE,
			NULL };

		MyStringTokener tok;
		tok.Tokenize(m_std_out.c_str());
		while ((hook_line = tok.GetNextToken("\n", true)))
		{
			if (!job_ad.Insert(hook_line))
			{
				dprintf(D_ALWAYS, "StatusClient::hookExited (%s): "
						"Failed to insert \"%s\" into "
						"ClassAd, ignoring invalid "
						"hook output.  Job status NOT "
						"updated.\n", m_routed_job->JobDesc().c_str(), hook_line);
				job_router->GracefullyRemoveJob(m_routed_job);
				return;
			}
		}
		// Delete attributes that may have been returned by the hook
		// but should not be included in the update
		for (int index = 0; attrs_to_delete[index] != NULL; ++index)
		{
			job_ad.Delete(attrs_to_delete[index]);
		}
		job_router->UpdateRoutedJobStatus(m_routed_job, job_ad);
	}
	else
	{
		if (0 == WEXITSTATUS(exit_status))
		{
			dprintf(D_FULLDEBUG, "StatusClient::hookExited (%s): "
					"Hook %s (pid %d) returned no data.\n",
					m_routed_job->JobDesc().c_str(),
					m_hook_path, (int)m_pid);
                	job_router->FinishCheckSubmittedJobStatus(m_routed_job);
		}
		else
		{
			dprintf(D_FULLDEBUG, "StatusClient::hookExited (%s): "						"Hook %s (pid %d) exited with return "
					"status %d.  Ignoring output.\n", 
					m_routed_job->JobDesc().c_str(),
					m_hook_path, (int)m_pid,
					(int)WEXITSTATUS(exit_status));
		}
	}
}


// // // // // // // // // // // // 
// ExitClient class
// // // // // // // // // // // // 

ExitClient::ExitClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_JOB_FINALIZE, hook_path, true)
{
	m_routed_job = r_job;
}


void
ExitClient::hookExited(int exit_status) {
	std::string key = m_routed_job->dest_key;
	if (false == JobRouterHookMgr::removeKnownHook(key.c_str(), HOOK_JOB_FINALIZE))
	{
		dprintf(D_ALWAYS|D_FAILURE, "ExitClient::hookExited (%s): "
			"Failed to remove hook info for job key %s.\n",
			m_routed_job->JobDesc().c_str(), key.c_str());
		EXCEPT("ExitClient::hookExited: Received exit notification for "
			"job with key %s, which isn't a key for a job known "
			"to have an exit hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.length())
	{
		dprintf(D_ALWAYS, "ExitClient::hookExited (%s): Warning, hook "
				"%s (pid %d) printed to stderr: %s\n",
				m_routed_job->JobDesc().c_str(), m_hook_path,
				(int)m_pid, m_std_err.c_str());
	}
	if (m_std_out.length())
	{
		if (0 == WEXITSTATUS(exit_status))
		{
			ClassAd job_ad;
			const char* hook_line = NULL;
			classad::ClassAdCollection *ad_collection = job_router->GetScheduler()->GetClassAds();
			classad::ClassAd *orig_ad = ad_collection->GetClassAd(m_routed_job->src_key);

			MyStringTokener tok;
			tok.Tokenize(m_std_out.c_str());
			while ((hook_line = tok.GetNextToken("\n", true)))
			{
				if (!job_ad.Insert(hook_line))
				{
					dprintf(D_ALWAYS, "ExitClient::hookExited (%s): "
							"Failed to insert \"%s\" into "
							"ClassAd, ignoring invalid "
							"hook output.  Job status NOT updated.\n", m_routed_job->JobDesc().c_str(), hook_line);
					job_router->RerouteJob(m_routed_job);
					return;
				}
			}
			if (false == m_routed_job->src_ad.Update(job_ad))
			{
				dprintf(D_ALWAYS, "ExitClient::hookExited (%s):"
						" Failed to update source job "
						"status.  Job status NOT "
						"updated.\n", m_routed_job->JobDesc().c_str());
				m_routed_job->SetSrcJobAd(m_routed_job->src_key.c_str(), orig_ad, ad_collection);
				job_router->RerouteJob(m_routed_job);
				return;
			}
			if (false == job_router->PushUpdatedAttributes(m_routed_job->src_ad))
			{
				dprintf(D_ALWAYS,"ExitClient::hookExited (%s): "
						"Failed to update src job in "
						"job queue.  Job status NOT "
						"updated.\n", m_routed_job->JobDesc().c_str());
				m_routed_job->SetSrcJobAd(m_routed_job->src_key.c_str(), orig_ad, ad_collection);
				job_router->RerouteJob(m_routed_job);
				return;
			}
			else
			{
				dprintf(D_FULLDEBUG,"ExitClient::hookExited "
						"(%s): updated src job\n",
						m_routed_job->JobDesc().c_str());
			}
		}
		else
		{
			dprintf(D_FULLDEBUG, "ExitClient::hookExited (%s): "
					"Hook exited with non-zero return "
					"code, ignoring hook output.\n",
					m_routed_job->JobDesc().c_str());
		}
	}

	// If the exit hook exited with non-zero status, tell the JobRouter to
	// re-route the job.
	if (0 != WEXITSTATUS(exit_status))
	{
		// Tell the JobRouter to reroute the job.
		job_router->RerouteJob(m_routed_job);
	}
	else
	{
		// Tell the JobRouter to finalize the job.
		job_router->FinishFinalizeJob(m_routed_job);
	}
}


// // // // // // // // // // // // 
// CleanupClient class
// // // // // // // // // // // // 

CleanupClient::CleanupClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_JOB_CLEANUP, hook_path, true)
{
	m_routed_job = r_job;
}


void
CleanupClient::hookExited(int exit_status)
{
	std::string key = m_routed_job->dest_key;
	if (false == JobRouterHookMgr::removeKnownHook(key.c_str(), HOOK_JOB_CLEANUP))
	{
		dprintf(D_ALWAYS|D_FAILURE, "CleanupClient::hookExited (%s): "
			"Failed to remove hook info for job key %s.\n", 
			m_routed_job->JobDesc().c_str(), key.c_str());
		EXCEPT("CleanupClient::hookExited: Received exit notification "
			"for job with key %s, which isn't a key for a job "
			"known to have a cleanup hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);
	if (m_std_err.length())
	{
		dprintf(D_ALWAYS,
				"CleanupClient::hookExited (%s): Warning, hook "
				"%s (pid %d) printed to stderr: %s\n",
				m_routed_job->JobDesc().c_str(), m_hook_path,
				(int)m_pid, m_std_err.c_str());
	}

	// Only tell the job router to finish the cleanup of the job if the
	// hook exited successfully
	if (WIFSIGNALED(exit_status) || ! WEXITSTATUS(exit_status))
	{
		// Tell the JobRouter to cleanup the job.
		job_router->FinishCleanupJob(m_routed_job);
	}
	else
	{
		// Hook failed
		std::string error_msg;
		statusString(exit_status, error_msg);
		dprintf(D_ALWAYS|D_FAILURE, "CleanupClient::hookExited (%s): "
			"HOOK_JOB_CLEANUP (%s) failed (%s)\n",
			m_routed_job->JobDesc().c_str(), m_hook_path, 
			error_msg.c_str());
	}
}
