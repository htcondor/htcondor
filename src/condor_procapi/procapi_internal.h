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

#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29) || defined(Solaris10) || defined(Solaris11)
# include <procfs.h>		// /proc stuff for Solaris 2.6, 2.7, 2.8, 2.9
#else
# include <sys/procfs.h>	// /proc stuff for everything else and
#endif

#endif /* ! HPUX && Darwin && CONDOR_FREEBSD */

#endif /* ifndef WIN32 */
#endif // PROCAPI_INTERNAL_H
