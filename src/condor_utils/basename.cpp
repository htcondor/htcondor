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


/* prototype in condor_includes/basename.h */

#include "condor_common.h"
#include "basename.h"

/*
  A basename() function that is happy on both Unix and NT.
  It returns a pointer to the last element of the path it was given,
  or the whole string, if there are no directory delimiters.  There's
  no memory allocated, overwritten or changed in anyway.
*/
const char *
condor_basename(const char *path)
{
	const char *s, *name;

	if( ! path ) {
		return "";
	}

    /* We need to initialize to make sure we return something real
	   even if the path we're passed in has no directory delimiters. */
	name = path;

	for (s = path; s && *s != '\0'; s++) {
		if (
#ifdef WIN32
			*s == '\\' ||
#endif
			*s == '/') {
				name = s+1;
		}
	}

	return name;
}

/*
  given a pointer to a file basename (see condor_basename) returns
  a pointer to the file extension, if no extension returns a pointer
  to the terminating null.
*/
const char*
condor_basename_extension_ptr(const char* filename) {
	if ( ! filename) return filename;
	const char *pend = filename + strlen(filename);
	const char *p = pend;
	while (p > filename) {
		if (*p == '.') return p;
		--p;
	}
	return pend;
}

/*
  A dirname() function that is happy on both Unix and NT.  If the given path
  has no directory delimiters, or is NULL, we just return ".".  
  Derek Wright 9/23/99
*/
std::string 
condor_dirname(const char *path) { 
	
	if (!path) { 
		return ".";
	}

	const char *lastDelim = nullptr;

	for (const char *s = path; *s != '\0'; s++) {
		if (*s == '\\' || *s == '/') {
			lastDelim = s;
		}
	}

	if ( lastDelim ) {
		if ( lastDelim != path) {
			return {path, lastDelim};
		} else {
			return {path, lastDelim + 1};
		}
	} else {
		return ".";
	}
}

/*
   return TRUE if the given path is a full pathname, FALSE if not.  by
   full pathname, we mean it either begins with "/" or "\" or "*:\"
   (something like "c:\..." on windoze).
*/
bool
fullpath( const char* path )
{
	if( ! path ) {
		return false;
	}
	if( path[0] == '/' || path[0]=='\\' ) {
		return true;
	}
		/*
		  for this next check, we should be careful not to walk off
		  the end of the string.  short circuit evaluation will save
		  us if we hit the NULL character.  if it's only 3 bytes long,
		  we the 3rd byte will be NULL, which we can still safely
		  compare against, it's just the thing *after* NULL we don't
		  want to touch. :)
		*/
		// "?:/" counts as a full path, since Windows allows forward
		// slashes -- wenger 2006-01-13.
	if( path[0] && path[1] && path[1]==':' &&
			(path[2]=='\\' || path[2]=='/') ) {
		return true;
	}
	return false;
}
