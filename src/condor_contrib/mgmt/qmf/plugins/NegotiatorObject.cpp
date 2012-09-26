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

#ifndef READ_ONLY_NEGOTIATOR_OBJECT
// Global from condor_negotiator, the matchMaker, holds the Accountant
extern Matchmaker matchMaker;

extern int main_shutdown_graceful();

void dc_reconfig();
#endif


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

	DOUBLE2(LastNegotiationCycleMatchRate0,MatchRate)
	DOUBLE2(LastNegotiationCycleMatchRateSustained0,MatchRateSustained)
	INTEGER2(LastNegotiationCycleMatches0,Matches)
	INTEGER2(LastNegotiationCycleDuration0,Duration)
	INTEGER2(LastNegotiationCyclePhase1Duration0,Phase1Duration)
	INTEGER2(LastNegotiationCyclePhase2Duration0,Phase2Duration)
	INTEGER2(LastNegotiationCyclePhase3Duration0,Phase3Duration)
	INTEGER2(LastNegotiationCyclePhase4Duration0,Phase4Duration)
	INTEGER2(LastNegotiationCycleSlotShareIter0,SlotShareIter)
	INTEGER2(LastNegotiationCycleNumSchedulers0,NumSchedulers)
	INTEGER2(LastNegotiationCycleActiveSubmitterCount0,ActiveSubmitterCount)
	STRING2(LastNegotiationCycleSubmittersFailed0,SubmittersFailed)
	STRING2(LastNegotiationCycleSubmittersOutOfTime0,SubmittersOutOfTime)
	STRING2(LastNegotiationCycleSubmittersShareLimit0,SubmittersShareLimit)
	INTEGER2(LastNegotiationCycleNumIdleJobs0,NumIdleJobs)
	INTEGER2(LastNegotiationCycleNumJobsConsidered0,NumJobsConsidered)
	INTEGER2(LastNegotiationCycleRejections0,Rejections)
	INTEGER2(LastNegotiationCycleTotalSlots0,TotalSlots)
	INTEGER2(LastNegotiationCycleCandidateSlots0,CandidateSlots)
	INTEGER2(LastNegotiationCycleTrimmedSlots0,TrimmedSlots)
	TIME_INTEGER2(LastNegotiationCycleTime0,Time)
	TIME_INTEGER2(LastNegotiationCycleEnd0,End)
	INTEGER2(LastNegotiationCyclePeriod0,Period)
}


#ifndef READ_ONLY_NEGOTIATOR_OBJECT
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

	limitAttrs.Delete(ATTR_CURRENT_TIME); // compat_classad insists on adding this
	limitAttrs.ResetExpr();
    const char* attr_name;
    while (limitAttrs.NextExpr(attr_name,expr)) {
		Variant::Map limit;
		std::string name = attr_name;

        // Get right to the limit's name
        // len("ConcurrencyLimitX") = 17
        // X can be any single char separator
		name = name.substr(17, name.length());

		limit["CURRENT"] = matchMaker.getAccountant().GetLimit(name);
		limit["MAX"] = matchMaker.getAccountant().GetLimitMax(name);

		limits[strdup(name.c_str())] = limit;

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

	if (!IsValidParamName(name,text)) {
		return STATUS_USER + 2;
	}

	MyString config;

	name += "_LIMIT";

	config.formatstr("%s=%f", name.c_str(), max);

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

	if (!IsValidParamName(name,text)) {
		return STATUS_USER + 2;
	}

	MyString config;

	config.formatstr("%s=%s", name.c_str(), value.c_str());

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
	dc_reconfig();

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
#endif


Manageable::status_t
NegotiatorObject::ManagementMethod(uint32_t methodId,
								   Args &args,
								   std::string &text)
{
#ifndef READ_ONLY_NEGOTIATOR_OBJECT
	switch (methodId) {
	case qmf::com::redhat::grid::Negotiator::METHOD_ECHO:
		if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;

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
#endif

	return STATUS_NOT_IMPLEMENTED;
}

bool
NegotiatorObject::AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId) {
	dprintf(D_FULLDEBUG, "AuthorizeMethod: checking '%s'\n", userId.c_str());
	if (0 == userId.compare("cumin")) {
		return true;
	}
	return false;
}
