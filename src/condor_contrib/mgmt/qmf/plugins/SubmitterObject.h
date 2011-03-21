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

#ifndef _SUBMITTEROBJECT_H
#define _SUBMITTEROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "Submitter.h"

#include "SchedulerObject.h"


namespace com {
namespace redhat {
namespace grid {

using namespace std;

class SubmitterObject : public qpid::management::Manageable
{

public:

	SubmitterObject(qpid::management::ManagementAgent *agent,
					SchedulerObject *_scheduler,
					const char *name);

	~SubmitterObject();


	void update(const ClassAd &ad);

	qpid::management::ManagementObject *GetManagementObject(void) const;

	status_t ManagementMethod(uint32_t methodId,
							  Args &args,
							  string &text);


private:

	qmf::com::redhat::grid::Submitter *mgmtObject;
};

}}} /* com::redhat::grid */

#endif /* _SUBMITTEROBJECT_H */
