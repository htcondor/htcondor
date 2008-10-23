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

#ifndef _CONDOR_JOB_ROUTER_HOOK_MGR_H
#define _CONDOR_JOB_ROUTER_HOOK_MGR_H

#include "condor_common.h"
#include "HookClient.h"
#include "HookClientMgr.h"
#include "enum_utils.h"
#include "RoutedJob.h"

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"


class TranslateClient;
class StatusClient;
class ExitClient;
class RoutedJob;


typedef struct
{
	HookType hook_type;
	char* key;
} HOOK_RUN_INFO;


class JobRouterHookMgr : public HookClientMgr
{
public:

	JobRouterHookMgr();
	~JobRouterHookMgr();

	bool initialize();
	bool reconfig();

	int hookTranslateJob(RoutedJob* r_job, std::string &route_info);
	int hookUpdateJobInfo(RoutedJob* r_job);
	int hookJobExit(RoutedJob* r_job, const char* sandbox);
	int hookJobCleanup(RoutedJob* r_job);

	static bool checkHookKnown(const char* key, HookType hook);
	static bool addKnownHook(const char* key, HookType hook);
	static bool removeKnownHook(const char* key, HookType hook);
	static void removeAllKnownHooks();

	// List of job ids and hooks currently running and awaiting output
	static SimpleList<HOOK_RUN_INFO*> m_job_hook_list;

private:

	// Hook definitions
	char* m_hook_translate;
	char* m_hook_update_job_info;
	char* m_hook_job_exit;
	char* m_hook_cleanup;

	char* getHookPath(HookType hook_type);
	void clearHookPaths(void);

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


class CleanupClient : public HookClient
{
public:

	friend class JobRouterHookMgr;

	CleanupClient(const char* hook_path, RoutedJob* r_job);
	virtual void hookExited(int exit_status);

protected:

	RoutedJob* m_routed_job;

};


#endif /* _CONDOR_JOB_ROUTER_HOOK_MGR_H */
