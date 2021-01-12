#ifndef   _PI_DYNLINK_H
#define   _PI_DYNLINK_H

#ifdef WIN32

/* Ignored on Windows. */
#define RTLD_LAZY 0x00001
#define RTLD_NOW  0x00002

typedef HINSTANCE dlopen_return_t;

inline dlopen_return_t
dlopen( const char * filename, int ) {
	return LoadLibrary( filename );
}

inline void *
dlsym( dlopen_return_t hlib, const char * symbol ) {
	return (void *)GetProcAddress( hlib, symbol );
}

inline int
dlclose( dlopen_return_t hlib ) {
	return FreeLibrary( hlib );
}

inline char *
dlerror() {
	static char buffer[64];
	snprintf( buffer, 64, "%lu", GetLastError );
	return buffer;
}

#else

#include <dlfcn.h>

typedef void * dlopen_return_t;

#endif /* WIN32 */

#endif /* _PI_DYNLINK_H */
