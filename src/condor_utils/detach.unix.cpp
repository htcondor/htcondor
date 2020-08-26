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
#include "condor_debug.h"
#include "util_lib_proto.h"

void
detach( void )
{
	int		fd;
	if( (fd=safe_open_wrapper_follow("/dev/tty",O_RDWR,0)) < 0 ) {
			/* There's no /dev/tty, nothing to detach from */
		return;
	}
	if( ioctl(fd,TIOCNOTTY,(char *)0) < 0 ) {
		dprintf( D_ALWAYS, 
				 "ioctl(%d, TIOCNOTTY) to detach from /dev/tty failed, errno: %d\n",
				 fd, errno );
		close(fd);
		return;
	}
	(void)close( fd );
}
