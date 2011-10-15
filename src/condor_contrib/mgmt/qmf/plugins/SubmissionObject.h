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

#ifndef _SUBMISSIONOBJECT_H
#define _SUBMISSIONOBJECT_H

#include <string>
#include <set>

#include <qpid/management/Manageable.h>
#include <qpid/management/ManagementObject.h>
#include <qpid/agent/ManagementAgent.h>

#include <qpid/types/Variant.h>

#include "Submission.h"

#include "JobServerObject.h"
#include "SubmitterObject.h"

#include "condor_classad.h"

#include "proc.h"

namespace com {
namespace redhat {
namespace grid {

using namespace qpid::management;
using namespace std;

struct cmpprocid {
        bool operator()(PROC_ID a, PROC_ID b) const {
                return a.proc < b.proc;
        }
};

class SubmissionObject : public Manageable
{

public:

	SubmissionObject(ManagementAgent *agent,
					 JobServerObject *_scheduler,
					 const char *_name,
					 const char *_owner);
	~SubmissionObject();

	ManagementObject *GetManagementObject(void) const;

	void updateStatus(const PROC_ID &id, const char *attr, int value);
	void updateQdate(const PROC_ID &id);

	status_t ManagementMethod(uint32_t methodId,
							  Args &args,
							  string &text);

	bool AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId);

    typedef set<PROC_ID, cmpprocid> ProcIDSet;
    ProcIDSet active_procs;

private:

	qmf::com::redhat::grid::Submission *mgmtObject;

	string m_name;

	status_t GetJobSummaries(Variant::List &jobs,
                     string &text);
};

}}} /* com::redhat::grid */

#endif /* _SUBMISSIONOBJECT_H */
