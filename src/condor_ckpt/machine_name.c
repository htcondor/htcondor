#define _POSIX_SOURCE

#include <sys/utsname.h>

int machine_name( char * buf, int buf_len )
{
	struct utsname	name;
	if( uname(&name) < 0 ) {
		return -1;
	}

	strncpy( buf, name.nodename, buf_len );

	return 0;
}
