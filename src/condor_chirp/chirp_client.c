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

#include "chirp_protocol.h"
#include "chirp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* Sockets */
#if defined(WIN32)
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
#else
	#define SOCKET int
	#define SOCKET_ERROR   (-1)
	#define INVALID_SOCKET (-1)
	#define closesocket close
	#include <unistd.h>
	#include <sys/errno.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
#endif

static SOCKET tcp_connect( const char *host, int port );
static void chirp_fatal_request( const char *name );
static void chirp_fatal_response(void);
static int get_result( FILE *s );
static int convert_result( int response );
static int simple_command(struct chirp_client *c,char const *fmt,...);
static void vsprintf_chirp(char *command,char const *fmt,va_list args);
static char const *read_url_param(char const *url,char *buffer,size_t length);

static int sockets_initialized = 0;
static int initialize_sockets(void);
//static int shutdown_sockets();


struct chirp_client {
	FILE *rstream;
	FILE *wstream;
};

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


static int
initialize_sockets()
{
#if defined(WIN32)
	int err;
	WORD wVersionRequested;
	WSADATA wsaData;
#endif

	if(sockets_initialized)
		return 1;

#if defined(WIN32)
	wVersionRequested = MAKEWORD( 2, 0 );
	err = WSAStartup( wVersionRequested, &wsaData );
	if(err)
		return 0;

	if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) 
	{
		WSACleanup();
		return 0;
	}
#endif

	sockets_initialized = 1;
	return 1;
}

DLLEXPORT struct chirp_client * 
chirp_client_connect_url( const char *url, const char **path_part)
{
	struct chirp_client *client;
	char const *str;
	char *host = NULL;
	int port = 0;

	if(strncmp(url,"chirp:",6)) {
		//bare file name
		*path_part = url;
		return chirp_client_connect_default();
	}

	url += 6; // "chirp:"

	if(*url != '/' && *url != '\\' && *url != ';' && *url != '.' \
	   && (str = strchr(url,':')))
	{
		char *end;
		port = strtol(str+1,&end,10);
		if(port && end > str+1 && 
		   (*end == '\0' || *end == '/' || *end == '\\' ||
		    *end == '.' || *end == ';'))
		{
			//URL form chirp:host.name:port...
			//Note that we try to avoid getting here on a "sloppy"
			//relative path that happens to contain a ':' but
			//which is not followed by a valid port/path.

			host = (char *)malloc(str-url+1);
			if ( ! host) {
				errno = ENOMEM;
				return NULL;
			}
			strncpy(host,url,str-url);
			host[str-url] = '\0';

			url = end;
		}
	}

	while(*url == ';') { //parse connection parameters
		char param[CHIRP_LINE_MAX];
		char value[CHIRP_LINE_MAX];

		url = read_url_param(++url,param,sizeof(param));
		if(!url) {
			errno = EINVAL;
			if (host) free(host);
			return NULL;
		}

		if(*url == '=') {
			url = read_url_param(++url,value,sizeof(value));
			if(!url) {
				errno = EINVAL;
				if (host) free(host);
				return NULL;
			}
		}
		else *value = '\0';

		//No connection parameters are defined at this time!
		//Handle them here when they are defined.
	}

	*path_part = url;

	if(!host) { //URL must be in form 'chirp:path'
		client = chirp_client_connect_default();
	}
	else {
		client = chirp_client_connect(host,port);
	}

	free(host);
	return client;
}

