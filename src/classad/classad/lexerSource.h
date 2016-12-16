/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef __CLASSAD_LEXER_SOURCE_H__
#define __CLASSAD_LEXER_SOURCE_H__

#include "classad/common.h"

namespace classad {

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
private:
    // The copy constructor and assignment operator are defined
    // to be private so we don't have to write them, or worry about
    // them being inappropriately used. The day we want them, we can 
    // write them. 
    LexerSource(const LexerSource &)            { return;       }
    LexerSource &operator=(const LexerSource &) { return *this; }
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
    FileLexerSource(const FileLexerSource &) : LexerSource() { return;  }
    FileLexerSource &operator=(const FileLexerSource &) { return *this; }
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
	const char *_string;
	int         _offset;
    CharLexerSource(const CharLexerSource &) : LexerSource() { return;       }
    CharLexerSource &operator=(const CharLexerSource &) { return *this; }
};

// This source allows input from a C++ string.
class StringLexerSource : public LexerSource
{
public:
	StringLexerSource(const std::string *string, int offset=0);
	virtual ~StringLexerSource();

	virtual void SetNewSource(const std::string *string, int offset=0);
	
	virtual int ReadCharacter(void);
	virtual void UnreadCharacter(void);
	virtual bool AtEnd(void) const;

	virtual int GetCurrentLocation(void) const;
private:
	const std::string *_string;
	int                _offset;
    StringLexerSource(const StringLexerSource &) : LexerSource() { return;       }
    StringLexerSource &operator=(const StringLexerSource &) { return *this; }
};

}

#endif /* __CLASSAD_LEXER_SOURCE_H__ */
