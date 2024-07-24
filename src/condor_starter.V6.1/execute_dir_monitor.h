/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
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
*	DiskUsage holds the size of the working directory
*	and the number of files in the working directory
*/
struct DiskUsage {
	filesize_t execute_size{0};
	size_t file_count{0};
};

/*
*	Base Abstract Class to monitor the jobs working directory:
*	    - Disk Usage
*/
class ExecDirMonitor {
public:
	/* Rule of five for abstract class*/
	ExecDirMonitor() = default;
	ExecDirMonitor(const ExecDirMonitor&) = default;
	ExecDirMonitor(ExecDirMonitor&&) = default;
	ExecDirMonitor& operator=(const ExecDirMonitor&) = default;
	ExecDirMonitor& operator=(ExecDirMonitor&&) = default;
	virtual ~ExecDirMonitor() = default;

	virtual DiskUsage GetDiskUsage() = 0;
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
	ManualExecDirMonitor(const std::string& dir) : workingDir(dir) {
		valid = workingDir.empty() ? false : true;
	};
	virtual DiskUsage GetDiskUsage();

private:
	std::string workingDir{};
};

/*
*	Derived class that gets jobs working directory disk usage
*	directly via statvfs(). Note: This counts symlinks
*/
class StatExecDirMonitor : public ExecDirMonitor {
public:
	StatExecDirMonitor() = default;
	virtual DiskUsage GetDiskUsage() { return du; };

	DiskUsage du{};
};

#endif /* _EXECUTE_DIR_MONITOR_H */
