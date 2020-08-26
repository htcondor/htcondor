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

#ifndef _CONDOR_GETMNT_H
#define _CONDOR_GETMNT_H

#if !defined(WIN32) && !defined(Darwin) && !defined(CONDOR_FREEBSD)
#	include <mntent.h>
#endif

#if !defined(NMOUNT)
#define NMOUNT 256
#endif

/* These are structs used by the Ultrix-specific getmnt() call, which
 * we simulate on other platforms.
 */
struct fs_data_req {
	dev_t	dev;
	char	*devname;
	char	*path;
};
struct fs_data {
	struct fs_data_req fd_req;
};
#define NOSTAT_MANY 0

#if NMOUNT < 256
#undef  NMOUNT
#define NMOUNT  256
#endif

#endif /* _CONDOR_GETMNT_H */
