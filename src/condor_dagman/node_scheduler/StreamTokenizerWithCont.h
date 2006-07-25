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

#ifndef GM_STREAM_TOKENIZER_WITH_CONT_H
#define GM_STREAM_TOKENIZER_WITH_CONT_H

/*
 *	FILE CONTENT:
 *	Declaration of a stream tokenizer class
 *	where a line may be continued after a backslash 
 *	to the next line in the file.
 *	Upon reading a token, we check if the token is equal to
 *	backslash, and if so we ignore the rest of the line in 
 *	the file and move to the next line in the file. 
 *	Backslash may also be the last character of a string, 
 *	which denotes that the string ends, but the line is 
 *	continued in the next line in the file.
 *	A line may be skipped, which skips the appropriate 
 *	lines in the text file.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	9/16/05	--	coding and testing finished
 *	9/16/05	--	added "const" in StreamTokenizerWithCont argument list
 *	9/29/05	--	added skipLine()
 */


class StreamTokenizerWithCont :
	public StreamTokenizer
{
public:
	StreamTokenizerWithCont(const char *fileName);
	virtual int readToken(char *tab, int max);
	virtual void skipLine(void);
};

void StreamTokenizerWithCont_test(void);

#endif
