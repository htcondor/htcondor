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
#ifndef CONDOR_FIX_ACCESS_H
#define CONDOR_FIX_ACCESS_H

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

#endif /* WIN32 */

#endif /* CONDOR_FIX_ACCESS_H */



