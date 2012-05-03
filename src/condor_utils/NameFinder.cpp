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

NameFinder::NameFinder(const std::string& logname) :
       log(logname)
{
       it = log.begin();       
}

std::string NameFinder::get()
{
	std::string entry;
		// Clear out prefix whitespace;
	while(it != log.end() && isspace(*it)) {
		++it;
	}
		// Pull in all characters until we see a semicolon
		// Space allowed in UserLog entries?	
	while(it != log.end() && *it != ';') {
		entry += *it;
		++it;
	}
		// Move past the semicolon, if we are at one
	if(it != log.end() && *it == ';') {
		++it;
	}
	return entry;
}

