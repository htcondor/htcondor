/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
#	define CONDOR_HAD_GNUC 2
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

#if defined(CONDOR_HAD_GNUC)
#	define __GNUC__ CONDOR_HAD_GNUC
#endif


#endif /* CONDOR_FIX_ASSERT_H */


