#include "condor_common.h"
#include "stdarg.h"

int fdprintf (int fd, const char *fmt, ...) 
{
	static char buffer[10240];
	int			len;
	va_list 	varargs;

	va_start (varargs, fmt);
	if ((len = vsprintf (buffer, fmt, varargs)) < 0) return -1;
	return write (fd, buffer, len);
}
