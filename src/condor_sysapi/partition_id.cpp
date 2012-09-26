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
#include "stl_string_utils.h"

#include "sysapi.h"
#include "sysapi_externs.h"

#if defined(WIN32)

int sysapi_partition_id_raw(char const *path,char **result)
{
	const int VOLUME_PATH_BUFFER_SIZE = 1024;
	const int RESULT_BUFFER_SIZE = 1024;

	BOOL ret;
	char* volume_path_name;


	sysapi_internal_reconfig();

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

int sysapi_partition_id_raw(char const *path,char **result)
{
	sysapi_internal_reconfig();

		// Use st_dev from stat() as the unique identifier for the partition
		// containing the specified path.

	struct stat statbuf;
	int rc = stat( path, &statbuf );

	if( rc < 0 ) {
		dprintf(D_ALWAYS,"Failed to stat %s: (errno %d) %s\n",
				path, errno, strerror(errno));
		return 0;
	}

	std::string buf;
	formatstr(buf,"%ld",(long)statbuf.st_dev);
	*result = strdup(buf.c_str());
	ASSERT( *result );

	return 1;
}

#endif

int sysapi_partition_id(char const *path,char **result) {
	sysapi_internal_reconfig();
	return sysapi_partition_id_raw(path,result);
}
