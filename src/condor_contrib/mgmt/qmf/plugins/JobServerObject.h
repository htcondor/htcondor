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

#ifndef _JOBSERVEROBJECT_H
#define _JOBSERVEROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>
#include <qpid/types/Variant.h>

#include "JobServer.h"
#include "SchedulerObject.h"

#include "condor_classad.h"

using namespace qpid::management;
using namespace qpid::types;
using namespace qmf::com::redhat::grid;
using namespace std;

namespace com {
namespace redhat {
namespace grid {

class JobServerObject : public Manageable
{

public:

	JobServerObject(ManagementAgent *agent,
				SchedulerObject *_scheduler,
				const char* _name);

	~JobServerObject();

	ManagementObject *GetManagementObject(void) const;

	void update(const ClassAd &ad);

	status_t ManagementMethod(uint32_t methodId,
							  Args &args,
							  string &text);

	bool AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId);

private:

	JobServer *mgmtObject;

	status_t GetJobAd(string id, Variant::Map &_map, string &text);
	status_t FetchJobData(string id, string &file, int32_t start, int32_t end,
				string &data, string &text);

};

}}} /* com::redhat::grid */

#endif /* _JOBSERVEROBJECT_H */
