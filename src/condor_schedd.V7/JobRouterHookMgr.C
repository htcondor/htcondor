#include "condor_common.h"
#include "condor_config.h"
//#include "basename.h"
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
	m_hook_vanilla_to_grid = NULL;
	m_hook_job_exit = NULL;
	m_hook_update_job_info = NULL;

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
	}
}


void
JobRouterHookMgr::clearHookPaths()
{
	if (m_hook_vanilla_to_grid != NULL)
	{
		free(m_hook_vanilla_to_grid);
		m_hook_vanilla_to_grid = NULL;
	}
	if (m_hook_job_exit != NULL)
	{
		free(m_hook_job_exit);
		m_hook_job_exit = NULL;
	}
	if (m_hook_update_job_info != NULL)
	{
		free(m_hook_update_job_info);
		m_hook_update_job_info = NULL;
	}
}


bool
JobRouterHookMgr::initialize()
{
	char* tmp = param("JOB_ROUTER_JOB_HOOK_KEYWORD");
	if (tmp)
	{
		m_hook_keyword = tmp;
		dprintf(D_FULLDEBUG, "Using JOB_ROUTER_JOB_HOOK_KEYWORD value from config file: \"%s\"\n", m_hook_keyword);
	}
	else
	{
		dprintf(D_FULLDEBUG, "No JOB_ROUTER_JOB_HOOK_HEYWORD defined so job router hooks will not be invoked.\n");
		return false;
	}
	dprintf(D_FULLDEBUG, "Initiating reconfig\n");
//	sleep(20);
	reconfig();
	return HookClientMgr::initialize();
}


bool
JobRouterHookMgr::reconfig()
{
	// Clear out our old copies of each hook's path.
	clearHookPaths();

	m_hook_vanilla_to_grid = getHookPath(HOOK_VANILLA_TO_GRID);
	m_hook_job_exit = getHookPath(HOOK_JOB_EXIT);
	m_hook_update_job_info = getHookPath(HOOK_UPDATE_JOB_INFO);

	return true;
}


char*
JobRouterHookMgr::getHookKeyword()
{
	if (!m_hook_keyword_initialized) {
		MyString param_name;
		m_hook_keyword = param("JOB_ROUTER_HOOK_KEYWORD");
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
JobRouterHookMgr::hookVanillaToGrid(RoutedJob* r_job)
{
	ClassAd temp_ad;

	if (m_hook_vanilla_to_grid == NULL)
	{
		return 0;
	}

	if (new_to_old(r_job->src_ad, temp_ad) == false)
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookVanillaToGrid: "
			"failed to convert classad");
		return -1;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

	TranslateClient* translate_client = new TranslateClient(HOOK_VANILLA_TO_GRID, m_hook_vanilla_to_grid, r_job);
	if (translate_client == NULL)
	{
		return -1;
	}
	if (spawn(translate_client, NULL, &hook_stdin) == 0)
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookVanillaToGrid: "
				"failed to spawn VANILLA_TO_GRID (%s)\n", m_hook_vanilla_to_grid);
		return -1;

	}
	dprintf(D_FULLDEBUG, "HOOK_VANILLA_TO_GRID (%s) invoked.\n",
			m_hook_vanilla_to_grid);
	return 1;
}


bool
JobRouterHookMgr::hookJobExit(classad::ClassAd src, classad::ClassAd &dst)
{
	ClassAd temp_ad;
	if (m_hook_job_exit == NULL)
	{
		return false;
	}

	if (new_to_old(src, temp_ad) == false)
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookJobExit: "
			"failed to convert classad");
		return false;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);
/*

	TranslateClient* translate_client = new TranslateClient(HOOK_JOB_EXIT, m_hook_job_exit);
	if (translate_client == NULL)
	{
		return false;
	}
	if (spawn(translate_client, NULL, &hook_stdin) == 0)
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookJobExit: "
				"failed to spawn JOB_EXIT (%s)\n", m_hook_vanilla_to_grid);
		return false;

	}
	dprintf(D_FULLDEBUG, "HOOK_JOB_EXIT (%s) invoked.\n",
			m_hook_vanilla_to_grid);
	while (translate_client->isDone() == false)
	{
		sleep(1);
	}
	dst = (*translate_client->reply());
*/
	return true;
}


bool
JobRouterHookMgr::hookUpdateJobInfo(classad::ClassAd src, classad::ClassAd &dst)
{
	ClassAd temp_ad;
	if (m_hook_update_job_info == NULL)
	{
		return false;
	}

	if (new_to_old(src, temp_ad) == false)
	{
		dprintf(D_ALWAYS|D_FAILURE,
			"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
			"failed to convert classad");
		return false;
	}

	MyString hook_stdin;
	temp_ad.sPrint(hook_stdin);

/*
	TranslateClient* translate_client = new TranslateClient(HOOK_UPDATE_JOB_INFO, m_hook_update_job_info);
	if (translate_client == NULL)
	{
		return false;
	}
	if (spawn(translate_client, NULL, &hook_stdin) == 0)
	{
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in JobRouterHookMgr::hookUpdateJobInfo: "
				"failed to spawn UPDATE_JOB_INFO (%s)\n", m_hook_update_job_info);
		return false;

	}
	dprintf(D_FULLDEBUG, "HOOK_UPDATE_JOB_INFO (%s) invoked.\n",
			m_hook_vanilla_to_grid);
	while (translate_client->isDone() == false)
	{
		sleep(10);
	}
	dst = (*translate_client->reply());
*/
	return true;
}


// // // // // // // // // // // // 
// TranslateClient class
// // // // // // // // // // // // 

TranslateClient::TranslateClient(HookType hook_type, const char* hook_path, RoutedJob* r_job)
	: HookClient(hook_type, hook_path, true)
{
	m_routed_job = r_job;
}


TranslateClient::~TranslateClient()
{
	// Nothing to clean up	
}


void
TranslateClient::hookExited(int exit_status)
{
	HookClient::hookExited(exit_status);

	if (m_std_err.Length())
	{
		dprintf(D_ALWAYS,
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
				dprintf(D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
						"ignoring invalid hook output\n", hook_line);
				return;
			}
		}
		if (old_to_new(old_job_ad, new_job_ad) == false)
		{
			dprintf(D_ALWAYS, "Failed to convert ClassAd, "
					"ignoring invalid hook output\n");
		}
		m_routed_job->dest_ad = new_job_ad;
	}
	else
	{
		dprintf(D_ALWAYS, "Failed to translate ClassAd.  Hook %s (pid %d) returned no data.\n",
				m_hook_path, (int)m_pid);
		return;
	}
	job_router->FinishSubmitJob(m_routed_job);
}
