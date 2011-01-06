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

#ifndef _LINK_H
#define _LINK_H

#if defined(WIN32)
// Windows doesn't have link(), so we provide one in terms
// of CreateHardLink(). on failure, -1 is returned and errno
// is set to either ENOENT (if the file couldn't be found) or
// EPERM (for anything else, since "operation not permitted"
// is fairly generic-sounding). 0 is returned on success
//
int link(const char* oldpath, const char* newpath);
#endif

// return the number of hard links to the given path, or -1
// on error
//
int link_count(const char* path);

#endif
