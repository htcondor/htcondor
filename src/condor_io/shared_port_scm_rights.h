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

#ifndef __SHARED_PORT_SCM_RIGHTS_H__
#define __SHARED_PORT_SCM_RIGHTS_H__

// This header defines stuff needed by the code that uses SCM_RIGHTS
// to pass fds.

#include <sys/un.h>

// Systems such as Solaris do not define the following macros.

#ifndef SUN_LEN
#  define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

// Rumor has it that some systems such as 64-bit Solaris require
// 32-bit alignment.  If we run into such a problem, the following
// may need to be adjusted.

#ifndef CMSG_SPACE
#  define CMSG_ALIGN(len, align) (((len) + align - 1) & ~(align - 1))

/* Alignment between cmsghdr and data */
#  define _CMSG_DATA_ALIGNMENT sizeof(size_t)

/* Alignment between data and next cmsghdr */
#  define _CMSG_HDR_ALIGNMENT sizeof(size_t)

#  define CMSG_SPACE(len) \
	(CMSG_ALIGN(sizeof(struct cmsghdr), _CMSG_DATA_ALIGNMENT) + \
	 CMSG_ALIGN(len, _CMSG_HDR_ALIGNMENT))
#  define CMSG_LEN(len) \
	(CMSG_ALIGN(sizeof(struct cmsghdr), _CMSG_DATA_ALIGNMENT) + (len))
#endif

#endif
