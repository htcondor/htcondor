/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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

#include "NameFinder.h"
#include <cctype>
#include <algorithm>
#include <iterator>
NameFinder::NameFinder(const std::string& logname, const char d) :
       delim(d), log(logname)
{
       it = log.begin();       
}

namespace {
bool not_space(char ch)
{
	return !std::isspace(ch);
}
}

std::string NameFinder::get()
{
	std::string entry;
	it = std::find_if(it,log.end(),not_space);
	if(it != log.end()) {
		std::string::iterator p = std::find(it,log.end(),delim);
		entry.assign(it,p);
		it = p + ( p == log.end() ? 0 : 1 );
	}
	return entry;
}

