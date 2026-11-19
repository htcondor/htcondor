/***************************************************************
 *
 * Copyright (C) 2026, Condor Team, Computer Sciences Department,
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

#ifndef _WASTED_MEMORY_H
#define _WASTED_MEMORY_H

#include "condor_classad.h"

// Set ATTR_CURRENT_WASTED_MEMORY_GB_HOURS in job_ad to the memory wasted
// so far by the activation that started at activation_start, in
// gigabyte-hours (where one gigabyte is 1024 MiB):
//
//   max(ActivationRequestMemory - MemoryUsage, 0) / 1024 * (now - activation_start) / 3600
//
// ATTR_JOB_ACTIVATION_REQUEST_MEMORY is what RequestMemory evaluated to
// when the activation started.  If it's missing (e.g. the job was started
// by an older schedd), the value can't be computed: RequestMemory itself
// may now evaluate differently than it did then.  MemoryUsage in job_ad
// must be the max seen over the lifetime of the job.
//
// If activation_ended is true, the activation has ended (or is ending, and
// the caller may not get another chance), now is when it ended as far as
// the caller knows, and the value is also copied into
// ATTR_LAST_WASTED_MEMORY_GB_HOURS.
//
// If the value can't be computed, ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, if
// an earlier call set it, is set to undefined, so it doesn't keep a value
// computed from inputs we no longer have; and if activation_ended,
// ATTR_LAST_WASTED_MEMORY_GB_HOURS is set to undefined, so it doesn't
// claim a previous activation's value.  (Undefined, rather than deleted,
// because the job queue updates that carry these attributes can't
// delete one.)
//
// Returns false if the value can't be computed (because
// ActivationRequestMemory or MemoryUsage is unknown).
bool UpdateWastedMemoryAttrs(classad::ClassAd &job_ad, time_t activation_start,
                             time_t now, bool activation_ended);

#endif
