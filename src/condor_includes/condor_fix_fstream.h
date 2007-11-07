
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

#if !defined(__CONDOR_FIX_FSTREAM_H) && defined(__cplusplus)
#define __CONDOR_FIX_FSTREAM_H

	/* Save what we redefined open() and fopen() to be, if anything.
 	*/
#ifndef _CONDOR_ALLOW_OPEN
#	define _CONDOR_OPEN_SAVE open
#	undef open
#endif
#ifndef _CONDOR_ALLOW_FOPEN
#	define _CONDOR_FOPEN_SAVE fopen
#	undef fopen
#endif

#include <fstream>
using namespace std;

	/* Restore the open() and fopen() macros back to what they were
	 */
#ifdef _CONDOR_OPEN_SAVE
#	define open _CONDOR_OPEN_SAVE
#endif
#ifdef _CONDOR_FOPEN_SAVE
#	define fopen _CONDOR_FOPEN_SAVE
#endif


#endif
