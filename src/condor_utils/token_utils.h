/***************************************************************
 *
 * Copyright (C) 2019, HTCondor Team, Computer Sciences Department,
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

#include <string>

#ifndef __TOKEN_UTILS_H
#define __TOKEN_UTILS_H

class CondorError;

namespace htcondor {

	// Append a given token's contents to a given token file.
	// The correct directory is automatically determined from the
	// HTCondor configuration.
int
write_out_token(const std::string &token_name, const std::string &token, const std::string &identity);

	// Generate a client ID appropriate for a token request
std::string generate_client_id();

	// Determine the preferred key name for signing
std::string
get_token_signing_key(CondorError &err);

}

#endif
