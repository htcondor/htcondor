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
 *	Implementation of the StreamTokenizerWithCont class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	9/16/05	--	coding and testing finished
 *	9/16/05	--	added "const" in StreamTokenizerWithCont argument list
 *	9/23/05	--	added handling of trailing continuation characters
 *	9/29/05	--	added skipLine()
 */


/*
 *	Method:
 *	StreamTokenizerWithCont::StreamTokenizerWithCont(const char *fileName) : StreamTokenizer(fileName)
 *
 *	Purpose:
 *	Initializes the stream tokenizer with a file of a given name
 *	just by invoking the constructor of the superclass.
 *
 *	Input:
 *	file name
 *
 *	Output:
 *	none
 */
StreamTokenizerWithCont::StreamTokenizerWithCont(const char *fileName) : StreamTokenizer(fileName)
{
}


/*
 *	Method:
 *	int StreamTokenizerWithCont::readToken(char *tab, int max)
 *
 *	Purpose:
 *	Reads a token from the file into tab observing the backslash
 *	tokens that denote line continuations.
 *
 *	Input:
 *	table
 *	maximum token size (including the ending zero)
 *
 *	Output:
 *	0,	if end of file or line was encountered before a token was reached
 *	1,	if a non-empty token was read
 */
int StreamTokenizerWithCont::readToken(char *tab, int max)
{
	while(true) {
		int ret = StreamTokenizer::readToken(tab,max);
		if( 0==ret )
			return 0;
		if( 0==strcmp(tab,"\\") )
			StreamTokenizer::skipLine();
		else {
			if( strlen(tab)>=1 && '\\'==tab[ strlen(tab)-1 ] )	{	// trailing continuation character
				tab[ strlen(tab)-1 ] = '\0';						// remove the continuation character
				StreamTokenizer::skipLine();
			};
			return ret;
		};
	};
}


/*
 *	Method:
 *	void StreamTokenizerWithCont::skipLine(void)
 *
 *	Purpose:
 *	Reads the file until and including the end of line or file
 *	(when a line contiuation character is encountered, then the line 
 *	does not yet end in this line).
 *	Places all the characters that have been read into the variable
 *	called line, excluding an EOF character.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void StreamTokenizerWithCont::skipLine(void)
{
	char tab[500];
	int ret;

	do {
		ret = readToken(tab,500);
	} while( 0!=ret );

	StreamTokenizer::skipLine();
	return;
}


/*
 *	Method:
 *	void StreamTokenizerWithCont_test(void)
 *
 *	Purpose:
 *	Tests the implementation of the StreamTokenizerWithCont
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void StreamTokenizerWithCont_test(void)
{
	char tab[50];
	int ret;

	StreamTokenizerWithCont s("StreamTokenizerWithContTest.txt");
	while( !s.isEOF() ) {
		ret = s.readToken(tab,50);
		if( 0==ret ) {
			s.skipLine();
			printf("NEWLINE\n");
			s.saveRecentLine(stderr);
			s.resetRecentLine();
			fprintf(stderr,"<END>");
			continue;
		};
		printf("TOKEN: %s\n",tab);
	};
	// in case the last line does not end with '\n'
	s.saveRecentLine(stderr);
	s.resetRecentLine();

	printf("\n\n second test \n\n");

	// read just one token per line
	StreamTokenizerWithCont s2("StreamTokenizerWithContTest2.txt");
	while( !s2.isEOF() ) {

		// read as many tokens as needed (here just two)
		ret = s2.readToken(tab,50);
		if( 0!=ret )
			printf("TOKEN: %s\n",tab);
		ret = s2.readToken(tab,50);
		if( 0!=ret )
			printf("TOKEN: %s\n",tab);

		// skip the rest of the line
		s2.skipLine();
		printf("THELINE\n");
		s2.saveRecentLine(stderr);
		s2.resetRecentLine();
		fprintf(stderr,"<END>");
	};
}
