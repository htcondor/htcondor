/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_qmgr.h"
#include "condor_config.h"

// local includes
#include "AviaryScheddPlugin.h"
#include "Axis2SoapProvider.h"
#include "SchedulerObject.h"

// Global from the condor_schedd, it's name
extern char * Name;

// Any key that begins with the '0' char is either the
// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])

using namespace std;
using namespace aviary::job;
using namespace aviary::soap;

// global SchedulerObject
// TODO: convert to singleton
Axis2SoapProvider* provider = NULL;
SchedulerObject* schedulerObj = NULL;

void
AviaryScheddPlugin::earlyInitialize()
{

    // Since this plugin is registered with multiple
    // PluginManagers it may be initialized more than once,
    // and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

	// config then env for our all-important axis2 repo dir
    const char* log_file = "./aviary_job.axis2.log";
	string repo_path;
	char *tmp = NULL;
	if (tmp = param("WSFCPP_HOME")) {
		repo_path = tmp;
		free(tmp);
	}
	else if (tmp = getenv("WSFCPP_HOME")) {
		repo_path = tmp;
	}
	else {
		EXCEPT("No WSFCPP_HOME in config or env");
	}

	int port = param_integer("HTTP_PORT",9090);
	int level = param_integer("AXIS2_DEBUG_LEVEL",AXIS2_LOG_LEVEL_CRITICAL);

    // init transport here
    provider = new Axis2SoapProvider(level,log_file,repo_path.c_str());
    string axis_error;
    if (!provider->init(port,AXIS2_HTTP_DEFAULT_SO_TIMEOUT,axis_error)) {
		dprintf(D_ALWAYS, "%s\n",axis_error.c_str());
        EXCEPT("Failed to initialize Axis2SoapProvider");
    }

	schedulerObj = SchedulerObject::getInstance();

	dirtyJobs = new DirtyJobsType();

	isHandlerRegistered = false;

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate transport socket");
	}
	if (!sock->assign(provider->getHttpListenerSocket())) {
		EXCEPT("Failed to bind transport socket");
	}
	int index;
	if (-1 == (index =
			   daemonCore->Register_Socket((Stream *) sock,
										   "Aviary Method Socket",
										   (SocketHandlercpp) ( &AviaryScheddPlugin::HandleTransportSocket ),
										   "Handler for Aviary Methods.",
										   this))) {
		EXCEPT("Failed to register transport socket");
	}

	dprintf(D_ALWAYS,"Axis2 listener on http port: %d\n",port);

	m_initialized = false;
}

void
AviaryScheddPlugin::initialize()
{
		// Since this plugin is registered with multiple
		// PluginManagers it may be initialized more than once,
		// and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

		// WalkJobQueue(int (*func)(ClassAd *))
	ClassAd *ad = GetNextJob(1);
	while (ad != NULL) {
		MyString key;
		PROC_ID id;
		int value;

		if (!ad->LookupInteger(ATTR_CLUSTER_ID, id.cluster)) {
			EXCEPT("%s on job is missing or not an integer", ATTR_CLUSTER_ID);
		}
		if (!ad->LookupInteger(ATTR_PROC_ID, id.proc)) {
			EXCEPT("%s on job is missing or not an integer", ATTR_PROC_ID);
		}
		if (!ad->LookupInteger(ATTR_JOB_STATUS, value)) {
			EXCEPT("%s on job is missing or not an integer", ATTR_JOB_STATUS);
		}

		key.sprintf("%d.%d", id.cluster, id.proc);

		processJob(key.Value(), ATTR_JOB_STATUS, value);

		FreeJobAd(ad);
		ad = GetNextJob(0);
	}
 
	m_initialized = true;
}


void
AviaryScheddPlugin::shutdown()
{
		// Since this plugin is registered with multiple
		// PluginManagers (eg, shadow) it may be shutdown
		// more than once, and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

	dprintf(D_FULLDEBUG, "AviaryScheddPlugin: shutting down...\n");

	if (schedulerObj) {
		delete schedulerObj;
		schedulerObj = NULL;
	}
}


void
AviaryScheddPlugin::update(int cmd, const ClassAd *ad)
{
	MyString hashKey;

	switch (cmd) {
	case UPDATE_SCHEDD_AD:
		dprintf(D_FULLDEBUG, "Received UPDATE_SCHEDD_AD\n");
		schedulerObj->update(*ad);
		break;
	default:
		dprintf(D_FULLDEBUG, "Unsupported command: %s\n",
				getCollectorCommandString(cmd));
	}
}


void
AviaryScheddPlugin::archive(const ClassAd */*ad*/) { };


void
AviaryScheddPlugin::newClassAd(const char */*key*/) { };


void
AviaryScheddPlugin::setAttribute(const char *key,
							   const char *name,
							   const char *value)
{
	if (!m_initialized) return;

//	dprintf(D_FULLDEBUG, "setAttribute: %s[%s] = %s\n", key, name, value);

	markDirty(key, name, value);
}


