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
#include "stringSpace.h"

const char *
StringSpace::
strdup_dedup(const char *input)
{
    if (input == NULL) return NULL;
    ssentry & value = ss_map[input];
    if (value.pstr == NULL) {
        value.pstr = strdup(input);
    }
    value.count++;

    return (const char *)value.pstr;    
}

int 
StringSpace::
free_dedup(const char *input)
{
    int ret_value = 0;

    if (input == NULL) return NULL;
    std::string key = input;
    ssentry & value = ss_map[key];
    if (value.pstr) {
        ASSERT(value.count > 0);
        ret_value = --value.count;
        if (value.count == 0) {
            ss_map.erase(key);
        }
    }
    else {
        // If we get here, the input pointer either was not created
        // with strdup_dedup(), or it was already deallocated with free_dedup()
        EXCEPT("free_dedup() called with invalid input");
    }

    return ret_value;
}

