/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

#ifndef _STR_ISXXX_H_
#define _STR_ISXXX_H_

#include "condor_common.h"
#include "condor_header_features.h"

// Like isdigit(), but for the whole string
bool str_isint( const char *string );

// Like str_isint(), but for "real" numbers (i.e. 1.2)
//  strict: 1. or .1 are invalid
bool str_isreal( const char *string, bool strict=false );

// Like str_isalpha(), but the whole string
bool str_isalpha( const char *string );

// Like str_isalnum(), but the whole string
bool str_isalnum( const char *string );

#endif
