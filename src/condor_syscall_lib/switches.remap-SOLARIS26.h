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

/* These are the ones that are new for Solaris 2.6 */

REMAP_TWO( creat, creat64, int , const char *, mode_t )
REMAP_TWO( creat, _creat64, int , const char *, mode_t )
REMAP_TWO( fstat64, _fstat64, int, int, struct stat64 * )
REMAP_THREE( getdents64, _getdents64, int, int, char*, unsigned int )
REMAP_TWO( getrlimit64, _getrlimit64, int, int, struct rlimit64 * ) 
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_THREE( llseek, _llseek, offset_t, int, offset_t, int )
REMAP_THREE( lseek64, _lseek64, off64_t, int, off64_t, int )
REMAP_TWO( lstat64, _lstat64, int, const char *, struct stat64 * )
REMAP_SIX( mmap64, _mmap64, caddr_t, caddr_t, size_t, int, int, int, off64_t )
REMAP_TWO( setrlimit64, _setrlimit64, int, int, struct rlimit64 * ) 
REMAP_TWO( stat64, _stat64, int, const char *, struct stat64 * )
