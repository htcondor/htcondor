/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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


#ifndef DAGMAN_SUBMIT_H
#define DAGMAN_SUBMIT_H

#include "condor_id.h"

enum class SubmitResult {
	SUCCESS = 0,     // Successfully submitted jobs
	FAILURE = 1,     // Hard failure that will never work
	RETRY   = 2,     // Transient failure that should be retried
};

struct CustomVar {
	CustomVar(std::string k, std::string v, bool a) : key(k), value(v), append(a) {};
	std::string key{};
	std::string value{};
	bool append{false};
};

class DagSubmit {
public:
	DagSubmit() = delete;
	DagSubmit(const Dagman& dagman) : dm(dagman) {}
	virtual ~DagSubmit() = default;
	virtual SubmitResult Submit(Node& node, CondorID& condorID, std::string& err, const std::string& log_file);
	virtual bool Reschedule();

	bool PreSkipSubmit(Node& node, const std::string& log_file);
	void SetFakeId(const int id);

protected:
	virtual SubmitResult SubmitInternal(Node& node, CondorID& condorID, std::string& err) = 0;
	SubmitResult FakeSubmit(Node& node, CondorID& condorID, const std::string& log_file);

	int GetFakeId();
	std::string GetMask();

	std::vector<CustomVar> InitVars(const Node& node);
	virtual bool DeferVar(std::vector<CustomVar>& deferred, const CustomVar& var, const std::set<std::string>& key_filter);

	const Dagman& dm;
	std::string m_eventmask{};
	int m_subprocid{0};
};

class DirectSubmit : public DagSubmit {
public:
	DirectSubmit() = delete;
	DirectSubmit(const Dagman& dagman) : DagSubmit(dagman) {}

	virtual bool Reschedule();

private:
	virtual SubmitResult SubmitInternal(Node& node, CondorID& condorID, std::string& err);
};

class ShellSubmit : public DagSubmit {
public:
	ShellSubmit() = delete;
	ShellSubmit(const Dagman& dagman) : DagSubmit(dagman) {}

private:
	virtual SubmitResult SubmitInternal(Node& node, CondorID& condorID, std::string& err);
};

#endif /* #ifndef DAGMAN_SUBMIT_H */
