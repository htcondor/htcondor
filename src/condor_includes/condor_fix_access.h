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



