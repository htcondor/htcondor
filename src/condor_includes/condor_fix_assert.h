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

/*
Get rid of __GNUC__, otherwise gcc will assume its own library when
generating code for asserts.  This will complicate linking of
non gcc programs (FORTRAN) - especially at sites where gcc isn't
installed!

Please note the way of doing things like this... what does NOT
work:

#define SAVE_FOO FOO
#undef FOO

...

#define FOO SAVE_FOO

so, please.... don't use it... okay?

*/

#ifndef __CONDOR_FIX_ASSERT_H
#define __CONDOR_FIX_ASSERT_H

#if !defined(LINUX)

#if defined(__GNUC__)
#if __GNUC__ == 2
#	define CONDOR_HAD_GNUC_2
#elif __GNUC__ == 3
#	define CONDOR_HAD_GNUC_3
#elif __GNUC__ == 4
#       define CONDOR_HAD_GNUC_4
#else
    /*NOTE: if you add another GNUC version above, make sure you
            also add it near the end of this file where __GNUC__
            is restored to its original value.*/
#	error "Please fix the definition of CONDOR_HAD_GNUC."
#endif
#	undef __GNUC__
#endif

#endif /* LINUX */

#ifndef WIN32	/* on Win32, we do EXCEPT instead of assert */
#include <assert.h>
#else
#include "condor_debug.h"
#endif	/* of else ifndef WIN32 */

#if defined(CONDOR_HAD_GNUC_2)
#	define __GNUC__ 2
#elif defined(CONDOR_HAD_GNUC_3)
#	define __GNUC__ 3
#elif defined(CONDOR_HAD_GNUC_4)
#   define __GNUC__ 4
#endif


#endif /* CONDOR_FIX_ASSERT_H */


