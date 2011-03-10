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

#include "condor_common.h"

#include "JobUtils.h"

#include "Utils.h"

#include "condor_classad.h"

#include "condor_qmgr.h"

using namespace qpid::types;

bool
PopulateVariantMapFromProcId(int clusterId, int procId, Variant::Map &_map)
{
	ClassAd *ad;

	if (NULL == (ad = ::GetJobAd(clusterId, procId, false))) {
		dprintf(D_ALWAYS,
				"::GetJobAd method called on %d.%d, which does not exist\n",
				clusterId, procId);
		return false;
	}

	return PopulateVariantMapFromAd(*ad, _map);
}
