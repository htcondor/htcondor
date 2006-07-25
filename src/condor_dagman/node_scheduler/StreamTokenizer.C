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

#include "headers.h"

/*
 *	FILE CONTENT:
 *	Implementation of the StreamTokenizer class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/7/05	--	coding and testing finished
 *	9/16/05	--	added "const" in StreamTokenizer argument list
 *	9/29/05	--	updated isEOF()
 */


/*
 *	Method:
 *	StreamTokenizer::StreamTokenizer( const char* fileName )
 *
 *	Purpose:
 *	Initializes the stream tokenizer with a file of a given name.
 *	The file must be a text file.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
StreamTokenizer::StreamTokenizer( const char* fileName )
{
	stream = fopen(fileName,"rt");
	if( NULL==stream )
		throw "StreamTokenizer::StreamTokenizer, stream is NULL";
}


/*
 *	Method:
 *	StreamTokenizer::~StreamTokenizer(void)
 *
 *	Purpose:
 *	Closes the file.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
StreamTokenizer::~StreamTokenizer(void)
{
	if( NULL!=stream )
		fclose(stream);
}


/*
 *	Method:
 *	int StreamTokenizer::readToken(char *tab, int max)
 *
 *	Purpose:
 *	Reads a token from the file into tab. Maximum length of token 
 *	is given by max. Tokens are separated by white spaces. 
 *	Places all the characters that have been read into the variable
 *	called line, excluding an EOF character.
 *
 *	Input:
 *	table
 *	maximum token size (including the ending zero)
 *
 *	Output:
 *	0,	if end of file or line was encountered before a token was reached
 *	1,	if a non-empty token was read
 */
int StreamTokenizer::readToken(char *tab, int max)
{
	if( NULL==stream )
		throw "StreamTokenizer::readToken, stream is NULL";
	if( NULL==tab )
		throw "StreamTokenizer::readToken, tab is NULL";
	if( max<=1 )
		throw "StreamTokenizer::readToken, max too small";

	int c;
	int i;

	// begin with an empty string
	tab[0]=0;

	// skip white spaces, but not the new line character
	c=fgetc(stream);
	while( c=='\t' || c==' ' ) {
		line.append(c);
		c=fgetc(stream);
	};

	// so we have a character, new line or end of file
	if( c==EOF || c=='\n' ) {
		ungetc(c,stream);
		return 0;
	};

	// read the token
	i=0;
	while( i<(max-1) && c!=EOF && c!='\t' && c!='\n' && c!=' ' ) {
		tab[i++]=c;
		line.append(c);
		c=fgetc(stream);
	};

	// close the token with a zero
	tab[i]=0;

	// if we read something other than EOF, return it back to the stream
	if( c!=EOF ) {
		ungetc(c,stream);
	};

	return 1;
}


/*
 *	Method:
 *	void StreamTokenizer::skipLine(void)
 *
 *	Purpose:
 *	Reads the file until and including the end of line or file.
 *	Places all the characters that have been read into the variable
 *	called line, excluding an EOF character.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void StreamTokenizer::skipLine(void)
{
	if( NULL==stream )
		throw "StreamTokenizer::skipLine, stream is NULL";

	int c;
	c=fgetc(stream);
	while( c!=EOF && c!='\n' ) {
		line.append(c);
		c=fgetc(stream);
	};
	if( c!=EOF )
		line.append(c);
}


/*
 *	Method:
 *	bool StreamTokenizer::isEOF(void)
 *
 *	Purpose:
 *	Tests if the end of file was reached.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	true,	end of file
 *	false,	not end of file
 */
bool StreamTokenizer::isEOF(void)
{
	if( NULL==stream )
		throw "StreamTokenizer::isEOF, stream is NULL";

	int c=fgetc(stream);  // we may have already read all file characters, but have not yet beyond, in which case feof(stream) is not yet true
	if( EOF==c ) {
		if( feof(stream) )
			return true;
		else
			throw "StreamTokenizer::isEOF, unable to read the file";
	}
	else {
		ungetc(c,stream);
		return false;
	};
}


/*
 *	Method:
 *	void StreamTokenizer::saveRecentLine(FILE *stream)
 *
 *	Purpose:
 *	Save the content of the variable line into the stream.
 *	The content would normally be the line of characters
 *	that have most recently been read by readToken() and 
 *	skipLine() methods.
 *
 *	Input:
 *	stream for output
 *
 *	Output:
 *	none
 */
void StreamTokenizer::saveRecentLine(FILE *stream)
{
	for( int i=0; i<line.getNumElem(); i++ )
		fputc( line.getElem(i), stream );
}


/*
 *	Method:
 *	void StreamTokenizer::resetRecentLine(void)
 *
 *	Purpose:
 *	Empties the content of the variable line.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void StreamTokenizer::resetRecentLine(void)
{
	line.reset();
}


/*
 *	Method:
 *	void StreamTokenizer_test(void)
 *
 *	Purpose:
 *	Tests the implementation of the StreamTokenizer
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void StreamTokenizer_test(void)
{
	char tab[10];
	int res;
	int line =0;

	StreamTokenizer *st = new StreamTokenizer("tokenizerTest.txt");
	while( ! st->isEOF() ) {
		line ++;
		printf("LINE %d\n",line);
		res = st->readToken(tab,10);
		printf("%d  %s\n", res, tab);
		res = st->readToken(tab,10);
		printf("%d  %s\n", res, tab);
		st->skipLine();
		st->saveRecentLine( stderr );
		st->resetRecentLine();
		fprintf(stderr,"<END>");
	};
	delete st;
	printf("read %d lines\n", line );
}
