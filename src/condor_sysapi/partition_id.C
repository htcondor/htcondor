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

#include "condor_common.h"

#include "sysapi.h"
#include "sysapi_externs.h"

#if defined(WIN32)

int sysapi_partition_id(char const *path,char **result)
{
		// TODO: implement this for windows.  For now, we just
		// return the path as the result.

	*result = (char *)malloc( strlen(path)+1 );
	if( *result == NULL ) {
		return 0;
	}

	strcpy( path, *result );
	return 1;
}

#else /* now the UNIX case */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int sysapi_partition_id(char const *path,char **result)
{
		// Use st_dev from stat() as the unique identifier for the partition
		// containing the specified path.

	struct stat statbuf;
	int rc = stat( path, &statbuf );

	if( rc < 0 ) {
		return 0;
	}

	*result = (char *)malloc( 50 );
	if( *result == NULL ) {
		return 0;
	}

	snprintf( *result, 50, "%ld", (long)statbuf.st_dev);

	return 1;
}

#endif
