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
	int _previous_character{0};
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
	FileLexerSource(FILE *file=nullptr) : _file(file) {}
	FileLexerSource(const LexerSource &) = delete;
	FileLexerSource &operator=(const LexerSource &) = delete;

	virtual ~FileLexerSource() { _file = nullptr; }

	virtual void SetNewSource(FILE *file) {
		_file = file;
	}
	
	virtual int ReadCharacter(void) {
		if (_file) {
			_previous_character = fgetc(_file);
		} else {
			_previous_character = EOF;
		}
		return _previous_character;
	}

	virtual void UnreadCharacter(void) {
		ungetc(_previous_character, _file);
	}

	virtual bool AtEnd(void) const {
		if (_file) {
			return (feof(_file) != 0);
		}
		return true;
	}

protected:
	FILE *_file{nullptr};
};

// This source allows input from a traditional C string.
class CharLexerSource : public LexerSource
{
public:
	CharLexerSource(const char *string, int offset=0) : _offset(offset), _string(string) {}
	virtual ~CharLexerSource() = default;
	
	virtual void SetNewSource(const char *string, int offset=0) { _string = string; _offset = offset; }
	virtual int ReadCharacter(void) {
		_previous_character = (unsigned char)_string[_offset];
		if (_previous_character == 0) {
			_previous_character = EOF;
		} else {
			_offset++;
		}
		return _previous_character;
	}
	virtual void UnreadCharacter(void) { if (_offset > 0) --_offset; }
	virtual bool AtEnd(void) const { return _string[_offset] == 0; }

	virtual int GetCurrentLocation(void) const { return _offset; }
protected:
	int         _offset{0};
	const char *_string{nullptr};
};

// This source allows input from a C++ string.
class StringLexerSource : public LexerSource
{
public:
	StringLexerSource(const std::string *string, int offset=0) : _offset(offset), _string(string) {}
	virtual ~StringLexerSource() = default;

	virtual void SetNewSource(const std::string *string, int offset=0) {
		_string = string;
		_offset = offset;
	}
	
	virtual int ReadCharacter(void) {
		if ((size_t)_offset >= _string->size()) {
			_previous_character = EOF;
			return EOF;
		}
		_previous_character = (unsigned char)(*_string)[_offset];
		if (_previous_character == 0) {
			_previous_character = EOF;
		} else {
			_offset++;
		}
		return _previous_character;
	}

	virtual void UnreadCharacter(void) {
		if (_offset > 0) { --_offset; }
	}

	virtual bool AtEnd(void) const {
		if ((size_t)_offset >= _string->size() || (*_string)[_offset] == 0) {
			return true;
		}
		return false;
	}

	virtual int GetCurrentLocation(void) const { return _offset; }
protected:
	int                _offset{0};
	const std::string *_string{nullptr};
};

class StringViewLexerSource : public LexerSource
{
public:
	StringViewLexerSource(std::string_view sv, int offset=0) : _offset(offset), _strview(sv) {};
	StringViewLexerSource() = default;
	virtual ~StringViewLexerSource() = default;

	virtual void SetNewSource(std::string_view sv, int offset=0) {
		_strview = sv;
		_offset = offset;
	}

	virtual int ReadCharacter(void) {
		if ((size_t)_offset >= _strview.size()) {
			_offset = (int)_strview.size();
			_previous_character = EOF;
			return EOF;
		}
		_previous_character = (unsigned char)_strview[_offset];
		if (_previous_character == 0) {
			_previous_character = EOF;
		} else {
			++_offset;
		}
		return _previous_character;
	}
	virtual void UnreadCharacter(void) {
		if (_offset > 0) { --_offset; }
	}
	virtual bool AtEnd(void) const {
		if ((size_t)_offset >= _strview.size() || _strview[_offset] == 0) {
			return true;
		}
		return false;
	}
	virtual int GetCurrentLocation(void) const {
		return _offset;
	}

protected:
	int              _offset{0};
	std::string_view _strview{nullptr, 0};
};


}

#endif /* __CLASSAD_LEXER_SOURCE_H__ */
