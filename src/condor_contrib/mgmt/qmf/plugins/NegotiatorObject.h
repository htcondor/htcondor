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

#ifndef _NEGOTIATOROBJECT_H
#define _NEGOTIATOROBJECT_H

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/types/Variant.h>

#include "Negotiator.h"

#include "condor_classad.h"


namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;

class NegotiatorObject : public Manageable
{

public:

	NegotiatorObject(ManagementAgent *agent, const char* _name);

	~NegotiatorObject();

	ManagementObject *GetManagementObject(void) const;

	void update(const ClassAd &ad);

	status_t ManagementMethod(uint32_t methodId, Args &args, std::string &text);

	bool AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId);

		
private:

	qmf::com::redhat::grid::Negotiator *mgmtObject;

#ifndef READ_ONLY_NEGOTIATOR_OBJECT
	status_t GetLimits(qpid::types::Variant::Map &limits, std::string &text);

	status_t SetLimit(std::string &name, double max, std::string &text);

	status_t GetStats(std::string &name, qpid::types::Variant::Map &stats, std::string &text);

	status_t GetRawConfig(std::string &name, std::string &value, std::string &text);

	status_t SetRawConfig(std::string &name, std::string &value, std::string &text);

	status_t Reconfig(std::string &text);

	status_t Stop(std::string &text);

	status_t SetPriority(std::string &name, double &priority, std::string &text);

	status_t SetPriorityFactor(std::string &name, double &priority, std::string &text);

	status_t SetUsage(std::string &name, double &usage, std::string &text);

	bool CanModifyRuntime(std::string &text);
#endif
};

}}} /* com::redhat::grid */

#endif /* _NEGOTIATOROBJECT_H */
