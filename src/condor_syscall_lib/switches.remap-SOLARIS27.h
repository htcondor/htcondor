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

/* This file is for remappings that are specific to Solaris 2.7 */


/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)

REMAP_THREE( readlink, _readlink, int , const char *, char *, size_t )
REMAP_THREE( lseek64, _lseek64, off64_t, int, off64_t, int )
REMAP_TWO( creat64, _creat64, int , const char *, mode_t )
REMAP_THREE_VARARGS( open64, _open64, int , const char *, int , int)
REMAP_THREE_VARARGS( open64, __open64, int , const char *, int , int)

#endif /* FILE_TABLE */

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

#if defined(X86)
	REMAP_THREE( _lxstat, lxstat, int , const int , const char *, struct stat *)
	REMAP_THREE( _xstat, xstat, int , const int , const char *, struct stat *)
#else 
	REMAP_THREE( _lxstat, lxstat, int , const int , int , struct stat *)
	REMAP_THREE( _xstat, xstat, int , const int , int , struct stat *)
#endif
REMAP_THREE( _fxstat, fxstat, int , const int , int , struct stat *)

REMAP_FOUR( ptrace, _ptrace, long , int , long , long , long )
REMAP_THREE( setitimer, _setitimer, int , int , struct itimerval *, struct itimerval *)
REMAP_FOUR_VOID( profil, _profil, void , unsigned short *, unsigned int , unsigned long , unsigned int )
REMAP_TWO( fstat64, _fstat64, int, int, struct stat64 * )
REMAP_TWO( lstat64, _lstat64, int, const char *, struct stat64 * )
REMAP_TWO( stat64, _stat64, int, const char *, struct stat64 * )
REMAP_THREE( getdents64, _getdents64, int, int, struct dirent64*, unsigned int )
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_SIX( mmap64, _mmap64, MMAP_T, MMAP_T, size_t, int, int, int, off64_t )
REMAP_TWO( getrlimit64, _getrlimit64, int, int, struct rlimit64 * ) 
REMAP_TWO( setrlimit64, _setrlimit64, int, int, const struct rlimit64 * ) 


#endif /* REMOTE_SYSCALLS */
