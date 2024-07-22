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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "execute_dir_monitor.h"
#include "directory.h"

std::tuple<filesize_t, size_t> ManualExecDirMonitor::GetDiskUsage() {
	filesize_t exec_size = 0;
	size_t file_count = 0;

	// make sure this computation is done with user priv, since that
	// is who owns the directory and it may not be world-readable
	Directory dir(workingDir, PRIV_USER);
	time_t begin_time = time(nullptr);
	exec_size = dir.GetDirectorySize(&file_count);
	time_t scan_time = time(nullptr) - begin_time;
	if (scan_time > 10) {
		dprintf(D_ALWAYS, "It took %ld seconds to determine DiskUsage: %lld for %ld dirs+files\n",
		                  scan_time, (long long)exec_size, file_count);
	}

	return {exec_size, file_count};
}
