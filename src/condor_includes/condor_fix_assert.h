/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
#else
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
#endif


#endif /* CONDOR_FIX_ASSERT_H */


