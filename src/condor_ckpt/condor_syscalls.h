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


#ifndef _CONDOR_SYSCALLS_H
#define _CONDOR_SYSCALLS_H

#if defined(Solaris)
#	include <sys/syscall.h> /* Solaris specific change ..dhaval 6/30 */
#else
#	include <syscall.h> 
#endif


typedef int BOOL;

static const int 	SYS_LOCAL = 1;
static const int 	SYS_REMOTE = 0;
static const int	SYS_RECORDED = 2;
static const int	SYS_MAPPED = 2;

static const int	SYS_UNRECORDED = 0;
static const int	SYS_UNMAPPED = 0;

#if defined(__cplusplus)
extern "C" {
#endif

int SetSyscalls( int mode );
int GetSyscallMode();
BOOL LocalSysCalls();
BOOL RemoteSysCalls();
BOOL MappingFileDescriptors();

#if defined(OSF1) || defined(HPUX) || defined (Solaris)
	int syscall( int, ... );
#endif

#if defined(Solaris)
  #include <sys/types.h>
  #include <sys/mman.h>
  caddr_t MMAP(caddr_t  addr,  size_t  len,  int  prot, int flags,
	  		   int fildes, off_t off);
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_SYSCALLS_H */
