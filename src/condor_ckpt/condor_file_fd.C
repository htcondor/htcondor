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

#include "condor_file_fd.h"

CondorFileFD::CondorFileFD() : CondorFileLocal()
{
}

CondorFileFD::~CondorFileFD()
{
}

int CondorFileFD::open(const char *url_in, int flags, int mode)
{
	strcpy( url, url_in );
	sscanf( url, "fd:%d", &fd );

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	size = -1;

	return fd;
}

int CondorFileFD::close()
{
	return 0;
}
