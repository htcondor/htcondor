/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_FIX_SYS_UTSNAME_H
#define _CONDOR_FIX_SYS_UTSNAME_H

#include "condor_header_features.h"

#if defined(CONDOR_KILLS_UNAME_DEFINITIONS)
	#define uname(a) __condor_hack_uname(a)
	#define _uname(a) __condor_hack__uname(a)
#endif

#include <sys/utsname.h>

#if defined(CONDOR_KILLS_UNAME_DEFINITIONS)
	#undef uname
	#undef _uname

	/* Now, we must provide the protoypes that we lost. */

	BEGIN_C_DECLS
	extern int uname( struct utsname * );
	extern int _uname( struct utsname * );
	END_C_DECLS
#endif

#endif /* _CONDOR_FIX_SYS_UTSNAME_H */
