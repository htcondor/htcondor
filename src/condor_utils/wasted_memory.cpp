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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "wasted_memory.h"

#include <algorithm>

bool
UpdateWastedMemoryAttrs(classad::ClassAd &job_ad, time_t activation_start,
                        time_t now, bool activation_ended)
{
		// Don't fall back to evaluating RequestMemory if
		// ActivationRequestMemory is missing (e.g. the activation was started
		// by an older schedd): RequestMemory may be an expression, such as
		// one involving MemoryUsage, that now evaluates differently than it
		// did when the activation started.  Better no value than a wrong one.
	double request_memory_mb = 0;
	double memory_usage_mb = 0;
	bool known =
		job_ad.EvaluateAttrNumber(ATTR_JOB_ACTIVATION_REQUEST_MEMORY, request_memory_mb) &&
		job_ad.EvaluateAttrNumber(ATTR_MEMORY_USAGE, memory_usage_mb);

	if ( ! known) {
		dprintf(D_FULLDEBUG, "Not setting %s: %s or %s is unknown\n",
		        ATTR_CURRENT_WASTED_MEMORY_GB_HOURS,
		        ATTR_JOB_ACTIVATION_REQUEST_MEMORY, ATTR_MEMORY_USAGE);
		if (activation_ended) {
			job_ad.AssignExpr(ATTR_LAST_WASTED_MEMORY_GB_HOURS, "undefined");
		}
		return false;
	}

	time_t duration = std::max<time_t>(now - activation_start, 0);
	double wasted_gb = std::max((request_memory_mb - memory_usage_mb) / 1024.0, 0.0);
	double wasted_gb_hours = wasted_gb * (double)duration / 3600.0;

	job_ad.InsertAttr(ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, wasted_gb_hours);
	if (activation_ended) {
		job_ad.InsertAttr(ATTR_LAST_WASTED_MEMORY_GB_HOURS, wasted_gb_hours);
	}
	dprintf(D_FULLDEBUG, "%s = %f%s (%s=%f MemoryUsage=%f duration=%lld)\n",
	        ATTR_CURRENT_WASTED_MEMORY_GB_HOURS, wasted_gb_hours,
	        activation_ended ? " (activation ended)" : "",
	        ATTR_JOB_ACTIVATION_REQUEST_MEMORY, request_memory_mb, memory_usage_mb,
	        (long long)duration);
	return true;
}
