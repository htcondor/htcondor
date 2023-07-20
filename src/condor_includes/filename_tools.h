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


#ifndef FILENAME_TOOLS_H
#define FILENAME_TOOLS_H

#include "condor_common.h"

/**
Take any pathname -- simple, relative, or complete -- and split it
into a directory and a file name.   Always succeeds, but return
code gives some information about the path actually present.
Returns true if there was a directory component to split off.
Returns false if there was no directory component, but in this case,
still fills "dir" with "."
The buffers dir and file must be as big as path.  They are not allocated
by this function.
*/

int filename_split( const char *path, char *dir, char *file );


void canonicalize_dir_delimiters( char *path );

char *alternate_exec_pathname( const char *path );

#include <string>

int filename_split( const char *path, std::string &dir, std::string &file );

int is_relative_to_cwd( std::string &path );

/** 
Take an input string which looks like this:
"filename = url ; filename = url ; ..."
Search for a filename matching the input string, and copy the 
corresponding url into output.
<p>
The delimiting characters = and ; may be escaped with a backslash.
<p>
Currently, this data is represented as a string attribute within
a ClassAd.  A much better implementation would be to store it
as a ClassAd within a ClassAd.  However, this will have to wait until
new ClassAds are deployed.
*/
int filename_remap_find( const char *input, const char *filename, std::string &output, int cur_nesting_level = 0);

void canonicalize_dir_delimiters( std::string &path );

#endif
