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


#ifndef WHICH_H
#define WHICH_H

#include <string>

// Searches the $PATH for the given filename.
// Returns the full path to the file, or "" if it wasn't found
// On WIN32, if the file ends in .dll, mimics LoadLibrary's search order
// EXCEPT that it does not attempt to check "the directory from which the 
// application loaded," (which is in the LoadLibrary search order) because
// you might want to call which in order to find what .dll file a specific
// application might use.  That's what the 2nd parameter is for.  
std::string which(const std::string &strFilename, const std::string &strAdditionalSearchDir = "");

#endif // WHICH_H
