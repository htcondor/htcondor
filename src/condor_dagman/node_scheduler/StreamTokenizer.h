/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#ifndef GM_STREAM_TOKENIZER_H
#define GM_STREAM_TOKENIZER_H

/*
 *	FILE CONTENT:
 *	Declaration of a stream tokenizer class.
 *	A stream tokenizer is initialized with the file name of a text file.
 *	We can then read the file token by token (separated by white spaces)
 *	until the end of line or file, and when needed skip to the next line.
 *	Any character that is read is placed into an internal buffer called line
 *	and can be saved to a file, and reset.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 *	9/16/05	--	added "const" in StreamTokenizer argument list
 */


class StreamTokenizer
{

private:

	FILE *stream;				// text file that tokenizer reads
	ResizableArray<int> line;	// characters of the line that is currently being read from the stream

public:

	StreamTokenizer( const char* fileName );
	virtual ~StreamTokenizer(void);
	bool isEOF(void);
	virtual int readToken(char *tab, int max);
	virtual void skipLine(void);
	void saveRecentLine(FILE *stream);
	void resetRecentLine(void);
};

void StreamTokenizer_test(void);

#endif
