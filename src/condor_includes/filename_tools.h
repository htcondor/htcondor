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

#ifndef FILENAME_TOOLS_H
#define FILENAME_TOOLS_H

#include "condor_common.h"

BEGIN_C_DECLS

/**
Take any pathname -- simple, relative, or complete -- and split it
into a directory and a file name.   Always succeeds, but return
code gives some information about the path actually present.
Returns true if there was a directory component to split off.
Returns false if there was no directory component, but in this case,
still fills "dir" with "."
*/

int filename_split( const char *path, char *dir, char *file );

/**
Take an input string in URL form, and split it into its components.
URLs are of the form "method://server:port/filename".  Any component
that is missing will be recorded as the string with one null character.
If the port is not specified, it is recorded as -1.
<p>
The outputs method, server, and path must all point to buffers of length
_POSIX_PATH_MAX.  This function will fill in that many characters and
ignore any remaining data in those input fields.  Use of _POSIX_PATH_MAX
as an upper bound is perhaps overkill, but it is a well-defined constant
that we use in many other places.
*/

void filename_url_parse( char *input, char *method, char *server, int *port, char *path );

/** 
Take an input string which looks like this:
"filename = url ; filename = url ; ..."
Search for a filename matching the input string, and copy the 
corresponding url into output.  The url can then be parsed by
filename_url_parse().
<p>
The filename argument may be up to _POSIX_PATH_MAX, and output must
point to a buffer of _POSIX_PATH_MAX*3.
<p>
The delimiting characters = and ; may be escaped with a backslash.
<p>
Currently, this data is represented as a string attribute within
a ClassAd.  A much better implementation would be to store it
as a ClassAd within a ClassAd.  However, this will have to wait until
new ClassAds are deployed.
*/

int filename_remap_find( const char *input, const char *filename, char *output );

void canonicalize_dir_delimiters( char *path );

/*
Returns true if path is a relative path; false if it is not.

Example: is_relative_to_cwd("some/path/to/file") is true
         is_relative_to_cwd("/some/path/to/file") is false
         is_relative_to_cwd("C:\path\to\my\file") is false

Just because a file is not relative does NOT mean it is in any sort of
absolute "canonical" format.  It may still contain references to ".." or to
symlinks or whatever.

*/
int is_relative_to_cwd( const char *path );

END_C_DECLS

#endif
