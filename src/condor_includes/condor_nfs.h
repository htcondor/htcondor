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


#ifndef _CONDOR_NFS_H
#define _CONDOR_NFS_H

#if defined(AIX)
#include <rpcsvc/mount.h>
#else
#include <sys/mount.h>
#endif

#if defined(LINUX)
#	include <linux/nfs.h>
#	include <linux/ipc.h>
#	include <dirent.h>
    typedef struct fhandle fhandle_t;

#elif !defined(IRIX)

    /* ALL platforms except Linux and IRIX */
#	include <rpc/rpc.h>

#	if defined(Darwin) || defined(CONDOR_FREEBSD)
#       include <nfs/rpcv2.h>
#	endif

#	if defined(CONDOR_FREEBSD) && !defined(CONDOR_FREEBSD4)
        /*
          <nfs/nfs.h> is only found in FreeBSD 4.X, every other
          FreeBSD needs to use this header file, instead.  I am not
          100% that this is the right file, but it seems to work.
    	  Andy - 04.20.2006 
        */
#		include <nfs/nfsproto.h>

#	else    // FreeBSD4, and all non-FreeBSD platforms...

#		include <nfs/nfs.h>

#   endif /* FreeBSDFreeBSD4 */
#endif /* ! defined(LINUX || IRIX) */ 

#if defined(HPUX10) 
#	include <nfs/export.h>
#endif

#endif /* _CONDOR_NFS_H */

