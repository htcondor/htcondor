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

#ifndef _EXECUTE_DIR_MONITOR_H
#define _EXECUTE_DIR_MONITOR_H

#include "condor_sys_types.h"
#include <string>
#include <tuple>

/*
*	Base Abstract Class to monitor the jobs working directory:
*	    - Disk Usage
*/
class ExecDirMonitor {
public:
	ExecDirMonitor() = default;
	virtual ~ExecDirMonitor() = default;
	virtual std::tuple<filesize_t, size_t> GetDiskUsage() = 0;
	bool IsValid() const { return valid; }
protected:
	bool valid{true};
};

/*
*	Derived class that manually iterates over the jobs working directory
*	for calculating disk usage.
*/
class ManualExecDirMonitor : public ExecDirMonitor {
public:
	ManualExecDirMonitor() = delete;
	ManualExecDirMonitor(const char* dir) : workingDir(dir) {
		valid = workingDir ? true : false;
	};
	~ManualExecDirMonitor() = default;
	virtual std::tuple<filesize_t, size_t> GetDiskUsage();

private:
	const char* workingDir{nullptr};
};

/*
*	Derived class that gets jobs working directory disk usage
*	directly via statvfs()
*/
class StatExecDirMonitor : public ExecDirMonitor {
public:
	StatExecDirMonitor() = default;
	~StatExecDirMonitor() = default;
	virtual std::tuple<filesize_t, size_t> GetDiskUsage() { return {execSize, numFiles}; };

	filesize_t execSize{0};
	size_t numFiles{0};
};

#endif /* _EXECUTE_DIR_MONITOR_H */
