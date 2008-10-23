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
#include "classad_newold.h"
#include "JobRouterHookMgr.h"
#include "status_string.h"
#include "JobRouter.h"
#include "set_user_from_ad.h"

extern JobRouter* job_router;

template class SimpleList<HOOK_RUN_INFO*>;
SimpleList<HOOK_RUN_INFO*> JobRouterHookMgr::m_job_hook_list;


// // // // // // // // // // // // 
// JobRouterHookMgr
// // // // // // // // // // // //

JobRouterHookMgr::JobRouterHookMgr()
	: HookClientMgr()
{
	m_hook_translate = NULL;
	m_hook_update_job_info = NULL;
	m_hook_job_exit = NULL;
	m_hook_cleanup = NULL;

	dprintf( D_FULLDEBUG, "Instantiating a JobRouterHookMgr\n" );
}


JobRouterHookMgr::~JobRouterHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the JobRouterHookMgr\n" );

	// Delete our copies of the paths for each hook.
	clearHookPaths();

	JobRouterHookMgr::removeAllKnownHooks();
}


void
JobRouterHookMgr::clearHookPaths()
{
	if (m_hook_translate != NULL)
	{
		free(m_hook_translate);
		m_hook_translate = NULL;
	}
	if (m_hook_update_job_info != NULL)
	{
		free(m_hook_update_job_info);
		m_hook_update_job_info = NULL;
	}
	if (m_hook_job_exit != NULL)
	{
		free(m_hook_job_exit);
		m_hook_job_exit = NULL;
	}
	if (m_hook_cleanup != NULL)
	{
		free(m_hook_cleanup);
		m_hook_cleanup = NULL;
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
	// Clear out our old copies of each hook's path.
	clearHookPaths();

	m_hook_translate = getHookPath(HOOK_TRANSLATE_JOB);
	m_hook_update_job_info = getHookPath(HOOK_UPDATE_JOB_INFO);
	m_hook_job_exit = getHookPath(HOOK_JOB_EXIT);
	m_hook_cleanup = getHookPath(HOOK_JOB_CLEANUP);

	return true;
}


char*
JobRouterHookMgr::getHookPath(HookType hook_type)
{
	MyString _param;
	_param.sprintf("JOB_ROUTER_HOOK_%s", getHookTypeString(hook_type));

	return validateHookPath(_param.Value());
}


int
JobRouterHookMgr::hookTranslateJob(RoutedJob* r_job, std::string &route_info)
{
	ClassAd temp_ad;

	if (NULL == m_hook_translate)
	{
		// hook not defined, which is ok
		dprintf(D_FULLDEBUG, "HOOK_TRANSLATE_JOB not configured.\n");
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

	if (false == new_to_old(r_job->src_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookTranslateJob: "
			"failed to convert classad\n");
		return -1;
	}

	MyString hook_stdin;
	hook_stdin = route_info.c_str();
	hook_stdin += "\n------\n";
	temp_ad.sPrint(hook_stdin);

	TranslateClient* translate_client = new TranslateClient(m_hook_translate, r_job);
	if (NULL == translate_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookTranslateJob: "
			"failed to create translation client\n");
		return -1;
	}

	set_user_from_ad(r_job->src_ad);
	if (0 == spawn(translate_client, NULL, &hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookTranslateJob: "
				"failed to spawn HOOK_TRANSLATE_JOB (%s)\n", m_hook_translate);
		return -1;

	}
	uninit_user_ids();
	
	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_TRANSLATE_JOB))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookTranslateJob: "
				"failed to add HOOK_TRANSLATE_JOB to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_TRANSLATE_JOB (%s) invoked.\n",
			m_hook_translate);
	return 1;
}


