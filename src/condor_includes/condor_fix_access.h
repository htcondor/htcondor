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

#ifndef CONDOR_FIX_ACCESS_H
#define CONDOR_FIX_ACCESS_H

#include "condor_header_features.h"

/* access() uses the real uid when performing its work. This is a problem
	because the daemons often run as root and just change the euid to perform
	checks to see if the uid of the user(as the euid of the root process) 
	could perform them. This is especially annoying across NFS mounts where
	root squash is turned on and root gets mapped to nobody. People generally
	use the access interface because it is a nice API, and not because of the
	using the real uid feature. So, since it does us more harm than good to
	always use the real uid in our code, we're going to fix access(under unix
	only) to call a different access implementation that honors the euid.
	psilord 5/1/2002 */

/* do not perform this remapping under NT */
#ifndef WIN32

BEGIN_C_DECLS

/* This function uses various methods to nearly copy what access() does, 
	except in euid spaces, not real uid space */
int access_euid(const char *path, int mode);

END_C_DECLS

#undef access
#define access(x,y) access_euid(x,y)

#else 

extern "C" int __access_(const char *path, int mode);
#undef access
#define access(x,y) __access_(x,y)

#endif /* WIN32 */

#endif /* CONDOR_FIX_ACCESS_H */



