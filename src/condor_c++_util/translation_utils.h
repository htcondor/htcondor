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


#ifndef TRANSLATION_UTIL_H
#define TRANSLATION_UTIL_H

#include "condor_common.h"
#include "condor_commands.h"

/* This file contains structures and methods for translation
   tables used by other utilities.  These tables allow us to map
   integers or enums into corresponding strings.

   A future implementation should use a hashtable, though we might not
   always want to incur the cost of hashing all the command names if
   we're not going to do lots of lookups.
*/

struct Translation {
	const char *name;
	int number;
};

const char* getNameFromNum( int num, const struct Translation *table );
int         getNumFromName( const char* str, const struct Translation *table );


#endif /* TRANSLATION_UTIL_H */
