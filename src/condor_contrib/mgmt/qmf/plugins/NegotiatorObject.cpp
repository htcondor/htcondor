/*
 * Copyright 2008 Red Hat, Inc.
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

#include "NegotiatorObject.h"

#include "ArgsNegotiatorGetLimits.h"
#include "ArgsNegotiatorSetLimit.h"
#include "ArgsNegotiatorGetRawConfig.h"
#include "ArgsNegotiatorSetRawConfig.h"
#include "ArgsNegotiatorGetStats.h"
#include "ArgsNegotiatorSetPriority.h"
#include "ArgsNegotiatorSetPriorityFactor.h"
#include "ArgsNegotiatorSetUsage.h"

#include <qpid/agent/ManagementAgent.h>

#include "condor_config.h"

#include "../condor_negotiator.V6/matchmaker.h"

#include "PoolUtils.h"
#include "Utils.h"

#include "MgmtConversionMacros.h"


// Global from condor_negotiator, the matchMaker, holds the Accountant
extern Matchmaker matchMaker;

extern int main_shutdown_graceful();

void dc_reconfig(bool is_full);


using namespace com::redhat::grid;

using namespace qpid::management;
using namespace qpid::types;
using namespace qmf::com::redhat::grid;

NegotiatorObject::NegotiatorObject(ManagementAgent *agent,
					const char* _name)
{
	mgmtObject = new qmf::com::redhat::grid::Negotiator(agent, this);

	// By default the negotiator will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);

}


NegotiatorObject::~NegotiatorObject()
{
	if (mgmtObject) {
		mgmtObject->resourceDestroy();
	}
}


ManagementObject *
NegotiatorObject::GetManagementObject(void) const
{
	return mgmtObject;
}


void
NegotiatorObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

		// Logic to set values on mgmtObject
//	printf("NegotiatorObject::update called...\n");

	mgmtObject->set_Pool(GetPoolName());

		// There is no CondorPlatform or CondorVersion ATTR_*
		// definition
	STRING(CondorPlatform);
	STRING(CondorVersion);

	TIME_INTEGER(DaemonStartTime);
	STRING(Machine);
	STRING(MyAddress);
	STRING(Name);
		//STRING(PublicNetworkIpAddr);

	mgmtObject->set_System(mgmtObject->get_Machine());

		// The MonitorSelf* attributes do not have ATTR_*
		// definitions
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);
	TIME_INTEGER(MonitorSelfTime);
}


Manageable::status_t
NegotiatorObject::GetLimits(Variant::Map &limits, std::string &/*text*/)
{
	AttrList limitAttrs;
	ExprTree *expr;

		// Ask the Accountant to populate an AttrList with
		// information about limits. The list will look like:
		//   ConcurrencyLimit.x = <x's current usage>
		//   ConcurrencyLimit.y = <y's current usage>
	matchMaker.getAccountant().ReportLimits(&limitAttrs);

	limitAttrs.ResetExpr();
	while (NULL != (expr = limitAttrs.NextExpr())) {
		Variant::Map limit;
		MyString name;
		name = ((Variable *) expr->LArg())->Name();

			// Get right to the limit's name
			// len("ConcurrencyLimitX") = 17
			// X can be any single char separator
		name = name.Substr(17, name.Length());

		limit["CURRENT"] = matchMaker.getAccountant().GetLimit(name);
		limit["MAX"] = matchMaker.getAccountant().GetLimitMax(name);

		limits[name.StrDup()] = limit;

	}

	return STATUS_OK;
}

bool NegotiatorObject::CanModifyRuntime(std::string &text) {
	if (!param_boolean( "ENABLE_RUNTIME_CONFIG", false )) {
		text = "Runtime configuration changes disabled";
		return false;
	}
	return true;
}