void
AviaryScheddPlugin::destroyClassAd(const char *_key)
{
	if (!m_initialized) return;

//	dprintf(D_FULLDEBUG, "destroyClassAd: %s\n", key);

	if (!IS_JOB(_key)) return;

		// If we wait to process the deletion the job ad will be gone
		// and we won't be able to lookup the Submission. So, we must
		// process the job immediately, but that also means we need to
		// process all pending changes for the job as well.
	DirtyJobsType::iterator entry = dirtyJobs->begin();
	while (dirtyJobs->end() != entry) {
		string key = (*entry).first;
		string name = (*entry).second.first;
		int value = (*entry).second.second;

		if (key == _key) {
			processJob(key.c_str(), name.c_str(), value);

				// No need to process this entry again later
			entry = dirtyJobs->erase(entry);
		} else {
			entry++;
		}
	}
}


void
AviaryScheddPlugin::deleteAttribute(const char */*key*/,
								  const char */*name*/) { }

int
AviaryScheddPlugin::HandleTransportSocket(Stream *)
{
    // TODO: respond to a transport callback here?
    string provider_error;
    if (!provider->processHttpRequest(provider_error)) {
        dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
    }

    return KEEP_STREAM;
}

void
AviaryScheddPlugin::processDirtyJobs()
{
	BeginTransaction();

	while (!dirtyJobs->empty()) {
		DirtyJobEntry entry = dirtyJobs->front(); dirtyJobs->pop_front();
		string key = entry.first;
		string name = entry.second.first;
		int value = entry.second.second;

		processJob(key.c_str(), name.c_str(), value);
	}

	CommitTransaction();

	isHandlerRegistered = false;
}


bool
AviaryScheddPlugin::processJob(const char *key,
							 const char *name,
							 int value)
{
	PROC_ID id;
	ClassAd *jobAd;

		// Skip any key that doesn't point to an actual job
	if (!IS_JOB(key)) return false;

//	dprintf(D_FULLDEBUG, "Processing: %s\n", key);

	id = getProcByString(key);
	if (id.cluster < 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Failed to parse key: %s - skipping\n", key);
		return false;
	}

		// Lookup the job ad assocaited with the key. If it is not
		// present, skip the key
	if (NULL == (jobAd = ::GetJobAd(id.cluster, id.proc, false))) {
		dprintf(D_ALWAYS,
				"NOTICE: Failed to lookup ad for %s - maybe deleted\n",
				key);
		return false;
	}

		// Store two pieces of information in the Job, 1. the
		// Submission's name, 2. the Submission's id
		//
		// Submissions indexed on their name, the id is present
		// for reconstruction of the Submission

		// XXX: Use the jobAd instead of GetAttribute below, gets us $$() expansion

	MyString submissionName;
	if (GetAttributeString(id.cluster, id.proc,
						   ATTR_JOB_SUBMISSION,
						   submissionName) < 0) {
			// Provide a default name for the Submission

			// If we are a DAG node, we default to our DAG group
		PROC_ID dagman;
		if (GetAttributeInt(id.cluster, id.proc,
							ATTR_DAGMAN_JOB_ID,
							&dagman.cluster) >= 0) {
			dagman.proc = 0;

			if (GetAttributeString(dagman.cluster, dagman.proc,
								   ATTR_JOB_SUBMISSION,
								   submissionName) < 0) {
					// This can only happen if the DAGMan job was
					// removed, and we remained, which should not
					// happen, but could. In such a case we are
					// orphaned, and we'll make a guess. We'll be
					// wrong if the DAGMan job didn't use the
					// default, but it is better to be wrong than
					// to fail entirely, which is the alternative.
				submissionName.sprintf("%s#%d", Name, dagman.cluster);
			}
		} else {
			submissionName.sprintf("%s#%d", Name, id.cluster);
		}

		MyString tmp;
		tmp += "\"";
		tmp += submissionName;
		tmp += "\"";
		SetAttribute(id.cluster, id.proc,
					 ATTR_JOB_SUBMISSION,
					 tmp.Value());
	}
}

void
AviaryScheddPlugin::markDirty(const char *key,
							const char *name,
							const char *value)
{
	if (!IS_JOB(key)) return;
	if (!(strcasecmp(name, ATTR_JOB_STATUS) == 0 ||
		  strcasecmp(name, ATTR_LAST_JOB_STATUS) == 0)) return;

	DirtyJobStatus status(name, atoi(value));
	DirtyJobEntry entry(key, status);
	dirtyJobs->push_back(DirtyJobEntry(key, DirtyJobStatus(name, atoi(value))));

	if (!isHandlerRegistered) {
			// To test destroyClassAd, set the timeout here to a few
			// seconds, submit a job and immediately delete it.
		daemonCore->Register_Timer(0,
								   (TimerHandlercpp)
								   &AviaryScheddPlugin::processDirtyJobs,
								   "Process Dirty",
								   this);
		isHandlerRegistered = true;
	}
}
