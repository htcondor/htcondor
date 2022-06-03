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

#ifndef _MISC_UTILS_H
#define _MISC_UTILS_H

#include <string>


/* Converts an int to a string w/ "st", "nd", "rd, or "th" added on. */
const char*	num_string( int );  


/* Returns the name of the local file the startd uses to write its
   current claim id into.  If the given slot_id is 0, we assume a
   non-SMP startd and use a generic name.  Otherwise, we append ".slotX"
   to the filename we create.
*/
std::string startdClaimIdFile( int slot_id );  

/* return the timezone on the local host */
const char* my_timezone(int isdst);

#endif /* _MISC_UTILS_H */
