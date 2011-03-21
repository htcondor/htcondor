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

#include <fcntl.h>
#include "utilfns.h"

int
fcntl_cmd_encode(int cmd)
{
	switch(cmd) {
	case F_DUPFD:	return 0;
	case F_GETFD:	return 1;
	case F_SETFD:	return 2;
	case F_GETFL:	return 3;
	case F_SETFL:	return 4;
	case F_GETLK:	return 5;
	case F_SETLK:	return 6;
	case F_SETLKW:	return 7;
	default:		return cmd;
	}
}

int
fcntl_cmd_decode(int cmd)
{
	switch(cmd) {
	case 0:		return F_DUPFD;
	case 1:		return F_GETFD;
	case 2:		return F_SETFD;
	case 3:		return F_GETFL;
	case 4:		return F_SETFL;
	case 5:		return F_GETLK;
	case 6:		return F_SETLK;
	case 7:		return F_SETLKW;
	default:	return cmd;
	}
}
