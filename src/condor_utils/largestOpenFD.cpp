/***************************************************************
 *
 * Copyright (C) 1990-2023, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include <filesystem>
#include <algorithm>
#include <charconv>

// When we fork/exec, we want to close all open files.  If
// ulimit is high, there could be millions of potential valid
// fds, and looping over all of them is very expensive. Instead
// on Linux, we can parse through /proc/self/fd to find the
// highest fd, and stop there, saving millions of system calls.

namespace stdfs = std::filesystem;

int largestOpenFD() {
#ifdef LINUX
	stdfs::path fds {"/proc/self/fd"};
	int max_fd = 0;
	std::error_code ec;
	for (const auto &d : stdfs::directory_iterator(fds,ec)) {
		std::string basename = d.path().filename().string();
		int fd = 0;
		std::ignore =  // should never fail, so don't bother to test if invalid
			std::from_chars(basename.data(), basename.data() + basename.size(), fd);
		max_fd = std::max(max_fd, fd);
	}
	return max_fd + 1;
#else
	return getdtablesize();
#endif
}

