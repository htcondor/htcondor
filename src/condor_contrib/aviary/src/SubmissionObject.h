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

// c++ includes
#include <string>
#include <map>
#include <set>

// coondor includes
#include "condor_common.h"

// local includes
#include "Job.h"
#include "JobServerObject.h"

using std::string;
using std::map;
using std::set;

namespace aviary {
namespace query {
		
struct cmpjob {
	bool operator()(const Job *a, const Job *b) const {
		return strcmp(a->getKey(), b->getKey()) < 0;
	}
};

class SubmissionObject
{
public:
    friend class Job;
	typedef set<const Job *, cmpjob> JobSet;

	SubmissionObject( const char *name, const char *owner);
	~SubmissionObject();

	const JobSet & getIdle();
	const JobSet & getRunning();
	const JobSet & getRemoved();
	const JobSet & getCompleted();
	const JobSet & getHeld();

	void setOwner(const char *owner);
	const char* getOwner() { return m_owner.c_str(); }
	const char* getName() { return m_name.c_str(); }
	void getJobSummaries(JobSummaryPairCollection& _jobs);

	int getOldest();
	void setOldest(int qdate);

protected:
	void increment(const Job *job);
	void decrement(const Job *job);

private:
	JobSet m_idle;
	JobSet m_running;
	JobSet m_removed;
	JobSet m_completed;
	JobSet m_held;

	bool ownerSet;
	int  m_oldest_qdate;

	string m_name;
	string m_owner;
	Codec* m_codec;

};

}}

#endif /* _SUBMISSIONOBJECT_H */
