
#include "condor_common.h"
#include "io_proxy_handler.h"

#include "NTsenders.h"
#include "../condor_chirp/chirp_protocol.h"

#include <errno.h>

IOProxyHandler::IOProxyHandler()
{
	cookie = 0;
	got_cookie = false;
}

IOProxyHandler::~IOProxyHandler()
{
	free(cookie);
}

/*
Initialize this handler on the given stream.
The process at the other side of the stream must present
the cookie passed in as an argument.  This r
Returns true on success, false otherwise.
*/

bool IOProxyHandler::init( Stream *s, const char *c )
{
	cookie = strdup(c);
	if(!cookie) return false;

	daemonCore->Register_Socket( s, "IOProxy client", (SocketHandlercpp) &IOProxyHandler::handle_request, "IOProxyHandler::handle_request", this );

	return true;
}

/*
This callback is invoked any time new data arrive on the socket.
If a valid cookie has been received, then decode and execute the request.
Otherwise, only accept cookie attempts.
Returns KEEP_STREAM if the stream is still valid, ~KEEP_STREAM otherwise.
*/

int IOProxyHandler::handle_request( Stream *s )
{
	char line[CHIRP_LINE_MAX];
	ReliSock *r = (ReliSock *) s;

	if(r->get_line_raw(line,CHIRP_LINE_MAX)>0)  {
		if( got_cookie ) {
			handle_standard_request(r,line);
		} else {
			handle_cookie_request(r,line);
		}
		return KEEP_STREAM;
	} else {
		dprintf(D_ALWAYS,"IOProxyHandler: closing connection to %s\n",r->endpoint_ip_str());
		delete this;
		return ~KEEP_STREAM;
	}
}

/*
Handle an incoming line from the client.
If it is a valid cookie request, then authenticate.
Otherwise, return a not-authenticated-error.
*/

void IOProxyHandler::handle_cookie_request( ReliSock *r, char *line )
{
	char check_cookie[CHIRP_LINE_MAX];

	if(sscanf(line,"cookie %s",check_cookie)==1) {
		if(!strcmp(check_cookie,cookie)) {
			dprintf( D_FULLDEBUG, 
					 "IOProxyHandler: client presented correct cookie.\n" );
			got_cookie = true;
		} else {
			dprintf( D_ALWAYS, "IOProxyHandler: client presented "
					 "*WRONG* cookie.\n", cookie );
			sleep(1);
		}
	} else {
		dprintf( D_ALWAYS, "IOProxyHandler: client started with "
				 "'%s' instead of a cookie\n", line );
	}

	if(got_cookie) {
		sprintf(line,"0");
	} else {
		sprintf(line,"%d",CHIRP_ERROR_NOT_AUTHENTICATED);
	}

	r->put_line_raw(line);
}

/*
Handle an incoming line from the client.
A valid cookie is assumedf to have been received, so decode and execute any request.
*/