int
JobRouterHookMgr::hookUpdateJobInfo(RoutedJob* r_job)
{
	ClassAd temp_ad;
	if (NULL == m_hook_update_job_info)
	{
		// hook not defined
		dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO not configured.\n");
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


	if (false == new_to_old(r_job->dest_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to convert classad\n");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	StatusClient* status_client = new StatusClient(m_hook_update_job_info, r_job);
	if (NULL == status_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to create status update client\n");
		return -1;
	}

	set_user_from_ad(r_job->src_ad);
	if (0 == spawn(status_client, NULL, &hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to spawn HOOK_UPDATE_JOB_INFO (%s)\n", m_hook_update_job_info);
		return -1;

	}
	uninit_user_ids();

	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_UPDATE_JOB_INFO))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to add HOOK_UPDATE_JOB_INFO to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			m_hook_update_job_info);
	return 1;
}


int
JobRouterHookMgr::hookJobExit(RoutedJob* r_job, const char* sandbox)
{
	ClassAd temp_ad;
	if (NULL == m_hook_job_exit)
	{
		// hook not defined
		dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT not configured.\n");
		return 0;
	}

	// Verify the exit hook hasn't already been spawned and that
	// we're not waiting for it to return.
	std::string key = r_job->dest_key;
	if (true == JobRouterHookMgr::checkHookKnown(key.c_str(),HOOK_JOB_EXIT))
	{
		dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookJobExit "
			"retried while still waiting for exit hook to return "
			"for job with key %s - ignoring\n", key.c_str());
		return 1;
	}

	if (false == new_to_old(r_job->dest_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to convert classad\n");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	ArgList args;
	args.AppendArg(sandbox);

	ExitClient *exit_client = new ExitClient(m_hook_job_exit, r_job);
	if (NULL == exit_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to create status exit client\n");
		return -1;
	}

	set_user_from_ad(r_job->src_ad);
	if (0 == spawn(exit_client, &args, &hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to spawn HOOK_JOB_EXIT (%s)\n", m_hook_job_exit);
		return -1;

	}
	uninit_user_ids();
	
	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_JOB_EXIT))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to add HOOK_JOB_EXIT to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT (%s) invoked.\n", m_hook_job_exit);
	return 1;
}


int
JobRouterHookMgr::hookJobCleanup(RoutedJob* r_job)
{
	ClassAd temp_ad;
	if (NULL == m_hook_cleanup)
	{
		// hook not defined
		dprintf(D_FULLDEBUG, "HOOK_JOB_CLEANUP not configured.\n");
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


	if (false == new_to_old(r_job->dest_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookJobCleanup: "
			"failed to convert classad\n");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	CleanupClient* cleanup_client = new CleanupClient(m_hook_cleanup, r_job);
	if (NULL == cleanup_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookJobCleanup: "
			"failed to create status update client\n");
		return -1;
	}

	set_user_from_ad(r_job->src_ad);
	if (0 == spawn(cleanup_client, NULL, &hook_stdin, PRIV_USER_FINAL))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::JobCleanup: "
				"failed to spawn HOOK_JOB_CLEANUP (%s)\n", m_hook_cleanup);
		return -1;

	}
	uninit_user_ids();

	// Add our info to the list of hooks currently running for this job.
	if (false == JobRouterHookMgr::addKnownHook(key.c_str(), HOOK_JOB_CLEANUP))
	{
		dprintf(D_ALWAYS, "ERROR in JobRouterHookMgr::hookJobCleanup: "
				"failed to add HOOK_JOB_CLEANUP to list of "
				"hooks running for job key %s\n", key.c_str());
	}

	dprintf(D_FULLDEBUG, "HOOK_JOB_CLEANUP (%s) invoked.\n",
			m_hook_cleanup);
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
	hook_info->hook_type = hook;
	hook_info->key = strdup(key);
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
		dprintf(D_ALWAYS|D_FAILURE, "TranslateClient::hookExited "
			"Failed to remove hook info for job key %s.\n", key.c_str());
		EXCEPT("TranslateClient::hookExited: Received exit "
			"notification for job with key %s, which isn't a key "
			"for a job known to have a translate hook running.",
			 key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.Length())
	{
		dprintf(D_ALWAYS, "TranslateClient::hookExited: "
				"Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.Value());
	}
	if (m_std_out.Length())
	{
		ClassAd old_job_ad;
		classad::ClassAd new_job_ad;
		m_std_out.Tokenize();
		const char* hook_line = NULL;
		while ((hook_line = m_std_out.GetNextToken("\n", true)))
		{
			if (!old_job_ad.Insert(hook_line))
			{
				dprintf(D_ALWAYS, "TranslateClient::hookExited: "
						"Failed to insert \"%s\" into ClassAd, "
						"ignoring invalid hook output\n", hook_line);
				return;
			}
		}
		if (false == old_to_new(old_job_ad, new_job_ad))
		{
			dprintf(D_ALWAYS, "TranslateClient::hookExited: "
					"Failed to convert ClassAd, "
					"ignoring invalid hook output\n");
			return;
		}
		m_routed_job->dest_ad = new_job_ad;
		m_routed_job->is_sandboxed = true;
	}
	else
	{
		dprintf(D_ALWAYS, "TranslateClient::hookExited: Hook %s (pid %d) returned no data.\n",
				m_hook_path, (int)m_pid);
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
		dprintf(D_ALWAYS|D_FAILURE, "StatusClient::hookExited "
			"Failed to remove hook info for job key %s.\n", key.c_str());
		EXCEPT("StatusClient::hookExited: Received exit notification "
			"for job with key %s, which isn't a key for a job "
			"known to have a status hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.Length())
	{
		dprintf(D_ALWAYS,
				"StatusClient::hookExited: Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.Value());
	}
	if (m_std_out.Length())
	{
		ClassAd old_job_ad;
		classad::ClassAd new_job_ad;
		m_std_out.Tokenize();
		const char* hook_line = NULL;
		while ((hook_line = m_std_out.GetNextToken("\n", true)))
		{
			if (!old_job_ad.Insert(hook_line))
			{
				dprintf(D_ALWAYS, "StatusClient::hookExited: "
						"Failed to insert \"%s\" into "
						"ClassAd, ignoring invalid "
						"hook output.  Job status NOT updated.\n", hook_line);
				return;
			}
		}
		if (false == old_to_new(old_job_ad, new_job_ad))
		{
			dprintf(D_ALWAYS, "StatusClient::hookExited: Failed "
					"to convert ClassAd, ignoring invalid "
					"hook output.  Job status NOT updated.\n");
			return;
		}
		m_routed_job->dest_ad = new_job_ad;
	}
	else
	{
		dprintf(D_FULLDEBUG, "StatusClient::hookExited: Hook %s (pid %d) returned no data.\n",
				m_hook_path, (int)m_pid);
	}
	job_router->UpdateJobStatus(m_routed_job);
}


// // // // // // // // // // // // 
// ExitClient class
// // // // // // // // // // // // 

ExitClient::ExitClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_JOB_EXIT, hook_path, true)
{
	m_routed_job = r_job;
}


void
ExitClient::hookExited(int exit_status)
{
	std::string key = m_routed_job->dest_key;
	if (false == JobRouterHookMgr::removeKnownHook(key.c_str(), HOOK_JOB_EXIT))
	{
		dprintf(D_ALWAYS|D_FAILURE, "ExitClient::hookExited: "
			"Failed to remove hook info for job key %s.\n", key.c_str());
		EXCEPT("ExitClient::hookExited: Received exit notification for "
			"job with key %s, which isn't a key for a job known "
			"to have an exit hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);

	if (m_std_err.Length())
	{
		dprintf(D_ALWAYS,
				"ExitClient::hookExited: Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.Value());
	}

	// Only tell the job router to finalize the job if the hook exited
	// successfully
	if (true == WIFSIGNALED(exit_status) || 0 == WEXITSTATUS(exit_status))
	{
		// Tell the JobRouter to finalize the job.
		job_router->FinishFinalizeJob(m_routed_job);
	}
	else
	{
		// Hook failed
		MyString error_msg = "";
		statusString(exit_status, error_msg);
		dprintf(D_ALWAYS|D_FAILURE, "ExitClient::hookExited: "
			"HOOK_JOB_EXIT (%s) failed (%s)\n", m_hook_path, 
			error_msg.Value());
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
		dprintf(D_ALWAYS|D_FAILURE, "CleanupClient::hookExited: "
			"Failed to remove hook info for job key %s.\n", key.c_str());
		EXCEPT("CleanupClient::hookExited: Received exit notification "
			"for job with key %s, which isn't a key for a job "
			"known to have a cleanup hook running.", key.c_str());
		return;
	}

	HookClient::hookExited(exit_status);
	if (m_std_err.Length())
	{
		dprintf(D_ALWAYS,
				"CleanupClient::hookExited: Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.Value());
	}

	// Only tell the job router to finish the cleanup of the job if the
	// hook exited successfully
	if (true == WIFSIGNALED(exit_status) || 0 == WEXITSTATUS(exit_status))
	{
		// Tell the JobRouter to cleanup the job.
		job_router->FinishCleanupJob(m_routed_job);
	}
	else
	{
		// Hook failed
		MyString error_msg = "";
		statusString(exit_status, error_msg);
		dprintf(D_ALWAYS|D_FAILURE, "CleanupClient::hookExited: "
			"HOOK_JOB_CLEANUP (%s) failed (%s)\n", m_hook_path, 
			error_msg.Value());
	}
}
