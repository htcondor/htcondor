/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_string.h"

#include "env.h"

// Since ';' is the PATH delimiter in Windows, we use a different
// delimiter for environment entries.
static const char unix_env_delim = ';';
static const char windows_env_delim = '|';

#ifndef WIN32
static const char env_delimiter = ';';
#else
static const char env_delimiter = '|';
#endif

Env::Env()
{
	_envTable = new HashTable<MyString, MyString>
		( 127, &MyStringHash, updateDuplicateKeys );
	ASSERT( _envTable );
	generate_parse_messages = false;
}

Env::~Env()
{
	delete _envTable;
}

void
Env::GenerateParseMessages(bool flag)
{
	generate_parse_messages=flag;
}

void
Env::ClearParseMessages()
{
	parse_messages = "";
}

char const *
Env::GetParseMessages()
{
	return parse_messages.Value();
}
void
Env::AddParseMessage(char const *msg)
{
	if(!generate_parse_messages) return;

	char const *existing_msg = parse_messages.Value();
	if(existing_msg && *existing_msg) {
		// each message is separated by a newline
		parse_messages += "\n";
	}
	parse_messages += msg;
}

bool
Env::Merge( const char **stringArray )
{
	if( !stringArray ) {
		return false;
	}
	int i;
	for( i = 0; stringArray[i] && stringArray[i][0] != '\0'; i++ ) {
		if( !Put( stringArray[i] ) ) {
			return false;
		}
	}
	return true;
}

bool
Env::IsSafeEnvValue(char const *str) const {
	// This is used to filter out stuff containing special
	// characters from the environment in condor_submit.
	// Once we support escaping, there will be no need for this.

	if(!str) return false;

	char specials[] = {'|','\n','\0'};
	// Some compilers do not like env_delimiter to be in the
	// initialization constant.
	specials[0] = env_delimiter;

	size_t safe_length = strcspn(str,specials);

	// If safe_length goes to the end of the string, we are okay.
	return !str[safe_length];
}

void
Env::WriteToDelimitedString(char const *input,MyString &output) {
	// Append input to output.
	// Would be nice to escape special characters here, but the
	// existing syntax does not support it, so we leave the
	// "specials" strings blank.

	char const inner_specials[] = {'\0'};
	char const first_specials[] = {'\0'};

	char const *specials = first_specials;
	char const *end;
	bool ret;

	if(!input) return;

	while(*input) {
		end = input + strcspn(input,specials);
		ret = output.sprintf_cat("%.*s",end-input,input);
		ASSERT(ret);
		input = end;

		if(*input != '\0') {
			// Escape this special character.
			// Escaping is not yet implemented, so we will never get here.
			ret = output.sprintf_cat("%c",*input);
			ASSERT(ret);
			input++;
		}

		// Switch out of first-character escaping mode.
		specials = inner_specials;
	}
}

bool
Env::ReadFromDelimitedString(char const *&input, char *output) {
	// output buffer must be big enough to hold next environment entry
	// (to be safe, it should be same size as input buffer)

	// strip leading (non-escaped) whitespace
	while( *input==' ' || *input=='\t' || *input=='\n'  || *input=='\r') {
		input++;
	}

	while( *input ) {
		if(*input == '\n' || *input == env_delimiter) {
			// for backwards compatibility with old env parsing in environ.C,
			// we also treat '\n' as a valid delimiter
			input++;
			break;
		}
		else {
			// all other characters are copied verbatim
			*(output++) = *(input++);
		}
	}

	*output = '\0';

	return true;
}

bool
Env::Merge( const char *delimitedString )
{
	char const *input;
	char *output;
	int outputlen;
	bool retval = true;

	if(!delimitedString) return true;

	// create a buffer big enough to hold any of the individual env expressions
	outputlen = strlen(delimitedString)+1;
	output = new char[outputlen];
	ASSERT(output);

	input = delimitedString;
	while( *input ) {
		retval = ReadFromDelimitedString(input,output);

		if(!retval) {
			break; //failed to parse environment string
		}

		if(*output) {
			retval = Put(output);
			if(!retval) {
				break; //failed to add environment expression
			}
		}
	}
	delete[] output;
	return retval;
}

