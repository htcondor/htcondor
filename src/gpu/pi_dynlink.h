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

// Stolen from condor_utils/get_last_error_string.WINDOWS.cpp
inline const char *
dlerror() {
	static std::string szError;
	LPTSTR lpMsgBuf = NULL;

	DWORD iErrCode = GetLastError();

	DWORD rc = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		(DWORD)iErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	if ( rc > 0 && lpMsgBuf ) {
		szError = (char *) lpMsgBuf;
		LocalFree(lpMsgBuf);  // must dealloc lpMsgBuf!
		// Get rid of the the ending /r/n we get from FormatMessage
		if(! szError.empty()) {
			if( szError[szError.length()-1] == '\n' ) {
				szError.erase(szError.length()-1);
			}
		}
		if(! szError.empty()) {
			if( szError[szError.length()-1] == '\r' ) {
				szError.erase(szError.length()-1);
			}
		}
	} else {
		char buf[32];
		snprintf( buf, 32, "unknown error %u", iErrCode );
		szError = buf;
	}

	return szError.c_str();
}

#else

#include <dlfcn.h>

typedef void * dlopen_return_t;

#endif /* WIN32 */

#endif /* _PI_DYNLINK_H */
