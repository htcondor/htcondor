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

#ifndef INCLUDE_CLASSAD_NEWOLD_H
#define INCLUDE_CLASSAD_NEWOLD_H

namespace classad { class ClassAd; }
class ClassAd;
class MyString;

/*
src - New classad to convert.  Will not be modified.

dst - Old classad to that will be modified to receive results.  Should
be empty/newly constructed, otherwise behavior is undefined.  Dirty flags
from src will be preserved.

returns: true on success, false on a problem.
*/
bool new_to_old(classad::ClassAd & src, ClassAd & dst);

/*
src - Old classad to convert.  Will not be modified.

dst - New classad to receive results.  Should probably be empty.
Dirty flags may or may not be preserved.  (As of 2005-10-10 they are
not, but it will likely be extended to do so later.)

returns: true on success, false on a problem.

*/
bool old_to_new(ClassAd & src, classad::ClassAd & dst);

/*
old_value - ClassAd value in "old" format
new_value_buffer - buffer to append ClassAd value in "new" format
err_msg - buffer for error messages (NULL for none)
*/
bool old_classad_value_to_new_classad_value(char const *old_value, MyString &new_value_buffer, MyString *err_msg);

#endif /* INCLUDE_CLASSAD_NEWOLD_H*/