void IOProxyHandler::handle_standard_request( ReliSock *r, char *line )
{
	char path[CHIRP_LINE_MAX];
	char newpath[CHIRP_LINE_MAX];
	char flags_string[CHIRP_LINE_MAX];
	int result, offset, whence, length, flags, mode, fd;

	dprintf(D_SYSCALLS,"IOProxyHandler: request: %s\n",line);

	if(sscanf(line,"open %s %s %d",path,flags_string,&mode)==3) {

		flags = 0;

		if( strchr(flags_string,'w') ) {
			if( strchr(flags_string,'r') ) {
				flags |= O_RDWR;
			} else {
				flags |= O_WRONLY;
			}
		} else {
			flags |= O_RDONLY;
		}

		if(strchr(flags_string,'c')) flags |= O_CREAT;
		if(strchr(flags_string,'t')) flags |= O_TRUNC;
		if(strchr(flags_string,'x')) flags |= O_EXCL;
		if(strchr(flags_string,'a')) flags |= O_APPEND;

		result = REMOTE_CONDOR_open(path,(open_flags_t)flags,mode);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"close %d",&fd)==1) {

		result = REMOTE_CONDOR_close(fd);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"lseek %d %d %d",&fd,&offset,&whence)) {

		int whence_valid = 1;

		switch(whence) {
			case 0:
				whence = SEEK_SET;
				break;
			case 1:
				whence = SEEK_CUR;
				break;
			case 2:
				whence = SEEK_END;
				break;
			default:
				whence_valid = 0;
				break;
		}

		if(whence_valid) {
			result = REMOTE_CONDOR_lseek(fd,offset,whence);
			result = convert(result,errno);
		} else {
			result = CHIRP_ERROR_INVALID_REQUEST;
		}

		sprintf(line,"%d",result);
		r->put_line_raw(line);

	} else if(sscanf(line,"unlink %s",path)==1) {

		result = REMOTE_CONDOR_unlink(path);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"rename %s %s",path,newpath)==2) {

		result = REMOTE_CONDOR_rename(path,newpath);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"mkdir %s %d",path,&mode)==2) {

		result = REMOTE_CONDOR_mkdir(path,mode);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"rmdir %s",path)==1) {

		result = REMOTE_CONDOR_rmdir(path);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"fsync %d",&fd)==1) {

		result = REMOTE_CONDOR_fsync(fd);
		sprintf(line,"%d",convert(result,errno));
		r->put_line_raw(line);

	} else if(sscanf(line,"read %d %d",&fd,&length)==2) {

		char *buffer = (char*) malloc(length);
		if(buffer) {
			result = REMOTE_CONDOR_read(fd,buffer,length);
			sprintf(line,"%d",convert(result,errno));
			r->put_line_raw(line);
			if(result>0) {
				r->put_bytes_raw(buffer,result);
			}
			free(buffer);
		} else {
			sprintf(line,"%d",CHIRP_ERROR_NO_MEMORY);
		}
		
	} else if(sscanf(line,"write %d %d",&fd,&length)==2) {

		char *buffer = (char*) malloc(length);
		if(buffer) {
			result = r->get_bytes_raw(buffer,length);
			if(result==length) {
				result = REMOTE_CONDOR_write(fd,buffer,length);
				sprintf(line,"%d",convert(result,errno));
			} else {
				sprintf(line,"%d",CHIRP_ERROR_INVALID_REQUEST);
			}
			free(buffer);
		} else {
			sprintf(line,"%d",CHIRP_ERROR_NO_MEMORY);
		}
		r->put_line_raw(line);
		
	} else {
		sprintf(line,"%d",CHIRP_ERROR_INVALID_REQUEST);
		r->put_line_raw(line);
	}

	dprintf(D_SYSCALLS,"IOProxyHandler: response: %s\n",line);
}

int IOProxyHandler::convert( int result, int unix_errno )
{
	if(result>=0) return result;

	switch(unix_errno) {
		case EPERM:
		case EACCES:
			return CHIRP_ERROR_NOT_AUTHORIZED;
		case ENOENT:
			return CHIRP_ERROR_DOESNT_EXIST;
		case EEXIST:
			return CHIRP_ERROR_ALREADY_EXISTS;
		case EFBIG:
			return CHIRP_ERROR_TOO_BIG;
		case ENOSPC:
			return CHIRP_ERROR_NO_SPACE;
		case ENOMEM:
			return CHIRP_ERROR_NO_MEMORY;
		case EBADF:
		case E2BIG:
		case EINVAL:
			return CHIRP_ERROR_INVALID_REQUEST;
		case ENFILE:
		case EMFILE:
			return CHIRP_ERROR_TOO_MANY_OPEN;
		case EBUSY:
#ifndef WIN32
		case ETXTBSY:
#endif
			return CHIRP_ERROR_BUSY;
		case EAGAIN:
		case EINTR:
			return CHIRP_ERROR_TRY_AGAIN;
		default:
			return CHIRP_ERROR_UNKNOWN;
	}
}
