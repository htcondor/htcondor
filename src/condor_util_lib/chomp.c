#include "condor_common.h"

//strip off newline if exists
char *
chomp( char *buffer ) {
	int size;

	if( (size = strlen(buffer)) ) {
		if ( buffer[size-1] == '\n' ) {
			buffer[size-1] = '\0';
		}
#if defined(WIN32)
		if ( ( size > 1 ) && ( buffer[size-2] == '\r' ) ) {
			buffer[size-2] = '\0';
		}
#endif
	}
	return( buffer );
}
