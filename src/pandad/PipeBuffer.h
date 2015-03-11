#ifndef PIPE_BUFFER_H
#define PIPE_BUFFER_H

#include <string>
#include <pthread.h>

class PipeBuffer {
	public:
		// Assumes that _fd is already open() and _mutex already held.
		PipeBuffer( int _fd, pthread_mutex_t * _mutex ) :
			fd( _fd ), eof( false ), error( false ), mutex( _mutex ),
			index( 0 ), bytesReadAhead( 0 ) { }

		bool isEOF() { return eof; }
		bool isError() { return error; }

		std::string * getNextLine();

	protected:
		int						fd;
		bool					eof;
		bool					error;
		pthread_mutex_t	*		mutex;

		unsigned				index;
		unsigned				bytesReadAhead;
		std::string				partialLine;
		static const unsigned	readAheadSize = 1024;
		unsigned char			readAheadBuffer[readAheadSize];
};

#endif