DLLEXPORT struct chirp_client *
chirp_client_connect_default()
{
	FILE *file;
	int fields;
	int save_errno;
	struct chirp_client *client;
	char *default_filename;
	char host[CHIRP_LINE_MAX];
	char cookie[CHIRP_LINE_MAX];
	int port;
	int result;

	if (!(default_filename = getenv("_CONDOR_CHIRP_CONFIG"))) {
		default_filename = ".chirp.config";
	}

	file = fopen(default_filename,"r");
	if(!file) return 0;

	fields = fscanf(file,"%s %d %s",host,&port,cookie);
	fclose(file);

	if(fields!=3) {
		errno = EINVAL;
		return 0;
	}

	client = chirp_client_connect(host,port);
	if(!client) return 0;

	result = chirp_client_cookie(client,cookie);
	if(result!=0) {
		save_errno = errno;
		chirp_client_disconnect(client);
		errno = save_errno;
		return 0;
	}

	return client;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // the seek, read, open, close, fileno, etc are deprecated, use _seek, etc instead.
#endif

DLLEXPORT struct chirp_client *
chirp_client_connect( const char *host, int port )
{
	struct chirp_client *c;
	int save_errno;
	SOCKET fd;
	int osfhr;
	int osfhw;

	c = (struct chirp_client*)malloc(sizeof(*c));
	if(!c) return 0;

	fd = tcp_connect(host,port);
	if(fd == INVALID_SOCKET) {
		save_errno = errno;
		free(c);
		errno = save_errno;
		return 0;
	}


#if defined(WIN32)
	// This allows WinNT to get a file handle from a socket
	// Note: this will not work on win95/98
	osfhr = _open_osfhandle(fd, _O_RDWR | _O_BINARY);
	if(osfhr < 0) {
	    closesocket(fd);
	    return 0;
	}
	osfhw = _dup(osfhr);
#else
	osfhr = fd;
	osfhw = dup(osfhr);
#endif


	c->rstream = fdopen(osfhr,"r");
	if(!c->rstream) {
		save_errno = errno;
		closesocket(fd);
		free(c);
		errno = save_errno;
		return 0;
	}
	setbuf(c->rstream, NULL);
	//setvbuf( c->rstream, NULL, _IONBF, 0 );

	c->wstream = fdopen(osfhw,"w");
	if(!c->wstream) {
		save_errno = errno;
		fclose(c->rstream);
		closesocket(fd);
		free(c);
		errno = save_errno;
		return 0;
	}

	setbuf(c->wstream, NULL);
	//setvbuf( c->rstream, NULL, _IONBF, 0 );

	return c;
}

#ifdef _MSC_VER 
#pragma warning(pop) // the seek, read, open, close, fileno, etc are deprecated, use _seek, etc instead.
#endif

DLLEXPORT void
chirp_client_disconnect( struct chirp_client *c )
{
	fclose(c->rstream);
	fclose(c->wstream);
	c->rstream = c->wstream = NULL;
	free(c);
}

DLLEXPORT int
chirp_client_cookie( struct chirp_client *c, const char *cookie )
{
	return simple_command(c,"cookie %s\n",cookie);
}

DLLEXPORT int
chirp_client_login( struct chirp_client *c, const char *name, const char *password )
{
	return simple_command(c,"login %s %s\n",name,password);
}

DLLEXPORT int
chirp_client_lookup( struct chirp_client *c, const char *logical_name, char **url )
{
	int result;
	int actual;

	result = simple_command(c,"lookup %s\n",logical_name);
	if(result>0) {
		*url = (char*)malloc(result);
		if(*url) {
			actual = (int)fread(*url,1,result,c->rstream);
			if(actual!=result) chirp_fatal_request("lookup");
		} else {
			chirp_fatal_request("lookup");
		}
	}

	return result;
}

DLLEXPORT int 
chirp_client_constrain( struct chirp_client *c, const char *expr)
{
	return simple_command(c,"constrain %s\n",expr);
}

DLLEXPORT int
chirp_client_get_job_attr( struct chirp_client *c, const char *name, char **expr )
{
	int result;
	int actual;

	result = simple_command(c,"get_job_attr %s\n",name);
	if(result>0) {
		*expr = (char*)malloc(result);
		if(*expr) {
			actual = (int)fread(*expr,1,result,c->rstream);
			if(actual!=result) chirp_fatal_request("get_job_attr");
		} else {
			chirp_fatal_request("get_job_attr");
		}
	}

	return result;
}

DLLEXPORT int
chirp_client_get_job_attr_delayed( struct chirp_client *c, const char *name, char **expr )
{
	int result;
	int actual;

	result = simple_command(c,"get_job_attr_delayed %s\n",name);
	if(result>0) {
		*expr = (char*)malloc(result);
		if(*expr) {
			actual = (int)fread(*expr,1,result,c->rstream);
			if(actual!=result) chirp_fatal_request("get_job_attr");
		} else {
			chirp_fatal_request("get_job_attr");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_set_job_attr( struct chirp_client *c, const char *name, const char *expr )
{
	return simple_command(c,"set_job_attr %s %s\n",name,expr);
}

DLLEXPORT int
chirp_client_set_job_attr_delayed( struct chirp_client *c, const char *name, const char *expr )
{
	return simple_command(c,"set_job_attr_delayed %s %s\n",name,expr);
}

DLLEXPORT int
chirp_client_open( struct chirp_client *c, const char *path, const char *flags, int mode )
{
	int result = -1;

	result = simple_command(c,"open %s %s %d\n",path,flags,mode);
	if(result >= 0) {
		struct chirp_stat buf;
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_stat(line, &buf) != 0) {
			chirp_fatal_request("get_stat");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_close( struct chirp_client *c, int fd )
{
	return simple_command(c,"close %d\n",fd);
}

DLLEXPORT int
chirp_client_read( struct chirp_client *c, int fd, void *buffer, int length )
{
	int result;
	int actual;

	result = simple_command(c,"read %d %d\n",fd,length);

	if( result>0 ) {
		actual = (int)fread(buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("read");
	}

	return result;
}

DLLEXPORT int
chirp_client_write( struct chirp_client *c, int fd, const void *buffer, int length )
{
	int actual;
	int result;

	char command[CHIRP_LINE_MAX];
	sprintf(command, "write %d %d\n", fd, length);

#ifdef DEBUG_CHIRP
	printf("chirp sending: %s", command);
#endif
	
	result = fputs(command, c->wstream);
	if(result < 0) chirp_fatal_request("write");
	
	result = fflush(c->wstream);
	if(result < 0) chirp_fatal_request("write");

	actual = (int)fwrite(buffer,1,length,c->wstream);
	if(actual!=length) chirp_fatal_request("write");

	return convert_result(get_result(c->rstream));
}

DLLEXPORT int
chirp_client_unlink( struct chirp_client *c, const char *path )
{
	return simple_command(c,"unlink %s\n",path);
}

DLLEXPORT int
chirp_client_rename( struct chirp_client *c, const char *oldpath, const char *newpath )
{
	return simple_command(c,"rename %s %s\n",oldpath,newpath);
}
DLLEXPORT int
chirp_client_fsync( struct chirp_client *c, int fd )
{
	return simple_command(c,"fsync %d\n",fd);
}

DLLEXPORT int
chirp_client_lseek( struct chirp_client *c, int fd, int offset, int whence )
{
	return simple_command(c,"lseek %d %d %d\n",fd,offset,whence);
}

DLLEXPORT int
chirp_client_mkdir( struct chirp_client *c, char const *name, int mode )
{
	return simple_command(c,"mkdir %s %d\n",name,mode);
}

DLLEXPORT int
chirp_client_rmdir( struct chirp_client *c, char const *name )
{
	return simple_command(c,"rmdir %s\n",name);
}

DLLEXPORT int
chirp_client_ulog( struct chirp_client *c, char const *message )
{
	return simple_command(c,"ulog %s\n",message);
}

DLLEXPORT int
chirp_client_pread( struct chirp_client *c, int fd, void *buffer, int length,
				   int offset )
{
	int result;
	int actual;
	
	result = simple_command(c,"pread %d %d %d\n",fd, length, offset);

	if(result > 0) {
		actual = (int)fread(buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("pread");
	}

	return result;
}

DLLEXPORT int
chirp_client_pwrite( struct chirp_client *c, int fd, const void *buffer,
					int length, int offset )
{
	int actual;
	int result;
	char command[CHIRP_LINE_MAX];
	sprintf(command, "pwrite %d %d %d\n", fd, length, offset);

#ifdef DEBUG_CHIRP
	printf("chirp sending: %s", command);
#endif
	
	result = fputs(command, c->wstream);
	if(result < 0) chirp_fatal_request("pwrite");

	result = fflush(c->wstream);
	if(result < 0) chirp_fatal_request("pwrite");

	actual = (int)fwrite(buffer,1,length,c->wstream);
	if(actual != length) chirp_fatal_request("pwrite");

	return convert_result(get_result(c->rstream));
}

DLLEXPORT int
chirp_client_sread( struct chirp_client *c, int fd, void *buffer, int length,
				   int offset, int stride_length, int stride_skip )
{
	int result;
	int actual;
	
	result = simple_command(c,"sread %d %d %d %d %d\n",fd, length, offset,
		stride_length, stride_skip);

	if(result > 0) {
		actual = (int)fread(buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("sread");
	}

	return result;
}

DLLEXPORT int
chirp_client_swrite( struct chirp_client *c, int fd, const void *buffer,
					int length, int offset, int stride_length, int stride_skip )
{
	int actual;
	int result;

	char command[CHIRP_LINE_MAX];
	sprintf(command, "swrite %d %d %d %d %d\n", fd, length, offset, 
			stride_length, stride_skip);

#ifdef DEBUG_CHIRP
	printf("chirp sending: %s", command);
#endif
	
	result = fputs(command, c->wstream);
	if(result < 0) chirp_fatal_request("swrite");

	result = fflush(c->wstream);
	if(result < 0) chirp_fatal_request("swrite");

	actual = (int)fwrite(buffer,1,length,c->wstream);
	if(actual != length) chirp_fatal_request("swrite");

	return convert_result(get_result(c->rstream));
}

DLLEXPORT int
chirp_client_rmall( struct chirp_client *c, const char *path ) {
	return simple_command(c,"rmall %s\n",path);
}

DLLEXPORT int
chirp_client_fstat( struct chirp_client *c, int fd,	struct chirp_stat *buf ) {

	int result;

	result = simple_command(c,"fstat %d\n",fd);
	if( result == 0 ) {
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_stat(line, buf) == -1) {
			chirp_fatal_request("get_stat");
		}
	}
	return result;
}

DLLEXPORT int
chirp_client_fstatfs( struct chirp_client *c, int fd,
					 struct chirp_statfs *buf)
{
	int result;

	result = simple_command(c,"fstatfs %d\n",fd);
	if(result == 0) {
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_statfs(line, buf) == -1) {
			chirp_fatal_request("get_statfs");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_fchown( struct chirp_client *c, int fd, int uid, int gid ) {
	return simple_command(c,"fchown %d %d %d\n", fd, uid, gid);
}

DLLEXPORT int
chirp_client_fchmod( struct chirp_client *c, int fd, int mode ) {
	return simple_command(c,"fchmod %d %d\n", fd, mode);
}

DLLEXPORT int
chirp_client_ftruncate( struct chirp_client *c, int fd, int length ) {
	return simple_command(c,"ftruncate %d %d\n", fd, length);
}

DLLEXPORT int
chirp_client_getfile_buffer( struct chirp_client *c, const char *path, 
							char **buffer )
{
	int result, actual=0;

	result = simple_command(c,"getfile %s\n",path);
	if(result>=0) {
		*buffer = (char*)malloc(result+1);
		if(*buffer) {
			actual = (int)fread(*buffer,1,result,c->rstream);
			if(actual!=result) chirp_fatal_request("getfile");
			(*buffer)[result] = '\0';
		}
		else {
			chirp_fatal_request("getfile");
		}
	}
	else {
		chirp_fatal_request("getfile");
	}

	return actual;
}

DLLEXPORT int
chirp_client_putfile_buffer( struct chirp_client *c, const char *path,
							const char *buffer, int mode, int length )
{
	int result, actual;

	// Send request to put file
	result = simple_command(c, "putfile %s %d %d\n", path, mode, length);
	if(result < 0) chirp_fatal_request("putfile");
	result = fflush(c->wstream);
	if(result < 0) chirp_fatal_request("putfile");
	
	// Now send actual file
	actual = (int)fwrite(buffer,1,length,c->wstream);
	if(actual!=length) chirp_fatal_request("putfile");
	
	return actual;
}

DLLEXPORT int
chirp_client_getlongdir( struct chirp_client *c, const char *path,
						char **buffer) 
{
	int result, actual;
	
	result = simple_command(c, "getlongdir %s\n", path);
	if(result< 0) chirp_fatal_request("getlongdir");
	*buffer = (char*)malloc(result+1);
	if(*buffer) {
		actual = (int)fread(*buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("getlongdir");
		(*buffer)[result] = '\0';
	} else {
		chirp_fatal_request("getlongdir");
	}
	
	return result;
}

DLLEXPORT int
chirp_client_getdir( struct chirp_client *c, const char *path, char **buffer) {
	int result, actual;
	
	result = simple_command(c, "getdir %s\n", path);
	if(result < 0) chirp_fatal_request("getdir");
	*buffer = (char*)malloc(result+1);
	if(*buffer) {
		actual = (int)fread(*buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("getdir");
		(*buffer)[result] = '\0';
	} else {
		chirp_fatal_request("getdir");
	}
	
	return result;
}

DLLEXPORT int
chirp_client_whoami( struct chirp_client *c, char *buffer, int length) {
	int result, actual;

	result = simple_command(c, "whoami %d\n", length);
	if(result > 0) {
		actual = (int)fread(buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("whoami");
	}

	return result;
}

DLLEXPORT int
chirp_client_whoareyou( struct chirp_client *c, const char *rhost,
					   char *buffer, int length )
{
	int result, actual;

	result = simple_command(c, "whoareyou %s %d\n", rhost, length);
	if(result > 0) {
		actual = (int)fread(buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("whoareyou");
	}

	return result;
}

DLLEXPORT int
chirp_client_link( struct chirp_client *c, const char *path,
				  const char *newpath )
{
	return simple_command(c,"link %s %s\n",path,newpath);
}

DLLEXPORT int
chirp_client_symlink( struct chirp_client *c, const char *path,
					 const char *newpath )
{
	return simple_command(c,"symlink %s %s\n",path,newpath);
}

DLLEXPORT int
chirp_client_readlink( struct chirp_client *c, const char *path, int length,
					  char **buffer )
{
	int result, actual;

	result = simple_command(c,"readlink %s %d\n",path, length);
	if(result > 0) {
		*buffer = (char*)malloc(result);
		actual = (int)fread(*buffer,1,result,c->rstream);
		if(actual!=result) chirp_fatal_request("readlink");
	}
	return result;
}

DLLEXPORT int
chirp_client_stat( struct chirp_client *c, const char *path,
				  struct chirp_stat *buf )
{
	int result;

	result = simple_command(c,"stat %s\n",path);
	if(result == 0) {
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_stat(line, buf) == -1) {
			chirp_fatal_request("get_stat");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_lstat( struct chirp_client *c, const char *path,
				   struct chirp_stat *buf )
{
	int result;

	result = simple_command(c,"lstat %s\n",path);
	if(result == 0) {
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_stat(line, buf) == -1) {
			chirp_fatal_request("get_stat");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_statfs( struct chirp_client *c, const char *path,
					struct chirp_statfs *buf )
{
	int result;

	result = simple_command(c,"statfs %s\n",path);
	if(result == 0) {
		char line[CHIRP_LINE_MAX];
		if(fgets(line,CHIRP_LINE_MAX,c->rstream) == NULL) {
			chirp_fatal_request("fgets");
		}
		if(get_statfs(line, buf) == -1) {
			chirp_fatal_request("get_statfs");
		}
	}
	
	return result;
}

DLLEXPORT int
chirp_client_access( struct chirp_client *c, const char *path, int mode ) {
	return simple_command(c,"access %s %d\n",path,mode);
}

DLLEXPORT int
chirp_client_chmod( struct chirp_client *c, const char *path, int mode ) {
	return simple_command(c,"chmod %s %d\n",path,mode);
}

DLLEXPORT int
chirp_client_chown( struct chirp_client *c, const char *path, int uid, int gid )
{
	return simple_command(c,"chown %s %d %d\n",path,uid,gid);
}

DLLEXPORT int
chirp_client_lchown( struct chirp_client *c, const char *path, int uid,
					int gid )
{
	return simple_command(c,"lchown %s %d %d\n",path,uid,gid);
}

DLLEXPORT int
chirp_client_truncate( struct chirp_client *c, const char *path, int length ) {
	return simple_command(c,"truncate %s %d\n",path,length);
}

DLLEXPORT int
chirp_client_utime( struct chirp_client *c, const char *path, int actime,
				   int modtime )
{
	return simple_command(c,"utime %s %d %d\n",path,actime,modtime);
}

static int
convert_result( int result )
{
	if(result>=0) {
		return result;
	} else {
		switch(result) {

			case CHIRP_ERROR_NOT_AUTHENTICATED:
			case CHIRP_ERROR_NOT_AUTHORIZED:
				errno = EACCES;
				break;
			case CHIRP_ERROR_DOESNT_EXIST:
				errno = ENOENT;
				break;
			case CHIRP_ERROR_ALREADY_EXISTS:
				errno = EEXIST;
				break;
			case CHIRP_ERROR_TOO_BIG:
				errno = EFBIG;
				break;
			case CHIRP_ERROR_NO_SPACE:
				errno = ENOSPC;
				break;
			case CHIRP_ERROR_NO_MEMORY:
				errno = ENOMEM;
				break;
			case CHIRP_ERROR_INVALID_REQUEST:
				errno = EINVAL;
				break;
			case CHIRP_ERROR_TOO_MANY_OPEN:
				errno = EMFILE;
				break;
			case CHIRP_ERROR_BUSY:
				errno = EBUSY;
				break;
			case CHIRP_ERROR_TRY_AGAIN:
				errno = EAGAIN;
				break;
			case CHIRP_ERROR_BAD_FD:
				errno = EBADF;
				break;
			case CHIRP_ERROR_IS_DIR:
				errno = EISDIR;
				break;
			case CHIRP_ERROR_NOT_DIR:
				errno = ENOTDIR;
				break;
			case CHIRP_ERROR_NOT_EMPTY:
				errno = ENOTEMPTY;
				break;
			case CHIRP_ERROR_CROSS_DEVICE_LINK:
				errno = EXDEV;
				break;
			case CHIRP_ERROR_OFFLINE:
				errno = ETIMEDOUT;
				break;
			case CHIRP_ERROR_UNKNOWN:
				chirp_fatal_response();
				break;
		}
		return -1;
	}
}

static int
get_result( FILE *s )
{
	char line[CHIRP_LINE_MAX];
	char *c;
	int result;
	int fields;

	c = fgets(line,CHIRP_LINE_MAX,s);
	if(!c) chirp_fatal_response();

	fields = sscanf(line,"%d",&result);
	if(fields!=1) chirp_fatal_response();

#ifdef DEBUG_CHIRP
	fprintf(stderr, "chirp received: %s\n",line);
#endif

	return result;
}

static void
chirp_fatal_request( const char *name )
{
	fprintf(stderr,"chirp: couldn't %s: %s\n",name,strerror(errno));
	abort();
}

static
void chirp_fatal_response()
{
	fprintf(stderr,"chirp: couldn't get response from server: %s\n",strerror(errno));
	abort();
}

static SOCKET
tcp_connect( const char *host, int port )
{
	struct addrinfo* result = 0;
	int success;
	SOCKET fd;
	struct addrinfo hint;

	union {
		struct sockaddr_in6 v6;
		struct sockaddr_in v4;
		struct sockaddr_storage storage;
	} sa;

	if(!initialize_sockets())
		return INVALID_SOCKET;

	// Ubuntu 10 and 12 don't implement AI_ADDRCONFIG the same way
	// everyone else does, so we have to turn it off explicitly.
	memset( & hint, 0, sizeof(hint) );
	hint.ai_family = AF_UNSPEC;
	hint.ai_flags = AI_CANONNAME;
	success = getaddrinfo(host, NULL, &hint, &result);

	if (success != 0)
		return INVALID_SOCKET;

	if (result == NULL)
		return INVALID_SOCKET;

	memcpy(&sa.storage, result->ai_addr, result->ai_addrlen );
	switch(result->ai_family) {
		case AF_INET: sa.v4.sin_port = htons(port); break;
		case AF_INET6: sa.v6.sin6_port = htons(port); break;
		default:
			freeaddrinfo( result );
			return INVALID_SOCKET;
	}

#if defined(WIN32)
	// Create the socket with no overlapped I/0 so we can later associate the socket
	// with a valid file descripter using _open_osfhandle.
	fd = WSASocket(result->ai_family, SOCK_STREAM, 0, NULL, 0, 0);
#else
	fd = socket( result->ai_family, SOCK_STREAM, 0 );
#endif
	if(fd == INVALID_SOCKET) {
		freeaddrinfo(result);
		return INVALID_SOCKET;
	}

	const int one = 1;
	int r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(one));
	if (r < 0) {
		fprintf(stderr, "Warning: error %d settting SO_REUSEADDR\n", errno);
	}

	success = connect( fd, (struct sockaddr*)&sa.storage, (int)result->ai_addrlen );
	freeaddrinfo(result);
	if(success == SOCKET_ERROR) {
		closesocket(fd);
		return INVALID_SOCKET;
	}

	return fd;
}

/*
  vsprintf_chirp -- simple sprintf capabilities with character escaping

  The following format characters are interpreted:

  %d -- decimal
  %s -- word (whitespace is escaped)
  %% -- output %
*/

void
vsprintf_chirp(char *command,char const *fmt,va_list args)
{
	char       *c;
	char const *f;

	c = command;
	f = fmt;
	while(*f) {
		if(*f == '%') {
			switch(*(++f)) {
			case 'd':
				f++;
				sprintf(c,"%d",va_arg(args,int));
				c += strlen(c);
				break;
			case 's': {
				char const *w = va_arg(args,char const *);
				f++;
				while(*w) {
					switch(*w) {
					case ' ':
					case '\t':
					case '\n':
					case '\r':
					case '\\':
						*(c++) = '\\';
						/*fall through*/
					default:
						*(c++) = *(w++);
					}
				}
				break;
			}
			case '%':
				*(c++) = *(f++);
				break;
			default:
				fprintf(stderr, "vsprintf_chirp error\n");
				chirp_fatal_request(f);
			}
		} else {
			*(c++) = *(f++);
		}
	}
	*(c++) = '\0';
}

int
simple_command(struct chirp_client *c,char const *fmt,...)
{
	int     result;
	char    command[CHIRP_LINE_MAX];
	va_list args;

	va_start(args,fmt);
	vsprintf_chirp(command,fmt,args);
	va_end(args);

#ifdef DEBUG_CHIRP
	fprintf(stderr, "chirp sending: %s",command);
#endif

	result = fputs(command,c->wstream);
	if(result < 0) chirp_fatal_request(fmt);

	result = fflush(c->wstream);
	if(result == EOF) chirp_fatal_request(fmt);

	return convert_result(get_result(c->rstream));
}

char const *
read_url_param(char const *url,char *buffer,size_t length)
{
	size_t bufpos = 0;

	while(*url != '\0' && *url != '.' && *url != '/'
	      && *url != '=' && *url != ';' && *url != '\\')
	{
		if(bufpos >= length) return NULL;	

		switch(*url) {
		case '+':
			buffer[bufpos++] = ' ';
			break;
		case '%': { //form-url-encoded escape sequence
			//following two characters are hex digits
			char d = tolower(*(++url));

			if(d >= '0' && d <= '9') d -= '0';
			else if(d >= 'a' && d <= 'f') d = d - 'a' + 0xA;
			else return NULL; //invalid hex digit

			buffer[bufpos] = d<<4;

			d = tolower(*(++url));

			if(d >= '0' && d <= '9') d -= '0';
			else if(d >= 'a' && d <= 'f') d = d - 'a' + 0xA;
			else return NULL; //invalid hex digit

			buffer[bufpos++] |= d;

			break;
		}
		default:
			buffer[bufpos++] = *url;
			break;
		}

		url++;
	}

	if(bufpos >= length) return NULL;
	buffer[bufpos] = '\0';

	return url;
}

int get_stat( const char *line, struct chirp_stat *info ) {
	int fields;

	memset(info,0,sizeof(*info));

	fields = sscanf(line,"%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
			&info->cst_dev,
			&info->cst_ino,
			&info->cst_mode,
			&info->cst_nlink,
			&info->cst_uid,
			&info->cst_gid,
			&info->cst_rdev,
			&info->cst_size,
			&info->cst_blksize,
			&info->cst_blocks,
			&info->cst_atime,
			&info->cst_mtime,
			&info->cst_ctime);

	if(fields!=13) {
		return -1;
	}

	return 0;
}

int get_statfs( const char *line, struct chirp_statfs *info ) {

	int fields;

	memset(info,0,sizeof(*info));

	fields = sscanf(line,"%ld %ld %ld %ld %ld %ld %ld\n",
			&info->f_type,
			&info->f_bsize,
			&info->f_blocks,
			&info->f_bfree,
			&info->f_bavail,
			&info->f_files,
			&info->f_ffree);

	if(fields!=7) {
		return -1;
	}

	return 0;
}
