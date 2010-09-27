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

#ifndef DIRECTORY_UTIL_H
#define DIRECTORY_UTIL_H


/** Returns a path to subdirectory to use for temporary files.
  @return The pointer returned must be de-allocated by the caller w/ free()
*/
char* temp_dir_path();

/** Take two strings, a directory path, and a filename, and
  concatenate them together.  If the directory path doesn't end with
  the appropriate directory deliminator for this platform, this
  function will ensure that the directory delimiter is included in the
  resulting full path.
  @param dirpath The directory path.
  @param filename The filename.
  @return A string created with new() that contains the full pathname.
*/
char* dircat( const char* dirpath, const char* filename );

/** Take two strings, a directory path, and a subdirectory, and
  concatenate them together.  If the directory or subdirectory paths 
  don't end with
  the appropriate directory deliminator for this platform, this
  function will ensure that the directory delimiters are included in the
  resulting full path. 
  @param dirpath The directory path.
  @param subdir The subdirectory.
  @return A string created with new() that contains the full pathname.
*/
char* dirscat( const char* dirpath, const char* subdir );

#endif
