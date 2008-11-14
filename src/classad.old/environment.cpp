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
#include "condor_ast.h"

void
evalFromEnvironment (const char *name, EvalResult *val)
{
	// Use stricmp comparison since ClassAd attribute
	// names are supposed to be case-insensitive.
	if (stricmp (name, "CurrentTime") == 0)
	{
		time_t	now = time (NULL);
		if (now == (time_t) -1)
		{
			val->type = LX_ERROR;
			return;
		}
		val->type = LX_INTEGER;
		val->i = (int) now;
		return;
	}

	val->type = LX_UNDEFINED;
	return;
}	
