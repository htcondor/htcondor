#ifndef _CONDOR_JOB_ROUTER_HOOK_MGR_H
#define _CONDOR_JOB_ROUTER_HOOK_MGR_H

#include "condor_common.h"
#include "HookClient.h"
#include "HookClientMgr.h"
#include "enum_utils.h"
#include "JobRouter.h"

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"


class TranslateClient;
class StatusClient;
class RoutedJob;


class JobRouterHookMgr : public HookClientMgr
{
public:

	JobRouterHookMgr();
	~JobRouterHookMgr();

	bool initialize();
	bool reconfig();

	int hookVanillaToGrid(RoutedJob* r_job);
	bool hookUpdateJobInfo(RoutedJob* r_job);

private:

	char* m_hook_keyword;
	bool m_hook_keyword_initialized;

	char* m_hook_vanilla_to_grid;
	char* m_hook_update_job_info;

	char* getHookPath(HookType hook_type);
	void clearHookPaths( void );
	char* getHookKeyword();

};

class TranslateClient : public HookClient
{
public:

	friend class JobRouterHookMgr;

	TranslateClient(const char* hook_path, RoutedJob* r_job);
	virtual void hookExited(int exit_status);

protected:

	RoutedJob* m_routed_job;

};


class StatusClient : public HookClient
{
public:

	friend class JobRouterHookMgr;

	StatusClient(const char* hook_path, RoutedJob* r_job);
	virtual void hookExited(int exit_status);

protected:

	RoutedJob* m_routed_job;

};

#endif /* _CONDOR_JOB_ROUTER_HOOK_MGR_H */
