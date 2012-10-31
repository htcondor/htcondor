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

#ifndef _SCHEDULEROBJECT_H
#define _SCHEDULEROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>
#include <qpid/types/Variant.h>

#include "Scheduler.h"

#include "condor_classad.h"


namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;
using namespace qpid::types;

class SchedulerObject : public qpid::management::Manageable
{

public:

	SchedulerObject(qpid::management::ManagementAgent *agent, const char* _name);

	~SchedulerObject();


	qpid::management::ManagementObject *GetManagementObject(void) const;

	void update(const ClassAd &ad);


	status_t ManagementMethod(uint32_t methodId,
							  Args &args,
							  std::string &text);

	bool AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId);

private:

	qmf::com::redhat::grid::Scheduler *mgmtObject;

#ifndef READ_ONLY_SCHEDULER_OBJECT
	status_t Submit(Variant::Map &ad, std::string &id, std::string &text);

	status_t GetJobs(std::string submission,
					 Variant::Map &jobs,
					 std::string &text);

	status_t SetAttribute(std::string id,
						  std::string name,
						  std::string value,
						  std::string &text);

	status_t Hold(std::string id, std::string &reason, std::string &text);

	status_t Release(std::string id, std::string &reason, std::string &text);

	status_t Remove(std::string id, std::string &reason, std::string &text);

	status_t Suspend(std::string id, std::string &reason, std::string &text);

	status_t Continue(std::string id, std::string &reason, std::string &text);
#endif

    void useNewStats(const ClassAd &ad);
    void useOldStats(const ClassAd &ad);
    bool m_new_stats;

};


}}} /* com::redhat::grid */

#endif /* _SCHEDULEROBJECT_H */
