/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

/* prototype in condor_includes/basename.h */

#include "condor_common.h"


/*
  A basename() function that is happy on both Unix and NT.
  It returns a pointer to the last element of the path it was given,
  or the whole string, if there are no directory delimiters.  There's
  no memory allocated, overwritten or changed in anyway.
*/
char *
basename(const char *path)
{
	char *s, *name;

    /* We need to initialize to make sure we return something real
	   even if the path we're passed in has no directory delimiters. */
	name = (char*)path;	

	for (s = (char*)path; s && *s != '\0'; s++) {
		if (*s == '\\' || *s == '/') {
			name = s+1;
		}
	}
	if ( name == NULL )
		name = (char*)path;
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
dirname(const char *path)
{
	char *s, *parent, *first = NULL, *end = NULL;
	char buf[2];

	if( ! path ) {
		return strdup( "." );
	}
	
	parent = strdup( path );
	for (s = parent; s && *s != '\0'; s++) {
		if (*s == '\\' || *s == '/') {
			if( ! first ) {
				first = s;
			} else if ( *(s+1) == '\0' ) {
					/* We're at the end */
				continue;
			} else {
				end = s;
			}
		}
	}
	if( first && !end ) {
			/* 
			   There's a single directory delimiter at the front, but
			   nothing else, so just return the delimiter (the root).
			   We try to be NT-friendly here by returning what we were
			   given, instead of assuming we want "/".
			*/
		sprintf( buf, "%c", parent[0] );
		free( parent );
		parent = strdup( buf );
		return parent;
	}
	if( first && end ) {
		*end = '\0';
		return parent;
	}
	free( parent );
	return strdup( "." );
}


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
	if( path[0] && path[1] && path[1]==':' && path[2]=='\\' ) {
		return TRUE;
	}
	return FALSE;
}
