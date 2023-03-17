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
#include "filename_tools.h"
#include "basename.h"

// keep in sync with version in filename_tools_cpp.C
int filename_split( const char *path, char *dir, char *file )
{
	const char *last_slash;

	last_slash = strrchr(path, DIR_DELIM_CHAR);
	if(last_slash) {
		strncpy(dir,path,(last_slash-path));
		dir[(last_slash-path)] = 0;
       		last_slash++;
		strcpy(file,last_slash);
		return 1;
	} else {
		strcpy(file,path);
		strcpy(dir,".");
		return 0;
	}
}

// changes all directory separators to match the DIR_DELIM_CHAR
// makes changes in place
void
canonicalize_dir_delimiters( char *path ) {
	if( !path ) {
		return;
	}
	while ( *path ) {
		if ( *path == '\\' || *path == '/' ) {
		   	*path = DIR_DELIM_CHAR;
		}
		path++;
	}	
}

// Give a possible alternate up path to an executable, perhaps
// fixing up directory delimiters and/or adding default platform-specific
// extentions like .exe on Windows.
// Caller is responsible for calling free() on returned pointer if
// returned pointer is not NULL.
char *
alternate_exec_pathname( const char *path ) 
{
	char *buf = NULL;

#ifdef WIN32
	int len;
	if ( path && path[0] ) 
	{
		len = strlen(path) + 20;
		buf = (char *) malloc(len);
		ASSERT(buf);
		strcpy(buf,path);
		canonicalize_dir_delimiters(buf);
		if (!strchr(condor_basename(buf),'.')) {
			strcat(buf,".exe");
		}
	}

		// return NULL if alternate path is the same as original
	if (buf && path && strcmp(buf,path)==0 ) {
		free(buf);
		buf = NULL;
	}
#else
	(void) path;		// Quiet 'unused parameter' warning
#endif

	return buf;
}

