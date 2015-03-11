#include "condor_common.h"
#include "condor_debug.h"

#include "PipeBuffer.h"

std::string * PipeBuffer::getNextLine() {
	if( index == bytesReadAhead ) {
		pthread_mutex_unlock( mutex );
		bytesReadAhead = read( fd, (void *)readAheadBuffer, readAheadSize );
		pthread_mutex_lock( mutex );

		if( bytesReadAhead == (unsigned)-1 ) {
			if( errno == EINTR ) { index = -1; return getNextLine(); }
			dprintf( D_ALWAYS, "Error reading from pipe %d: %s (%d); returning NULL.\n", fd, strerror( errno ), errno );
			error = true;
			return NULL;
		}

		if( bytesReadAhead == 0 ) {
			dprintf( D_ALWAYS, "Reached EOF on pipe %d; returning NULL.\n", fd );
			eof = true;
			return NULL;
		}

		eof = false;
		error = false;
		index = 0;
	}

	while( index != bytesReadAhead ) {
		char c = readAheadBuffer[ index++ ];
		if( c == '\n' ) {
			std::string * line = new std::string( partialLine );
			partialLine = "";
			return line;
		} else {
			partialLine += c;
		}
	}

	// Block at once most per call (for some reason).
	return NULL;
} /* end getNextLine() */
