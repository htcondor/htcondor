/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#include "common.h"
#include "lexerSource.h"
using namespace std;

/*--------------------------------------------------------------------
 *
 * FileLexerSource
 *
 *-------------------------------------------------------------------*/

FileLexerSource::FileLexerSource(FILE *file)
{
	SetNewSource(file);
	return;
}

FileLexerSource::~FileLexerSource()
{
	_file = NULL;
	return;
}

void FileLexerSource::SetNewSource(FILE *file)
{
	_file = file;
	return;
}

int 
FileLexerSource::ReadCharacter(void)
{
	int character;

	if (_file != NULL) {
		character = fgetc(_file);
	} else {
		character = -1;
	}
 	_previous_character = character;
	return character;
}

void 
FileLexerSource::UnreadCharacter(void)
{
	//fseek(_file, -1, SEEK_CUR);
	ungetc(_previous_character, _file);
	return;
}

bool 
FileLexerSource::AtEnd(void) const
{
	bool at_end;
	
	if (_file != NULL) {
		at_end = (feof(_file) != 0);
	} else {
		at_end = true;
	}
	return at_end;
}

/*--------------------------------------------------------------------
 *
 * InputStreamLexerSource
 *
 *-------------------------------------------------------------------*/

InputStreamLexerSource::InputStreamLexerSource(istream &stream) 
{
	SetNewSource(stream);
	return;
}

InputStreamLexerSource::~InputStreamLexerSource()
{
	return;
}

void InputStreamLexerSource::SetNewSource(istream &stream)
{
	_stream = &stream;
	return;
}

int 
InputStreamLexerSource::ReadCharacter(void)
{
	char real_character;
	int  character;

	if (_stream != NULL && !_stream->eof()) {
		_stream->get(real_character);
		character = real_character;
	} else {
		character = -1;
	}
   _previous_character = character;
	return character;
}

void 
InputStreamLexerSource::UnreadCharacter(void)
{
	//doesn't work on cin
	//_stream->seekg(-1, ios::cur);
	_stream->putback(_previous_character);
	return;
}

bool 
InputStreamLexerSource::AtEnd(void) const
{
	bool at_end;
	
	if (_stream != NULL) {
		at_end = (_stream->eof());
	} else {
		at_end = true;
	}
	return at_end;
}

/*--------------------------------------------------------------------
 *
 * CharLexerSource
 *
 *-------------------------------------------------------------------*/

CharLexerSource::CharLexerSource(const char *string, int offset)
{
	SetNewSource(string, offset);
	return;
}

CharLexerSource::~CharLexerSource()
{
	return;
}

void CharLexerSource::SetNewSource(const char *string, int offset)
{
    _string = string;
    _offset = offset;
	return;
}

int 
CharLexerSource::ReadCharacter(void)
{
	int character;

	character = _string[_offset];
	if (character == 0) {
		character = -1; 
	} else {
        _offset++;
	}

 	_previous_character = character;
	return character;
}

void 
CharLexerSource::UnreadCharacter(void)
{
	if (_offset > 0) {
		_offset--;
	}
	return;
}

bool 
CharLexerSource::AtEnd(void) const
{
	bool at_end;

    if (_string[_offset] == 0) {
        at_end = true;
    } else {
        at_end = false;
    }
	return at_end;
}

int 
CharLexerSource::GetCurrentLocation(void) const
{
	return _offset;
}

/*--------------------------------------------------------------------
 *
 * StringLexerSource
 *
 *-------------------------------------------------------------------*/

StringLexerSource::StringLexerSource(const string *string, int offset)
{
	SetNewSource(string, offset);
	return;
}

StringLexerSource::~StringLexerSource()
{
	return;
}
	
void StringLexerSource::SetNewSource(const string *string, int offset)
{
	_string = string;
	_offset = offset;
	return;
}

int 
StringLexerSource::ReadCharacter(void)
{
	int character;

	character = (*_string)[_offset];
	if (character == 0) {
		character = -1;
	} else {
		_offset++;
	}
 	_previous_character = character;
	return character;
}

void 
StringLexerSource::UnreadCharacter(void)
{
	if (_offset > 0) {
		_offset--;
	}
	return;
}

bool 
StringLexerSource::AtEnd(void) const
{
	bool at_end;

	if ((*_string)[_offset] == 0) {
		at_end = true;
	} else {
		at_end = false;
	}
	return at_end;
}

int 
StringLexerSource::GetCurrentLocation(void) const
{
	return _offset;
}
