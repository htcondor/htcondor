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

struct chirp_client * chirp_client_connect_default();
struct chirp_client * chirp_client_connect( const char *host, int port );
struct chirp_client * chirp_client_connect_url( const char *url, const char **path_part);

void chirp_client_disconnect( struct chirp_client *c );

int chirp_client_cookie( struct chirp_client *c, const char *cookie );

int chirp_client_lookup( struct chirp_client *c, const char *logical_name, char **url );
int chirp_client_constrain( struct chirp_client *c, const char *expr );
int chirp_client_get_job_attr( struct chirp_client *c, const char *name, char **expr );
int chirp_client_set_job_attr( struct chirp_client *c, const char *name, const char *expr );
int chirp_client_open( struct chirp_client *c, const char *path, const char *flags, int mode );
int chirp_client_close( struct chirp_client *c, int fd );
int chirp_client_read( struct chirp_client *c, int fd, void *buffer, int length );
int chirp_client_write( struct chirp_client *c, int fd, const void *buffer, int length );
int chirp_client_unlink( struct chirp_client *c, const char *path );
int chirp_client_rename( struct chirp_client *c, const char *oldpath, const char *newpath );
int chirp_client_fsync( struct chirp_client *c, int fd );
int chirp_client_lseek( struct chirp_client *c, int fd, int offset, int whence );
int chirp_client_mkdir( struct chirp_client *c, char const *name, int mode );
int chirp_client_rmdir( struct chirp_client *c, char const *name );

#ifdef __cplusplus
}
#endif

#endif

