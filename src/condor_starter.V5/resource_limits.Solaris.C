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

 


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "proto.h"
#include "limit.h"   // for limit() (duh)


#define MEG (1024 * 1024)


void
set_resource_limits()
{
	limit( RLIMIT_CPU, RLIM_INFINITY );
	limit( RLIMIT_FSIZE, RLIM_INFINITY );
	limit( RLIMIT_DATA, RLIM_INFINITY );
	limit( RLIMIT_VMEM, RLIM_INFINITY ); /* VMEM seems to be analogous to RSS on 
	Sun ..dhaval 7/2 */

/*
	The stack segment in the sun core file always has a virtual size which
	is the same as the stack size limit in force at the time of the dump.
	This is implemented as a file "hole" so that the actual amount of space
	taken up on disk is only that required to hold the valid part of the
	stack.  Thus the virtual size of the core file is generally much larger
	than the actual size.  This has some unfortunate interactions with
	process limits, see comments below regarding specific limits.
*/

/*
	We would like to unlimit the stack size so that the process can have
	complete flexibility in how it uses its virtual memory resources.
	Unfortunately if we do that, the virtual size of the stack segment
	in the core file will be 2 gigabytes.  For some reason the operating
	system won't produce such a core, even though there is plenty of disk
	to accomodate the actual size of the file.  We therefore set the
	stack limit to a default of 8 megabytes.
*/
	limit( RLIMIT_STACK, 8 * MEG );

/*
	We would like to limit the size of the coredump to something less than
	the actual free disk in the filesystem.  That way we could ensure
	that the system would not try to produce a core that won't fit, overflow
	the file system, and produce ugly messages on the console.  Unfortunately
	if seems that the coredump size limit controls the virtual rather
	than the actual size of the core.  That means if there is 6 Meg
	of free disk and we set the limit accordingly, we would never get
	a core at all, even if the actual size of the core would only be
	2 meg!!  For this reason, we don't limit the core file size on
	Suns.
*/

	limit( RLIMIT_CORE, RLIM_INFINITY );
}
