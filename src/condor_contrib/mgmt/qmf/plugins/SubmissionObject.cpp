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
#include "condor_config.h"

#include "SubmissionObject.h"
#include "ArgsSubmissionGetJobSummaries.h"

#include "MgmtScheddPlugin.h"

#include "set_user_priv_from_ad.h"
#include "../condor_schedd.V6/scheduler.h"

#ifndef WIN32
	#include "stdint.h"
#endif

#include "JobUtils.h"
#include "Utils.h"

#include "MgmtConversionMacros.h"

#include <sstream>

extern MgmtScheddPlugin *scheddPluginInstance;

using namespace std;
using namespace com::redhat::grid;

using namespace qpid::management;
using namespace qmf::com::redhat::grid;

SubmissionObject::SubmissionObject(ManagementAgent *agent,
					JobServerObject *_jobServer,
					const char *_name,
					const char *_owner): m_name(_name)
{
	mgmtObject = new Submission(agent, this, _jobServer);

	mgmtObject->set_Name(string(_name));
	mgmtObject->set_Owner(string(_owner));

	// By default the submission will be persistent.
	bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
	agent->addObject(mgmtObject, _name, _lifetime);
}


SubmissionObject::~SubmissionObject()
{
	if ( mgmtObject )
	{
		mgmtObject->resourceDestroy();
	}
}


ManagementObject *
SubmissionObject::GetManagementObject(void) const
{
	return mgmtObject;
}


void
SubmissionObject::updateStatus(const PROC_ID &id,
						 const char *attr,
						 int value)
{
	dprintf(D_FULLDEBUG, "Submission[%s]::update(%d.%d, %s, %s)\n",
			mgmtObject->get_Name().c_str(),
            id.cluster, id.proc, attr, getJobStatusString(value));

	// Note: UNEXPANDED means no previous state

	if (strcasecmp(attr, ATTR_LAST_JOB_STATUS) == 0) {
		switch (value) {
		case IDLE:
			mgmtObject->dec_Idle();
			break;
		case RUNNING:
			mgmtObject->dec_Running();
			break;
		case REMOVED:
			mgmtObject->dec_Removed();
			break;
		case COMPLETED:
			mgmtObject->dec_Completed();
			break;
		case HELD:
			mgmtObject->dec_Held();
			break;
		case TRANSFERRING_OUTPUT:
			mgmtObject->dec_TransferringOutput();;
			break;
		case SUSPENDED:
			mgmtObject->dec_Suspended();
			break;
		default:
			dprintf(D_ALWAYS, "error: Unknown %s of %d on %d.%d\n",
					ATTR_LAST_JOB_STATUS, value, id.cluster, id.proc);
			break;
		}
	} else if (strcasecmp(attr, ATTR_JOB_STATUS) == 0) {
		switch (value) {
		case IDLE:
			mgmtObject->inc_Idle();
            active_procs.insert(id);
			break;
		case RUNNING:
			mgmtObject->inc_Running();
            active_procs.insert(id);
			break;
		case REMOVED:
			mgmtObject->inc_Removed();
			// adjust our cleanup set
            active_procs.erase(id);
			break;
		case COMPLETED:
			mgmtObject->inc_Completed();
			// adjust our cleanup set
			active_procs.erase(id);
			break;
		case HELD:
			mgmtObject->inc_Held();
            active_procs.insert(id);
			break;
		case TRANSFERRING_OUTPUT:
			mgmtObject->inc_TransferringOutput();
            active_procs.insert(id);
			break;
		case SUSPENDED:
			mgmtObject->inc_Suspended();
            active_procs.insert(id);
			break;
		default:
			dprintf(D_ALWAYS, "error: Unknown %s of %d on %d.%d\n",
					ATTR_JOB_STATUS, value, id.cluster, id.proc);
			break;
		}
	}

}

// update the oldest job qdate for this submission
void
SubmissionObject::updateQdate(const PROC_ID &id) {
	int q_date, old;
	if (GetAttributeInt(id.cluster, id.proc, ATTR_Q_DATE, &q_date) >= 0) {
		old = mgmtObject->get_QDate();
		if ((q_date < old)) {
			mgmtObject->set_QDate((uint64_t) q_date*1000000000);
		}
	}
}

Manageable::status_t
SubmissionObject::GetJobSummaries ( Variant::List &jobs,
                            std::string &text )
{
	ClassAd *ad = NULL;
	MyString constraint;
	// id, timestamp (which?), command, args, ins, outs, state, message
	const char *ATTRS[] = {ATTR_CLUSTER_ID,
			ATTR_PROC_ID,
			ATTR_GLOBAL_JOB_ID,
			ATTR_Q_DATE,
			ATTR_ENTERED_CURRENT_STATUS,
			ATTR_JOB_STATUS,
			ATTR_JOB_CMD,
			ATTR_JOB_ARGUMENTS1,
			ATTR_JOB_ARGUMENTS2,
			ATTR_RELEASE_REASON,
			ATTR_HOLD_REASON,
			NULL
			};

	constraint.formatstr("%s == \"%s\"",
					   ATTR_JOB_SUBMISSION, this->m_name.c_str());

	dprintf(D_FULLDEBUG,"GetJobSummaries for submission: %s\n",constraint.Value());

	Variant::Map job;
	int init_scan = 1;
	while (NULL != (ad = GetNextJobByConstraint(constraint.Value(), init_scan))) {

		// debug
//		if (IsFulldebug(D_FULLDEBUG)) {
//			ad->dPrint(D_FULLDEBUG|D_NOHEADER);
//		}

		for (int i = 0; NULL != ATTRS[i]; i++) {
			if (!AddAttribute(*ad, ATTRS[i], job)) {
				dprintf(D_FULLDEBUG,"Warning: %s attribute not found for job of %s\n",ATTRS[i],constraint.Value());
			}
		}

		jobs.push_back(job);
		init_scan = 0;

		// debug
//		if (IsFulldebug(D_FULLDEBUG)) {
//			std::ostringstream oss;
//			oss << jobs;
//			dprintf(D_FULLDEBUG|D_NOHEADER, "%s\n",oss.str().c_str());
//		}

		FreeJobAd(ad);
		ad = NULL;
	}

	return STATUS_OK;
}

Manageable::status_t
SubmissionObject::ManagementMethod ( uint32_t methodId,
                                     Args &args,
                                     std::string &text )
{
    switch ( methodId )
    {
	case qmf::com::redhat::grid::Submission::METHOD_ECHO:
		if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;

            return STATUS_OK;
        case qmf::com::redhat::grid::Submission::METHOD_GETJOBSUMMARIES:
            return GetJobSummaries ( ( ( ArgsSubmissionGetJobSummaries & ) args ).o_Jobs,
                             text );
    }

    return STATUS_NOT_IMPLEMENTED;
}

bool
SubmissionObject::AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId) {
	dprintf(D_FULLDEBUG, "AuthorizeMethod: checking '%s'\n", userId.c_str());
	if (0 == userId.compare("cumin")) {
		return true;
	}
	return false;
}
