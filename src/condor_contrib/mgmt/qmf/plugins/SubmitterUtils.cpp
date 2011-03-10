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

#ifndef WIN32
	#include "stdint.h"
#endif

#include "condor_qmgr.h"
#include "condor_attributes.h"
#include "condor_classad.h"

#include "proc.h"


static const int HEADER_CLUSTER = 0;
static const int HEADER_PROC = 0;

bool GenerateId(uint32_t &id)
{
	static const char * MGMT_ID = "MgmtId";

	if (GetAttributeInt(HEADER_CLUSTER, HEADER_PROC,
						MGMT_ID, (int *) &id) < 0) {
		id = 2; // Id 1 is reserved for the Scheduler
	}

	if (SetAttributeInt(HEADER_CLUSTER, HEADER_PROC, MGMT_ID, (int) ++id)) {
		return false;
	}

	return true;
}

void SanitizeSubmitterName(MyString &name)
{
		// We /may/ (will!) use the name as the name of an
		// attribute, so we must strip invalid characters that we
		// expect to find in the name.

	static const int invalid_char_len = 4;
	static const char *invalid_chars[invalid_char_len] =
		{"-", "@", ".", " "}; // XXX: Invert this, use [a-zA-Z_][a-zA-Z0-9_]*
	for (int i = 0; i < invalid_char_len; i++) {
		while (-1 != name.find(invalid_chars[i])) {
			name.replaceString(invalid_chars[i], "_");
		}
	}
}

bool GetSubmitterNameFromAd(ClassAd &ad, MyString &name)
{
	if (!ad.LookupString(ATTR_NAME, name)) {
		return false;
	}

	SanitizeSubmitterName(name);
	return true;
}

bool GetSubmitterNameFromJob(const PROC_ID &id, MyString &name)
{
	if (GetAttributeString(id.cluster, id.proc,
						   ATTR_USER, name) < 0) {
		return false;
	}

	SanitizeSubmitterName(name);
	return true;
}

bool GetSubmitterId(const char *name, uint64_t &id)
{
	uint32_t mgmtId;

	if (GetAttributeInt(HEADER_CLUSTER, HEADER_PROC, name,
						(int *) &mgmtId) < 0) {
		if (!GenerateId(mgmtId)) {
				// Failed to generate a new id, this seems fatal
			return false;
		}

		if (SetAttributeInt(HEADER_CLUSTER, HEADER_PROC, name,
							(int) mgmtId)) {
				// Failed to record the new id, this seems fatal
			return false;
		}
	}

		// The ((uint64_t) 0) << 32 id space is reserved for us
	id = (uint64_t) mgmtId;

	return true;
}
