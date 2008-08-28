#include "condor_common.h"
#include "condor_config.h"
#include "hook_utils.h"
#include "classad_newold.h"
#include "JobRouterHookMgr.h"

extern JobRouter* job_router;

// // // // // // // // // // // // 
// JobRouterHookMgr
// // // // // // // // // // // //

JobRouterHookMgr::JobRouterHookMgr()
	: HookClientMgr()
{
	m_hook_keyword = NULL;
	m_hook_translate = NULL;
	m_hook_update_job_info = NULL;
        m_hook_job_exit = NULL;

	dprintf( D_FULLDEBUG, "Instantiating a JobRouterHookMgr\n" );
}


JobRouterHookMgr::~JobRouterHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the JobRouterHookMgr\n" );

	// Delete our copies of the paths for each hook.
	clearHookPaths();

	if (m_hook_keyword)
	{
		free(m_hook_keyword);
		m_hook_keyword = NULL;
	}
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
}


bool
JobRouterHookMgr::initialize()
{
	char* tmp = param("JOB_HOOK_KEYWORD");
	if (tmp)
	{
		m_hook_keyword = tmp;
		dprintf(D_FULLDEBUG, "Using JOB_HOOK_KEYWORD value from config file: \"%s\"\n", m_hook_keyword);
	}
	else
	{
		dprintf(D_FULLDEBUG, "No JOB_HOOK_HEYWORD defined so job router hooks will not be invoked.\n");
		return false;
	}
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

	return true;
}


char*
JobRouterHookMgr::getHookKeyword()
{
	if (!m_hook_keyword_initialized) {
		MyString param_name;
		m_hook_keyword = param("JOB_HOOK_KEYWORD");
		m_hook_keyword_initialized = true;
	}
	return m_hook_keyword;
}


char*
JobRouterHookMgr::getHookPath(HookType hook_type)
{
	if (!m_hook_keyword)
	{
		return NULL;
	}
	MyString _param;
	_param.sprintf("%s_HOOK_%s", m_hook_keyword, getHookTypeString(hook_type));

	return validateHookPath(_param.Value());
}


int
JobRouterHookMgr::hookTranslateJob(RoutedJob* r_job)
{
	ClassAd temp_ad;

	if (NULL == m_hook_translate)
	{
		// hook not defined, which is ok
		return 0;
	}

	if (false == new_to_old(r_job->src_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookTranslateJob: "
			"failed to convert classad");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	TranslateClient* translate_client = new TranslateClient(m_hook_translate, r_job);
	if (NULL == translate_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookTranslateJob: "
			"failed to create translation client");
		return -1;
	}
	if (0 == spawn(translate_client, NULL, &hook_stdin))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookTranslateJob: "
				"failed to spawn HOOK_TRANSLATE_JOB (%s)\n", m_hook_translate);
		return -1;

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
		return 0;
	}

	if (false == new_to_old(r_job->dest_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to convert classad");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	StatusClient* status_client = new StatusClient(m_hook_update_job_info, r_job);
	if (NULL == status_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to create status update client");
		return -1;
	}
	if (0 == spawn(status_client, NULL, &hook_stdin))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to spawn HOOK_UPDATE_JOB_INFO (%s)\n", m_hook_update_job_info);
		return -1;

	}
	dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			m_hook_update_job_info);
	return 1;
}


int
JobRouterHookMgr::hookJobExit(RoutedJob* r_job, MyString sandbox)
{
	ClassAd temp_ad;
	if (NULL == m_hook_job_exit)
	{
		dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT not configured.\n");
		return 0;
	}

	// Verify the exit hook hasn't already been spawned and that
	// we're not waiting for it to return.
	HookClient *hook_client;
	m_client_list.Rewind();
	while (m_client_list.Next(hook_client)) {
		if (hook_client->type() == HOOK_JOB_EXIT) {
			dprintf(D_FULLDEBUG, "JobRouterHookMgr::hookJobExit "
				"retried while still waiting for HOOK_JOB_EXIT "
				"to return - ignoring\n");
			return 1;
		}
	}

	if (false == new_to_old(r_job->dest_ad, temp_ad))
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to convert classad");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	ArgList args;
	args.AppendArg(sandbox);

	hook_client = new ExitClient(m_hook_job_exit, r_job);
	if (NULL == hook_client)
	{
		dprintf(D_ALWAYS|D_FAILURE, 
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to create status exit client");
		return -1;
	}
	if (0 == spawn(hook_client, &args, &hook_stdin))
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to spawn HOOK_JOB_EXIT (%s)\n", m_hook_job_exit);
		return -1;

	}
	dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT (%s) invoked.\n", m_hook_job_exit);
	return 1;
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
		dprintf(D_ALWAYS, "StatusClient::hookExited: Hook %s (pid %d) returned no data.\n",
				m_hook_path, (int)m_pid);
		return;
	}
	job_router->UpdateJobStatus(m_routed_job);
}


// // // // // // // // // // // // 
// ExitClient class
// // // // // // // // // // // // 

ExitClient::ExitClient(const char* hook_path, RoutedJob* r_job)
	: HookClient(HOOK_UPDATE_JOB_INFO, hook_path, true)
{
	m_routed_job = r_job;
}


void
ExitClient::hookExited(int exit_status)
{
	HookClient::hookExited(exit_status);
	// Tell the JobRouter to finalize the job.
	job_router->FinishFinalizeJob(m_routed_job);
}
