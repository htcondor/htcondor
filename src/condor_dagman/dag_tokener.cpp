/***************************************************************
 *
 * Copyright (C) 2017, Condor Team, Computer Sciences Department,
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
#include "dag_tokener.h"
#include "MyString.h"
#include "tokener.h"

#if 0 // Moved to dagman_utils
dag_tokener::dag_tokener(const char * line_in)
{
	tokener tkns(line_in);
	while(tkns.next()) {
		std::string token;
		tkns.copy_token(token);
		tokens.Append(&token);
	}
}
#endif