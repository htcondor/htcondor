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
#include "param_info.h"

#define PARAM_DECLARE_HELP_TABLES 1 // so paramhelp_table will 
#include "param_info_tables.h"

int param_default_help_by_id(int ix, const char * & descrip, const char * & tags, const char * & used_for)
{
	descrip = NULL;
	tags = NULL;
	used_for = NULL;

	if (ix >= 0 && ix < condor_params::paramhelp_table_size) {
		const struct condor_params::paramhelp_entry* phe = condor_params::paramhelp_table[ix];
		if (phe) {
			const char * p = phe->strings;
			if (p) {
				descrip = *p ? p : NULL;
				p += strlen(p)+1;
				tags = *p ? p : NULL;
				p += strlen(p)+1;
				used_for = *p ? p : NULL;
			}
			return phe->flags;
		}
	}
	return 0;
}

