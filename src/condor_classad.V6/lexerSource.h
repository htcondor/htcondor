/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2002, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __LEXER_SOURCE_H__
#define __LEXER_SOURCE_H__

#include "common.h"
#include <iostream>

/*
 * LexerSource is a class that provides an abstract interface to the
 * lexer. The lexer reads tokens and gives them to the parser.  The
 * lexer reads single characters from a LexerSource. Because we want
 * to read characters from different types of sources (files, strings,
 * etc.) without burdening the lexer, the lexer uses this LexerSource
 * abstract class. There are several implementations of the
 * LexerSource that provide access to specific types of sources. 
 *
 */

class LexerSource
{
public:
	LexerSource()
	{
		return;
	}
	virtual ~LexerSource()
	{
		return;
	}
	
	// Reads a single character from the source
	virtual int ReadCharacter(void) = 0;

	// Returns the last character read (from ReadCharacter()) from the
	// source
	virtual int ReadPreviousCharacter(void) { return _previous_character; };

	// Puts back a character so that when ReadCharacter is called
	// again, it returns the character that was previously
	// read. Although this interface appears to require the ability to
	// put back an arbitrary number of characters, in practice we only
	// ever put back a single character. 
	virtual void UnreadCharacter(void) = 0;
	virtual bool AtEnd(void) const = 0;
protected:
	int _previous_character;
};

// This source allows input from a traditional C FILE *
class FileLexerSource : public LexerSource
{
public:
	FileLexerSource(FILE *file);
	virtual ~FileLexerSource();

	virtual void SetNewSource(FILE *file);
	
	virtual int ReadCharacter(void);
	virtual void UnreadCharacter(void);
	virtual bool AtEnd(void) const;

private:
	FILE *_file;
};

// This source allows input from a C++ stream. Note that
// the user passes in a pointer to the stream.
class InputStreamLexerSource : public LexerSource
{
public:
	InputStreamLexerSource(istream *stream);
	virtual ~InputStreamLexerSource();

	virtual void SetNewSource(istream *stream);
	
	virtual int ReadCharacter(void);
	virtual void UnreadCharacter(void);
	virtual bool AtEnd(void) const;

private:
	istream *_stream;
};

// This source allows input from a traditional C string.
class CharLexerSource : public LexerSource
{
public:
	CharLexerSource(const char *string, int offset=0);
	virtual ~CharLexerSource();
	
	virtual void SetNewSource(const char *string, int offset=0);
	virtual int ReadCharacter(void);
	virtual void UnreadCharacter(void);
	virtual bool AtEnd(void) const;

	virtual int GetCurrentLocation(void) const;
private:
	const char *_source_start;
	const char *_current;
};

// This source allows input from a C++ string.
class StringLexerSource : public LexerSource
{
public:
	StringLexerSource(const string *string, int offset=0);
	virtual ~StringLexerSource();

	virtual void SetNewSource(const string *string, int offset=0);
	
	virtual int ReadCharacter(void);
	virtual void UnreadCharacter(void);
	virtual bool AtEnd(void) const;

	virtual int GetCurrentLocation(void) const;
private:
	const string *_string;
	int           _offset;
};

#endif /* __LEXER_SOURCE_H__ */
