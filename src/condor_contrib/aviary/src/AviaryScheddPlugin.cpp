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
#include "get_daemon_name.h"

// local includes
#include "AviaryScheddPlugin.h"
#include "AviaryProvider.h"
#include "SchedulerObject.h"
#include "AviaryUtils.h"
#include "LocatorObject.h"

// Global from the condor_schedd, it's name
extern char * Name;

// Any key that begins with the '0' char is either the
// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])

using namespace std;
using namespace aviary::job;
using namespace aviary::transport;
using namespace aviary::util;
using namespace aviary::locator;

// global SchedulerObject
// TODO: convert to singleton
AviaryProvider* provider = NULL;
SchedulerObject* schedulerObj = NULL;

void
AviaryScheddPlugin::earlyInitialize()
{

    // Since this plugin is registered with multiple
    // PluginManagers it may be initialized more than once,
    // and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

    string log_name("aviary_job.log");
    string id_name("job"); id_name+=SEPARATOR; id_name+=getScheddName();
    provider = AviaryProviderFactory::create(log_name,id_name,
											 "SCHEDULER","JOB","services/job/");
    if (!provider) {
        EXCEPT("Unable to configure AviaryProvider. Exiting...");
    }

	schedulerObj = SchedulerObject::getInstance();

	dirtyJobs = new DirtyJobsType();

	isHandlerRegistered = false;

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate transport socket");
	}
	if (!sock->assign(provider->getListenerSocket())) {
		EXCEPT("Failed to bind transport socket");
	}
	int index;
	if (-1 == (index =
			   daemonCore->Register_Socket((Stream *) sock,
										   "Aviary Method Socket",
										   (SocketHandlercpp) ( &AviaryScheddPlugin::handleTransportSocket ),
										   "Handler for Aviary Methods.",
										   this))) {
		EXCEPT("Failed to register transport socket");
	}

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

		key.formatstr("%d.%d", id.cluster, id.proc);

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
	if (provider) {
		provider->invalidate();
		delete provider;
		provider = NULL;
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
AviaryScheddPlugin::handleTransportSocket(Stream *)
{
    // TODO: respond to a transport callback here?
    string provider_error;
    if (!provider->processRequest(provider_error)) {
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
							 const char *,
							 int )
{
	PROC_ID id;
	ClassAd *jobAd;

		// Skip any key that doesn't point to an actual job
	if (!IS_JOB(key)) return false;

//	dprintf(D_FULLDEBUG, "Processing: %s\n", key);

	id = getProcByString(key);
	if (id.cluster <= 0 || id.proc < 0) {
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
    char* value = NULL;
    if ( (GetAttributeString(id.cluster, id.proc,ATTR_JOB_SUBMISSION,submissionName) < 0) 
        && (GetAttributeExprNew(id.cluster, id.proc, ATTR_JOB_SUBMISSION,&value) < 0) ) {
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
				submissionName.formatstr("%s#%d", Name, dagman.cluster);
			}
		} else {
			submissionName.formatstr("%s#%d", Name, id.cluster);
		}

		MyString tmp;
		tmp += "\"";
		tmp += submissionName;
		tmp += "\"";
		SetAttribute(id.cluster, id.proc,
					 ATTR_JOB_SUBMISSION,
					 tmp.Value());
	}
    if (value) free (value);

	return true;
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
