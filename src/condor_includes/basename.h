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

#if !defined( _CONDOR_BASENAME_H )
#define _CONDOR_BASENAME_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
  A basename() function that is happy on both Unix and NT.
  It returns a pointer to the last element of the path it was given,
  or the whole string, if there are no directory delimiters.  There's
  no memory allocated, overwritten or changed in anyway.
*/
char* basename( const char* path );


/*
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
