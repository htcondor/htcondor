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

#include "common.h"
#include "lexerSource.h"

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
	fseek(_file, -1, SEEK_CUR);
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

InputStreamLexerSource::InputStreamLexerSource(istream *stream)
{
	SetNewSource(stream);
	return;
}

InputStreamLexerSource::~InputStreamLexerSource()
{
	_stream = NULL;
	return;
}

void InputStreamLexerSource::SetNewSource(istream *stream)
{
	_stream = stream;
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
	_stream->seekg(-1, ios::cur);
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
	_source_start = string+offset;
	_current      = _source_start;
	return;
}

int 
CharLexerSource::ReadCharacter(void)
{
	int character;

	character = *_current;
	if (character == 0) {
		character = -1; 
	} else {
		_current++;
	}

 	_previous_character = character;
	return character;
}

void 
CharLexerSource::UnreadCharacter(void)
{
	if (_current > _source_start) {
		_current--;
	}
	return;
}

bool 
CharLexerSource::AtEnd(void) const
{
	bool at_end;

	at_end = (*_current == 0);
	return at_end;
}

int 
CharLexerSource::GetCurrentLocation(void) const
{
	return _current - _source_start;
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
