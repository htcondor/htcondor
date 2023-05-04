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

#ifndef __TOKEN_CACHE_H
#define __TOKEN_CACHE_H

#ifdef WIN32

#include "condor_common.h"
#include <map>

#define MAX_CACHE_SIZE 10		// maximum number of tokens to cache. This 
								// might be a config file knob one day.


/* define a couple data structures */

struct token_cache_entry {
	int age;
	HANDLE user_token;
};

/* This class manages our cached user tokens. */

class token_cache {
	public:
		token_cache();
		~token_cache();

		HANDLE getToken(const char* username, const char* domain);
		bool storeToken(const char* username, const char* domain, 
			const HANDLE token);
		std::string cacheToString();
		
	private:

		/* Data Members */
		std::map<std::string, token_cache_entry*> TokenTable;
		int current_age;
		int dummy;

		/* helper functions */
		int getNextAge() { return current_age++; }
		void removeOldestToken();
		bool isOlder(int index, int olderIndex) { 
			return ( index < olderIndex); 
		}
		size_t getCacheSize() { return TokenTable.size(); }
	
};

#endif // WIN32
#endif // __TOKEN_CACHE_H
