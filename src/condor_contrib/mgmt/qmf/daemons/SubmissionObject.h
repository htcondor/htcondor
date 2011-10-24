/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _SUBMISSIONOBJECT_H
#define _SUBMISSIONOBJECT_H

#include "condor_common.h"
#include "condor_debug.h"

#include "Job.h"

#include <string>
#include <map>
#include <set>

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>
#include <qpid/types/Variant.h>

#include "Submission.h"
#include "JobServerObject.h"

using namespace qpid::management;
using namespace qpid::types;

using std::string;
using std::map;
using std::set;

struct cmpjob {
	bool operator()(const Job *a, const Job *b) const {
		return strcmp(a->GetKey(), b->GetKey()) < 0;
	}
};

class SubmissionObject : public Manageable
{
public:
    friend class Job;
	typedef set<const Job *, cmpjob> JobSet;

	SubmissionObject(ManagementAgent *agent,
			com::redhat::grid::JobServerObject* job_server,
			const char *name,
			const char *owner);
	~SubmissionObject();

	const JobSet & GetIdle();
	const JobSet & GetRunning();
	const JobSet & GetRemoved();
	const JobSet & GetCompleted();
	const JobSet & GetHeld();

	ManagementObject *GetManagementObject(void) const;

	void SetOwner(const char *owner);
	void UpdateQdate(int q_date);

    status_t ManagementMethod(uint32_t methodId,
                              Args &args,
                              string &text);

	bool AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId);

protected:
	void Increment(const Job *job);
	void Decrement(const Job *job);

private:
	JobSet m_idle;
	JobSet m_running;
	JobSet m_removed;
	JobSet m_completed;
	JobSet m_held;

	bool ownerSet;

	qmf::com::redhat::grid::Submission *mgmtObject;

    status_t GetJobSummaries(Variant::List &jobs,
                     string &text);
};

#endif /* _SUBMISSIONOBJECT_H */
