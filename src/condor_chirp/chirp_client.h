
#ifndef CHIRP_CLIENT_H
#define CHIRP_CLIENT_H

struct chirp_client * chirp_client_connect_default();
struct chirp_client * chirp_client_connect( const char *host, int port );
void chirp_client_disconnect( struct chirp_client *c );

int chirp_client_cookie( struct chirp_client *c, const char *cookie );

int chirp_client_open( struct chirp_client *c, const char *path, const char *flags, int mode );
int chirp_client_close( struct chirp_client *c, int fd );
int chirp_client_read( struct chirp_client *c, int fd, char *buffer, int length );
int chirp_client_write( struct chirp_client *c, int fd, const char *buffer, int length );
int chirp_client_unlink( struct chirp_client *c, const char *path );
int chirp_client_rename( struct chirp_client *c, const char *oldpath, const char *newpath );

#endif

