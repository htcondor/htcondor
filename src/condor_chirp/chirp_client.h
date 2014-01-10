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

/*
Chirp C Client
*/

#include <sys/types.h>
#include <sys/stat.h>

#ifndef CHIRP_CLIENT_H
#define CHIRP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Dll Export */
#if defined(WIN32) && defined(CHIRP_DLL)
	#define DLLEXPORT __declspec(dllexport)
#else
	#define DLLEXPORT
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
-12  BAD_FD:           The file descriptor requested is invalid.
-13  IS_DIR            A file-only operation was attempted on a directory.
-14  NOT_DIR           A directory operation was attempted on a file.
-15  NOT_EMPTY         A directory cannot be removed because it is not empty.
-16  CROSS_DEVICE_LINK A hard link was attempted across devices.
-17  OFFLINE           The requested resource is temporarily not available.
-127 UNKNOWN           An unknown error occurred.
*/

struct chirp_stat {
	long cst_dev;
	long cst_ino;
	long cst_mode;
	long cst_nlink;
	long cst_uid;
	long cst_gid;
	long cst_rdev;
	long cst_size;
	long cst_blksize;
	long cst_blocks;
	long cst_atime;
	long cst_mtime;
	long cst_ctime;
};

struct chirp_statfs {
	long f_type;
	long f_bsize;
	long f_blocks;
	long f_bfree;
	long f_bavail;
	long f_files;
	long f_ffree;
};

int get_stat( const char *line, struct chirp_stat *info );
int get_statfs( const char *line, struct chirp_statfs *info );

DLLEXPORT struct chirp_client * chirp_client_connect_default(void);
/*chirp_client_connect_default()
  Opens connection to the default chirp server.  The default connection
  information is determined by reading ./.chirp.config.  Under Condor,
  the starter can automatically create this file if you specify
  +WantIOProxy=True in the submit file.
*/

DLLEXPORT struct chirp_client * chirp_client_connect( const char *host, int port );
/*chirp_client_connect()
  Connect to a chirp server at the specified address.
 */

DLLEXPORT struct chirp_client * chirp_client_connect_url( const char *url, const char **path_part);
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

DLLEXPORT void chirp_client_disconnect( struct chirp_client *c );
/*chirp_client_disconnect()
  Closes the connection to a chirp server.
*/

DLLEXPORT int chirp_client_cookie( struct chirp_client *c, const char *cookie );
/*chirp_client_cookie
  Authenticate with the server using the specified cookie.
 */

DLLEXPORT int chirp_client_login( struct chirp_client *c, const char *name, const char *password );
/*chirp_client_login
 */

DLLEXPORT int chirp_client_lookup( struct chirp_client *c, const char *logical_name, char **url );
/*chirp_client_lookup()
  Ask the chirp server to translate the filename "logical_name".  This is
  done internally on any filename presented to chirp_client_open().  Under
  Condor, the shadow may translate the path using the same mechanisms
  available to redirect paths in Standard Universe.
*/

DLLEXPORT int chirp_client_constrain( struct chirp_client *c, const char *expr );
/*chirp_client_constrain()
  When running under Condor, set the job's requirement expression.
*/

DLLEXPORT int chirp_client_get_job_attr( struct chirp_client *c, const char *name, char **expr );
/*chirp_client_get_job_attr()
  When running under Condor, obtain the value of a job ClassAd attribute.
*/

DLLEXPORT int chirp_client_get_job_attr_delayed( struct chirp_client *c, const char *name, char **expr );
/*chirp_client_get_job_attr_delayed()
 When running under Condor, obtain the value of a job ClassAd attribute from the local starter.
 This may differ from the value in the schedd!
*/

DLLEXPORT int chirp_client_set_job_attr( struct chirp_client *c, const char *name, const char *expr );
/*chirp_client_set_job_attr()
  When running under Condor, set the value of a job ClassAd attribute.
*/

