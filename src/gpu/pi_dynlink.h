#ifndef   _PI_DYNLINK_H
#define   _PI_DYNLINK_H

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>

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

#else

#include <dlfcn.h>

typedef void * dlopen_return_t;

#endif /* WIN32 */

#endif /* _PI_DYNLINK_H */
