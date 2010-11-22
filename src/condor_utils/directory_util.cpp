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
#include "condor_config.h"
#include "directory_util.h"

/*
  Returns a path to subdirectory to use for temporary files.
  The pointer returned must be de-allocated by the caller w/ free().
*/
char*
temp_dir_path()
{
	char *prefix = param("TMP_DIR");
	if  (!prefix) {
		prefix = param("TEMP_DIR");
	}
	if (!prefix) {
#ifndef WIN32
		prefix = strdup("/tmp");
#else
			// try to get the temp dir, then try SPOOL,
			// then use the root directory
		char buf[MAX_PATH];
		int len;
		if ((len = GetTempPath(sizeof(buf), buf)) <= sizeof(buf)) {
			buf[len - 1] = '\0';
			prefix = strdup(buf);
		} else {
			dprintf(D_ALWAYS, "GetTempPath: buffer of size %d too small\n", sizeof(buf));

			prefix = param("SPOOL");
			if (!prefix) {
				prefix = strdup("\\");
			}
		}
#endif
	}
	return prefix;
}

/*
  Concatenates a given directory path and filename into a single
  string, stored in space allocated with new[].  This function makes
  sure that if the given directory path doesn't end with the
  appropriate directory delimiter for this platform, that the new
  string includes that.  Delete return string with delete[].
*/
char*
dircat( const char *dirpath, const char *filename )
{
	ASSERT(dirpath);
	ASSERT(filename);
	bool needs_delim = true;
	int extra = 2, dirlen = strlen(dirpath);
	char* rval;
	if( dirpath[dirlen - 1] == DIR_DELIM_CHAR ) {
		needs_delim = false;
		extra = 1;
	}
	rval = new char[ extra + dirlen + strlen(filename)];
	if( needs_delim ) {
		sprintf( rval, "%s%c%s", dirpath, DIR_DELIM_CHAR, filename );
	} else {
		sprintf( rval, "%s%s", dirpath, filename );
	}
	return rval;
}

/*
  Concatenates a given directory path and subdirectory path into a single
  string, stored in space allocated with new[].  This function makes
  sure that if the given directory path doesn't end with the
  appropriate directory delimiter for this platform, that the new
  string includes that -- it will contain the trailing delimiter too.  
  Delete return string with delete[].
*/
char*
dirscat( const char *dirpath, const char *subdir )
{
	ASSERT(dirpath);
	ASSERT(subdir);
	bool needs_delim1 = true, needs_delim2 = true;
	int extra = 3, dirlen = strlen(dirpath), subdirlen = strlen(subdir);
	char* rval;
	if( dirpath[dirlen - 1] == DIR_DELIM_CHAR ) {
		needs_delim1 = false;
		--extra;
	}
	if (subdir[subdirlen - 1] == DIR_DELIM_CHAR ) {
		--extra;
		needs_delim2 = false;
	}
	rval = new char[ extra + dirlen + strlen(subdir)];
	if( needs_delim1 ) {
		if ( needs_delim2 ) {
			sprintf( rval, "%s%c%s%c", dirpath, DIR_DELIM_CHAR, subdir, DIR_DELIM_CHAR );
		} else {
			sprintf( rval, "%s%c%s", dirpath, DIR_DELIM_CHAR, subdir );
		}
	} else {
		if ( needs_delim2 ) {
			sprintf( rval, "%s%s%c", dirpath, subdir, DIR_DELIM_CHAR );
		} else {
			sprintf( rval, "%s%s", dirpath, subdir );
		}
	}
	return rval;
}
