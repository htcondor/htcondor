/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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

#pragma once

#include <chrono>

#ifdef WIN32
#  include <windows.h>
#  include <psapi.h>
#elif defined(__APPLE__)
#  include <sys/resource.h>
#else
#  include <sys/resource.h>
#endif

namespace Profiling {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// Returns wall-clock seconds elapsed between two time points.
inline double elapsed_s(TimePoint start, TimePoint end) {
	return std::chrono::duration<double>(end - start).count();
}

// Returns process peak RSS in KiB (high-water mark since process start).
//   Linux  : ru_maxrss is already KiB
//   macOS  : ru_maxrss is bytes; divided by 1024 to normalize
//   Windows: PeakWorkingSetSize from GetProcessMemoryInfo, converted to KiB
inline long peak_rss_kb() {
#ifdef WIN32
	PROCESS_MEMORY_COUNTERS pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		return static_cast<long>(pmc.PeakWorkingSetSize / 1024);
	}
	return 0L;
#else
	struct rusage ru;
	getrusage(RUSAGE_SELF, &ru);
#  ifdef __APPLE__
	return ru.ru_maxrss / 1024; // bytes -> KiB
#  else
	return ru.ru_maxrss;        // already KiB
#  endif
#endif
}

} // namespace Profiling
