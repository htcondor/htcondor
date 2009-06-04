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
		if (*s == '\\' || *s == '/') {
			name = s+1;
		}
	}

	return name;
}


/*
  A dirname() function that is happy on both Unix and NT.
  This allocates space for a new string that holds the path of the
  parent directory of the path it was given.  If the given path has no
  directory delimiters, or is NULL, we just return ".".  In all
  cases, the string we return is new space, and must be deallocated
  with free().   Derek Wright 9/23/99
*/
char *
condor_dirname(const char *path)
{
	char *s, *parent;
	char *lastDelim = NULL;

	if( ! path ) {
		return strdup( "." );
	}
	
	parent = strdup( path );
	for (s = parent; s && *s != '\0'; s++) {
		if (*s == '\\' || *s == '/') {
			lastDelim = s;
		}
	}

	if ( lastDelim ) {
		if ( lastDelim != parent ) {
			*lastDelim = '\0';
		} else {
				// Last delimiter is first char of path.
			*(lastDelim+1) = '\0';
		}
		return parent;
	} else {
		free(parent);
		return strdup( "." );
	}
}

/*
  A dirname() function appropriate to URLs that is happy on both Unix
  and NT.  This allocates space for a new string that holds the path
  of the parent directory of the path it was given.   The returned
  directory name ends with the last directory delimiter found in the
  URL.  If the given path has no directory delimiters, or is NULL, we
  just return ".".  In all cases, the string we return is new space,
  and must be deallocated with free().
*/
char *
condor_url_dirname(const char *path)
{
    char *s, *parent;
    char *lastDelim = NULL;

    if( ! path || path[0] == '\0') {
        return strdup( "." );
    }

    parent = strdup( path );
    for (s = parent; s && *s != '\0'; s++) {
        if (*s == '\\' || *s == '/') {
            lastDelim = s;
        }
    }

    if ( lastDelim ) {
        *(lastDelim+1) = '\0';
        return parent;
    } else {
        free(parent);
        return strdup( "." );
    }
}

/*
  DEPRECATED: just in case we need changes along the lines of
  condor_basename() some time in the future.

  A dirname() function that is happy on both Unix and NT. 
  This allocates space for a new string that holds the path of the
  parent directory of the path it was given.  If the given path has no
  directory delimiters, or is NULL, we just return ".".  In all  
  cases, the string we return is new space, and must be deallocated
  with free(). 
*/
#if 0
char*
dirname( const char* path )
{
	return condor_dirname( path );
}
#endif


/* 
   return TRUE if the given path is a full pathname, FALSE if not.  by
   full pathname, we mean it either begins with "/" or "\" or "*:\"
   (something like "c:\..." on windoze).
*/
int
fullpath( const char* path )
{
	if( ! path ) {
		return FALSE;
	}
	if( path[0] == '/' || path[0]=='\\' ) {
		return TRUE;
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
		return TRUE;
	}
	return FALSE;
}
