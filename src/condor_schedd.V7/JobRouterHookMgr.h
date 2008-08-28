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
class ExitClient;
class RoutedJob;


class JobRouterHookMgr : public HookClientMgr
{
public:

	JobRouterHookMgr();
	~JobRouterHookMgr();

	bool initialize();
	bool reconfig();

	int hookTranslateJob(RoutedJob* r_job);
	int hookUpdateJobInfo(RoutedJob* r_job);
	int hookJobExit(RoutedJob* r_job, MyString sandbox);

private:

	char* m_hook_keyword;
	bool m_hook_keyword_initialized;

	char* m_hook_translate;
	char* m_hook_update_job_info;
	char* m_hook_job_exit;

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


class ExitClient : public HookClient
{
public:

	friend class JobRouterHookMgr;

	ExitClient(const char* hook_path, RoutedJob* r_job);
	virtual void hookExited(int exit_status);

protected:

	RoutedJob* m_routed_job;

};


#endif /* _CONDOR_JOB_ROUTER_HOOK_MGR_H */
