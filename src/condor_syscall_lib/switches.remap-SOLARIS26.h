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
