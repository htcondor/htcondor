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


#include "condor_common.h"

#include "MgmtScheddPlugin.h"

#include "SchedulerObject.h"
#include "SubmitterObject.h"
#include "JobServerObject.h"

#include "SubmitterUtils.h"

#include "condor_qmgr.h"
#include "condor_config.h"
#include "get_daemon_name.h"
#include "broker_utils.h"


// Global from the condor_schedd, it's name
extern char * Name;


	// Any key that begins with the '0' char is either the
 	// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])


using namespace std;
using namespace qpid::management;
using namespace qpid::types;
using namespace qmf::com::redhat;
using namespace qmf::com::redhat::grid;
using namespace com::redhat::grid;


void
MgmtScheddPlugin::earlyInitialize()
{
	char *host;
	char *username;
	char *password;
	char *mechanism;
	int port;
	char *tmp;
	string storefile;
	string schedd_name;

		// Since this plugin is registered with multiple
		// PluginManagers it may be initialized more than once,
		// and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

	singleton = new ManagementAgent::Singleton();

	submitterAds = new SubmitterHashTable(512, &hashFuncMyString);

	ManagementAgent *agent = singleton->getInstance();

	Scheduler::registerSelf(agent);
	Submitter::registerSelf(agent);
	m_isPublishing = param_boolean("QMF_PUBLISH_SUBMISSIONS", false);
	if (m_isPublishing) {
		JobServer::registerSelf(agent);
		Submission::registerSelf(agent);
	}

	port = param_integer("QMF_BROKER_PORT", 5672);
	if (NULL == (host = param("QMF_BROKER_HOST"))) {
		host = strdup("localhost");
	}

	if (NULL == (username = param("QMF_BROKER_USERNAME")))
	{
		username = strdup("");
	}

	if (NULL == (mechanism = param("QMF_BROKER_AUTH_MECH")))
	{
		mechanism = strdup("ANONYMOUS");
	}

	password = getBrokerPassword();


	tmp = param("QMF_STOREFILE");
	if (NULL == tmp) {
		storefile = ".schedd_storefile";
	} else {
		storefile = tmp;
		free(tmp); tmp = NULL;
	}

	tmp = param("SCHEDD_NAME");
	if (NULL == tmp) {
		schedd_name = default_daemon_name();
	} else {
		schedd_name = build_valid_daemon_name(tmp);
		free(tmp); tmp = NULL;
	}
	agent->setName("com.redhat.grid","scheduler", schedd_name.c_str());

	agent->init(string(host), port,
				param_integer("QMF_UPDATE_INTERVAL", 10),
				true,
				storefile,
				username,
				password,
				mechanism);

	free(host);
	free(username);
	free(password);
	free(mechanism);

	scheduler = new SchedulerObject(agent,schedd_name.c_str());

	if (m_isPublishing) {
		string jsName = schedd_name;
		jsName.insert(0,"jobs@");
		jobServer = new JobServerObject(agent, scheduler, jsName.c_str());
	}

	dirtyJobs = new DirtyJobsType();

	isHandlerRegistered = false;

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate Mgmt socket");
	}
	if (!sock->assign(agent->getSignalFd())) {
		EXCEPT("Failed to bind Mgmt socket");
	}
	int index;
	if (-1 == (index =
			   daemonCore->Register_Socket((Stream *) sock,
										   "Mgmt Method Socket",
										   (SocketHandlercpp) ( &MgmtScheddPlugin::HandleMgmtSocket ),
										   "Handler for Mgmt Methods.",
										   this))) {
		EXCEPT("Failed to register Mgmt socket");
	}

	m_initialized = false;
}


void
MgmtScheddPlugin::initialize()
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
MgmtScheddPlugin::shutdown()
{
		// Since this plugin is registered with multiple
		// PluginManagers (eg, shadow) it may be shutdown
		// more than once, and we don't want that
	static bool skip = false;
	if (skip) return; skip = true;

	if (!param_boolean("QMF_DELETE_ON_SHUTDOWN", true)) {
		return;
	}

	dprintf(D_FULLDEBUG, "MgmtScheddPlugin: shutting down...\n");

	// remove from agent
	if (m_isPublishing && jobServer) {
		delete jobServer;
		jobServer = NULL;
	}
	if (scheduler) {
		delete scheduler;
		scheduler = NULL;
	}
	if (singleton) {
		delete singleton;
		singleton = NULL;
	}
}


