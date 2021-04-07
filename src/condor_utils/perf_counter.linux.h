/***************************************************************
*
* Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

// Encapsulate the linux peformance counter, for insn retired
// Note this is included in the procd, so we can't use full condor libraries

#ifndef __PERF_COUNTER_H
#define __PERF_COUNTER_H

#include <sys/types.h>

class PerfCounter {
	public:
		PerfCounter(pid_t pid);
		virtual ~PerfCounter();
		void start() const;
		void stop() const;
		long long getInsns() const;
	private:
		pid_t pid;
		int fd;
};

#endif
