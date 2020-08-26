/***************************************************************
 *
 * Copyright (C) 1990-2019, HTCondor Team, Computer Sciences Department,
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
#include "stringSpace.h"


StringSpace::ssentry* StringSpace::new_entry(const char * str) {
	if (!str) {
		return nullptr;
	}
	size_t cch = 0; 
	cch = strlen(str);
	// note: in malloc below (cch & ~3) subtracts up to 3 from cch
	// in order to keep the malloc request aligned to 4 bytes.
	ssentry* ptr = (ssentry*)malloc(sizeof(ssentry) + (cch & ~3));
	ptr->count = 1;
	strcpy(ptr->str, str);
	return ptr;
}

void StringSpace::clear() {
	for (auto it = ss_map.begin(); it != ss_map.end(); ++it) {
		free(it->second);
	}
	ss_map.clear();
}

const char *
StringSpace::
strdup_dedup(const char *input)
{
    if (input == NULL) return NULL;
	auto it = ss_map.find(input);
	if (it != ss_map.end()) {
		it->second->count += 1;
		return &it->second->str[0];
	}
	ssentry * ptr = new_entry(input);
	ptr->count = 1;
	ss_map[ptr->str] = ptr;
	return &ptr->str[0];
}

int 
StringSpace::
free_dedup(const char *input)
{
    int ret_value = 0;

    if (input == NULL) return INT_MAX;
	auto it = ss_map.find(input);
	if (it != ss_map.end()) {
		ASSERT(it->second->count > 0);
		ret_value = --(it->second->count);
		if (it->second->count == 0) {
			auto temp = it->second;
			ss_map.erase(it);
			free(temp);
		}
	}
    else {
        // If we get here, the input pointer either was not created
        // with strdup_dedup(), or it was already deallocated with free_dedup()
        dprintf(D_ALWAYS | D_BACKTRACE, "free_dedup() called with invalid input");
    }

    return ret_value;
}

