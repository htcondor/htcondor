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
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

#include "perf_counter.linux.h"

// The syscall perf_event_open isn't wrapped in glibc, so we have to do it ourselves
static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);
	return ret;
}

PerfCounter::PerfCounter(pid_t pid): pid(pid), fd(-1)  {
	struct perf_event_attr pe;

	memset(&pe, 0, sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_HARDWARE;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = PERF_COUNT_HW_INSTRUCTIONS;
	pe.inherit = 1; // also trace kids
	pe.disabled = 1;
	pe.exclude_kernel = 1; // Otherwise we need to be root (?)
	pe.exclude_hv = 1; // exclude hypervisor
	//
	// create the perf fd for this pid & kids
	fd = perf_event_open(&pe, 
			 pid,  // pid:
			-1,  // cpu:   -1 means "all"
			-1,  // group: -1 means no group
			PERF_FLAG_FD_CLOEXEC);

	if (fd == -1) {
		//dprintf(D_ALWAYS, "Error opening leader %llx\n", pe.config);
	}
	//
	// clear counters
	ioctl(fd, PERF_EVENT_IOC_RESET, 0);

}

void
PerfCounter::start() const {
	if (fd > -1) ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

void
PerfCounter::stop() const {
	if (fd > -1) ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}

PerfCounter::~PerfCounter() {
	if (fd > -1) close(fd);
}

long long
PerfCounter::getInsns() const {
	long long insn = -1;
	if (fd > -1) {
		long long count = -1;
		int r = read(fd, &count, sizeof(long long));
		if (r != -1) insn = count;
	}
	return insn;
}

#ifdef TESTME
int
main(int argc, char **argv)
{

	PerfCounter p(getpid());
	p.start();

	int pid = fork();
	if (pid == 0) {
		printf("Measuring instruction count for this printf\n");
		exit(0);
	} else {
		int status;
		waitpid(pid,&status, 0);
	}

	long long count = p.getInsns();

	printf("Used %lld instructions\n", count);

}
#endif
