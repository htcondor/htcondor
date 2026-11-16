/***************************************************************
 *
 * Copyright (C) 2026, Condor Team, Computer Sciences Department,
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

#ifndef _TOKENS_DB_H_
#define _TOKENS_DB_H_ 1

#include <string>

struct TokenInfo
{
	std::string iss;
	std::string kid;
	std::string jti;
	std::string sub;
	std::string scope;
	long long iat{0};
	long long exp{0};
	std::map<std::string, std::string> extra_claims;
	bool cap{false};
};

bool LookupToken(const std::string& jti, const std::string& kid, TokenInfo& token);

bool AddToken(const TokenInfo& token);

#endif /* _TOKENS_DB_H_ */