DLLEXPORT int chirp_client_set_job_attr_delayed( struct chirp_client *c, const char *name, const char *expr );
/*chirp_client_set_job_attr_delayed()
  When running under HTCondor, set the value of a job ClassAd attribute to a given expression.
  This variant of set_job_attr will not push the update immediately, but rather as a non-durable
  update during the next communication between starter and shadow.
*/

DLLEXPORT int chirp_client_open( struct chirp_client *c, const char *path, const char *flags, int mode );
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

DLLEXPORT int chirp_client_close( struct chirp_client *c, int fd );
/*chirp_client_close()
  Closes a file opened via chirp_client_open().
*/

DLLEXPORT int chirp_client_read( struct chirp_client *c, int fd, void *buffer, int length );
/*chirp_client_read()
  Reads from a file and puts the result in a buffer.  The number of bytes
  actually read is returned.
*/

DLLEXPORT int chirp_client_write( struct chirp_client *c, int fd, const void *buffer, int length );
/*chirp_client_write()
  Writes from a buffer into a file.  On success, returns the number of
  bytes written, which will always be the number of bytes requested.
  On error, returns a negative error code.
*/

DLLEXPORT int chirp_client_unlink( struct chirp_client *c, const char *path );
/*chirp_client_unlink()
  Removes a file.
*/

DLLEXPORT int chirp_client_rename( struct chirp_client *c, const char *oldpath, const char *newpath );
/*chirp_client_rename()
  Renames a file.
*/

DLLEXPORT int chirp_client_fsync( struct chirp_client *c, int fd );
/*chirp_client_fsync()
  Flushes any buffered data to disk.
*/

DLLEXPORT int chirp_client_lseek( struct chirp_client *c, int fd, int offset, int whence );
/*chirp_client_lseek()
  Changes the read/write file offset.  The values for whence are the same
  as for the POSIX lseek() call.
*/

DLLEXPORT int chirp_client_mkdir( struct chirp_client *c, char const *name, int mode );
/*chirp_client_mkdir()
  Create a directory with the specified mode.
*/

DLLEXPORT int chirp_client_rmdir( struct chirp_client *c, char const *name );
/*chirp_client_rmdir()
  Removes an empty directory.
*/

DLLEXPORT int chirp_client_ulog( struct chirp_client *c, char const *message );
/*chirp_client_ulog()
  logs a generic string to the user log
*/

DLLEXPORT int chirp_client_pread( struct chirp_client *c, int fd, void *buffer,
	int length, int offset);
/*chirp_client_pread
  Reads from a file starting at the offset and puts the result in a buffer.
  The number of bytes actually read is returned, or -1 on error.
*/

DLLEXPORT int chirp_client_pwrite( struct chirp_client *c, int fd, 
	const void *buffer,	int length, int offset );
/*chirp_client_pwrite
  Writes from a buffer into a file at the given offset.  The number of bytes
  actually written is returned, or -1 on error.
*/

DLLEXPORT int chirp_client_sread( struct chirp_client *c, int fd, void *buffer,
	int length, int offset, int stride_length, int stride_skip );
/*chirp_client_sread
  Reads from a file starting at the offset, reading stride_length bytes every
  stride_skip bytes. Puts the result in a buffer. The number of bytes actually
  read is returned, or -1 on error.
*/


DLLEXPORT int chirp_client_swrite( struct chirp_client *c, int fd, 
	const void *buffer,	int length, int offset, int stride_length, 
	int stride_skip );
/*chirp_client_swrite
  Writes from a buffer into a file starting at the given offset.  Writes 
  stride_length bytes every stride_skip bytes. The total number of bytes
  written is returned, or -1 on error.
*/

