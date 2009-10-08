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


#if !defined( _CONDOR_BASENAME_H )
#define _CONDOR_BASENAME_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
  NOTE: The semantics of condor_basename() and condor_dirname()
  are slightly different tha the semantics of the default basename()
  and dirname().  For example, if the path is "foo/bar/",
  condor_basename() and condor_dirname() return "" and "foo/bar", while
  the default basename() and dirname() return "bar" and "foo".
  (See test_basename.c for examples of how things work.)
*/

/*
  A basename() function that is happy on both Unix and NT.
  It returns a pointer to the last element of the path it was given,
  or the whole string, if there are no directory delimiters.  There's
  no memory allocated, overwritten or changed in anyway.
*/
const char* condor_basename( const char* path );

/*
  A dirname() function that is happy on both Unix and NT.
  This allocates space for a new string that holds the path of the
  parent directory of the path it was given.  If the given path has no
  directory delimiters, or is NULL, we just return ".".  In all
  cases, the string we return is new space, and must be deallocated
  with free().
*/
char* condor_dirname( const char* path );

/*
  A dirname() function appropriate to URLs that is happy on both Unix
  and NT.  This allocates space for a new string that holds the path
  of the parent directory of the path it was given.   The returned
  directory name ends with the last directory delimiter found in the
  URL.  If the given path has no directory delimiters, or is NULL, we
  just return ".".  In all cases, the string we return is new space,
  and must be deallocated with free().
*/
char* condor_url_dirname( const char* path );

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
char* dirname( const char* path );


/* 
   return TRUE if the given path is a full pathname, FALSE if not.  by
   full pathname, we mean it either begins with "/" or "\" or "*:\"
   (something like "c:\..." on windoze).
*/
int fullpath( const char* path );


#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_BASENAME_H */
