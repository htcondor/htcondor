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
#ifndef PROCAPI_INTERNAL_H
#define PROCAPI_INTERNAL_H

/* This file is the dumping ground for strange headers that should ONLY pollute
	the procapi source files, not everything that included the procapi.h
	interface header file. Eventually, more things will migrate into here.
	
	An example of something tht could cause this to happen is in Linux, where
	user land header files will have an enum of some symbols but the 
	kernel header files--which the procapi code often uses, would #define
	them. If things get included in just the right way, the #defines will 
	override the symbols in the enum leading to an unparsable mess. */

/* All of the following is unix-only */
#ifndef WIN32
#if (!defined(HPUX) && !defined(Darwin) && !defined(CONDOR_FREEBSD))	 // neither of these are in hpux.

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29)
#include <procfs.h>		// /proc stuff for Solaris 2.6, 2.7, 2.8, 2.9
#else
#include <sys/procfs.h>	// /proc stuff for everything else and
#endif

#endif /* ! HPUX && Darwin && CONDOR_FREEBSD */

#endif /* ifndef WIN32 */
#endif // PROCAPI_INTERNAL_H
