/*
 * Copyright 2000-2011 Red Hat, Inc.
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

#ifndef _COLLECTOROBJECT_H
#define _COLLECTOROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include "Collector.h"


namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;
using namespace qmf::com::redhat::grid;
using namespace std;

class CollectorObject : public Manageable
{
public:

	CollectorObject(ManagementAgent *agent, const char* _name);

	~CollectorObject();

	ManagementObject *GetManagementObject(void) const;

	void update(const ClassAd &ad);
/* disable for now
	status_t ManagementMethod(uint32_t methodId,
							  Args &args,
							  string &text);
*/

private:

	Collector *mgmtObject;

	void advertise();
};

}}} /* com::redhat::grid */

#endif /* _COLLECTOROBJECT_H */
