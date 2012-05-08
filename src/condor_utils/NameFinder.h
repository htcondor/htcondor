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

#ifndef _CONDOR_NAMEFINDER_CPP_H
#define _CONDOR_NAMEFINDER_CPP_H

#include <string>

// This class basically provides an iterator to split strings delimited by
// semicolons. It would be better to use a regular expression class to split
// the string, but this is prototype code
// This is used by submit, schedd, and shadow.  To avoid duplication, I
// factored it into this file
//
// nwp 26 Apr 2012

class NameFinder {
public:
       NameFinder(const std::string& logname);
       operator bool() const { return it != log.end(); } 
       std::string get();
private:
       std::string log;
       std::string::iterator it;
       NameFinder(); 
};

#endif

