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

/* This file is for remappings that are specific to Solaris 2.6 */

/* These remaps are needed by file_state.c */
#if defined(FILE_TABLE)

REMAP_TWO( creat64, _creat64, int , const char *, mode_t )
REMAP_THREE( lseek64, _lseek64, off64_t, int, off64_t, int )
REMAP_THREE_VARARGS( open64, _open64, int , const char *, int , int)
REMAP_THREE_VARARGS( open64, __open64, int , const char *, int , int)
REMAP_THREE( readlink, _readlink, int , const char *, char *, size_t )

#endif /* FILE_TABLE */

/* These remaps fill out the rest of the remote syscalls */
#if defined(REMOTE_SYSCALLS)

REMAP_TWO( fstat64, _fstat64, int, int, struct stat64 * )
REMAP_THREE( getdents64, _getdents64, int, int, struct dirent64*, unsigned int )
REMAP_TWO( getrlimit64, _getrlimit64, int, int, struct rlimit64 * ) 
REMAP_TWO( link, __link, int , const char *, const char *)
REMAP_TWO( lstat64, _lstat64, int, const char *, struct stat64 * )
REMAP_SIX( mmap64, _mmap64, MMAP_T, MMAP_T, size_t, int, int, int, off64_t )
REMAP_THREE( setitimer, _setitimer, int , int , struct itimerval *, struct itimerval *)
REMAP_TWO( setrlimit64, _setrlimit64, int, int, const struct rlimit64 * ) 
REMAP_TWO( stat64, _stat64, int, const char *, struct stat64 * )
REMAP_FOUR_VOID( profil, _profil, void , unsigned short *, unsigned int , unsigned int , unsigned int )


#endif /* REMOTE_SYSCALLS */
