#include <condor_common.h>
#include "console-utils.h"

#ifdef WIN32

int
getConsoleWindowSize( int * pHeight ) {
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if( GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), & ws ) ) {
		if( pHeight ) {
			* pHeight = (int)(ws.srWindow.Bottom - ws.srWindow.Top)+1;
		}
		return (int)ws.dwSize.X;
	}
	return -1;
}

#else

#include <sys/ioctl.h>

int
getConsoleWindowSize( int * pHeight ) {
	struct winsize ws;
	if( 0 == ioctl( 1, TIOCGWINSZ, & ws ) ) {
		if( pHeight ) {
			* pHeight = (int)ws.ws_row;
		}
		return (int) ws.ws_col;
	}
	return -1;
}

#endif