Manageable::status_t
NegotiatorObject::SetLimit(std::string &name, double max, std::string &text)
{

	if (!CanModifyRuntime(text)) {
		return STATUS_USER + 1;
	}

	if (!IsValidGroupUserName(name,text)) {
		return STATUS_USER + 2;
	}

	MyString config;

	name += "_LIMIT";

	config.sprintf("%s=%f", name.c_str(), max);

	if (-1 == set_runtime_config(strdup(name.c_str()),
								 config.StrDup())) {
		text = "Failed to set " + name;
		return STATUS_USER + 3;
	}

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::GetRawConfig(std::string &name, std::string &value, std::string &text)
{
	char *val = NULL;

	if (NULL == (val = param(name.c_str()))) {
		text = "Unknown config: " + name;
		return STATUS_USER + 1;
	}

	value = val;

	free(val); val = NULL;

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::SetRawConfig(std::string &name, std::string &value, std::string &text)
{
	if (!CanModifyRuntime(text)) {
		return STATUS_USER + 1;
	}

	if (!IsValidAttributeName(name,text)) {
		return STATUS_USER + 2;
	}

	MyString config;

	config.sprintf("%s=%s", name.c_str(), value.c_str());

	if (-1 == set_runtime_config(strdup(name.c_str()),
								 strdup(config.Value()))) {
		text = "Failed to set: " + name + " = " + value;
		return STATUS_USER + 2;
	}

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::Reconfig(std::string &/*text*/)
{
	dc_reconfig(false);

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::GetStats(std::string &name, qpid::types::Variant::Map &stats, std::string &text)
{
	ClassAd *ad = NULL;
		// NOTE: "Customer." is really Accountant::CustomerRecord
	MyString customer(("Customer." + name).c_str());

	if (NULL == (ad = matchMaker.getAccountant().GetClassAd(customer))) {
		text = "Unknown: " + name;
		return STATUS_USER + 1;
	}

	if (!PopulateVariantMapFromAd(*ad, stats)) {
		text = "Failed processing stats ad for " + name;
		return STATUS_USER + 2;
	}

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::SetPriority(std::string &name, double &priority, std::string &text)
{
	if (!IsValidGroupUserName(name,text)) {
		return STATUS_USER + 1;
	}

	matchMaker.getAccountant().SetPriority(MyString(name.c_str()), (float) priority);

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::SetPriorityFactor(std::string &name, double &priority, std::string &text)
{
	if (!IsValidGroupUserName(name,text)) {
		return STATUS_USER + 1;
	}

	matchMaker.getAccountant().SetPriorityFactor(MyString(name.c_str()), (float) priority);

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::SetUsage(std::string &name, double &usage, std::string &text)
{
	if (!IsValidGroupUserName(name,text)) {
		return STATUS_USER + 1;
	}

	matchMaker.getAccountant().SetAccumUsage(MyString(name.c_str()), (float) usage);

	return STATUS_OK;
}


Manageable::status_t
NegotiatorObject::ManagementMethod(uint32_t methodId,
								   Args &args,
								   std::string &text)
{
	switch (methodId) {
	case qmf::com::redhat::grid::Negotiator::METHOD_ECHO:
		return STATUS_OK;
	case Negotiator::METHOD_GETLIMITS:
		return GetLimits(((ArgsNegotiatorGetLimits &) args).o_Limits, text);
	case Negotiator::METHOD_SETLIMIT:
		return SetLimit(((ArgsNegotiatorSetLimit &) args).i_Name,
						((ArgsNegotiatorSetLimit &) args).i_Max,
						text);
	case Negotiator::METHOD_RECONFIG:
		return Reconfig(text);
	case Negotiator::METHOD_GETRAWCONFIG:
		return GetRawConfig(((ArgsNegotiatorGetRawConfig &) args).i_Name,
							((ArgsNegotiatorGetRawConfig &) args).o_Value,
							text);
	case Negotiator::METHOD_SETRAWCONFIG:
		return SetRawConfig(((ArgsNegotiatorSetRawConfig &) args).i_Name,
							((ArgsNegotiatorSetRawConfig &) args).i_Value,
							text);
	case Negotiator::METHOD_GETSTATS:
		return GetStats(((ArgsNegotiatorGetStats &) args).i_Name,
						((ArgsNegotiatorGetStats &) args).o_Ad,
						text);
	case Negotiator::METHOD_SETPRIORITY:
		return SetPriority(((ArgsNegotiatorSetPriority &) args).i_Name,
						   ((ArgsNegotiatorSetPriority &) args).i_Priority,
						   text);
	case Negotiator::METHOD_SETPRIORITYFACTOR:
		return SetPriorityFactor(((ArgsNegotiatorSetPriorityFactor &) args).i_Name,
								 ((ArgsNegotiatorSetPriorityFactor &) args).i_PriorityFactor,
								 text);
	case Negotiator::METHOD_SETUSAGE:
		return SetUsage(((ArgsNegotiatorSetUsage &) args).i_Name,
						((ArgsNegotiatorSetUsage &) args).i_Usage,
						text);
	}

	return STATUS_NOT_IMPLEMENTED;
}

