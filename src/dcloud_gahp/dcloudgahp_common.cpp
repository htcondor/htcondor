#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include "dcloudgahp_common.h"

static pthread_mutex_t dcloudprintf_mutex = PTHREAD_MUTEX_INITIALIZER;

void dcloudprintf_internal(const char *function, const char *fmt, ...)
{
    va_list va_args;

    pthread_mutex_lock(&dcloudprintf_mutex);
    fprintf(logfp, "%s: ", function);
    va_start(va_args, fmt);
    vfprintf(logfp, fmt, va_args);
    va_end(va_args);
    fflush(logfp);
    pthread_mutex_unlock(&dcloudprintf_mutex);
}

const char *itoa( int i )
{
    static char buf[20];
    snprintf( buf, sizeof(buf), "%d", i );
    return buf;
}

std::string create_failure(int req_id, const char *err_msg)
{
    std::string buffer;

    buffer += itoa(req_id);
    buffer += ' ';

    buffer += err_msg;
    buffer += '\n';

    return buffer;
}

Gahp_Args::Gahp_Args()
{
	argv = NULL;
	argc = 0;
	argv_size = 0;
}

Gahp_Args::~Gahp_Args()
{
	reset();
}

/* Restore the object to its fresh, clean state. This means that argv is
 * completely freed and argc and argv_size are set to zero.
 */
void
Gahp_Args::reset()
{
	if ( argv == NULL ) {
		return;
	}

	for ( int i = 0; i < argc; i++ ) {
		free( argv[i] );
		argv[i] = NULL;
	}

	free( argv );
	argv = NULL;
	argv_size = 0;
	argc = 0;
}

/* Add an argument to the end of the args array. argv is extended
 * automatically if required. The string passed in becomes the property
 * of the Gahp_Args object, which will deallocate it with free(). Thus,
 * you would typically give add_arg() a strdup()ed string.
 */
void
Gahp_Args::add_arg( char *new_arg )
{
	if ( new_arg == NULL ) {
		return;
	}
	if ( argc >= argv_size ) {
		argv_size += 60;
		argv = (char **)realloc( argv, argv_size * sizeof(char *) );
	}
	argv[argc] = new_arg;
	argc++;
}
