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

#ifndef _STRING_CONVERSION_WINDOWS_H_
#define _STRING_CONVERSION_WINDOWS_H_

// #include "ntsecapi.h"

/* convert an ANSI string to Unicode.
   NOTE: you must delete [] foo the returned value when you 
         are done with it */
LPWSTR ProduceWFromA ( PCSTR as );

/* convert a Unicode string to ANSI.
   NOTE: you must delete [] foo the returned value when you 
		 are done with it */
LPSTR ProduceAFromW ( PCWSTR ws );

/* convert a Unicode string to a UNICODE_STRING struct.
   NOTE: you must delete [] foo the returned value when you 
         are done with it */
/*
PUNICODE_STRING
ProduceUSFromW ( PCWSTR source );
*/

/* convert a Unicode string to a UNICODE_STRING struct.
   NOTE: you must delete [] foo the returned value when you 
         are done with it */
/*
PLSA_UNICODE_STRING 
ProduceLSAUSFromW ( PCWSTR source );
*/

#endif // _STRING_CONVERSION_WINDOWS_H_
