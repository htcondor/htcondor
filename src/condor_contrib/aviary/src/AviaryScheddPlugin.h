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

#ifndef _AVIARYSCHEDDPLUGIN_H
#define _AVIARYSCHEDDPLUGIN_H

// c++ includes
#include <list>
#include <string>

// condor includes
#include "condor_qmgr.h"
#include "../condor_schedd.V6/ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// local includes
#include "SchedulerObject.h"
#include "PROC_ID_comparator.h"


namespace aviary {
namespace job {

// BIG NOTE: If Service is not first in the parent list the
//           processDirtyJobs handler will segfault when using the
//           dirtyJobs list
class AviaryScheddPlugin : public Service, ClassAdLogPlugin, ScheddPlugin
{

public:

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

	typedef std::pair<std::string, int> DirtyJobStatus;
	typedef std::pair<std::string, DirtyJobStatus> DirtyJobEntry;
	typedef std::list<DirtyJobEntry> DirtyJobsType;
	DirtyJobsType *dirtyJobs;


	bool isHandlerRegistered;

	bool m_initialized;

	bool m_isPublishing;

	int HandleTransportSocket(Stream *);

	void processDirtyJobs();

	bool processJob(const char *key, const char *name, int value);

	void markDirty(const char *key, const char *name, const char *value);

};

}} /* aviary::job */

#endif /* _AVIARYSCHEDDPLUGIN_H */