bool
Env::Put( const char *nameValueExpr )
{
	char *expr, *delim;
	int retval;

	if( nameValueExpr == NULL || nameValueExpr[0] == '\0' ) {
		return false;
	}

	// make a copy of nameValueExpr for modifying
	expr = strnewp( nameValueExpr );
	ASSERT( expr );

	// find the delimiter
	delim = strchr( expr, '=' );

	// fail if either name or delim is missing
	if( expr == delim || delim == NULL ) {
		if(generate_parse_messages) {
			MyString msg;
			if(delim == NULL) {
				msg.sprintf(
				  "ERROR: Missing '=' after environment variable '%s'.",
				  nameValueExpr);
			}
			else {
				msg.sprintf("ERROR: missing variable in '%s'.",expr);
			}
			AddParseMessage(msg.Value());
		}
		delete[] expr;
		return false;
	}

	// overwrite delim with '\0' so we have two valid strings
	*delim = '\0';

	// do the deed
	retval = Put( expr, delim + 1 );
	delete[] expr;
	return retval;
}

bool
Env::Put( const char* var, const char* val )
{
	MyString myVar = var;
	MyString myVal = val;
	return Put( myVar, myVal );
}

bool
Env::Put( const MyString & var, const MyString & val )
{
	if( var.Length() == 0 ) {
		return false;
	}
	return _envTable->insert( var, val ) == 0;
}

char *
Env::getDelimitedString(char delim)
{
	MyString var, val, string;

	bool emptyString = true;
	if(!delim) {
		delim = env_delimiter;
	}
	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		// only insert the delimiter if there's already an entry...
        if( !emptyString ) {
			string += delim;
        }
		WriteToDelimitedString(var.Value(),string);
		WriteToDelimitedString("=",string);
		WriteToDelimitedString(val.Value(),string);
		emptyString = false;
	}

	// return a string allocated by "new"

	int slen = string.Length();
	char *s = new char[ slen + 1 ];
	ASSERT( s );
	strcpy( s, string.Value() );
	s[slen] = '\0';
	return s;
}

char *
Env::getDelimitedStringForOpSys(char const *opsys)
{
	char delim;
	if(!opsys || !*opsys) {
		if(generate_parse_messages) {
			AddParseMessage("OpSys is not defined.");
		}
		return NULL;
	}
	if(!strncmp(opsys,"WINNT",5) || !strncmp(opsys,"WIN32",5)) {
		delim = windows_env_delim;
	}
	else {
		delim = unix_env_delim;
	}
	return getDelimitedString(delim);
}

char *
Env::getNullDelimitedString()
{
	MyString var, val;
	char *output, *output_pos;
	size_t length = 1; // reserve one space for final null

	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		// reserve space for "var=val" plus null
		length += var.Length() + val.Length() + 2;
	}

	output = new char[length];
	ASSERT( output );
	output_pos = output;

	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		sprintf(output_pos,"%s=%s",var.Value(),val.Value());
		output_pos += var.Length() + val.Length() + 2;
	}

	*output_pos = '\0'; //append the final null
	return output;
}

char **
Env::getStringArray() {
	char **array = NULL;
	int numVars = _envTable->getNumElements();
	int i;

	array = new char*[ numVars+1 ];
	ASSERT( array );

    MyString var, val;

    _envTable->startIterations();
	for( i = 0; _envTable->iterate( var, val ); i++ ) {
		ASSERT( i < numVars );
		ASSERT( var.Length() > 0 );
		array[i] = new char[ var.Length() + val.Length() + 2 ];
		ASSERT( array[i] );
		strcpy( array[i], var.Value() );
		strcat( array[i], "=" );
		strcat( array[i], val.Value() );
	}
	array[i] = NULL;
	return array;
}

bool
Env::getenv(MyString const &var,MyString &val) const
{
	// lookup returns 0 on success
	return _envTable->lookup(var,val) == 0;
}
