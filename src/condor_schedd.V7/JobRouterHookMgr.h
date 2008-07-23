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
class RoutedJob;


class JobRouterHookMgr : public HookClientMgr
{
public:

	JobRouterHookMgr();
	~JobRouterHookMgr();

	bool initialize();
	bool reconfig();

	bool hookVanillaToGrid(RoutedJob* r_job);
	bool hookJobExit(classad::ClassAd src, classad::ClassAd &dst);
	bool hookUpdateJobInfo(classad::ClassAd src, classad::ClassAd &dst);

private:

	char* m_hook_keyword;
	bool m_hook_keyword_initialized;

	char* m_hook_vanilla_to_grid;
	char* m_hook_job_exit;
	char* m_hook_update_job_info;

	char* getHookPath(HookType hook_type);
	void clearHookPaths( void );
	char* getHookKeyword();

};

class TranslateClient : public HookClient
{
public:

	friend class JobRouterHookMgr;

	TranslateClient(HookType hook_type, const char* hook_path, RoutedJob* r_job);
	virtual ~TranslateClient();
	virtual void hookExited(int exit_status);

	bool isDone();
	classad::ClassAd* reply();

protected:

	RoutedJob* m_routed_job;

};

#endif /* _CONDOR_JOB_ROUTER_HOOK_MGR_H */
