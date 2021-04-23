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

class MyString;
#include <string>

/** Returns a path to subdirectory to use for temporary files.
  @return The pointer returned must be de-allocated by the caller w/ free()
*/
char* temp_dir_path();

/** Take two strings, a directory path, and a filename, and
  concatenate them together.  If the directory path doesn't end with
  the appropriate directory deliminator for this platform or has excess delimiters, this
  function will ensure that only a single, correct directory delimiter is included in the
  resulting full path.
  for example all of the examples below write path/to/file" into buf
      dirscat("path/to", "file", buf)
      dirscat("path/to/", "/file", buf)
      dirscat("path/to//", "file", buf)
  @param dirpath The directory path.
  @param filename The filename.
  @param result  The output buffer
  @return Value() of result parameter after it has been set by this function
*/
const char* dircat(const char* dirpath, const char* filename, std::string &result);

/*
 same as above but filename and fileext are concatenated together to produce the effect filename
*/
const char* dircat(const char* dirpath, const char* filename, const char * fileext, std::string & result);


/** Take two strings, a directory path, and a subdirectory, and
  concatenate them together.  If the directory or subdirectory paths
  don't end with the appropriate directory deliminator for this platform, or have excess delimiters
  this function will ensure that a single, correct directory delimiters are included in the
  resulting full path.  This is just like the above dircat except it also includes a trailing directory delimiter
  for example all of the examples below write path/to/file/" into buf
      dirscat("path/to", "file", buf)
      dirscat("path/to/", "/file", buf)
      dirscat("path/to//", "file//", buf)
  @param dirpath The directory path.
  @param subdir The subdirectory.
  @param result  The output buffer
  @return Value() of result parameter after it has been set by this function
*/
const char* dirscat(const char* dirpath, const char* subdir, std::string &result);

/** Touch a file and create directory path as well if necessary
	@param path: the full path to the file to be touched
	@param file_mode: the mode in which the file should be opened (likely 0644)
	@param directory_mode: the mode in which the directories should be created (likely world RW)
	@param pos: current position in path string
	@return Either the open file descriptor or -1 if file could not be opened
*/
int rec_touch_file(char *path, mode_t file_mode, mode_t directory_mode , int pos = 0); 

/** Clean up a file path recursively up to @depth directories deep.
	@param path: the file path to clean up (likley requires the file under @path to be locked), depth 0 just deletes the file and no directory
	@param depth: how many directories deep should be deleted
	@return 0 upon success, -1 otherwise
*/
int rec_clean_up(char *path, int depth, int pos = -1);

#ifdef WIN32
inline int IS_ANY_DIR_DELIM_CHAR(int ch) { return ch == DIR_DELIM_CHAR || ch == '/'; }
#else
inline int IS_ANY_DIR_DELIM_CHAR(int ch) { return ch == DIR_DELIM_CHAR; }
#endif

#endif
