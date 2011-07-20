/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_debug.h"
#include "proc.h"

// local includes
#include "PROC_ID_comparator.h"

using namespace aviary::util;

bool
PROC_ID_comparator::operator()(const std::string &lhs, const std::string &rhs)
{
	PROC_ID lhs_id, rhs_id;

		// !!! The EXCEPT macro is "#define EXCEPT \", so we must
		// !!! enclose it in { }'s else we'll always EXCEPT

	if (!StrToProcId(lhs.c_str(), lhs_id)) { EXCEPT("Invalid LHS PROC_ID %s", lhs.c_str()); }
	if (!StrToProcId(rhs.c_str(), rhs_id)) { EXCEPT("Invalid RHS PROC_ID %s", rhs.c_str()); }

	return
		(lhs_id.cluster < rhs_id.cluster) ||
		((lhs_id.cluster == rhs_id.cluster) && (lhs_id.proc < rhs_id.proc));
}
