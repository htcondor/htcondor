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


#include "condor_common.h"


/*
** free_fs_blocks: reports the number of kbytes free for the filesystem where
** given filename resides.
*/

#include "sysapi.h"
#include "sysapi_externs.h"

/* the code starts here */

#if defined(WIN32)

#include <limits.h>

long long
sysapi_disk_space_raw(const char *filename)
{
	ULARGE_INTEGER FreeBytesAvailableToCaller, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	unsigned int t_hi, t_lo, temp;
	const unsigned int lowest_ten_mask = 0x000003ff;
	
	t_hi = t_lo = temp = 0;
	sysapi_internal_reconfig();

	
	if (GetDiskFreeSpaceEx(filename, &FreeBytesAvailableToCaller, &TotalNumberOfBytes, 
		&TotalNumberOfFreeBytes) == 0) {
		return -1;
	} else {
		return FreeBytesAvailableToCaller.QuadPart / 1024;
	}
}

#else

#include "condor_debug.h"

#endif

/*
  How much disk space we need to reserve for the regular file system.
  Answer is in kilobytes.
*/
long long
sysapi_reserve_for_fs()
{
	return _sysapi_reserve_disk;
}


#if defined(__STDC__)
long long sysapi_disk_space_raw( const char *filename);
#else
long long sysapi_disk_space_raw();
#endif

#if defined(LINUX) || defined(Darwin) || defined(CONDOR_FREEBSD)

#include <limits.h>

long long sysapi_disk_space_raw(const char * filename)
{
	struct statfs statfsbuf;
	long long free_kbytes;
	double kbytes_per_block;

	sysapi_internal_reconfig();

	if(statfs(filename, &statfsbuf) < 0) {
		if (errno != EOVERFLOW) {
			dprintf(D_ALWAYS, "sysapi_disk_space_raw: statfs(%s,%p) failed\n",
													filename, &statfsbuf );
			dprintf(D_ALWAYS, "errno = %d\n", errno );

			return(0);
		}

		// if we get here, it means that statfs failed because
		// there are more free blocks than can be represented
		// in a long.  Let's lie and make it INT_MAX - 1.

		dprintf(D_FULLDEBUG, "sysapi_disk_space_raw: statfs overflowed, setting to %d\n", (INT_MAX - 1));
		statfsbuf.f_bavail = (INT_MAX - 1);	
		statfsbuf.f_bsize = 1024; // this isn't set if result < 0, guess
	}

	/* Convert to kbyte blocks: available blks * blksize / 1k bytes. */
	/* overflow problem fixed by using doubles to hold result - 
		Keller 05/17/99 */

	kbytes_per_block = ( (unsigned long)statfsbuf.f_bsize / 1024.0 );

	free_kbytes = (long long)(statfsbuf.f_bavail * kbytes_per_block);

	return free_kbytes;
}
#endif

/*
  Return number of kbytes condor may play with in the named file
  system.  System administrators may reserve space which condor should
  leave alone.
*/
long long
sysapi_disk_space(const char *filename)
{
	long long answer;

	sysapi_internal_reconfig();

	answer =  sysapi_disk_space_raw(filename)
			- sysapi_reserve_for_fs();
	return answer < 0 ? 0 : answer;

}


