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

#ifndef CONDOR_SYS_BSD_H
#define CONDOR_SYS_BSD_H

#include <sys/types.h>

#include <unistd.h>
/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/* BSD also declares a dprintf, wonder of wonders */
#   define dprintf _hide_dprintf
#   define getline _hide_getline
#include <stdio.h>
#   undef dprintf
#   undef getline


/* There is no <sys/select.h> on HPUX, select() and friends are 
   defined in <sys/time.h> */
#include <sys/time.h>

/* Need these to get statfs and friends defined */
#include <sys/stat.h>

/* Since both <sys/param.h> and <values.h> define MAXINT without
   checking, and we want to include both, we include them, but undef
   MAXINT inbetween. */
#include <sys/param.h>

#include <sys/fcntl.h>


/* nfs/nfs.h is needed for fhandle_t.
   struct export is required to pacify prototypes
   of type export * before struct export is defined. */


/* mount prototype */
#include <sys/mount.h>

/* for DBL_MAX */
#include <float.h>
/* Darwin does not define a SYS_NMLN, but rather calls it __SYS_NAMELEN */
#ifndef CONDOR_FREEBSD
#define SYS_NMLN  _SYS_NAMELEN
#endif
/****************************************
** Condor-specific system definitions
****************************************/

#define MAXINT				INT_MAX

#endif /* CONDOR_SYS_BSD_H */

