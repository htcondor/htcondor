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

#ifndef _MGMTSCHEDDPLUGIN_H
#define _MGMTSCHEDDPLUGIN_H

#include "SchedulerObject.h"
#include "SubmitterObject.h"
#include "SubmissionObject.h"
#include "JobServerObject.h"

#include "SubmitterUtils.h"

#include "condor_qmgr.h"

#include "../condor_schedd.V6/ScheddPlugin.h"
#include "ClassAdLogPlugin.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include <list>
#include <string>

#include "PROC_ID_comparator.h"

using namespace qpid::management;
using namespace qpid::types;
using namespace qmf::com::redhat;
using namespace qmf::com::redhat::grid;
using namespace com::redhat::grid;

namespace com {
namespace redhat {
namespace grid {

// BIG NOTE: If Service is not first in the parent list the
//           processDirtyJobs handler will segfault when using the
//           dirtyJobs list
class MgmtScheddPlugin : public Service, ClassAdLogPlugin, ScheddPlugin
{

public:

	typedef std::map<std::string, SubmissionObject *> SubmissionMapType;

	void earlyInitialize();

	void initialize();

	void shutdown();

	void update(int cmd, const ClassAd *ad);

	void archive(const ClassAd *ad);

	void newClassAd(const char */*key*/);

	void setAttribute(const char *key,
					  const char *name,
					  const char *value);

	void destroyClassAd(const char *key);

	void deleteAttribute(const char *key,
						 const char *name);


private:

		// ManagementAgent::Singleton cleans up the ManagementAgent
		// instance if there are no ManagementAgent::Singleton's in
		// scope!
	ManagementAgent::Singleton *singleton;

	SubmissionMapType m_submissions;
	typedef std::pair<std::string, int> DirtyJobStatus;
	typedef std::pair<std::string, DirtyJobStatus> DirtyJobEntry;
	typedef std::list<DirtyJobEntry> DirtyJobsType;
	DirtyJobsType *dirtyJobs;
	SchedulerObject *scheduler;
	JobServerObject *jobServer;
	typedef HashTable<MyString, SubmitterObject *> SubmitterHashTable;
	SubmitterHashTable *submitterAds;

	bool isHandlerRegistered;

	bool m_initialized;

	bool m_isPublishing;

	int HandleMgmtSocket(/*Service *,*/ Stream *);

	void processDirtyJobs();

	bool processJob(const char *key, const char *name, int value);

	void markDirty(const char *key, const char *name, const char *value);

	bool GetSubmitter(MyString &name, SubmitterObject *&submitter);

};

}}} /* com::redhat::grid */

#endif /* _MGMTSCHEDDPLUGIN_H */
