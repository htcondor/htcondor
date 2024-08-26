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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "execute_dir_monitor.h"
#include "directory.h"

DiskUsage ManualExecDirMonitor::GetDiskUsage() {
	DiskUsage du;

	// make sure this computation is done with user priv, since that
	// is who owns the directory and it may not be world-readable
	Directory dir(workingDir.c_str(), PRIV_USER);
	time_t begin_time = time(nullptr);
	du.execute_size = dir.GetDirectorySize(&du.file_count);
	time_t scan_time = time(nullptr) - begin_time;
	if (scan_time > 10) {
		dprintf(D_ALWAYS, "It took %ld seconds to determine DiskUsage: %lld for %ld dirs+files\n",
		                  scan_time, (long long)du.execute_size, du.file_count);
	}

	return du;
}
