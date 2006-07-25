/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include <fcntl.h>

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
