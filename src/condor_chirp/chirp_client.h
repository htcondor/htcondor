/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
/*
Chirp C Client

This public domain software is provided "as is".  See the Chirp License
for details.
*/

#ifndef CHIRP_CLIENT_H
#define CHIRP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
Error codes:

-1   NOT_AUTHENTICATED The client has not authenticated its identity.
-2   NOT_AUTHORIZED    The client is not authorized to perform that action.
-3   DOESNT_EXIST      There is no object by that name.
-4   ALREADY_EXISTS    There is already an object by that name.
-5   TOO_BIG           That request is too big to execute.
-6   NO_SPACE          There is not enough space to store that.
-7   NO_MEMORY         The server is out of memory.
-8   INVALID_REQUEST   The form of the request is invalid.
-9   TOO_MANY_OPEN     There are too many resources in use.
-10  BUSY              That object is in use by someone else.
-11  TRY_AGAIN         A temporary condition prevented the request.
-12  UNKNOWN           An unknown error occurred.
*/

struct chirp_client * chirp_client_connect_default();
/*chirp_client_connect_default()
  Opens connection to the default chirp server.  The default connection
  information is determined by reading ./chirp.config.  Under Condor,
  the starter can automatically create this file if you specify
  +WantIOProxy=True in the submit file.
*/
struct chirp_client * chirp_client_connect( const char *host, int port );
/*chirp_client_connect()
  Connect to a chirp server at the specified address.
 */

struct chirp_client * chirp_client_connect_url( const char *url, const char **path_part);
/*
  chirp_client_connect_url()

  Sets path_part to the position of the start of the path information
  in the URL and connects to the specified Chirp server.

  URL format:
    chirp:host.name:port/path   (absolute path)
    chirp:host.name:port./path  (relative path)
    chirp:/path                 (absolute path)
    chirp:./path                (relative path)
    chirp:path                  (sloppy relative path)

  Note that the initial part of a sloppy relative path can be confused
  for a host:port specification if it happens to look like one.  Example:
  'chirp:strange:1/file'.  For this reason, it is better to use one of
  the non-sloppy formats.

  In all of the above URL formats, any number of extra connection
  parameters may be inserted before the path part (including  the leading
  '/' or '.').  These are of the form ";parameter=value" where parameter
  and value are encoded using the standard Mime type
  application/x-www-form-url encoded, just like the parameters in a typical
  HTTP GET CGI request, but using ';' instead of '&' as the delimiter.

  At this time, no connection parameters are defined, and any that
  are supplied are simply ignored.

*/

void chirp_client_disconnect( struct chirp_client *c );
/*chirp_client_disconnect()
  Closes the connection to a chirp server.
*/

int chirp_client_cookie( struct chirp_client *c, const char *cookie );
/*chirp_client_cookie
  Authenticate with the server using the specified cookie.
 */

int chirp_client_lookup( struct chirp_client *c, const char *logical_name, char **url );
/*chirp_client_lookup()
  Ask the chirp server to translate the filename "logical_name".  This is
  done internally on any filename presented to chirp_client_open().  Under
  Condor, the shadow may translate the path using the same mechanisms
  available to redirect paths in Standard Universe.
*/

int chirp_client_constrain( struct chirp_client *c, const char *expr );
/*chirp_client_constrain()
  When running under Condor, set the job's requirement expression.
*/

int chirp_client_get_job_attr( struct chirp_client *c, const char *name, char **expr );
/*chirp_client_get_job_attr()
  When running under Condor, obtain the value of a job ClassAd attribute.
*/

int chirp_client_set_job_attr( struct chirp_client *c, const char *name, const char *expr );
/*chirp_client_set_job_attr()
  When running under Condor, set the value of a job ClassAd attribute.
*/

int chirp_client_open( struct chirp_client *c, const char *path, const char *flags, int mode );
/*chirp_client_open()
  Open a file through the chirp server.  Note that if you want to create a
  new file, you _must_ include "c" or "x" in the flags.  The mode is the same
  as in the POSIX open() call.

  Flags:
		r - open for reading
		w - open for writing
		a - force all writes to append
		t - truncate before use
		c - create if it doesn't exist
		x - fail if 'c' is given and the file already exists

  Returns an integer file handle to be used in future chirp_client() functions.
  On error, returns a value less than 0.
*/

int chirp_client_close( struct chirp_client *c, int fd );
/*chirp_client_close()
  Closes a file opened via chirp_client_open().
*/

int chirp_client_read( struct chirp_client *c, int fd, void *buffer, int length );
/*chirp_client_read()
  Reads from a file and puts the result in a buffer.  The number of bytes
  actually read is returned.
*/

int chirp_client_write( struct chirp_client *c, int fd, const void *buffer, int length );
/*chirp_client_write()
  Writes from a buffer into a file.  On success, returns the number of
  bytes written, which will always be the number of bytes requested.
  On error, returns a negative error code.
*/

int chirp_client_unlink( struct chirp_client *c, const char *path );
/*chirp_client_unlink()
  Removes a file.
*/

int chirp_client_rename( struct chirp_client *c, const char *oldpath, const char *newpath );
/*chirp_client_rename()
  Renames a file.
*/

int chirp_client_fsync( struct chirp_client *c, int fd );
/*chirp_client_fsync()
  Flushes any buffered data to disk.
*/

int chirp_client_lseek( struct chirp_client *c, int fd, int offset, int whence );
/*chirp_client_lseek()
  Changes the read/write file offset.  The values for whence are the same
  as for the POSIX lseek() call.
*/

int chirp_client_mkdir( struct chirp_client *c, char const *name, int mode );
/*chirp_client_mkdir()
  Create a directory with the specified mode.
*/

int chirp_client_rmdir( struct chirp_client *c, char const *name );
/*chirp_client_rmdir()
  Removes an empty directory.
*/

#ifdef __cplusplus
}
#endif

#endif

