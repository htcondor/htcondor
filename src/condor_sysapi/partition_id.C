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
	const int VOLUME_PATH_BUFFER_SIZE = 1024;
	const int RESULT_BUFFER_SIZE = 1024;

	BOOL ret;
	char* volume_path_name;

	// basic idea:
	//   1) call GetVolumePathName to get the mount point for the
	//      the volume containing path
	//   2) call GetVolumeNameForVolumeMountPoint to get the name
	//      of the volume
	//
	volume_path_name = (char*)malloc(VOLUME_PATH_BUFFER_SIZE);
	ASSERT(volume_path_name != NULL);
	ret = GetVolumePathName(path, volume_path_name, VOLUME_PATH_BUFFER_SIZE);
	if (ret == FALSE) {
		dprintf(D_ALWAYS,
		        "sysapi_partition_id: GetVolumePathName error: %u\n",
		        GetLastError());
		free(volume_path_name);
		return 0;
	}
	*result = (char*)malloc(RESULT_BUFFER_SIZE);
	ASSERT(*result != NULL);
	ret = GetVolumeNameForVolumeMountPoint(volume_path_name,
	                                       *result,
	                                       RESULT_BUFFER_SIZE);
	free(volume_path_name);
	if (ret == FALSE) {
		dprintf(D_ALWAYS,
		        "sysapi_partition_id: "
		            "GetVolumeNameForVolumeMountPoint error: %u\n",
		        GetLastError());
		free(*result);
		return 0;
	}

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
