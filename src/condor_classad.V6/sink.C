#include "sink.h"

Sink::
Sink ()
{
	sink  = NO_SINK;
}


Sink::
~Sink()
{
}


void Sink::
setSink (char *&s, int &sz)
{
	sink = STRING_SINK;
	last = 0;

	if (s == NULL)
	{
		// we have to perform the memory management for the string
		buffer = new char [1024];
		bufLen = 1024;

		// set up references for update
		bufRef = &s;
		bufLenRef = &sz;

		// perform first update
		*bufRef = buffer;
		*bufLenRef = 0;
	}
	else
	{
		// user has provided a buffer
		buffer = s;
		bufLen = sz;

		// don't use references in this case
		bufRef = NULL;
		bufLenRef = NULL;
	}
}


void Sink::
setSink (Sock &s)
{
	sink = CEDAR_SINK;
	sock = &s;
}


void Sink::
setSink (int i)
{
    sink = FILE_DESC_SINK;
    fd = i;
}


void Sink::
setSink (FILE *f)
{
	sink = FILE_SINK;
	file = f;
}


bool Sink::
sendToSink (void *bytes, int len)
{
	char 	*buf = (char *) bytes;
	int 	rval;

	switch (sink)
	{
		case STRING_SINK:
			// first check that we have enough room
			if (last+len >= bufLen)
			{
				// no ... return false if we cannot resize
				if (bufRef == NULL) return false;

				// resize to twice original size
				char *temp = new char [2*bufLen];
				if (!temp)
				{
					delete [] buffer;
					return false;
				}
				bufLen *= 2;
				strcpy (temp, buffer);

				// make the switch
				delete [] buffer;
				buffer = temp;
				*bufRef = buffer;
			}

			// have enough room; copy and update lengths
			strcpy (buffer+last, buf);
			last += len;
			if (bufLenRef) *bufLenRef = last;	

			return true;


		case FILE_SINK:
			// writing 'len' number of objects, each of size 1
			rval = fwrite (bytes, 1, len, file);
			return (rval == len);


		case FILE_DESC_SINK:
			rval = write (fd, bytes, len);
			return (rval == len);


		case CEDAR_SINK:
			rval = sock->put_bytes (bytes, len);
			return (rval == len);


		default:
			return false;
	}

	// should not reach here
	return false;
}


bool Sink::
flushSink ()
{
	if (!sendToSink((void*)"\0", 1)) return false;

	switch (sink)
	{
		case STRING_SINK:
			// already null terminated
			return (true);

		case FILE_SINK:
			// fflush() returns EOF on error; 0 otherwise
			return (fflush(file) == 0);

		case FILE_DESC_SINK:
		case CEDAR_SINK:
			// no-op
			return true;
	}

	return false;
}
