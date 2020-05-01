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

#ifndef CONDOR_SYS_LINUX_H
#define CONDOR_SYS_LINUX_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
# ifndef _DEFAULT_SOURCE
#  define _DEFAULT_SOURCE
# endif
#endif

#if defined(__GLIBC__)
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

#include <sys/types.h>

#include <sys/stat.h>
#include <unistd.h>

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/* <stdio.h> on glibc Linux defines a "dprintf()" function, which
   we've got hide since we've got our own. */
#if defined(__GLIBC__)
#	define dprintf _hide_dprintf
#	define getline _hide_getline
#endif
#include <stdio.h>
#if defined(__GLIBC__)
#	undef dprintf
#	undef getline
#endif

#define SignalHandler _hide_SignalHandler
#include <signal.h>
#undef SignalHandler

#include <sys/time.h>

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/vfs.h>

#include <sys/resource.h>
#include <sys/wait.h>

#include <search.h>

/* include stuff for malloc control */
#include <malloc.h>

#include <sys/mman.h>

#include <sys/syscall.h>

#endif /* CONDOR_SYS_LINUX_H */

