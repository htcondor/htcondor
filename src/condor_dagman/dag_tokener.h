/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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

#ifndef __DAG_TOKENER__
#define __DAG_TOKENER__
#include <string>

#if 0 // Moved to dagman_utils
// this is a simple tokenizer class for parsing keywords out of a line of text
// token separator defaults to whitespace, "" or '' can be used to have tokens
// containing whitespace, but there is no way to escape " inside a "" string or
// ' inside a '' string. outer "" and '' are not considered part of the token.

class dag_tokener {
public:
	dag_tokener(const char * line_in);
	void rewind() { tokens.Rewind(); }
	const char * next() { return tokens.AtEnd() ? NULL : tokens.Next()->c_str(); }
protected:
	std::list<std::string> tokens; // parsed tokens
};
#endif


#endif // __DAG_TOKENER__
