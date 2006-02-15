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

/* 
   This file contains special stubs from the CEDAR library (ReliSock,
   SafeSock, etc) that are needed to make things link correctly within
   a user job that can't use their normal functionality.
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"


int
ReliSock::get_file_with_permissions( filesize_t*, const char *, bool )
{
	EXCEPT( "ReliSock::get_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}


int
ReliSock::put_file_with_permissions( filesize_t *, const char * )
{
	EXCEPT( "ReliSock::put_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

