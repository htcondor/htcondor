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

#ifndef _MASTEROBJECT_H
#define _MASTEROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/types/Variant.h>

// need this for QMF integration
#ifdef WIN32
#include <Rpc.h>
#ifdef uuid_t   /*  Done in rpcdce.h */
#  undef uuid_t
#endif
#endif

#include "Master.h"

#include "condor_classad.h"


namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;

class MasterObject : public Manageable
{
public:

	MasterObject(ManagementAgent *agent, const char* _name);

	~MasterObject();

	ManagementObject *GetManagementObject(void) const;

	void update(const ClassAd &ad);

	status_t ManagementMethod(uint32_t methodId, Args &args, std::string &text);

	status_t Stop(std::string subsys, std::string &text);

	status_t Start(std::string subsys, std::string &text);

private:

	qmf::com::redhat::grid::Master *mgmtObject;

};

}}} /* com::redhat::grid */

#endif /* _MASTEROBJECT_H */