DLLEXPORT int chirp_client_rmall( struct chirp_client *c, const char *path );
/*chirp_client_rmall
  Recursively deletes a directory. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_fstat( struct chirp_client *c, int fd,
	struct chirp_stat *buf );
/*chirp_client_fstat
  Gets metadata. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_fstatfs( struct chirp_client *c, int fd,
	struct chirp_statfs *buf);
/*chirp_client_fstatfs
  Gets filesystem metadata. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_fchown( struct chirp_client *c, int fd, int uid,
	int gid );
/*chirp_client_fchown
  Changes ownership of a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_fchmod( struct chirp_client *c, int fd,
	int mode );
/*chirp_client_fchmod
  Changes mode of a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_ftruncate( struct chirp_client *c, int fd,
	int length );
/*chirp_client_ftruncate
  Truncates a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_getfile_buffer( struct chirp_client *c, 
	const char *name, char **buffer );
/*chirp_client_getfile
  Retrieves an entire file. Returns number of bytes in the file, or -1 on error.
*/

DLLEXPORT int chirp_client_putfile_buffer( struct chirp_client *c,
	const char *path, const char *buffer, int mode, int length );
/*chirp_client_putfile_buffer
  Stores an entire file. Returns size of written file, or -1 on failure.
*/

DLLEXPORT int chirp_client_getlongdir( struct chirp_client *c, const char *path,
	char **buffer );
/*chirp_client_getlongdir
  Gets directory contents and all metadata. Returns size of reply, or -1 on
  error.
*/

DLLEXPORT int chirp_client_getdir( struct chirp_client *c, const char *path,
	char **buffer );
/*chirp_client_getdir
  Gets directory contents. Returns size of reply, or -1 on error.
*/

DLLEXPORT int chirp_client_whoami( struct chirp_client *c, char *buf,
	int length);
/*chirp_client_whoami
  Get user's identity. Returns size of reply, -1 on failure.
*/

DLLEXPORT int chirp_client_whoareyou( struct chirp_client *c, const char *rhost,
	char *buf, int length );
/*chirp_client_whoareyou
  Get server's identity with respect to host. Returns 1 on success, -1 on
  failure.
*/

DLLEXPORT int chirp_client_link( struct chirp_client *c, const char *path,
	const char *newpath );
/*chirp_client_link
  Creates a hard link. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_symlink( struct chirp_client *c, const char *path,
	const char *newpath );
/*chirp_client_symlink
  Creates a symbolic link. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_readlink( struct chirp_client *c, const char *path,
	int length, char **buf );
/*chirp_client_readlink
  Reads up to length bytes of a symbolic link. Returns number of byes read, or 
  -1 on error.
*/

DLLEXPORT int chirp_client_stat( struct chirp_client *c, const char *path,
	struct chirp_stat *buf );
/*chirp_client_stat
  Get metadata. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_lstat( struct chirp_client *c, const char *path,
	struct chirp_stat *buf );
/*chirp_client_lstat
  Get metadata. For symbolic links, examine the link itself. Returns 0 on success,
  -1 on failure.
*/

DLLEXPORT int chirp_client_statfs( struct chirp_client *c, const char *path,
	struct chirp_statfs *buf );
/*chirp_client_statfs
  Gets filesystem metadata. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_access( struct chirp_client *c, const char *path,
	int mode );
/*chirp_client_access
  Checks access permissions. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_chmod( struct chirp_client *c, const char *path,
	int mode );
/*chirp_client_chmod
  Changes mode of a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_chown( struct chirp_client *c, const char *path,
	int uid, int gid );
/*chirp_client_chown
  Changes the ownership of a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_lchown( struct chirp_client *c, const char *path,
	int uid, int gid );
/*chirp_client_lchown
  Changes the ownership of a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_truncate( struct chirp_client *c, const char *path,
	int length );
/*chirp_client_truncate
  Truncates a file. Returns 0 on success, -1 on failure.
*/

DLLEXPORT int chirp_client_utime( struct chirp_client *c, const char *path,
	int actime, int modtime );
/*chirp_client_utime
  Changes the access and modification times of a file. Returns 0 on success,
  -1 on failure.
*/

#ifdef __cplusplus
}
#endif

#endif