void
MgmtScheddPlugin::update(int cmd, const ClassAd *ad)
{
	MyString hashKey;
	SubmitterObject *submitterObject;

	switch (cmd) {
	case UPDATE_SCHEDD_AD:
		dprintf(D_FULLDEBUG, "Received UPDATE_SCHEDD_AD\n");
		scheduler->update(*ad);
		if (m_isPublishing) {
			jobServer->update(*ad);
		}

		break;
	case UPDATE_SUBMITTOR_AD:
		dprintf(D_FULLDEBUG, "Received UPDATE_SUBMITTOR_AD\n");
		if (!GetSubmitterNameFromAd((ClassAd &)*ad, hashKey)) {
			dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");

			return;
		}

		if (!GetSubmitter(hashKey, submitterObject)) {
			EXCEPT("Failed to get submitter: %s", hashKey.Value());
		}

		submitterObject->update(*ad);

		break;
	default:
		dprintf(D_FULLDEBUG, "Unsupported command: %s\n",
				getCollectorCommandString(cmd));
	}
}


void
MgmtScheddPlugin::archive(const ClassAd */*ad*/) { };


void
MgmtScheddPlugin::newClassAd(const char */*key*/) { };


void
MgmtScheddPlugin::setAttribute(const char *key,
							   const char *name,
							   const char *value)
{
	if (!m_initialized) return;

//	dprintf(D_FULLDEBUG, "setAttribute: %s[%s] = %s\n", key, name, value);

	markDirty(key, name, value);
}


void
MgmtScheddPlugin::destroyClassAd(const char *_key)
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
MgmtScheddPlugin::deleteAttribute(const char */*key*/,
								  const char */*name*/) { }


int
MgmtScheddPlugin::HandleMgmtSocket(/*Service *,*/ Stream *)
{
	singleton->getInstance()->pollCallbacks();

	return KEEP_STREAM;
}


void
MgmtScheddPlugin::processDirtyJobs()
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
MgmtScheddPlugin::processJob(const char *key,
							 const char *name,
							 int value)
{
	PROC_ID id;
	ClassAd *jobAd;
	SubmissionObject *submission;

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
	if (NULL == (jobAd = GetJobAd(id.cluster, id.proc, false))) {
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
    char* val = NULL;
    if ( (GetAttributeString(id.cluster, id.proc,ATTR_JOB_SUBMISSION,submissionName) < 0) 
        && (GetAttributeExprNew(id.cluster, id.proc, ATTR_JOB_SUBMISSION,&val) < 0) ) {
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
    if (val) free (val);

		// The ATTR_JOB_SUBMISSION has been set, if we aren't
		// publishing submissions ourselve we can stop here.
	if (!m_isPublishing) {
		return true;
	}

	SubmissionMapType::iterator pair =
		m_submissions.find(string(submissionName.Value()));
	if (m_submissions.end() == pair) {
			// If it is not present create a new one

		MyString submitterName;
		if (GetAttributeString(id.cluster, id.proc,
							   ATTR_OWNER, submitterName) < 0) {
			//EXCEPT("Failed to get submitter name from job ad");
			//TODO: except hoses inside txn?
			dprintf(D_ALWAYS,"Failed to get submitter name from job ad\n");
			return false;
		}

		dprintf(D_FULLDEBUG, "Creating new Submission, %s\n", submissionName.Value());
		submission = new SubmissionObject(singleton->getInstance(),
							jobServer,
							submissionName.Value(),
							submitterName.Value());
		if (!submission) {
			dprintf(D_ALWAYS, "ERROR: Failed to create submission - skipping\n");
			return false;
		}

		m_submissions[string(submissionName.Value())] = submission;
	} else {
		submission = pair->second;
	}

    // Call some add() function on submission, which adds SubmissionId, help avoid letting the secret out about the SubmissionId

	submission->updateStatus(id, name, value);

	submission->updateQdate(id);

	// TODO:
	// after the update, see where we are
	// with the completed+removed jobs v. proc count
	dprintf(D_FULLDEBUG, "proc count for %s is %d\n", submissionName.Value(),submission->active_procs.size());
	if (submission->active_procs.empty()) {
	    // clean this one up, since we have already
	    // updated its job status
	    m_submissions.erase(string(submissionName.Value()));
	    delete submission;
	}

}


void
MgmtScheddPlugin::markDirty(const char *key,
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
								   &MgmtScheddPlugin::processDirtyJobs,
								   "Process Dirty",
								   this);
		isHandlerRegistered = true;
	}
}


bool
MgmtScheddPlugin::GetSubmitter(MyString &name, SubmitterObject *&submitter)
{
	if (submitterAds->lookup(name, submitter)) {
		submitter = new SubmitterObject(singleton->getInstance(),
										scheduler,
										name.Value());
		if (!submitter) {
			dprintf(D_ALWAYS, "ERROR: Could not allocate Submitter\n");

			return false;
		}

			// Ignore old value, if it existed (returned)
		submitterAds->insert(name, submitter);
	}

	return true;
}
