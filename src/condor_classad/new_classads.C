/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_new_classads.h"

/*-------------------------------------------------------------------------
 *
 * Private Datatypes
 *
 * (Note that these are shared by the two classes (NewClassAdParser and
 *  NewClassAdUnparser) so they don't belong in the class definition, yet
 *  they are certainly not public.)
 *
 *-------------------------------------------------------------------------*/

class DataSource
{
public:
	DataSource()
	{
		return;
	}
	virtual ~DataSource()
	{
		return;
	}
	
	virtual int ReadCharacter(void) = 0;
	virtual void PushbackCharacter(void) = 0;
	virtual bool AtEnd(void) const = 0;
};

class FileDataSource : public DataSource
{
public:
	FileDataSource(FILE *file);
	virtual ~FileDataSource();
	
	virtual int ReadCharacter(void);
	virtual void PushbackCharacter(void);
	virtual bool AtEnd(void) const;

private:
	FILE *_file;
};

class CharDataSource : public DataSource
{
public:
	CharDataSource(const char *string);
	virtual ~CharDataSource();
	
	virtual int ReadCharacter(void);
	virtual void PushbackCharacter(void);
	virtual bool AtEnd(void) const;

	int GetCurrentLocation(void) const;
private:
	const char *_source_start;
	const char *_current;
};


/*-------------------------------------------------------------------------
 *
 * NewClassAdParser
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: NewClassAdParser Constructor
 * Purpose:  Constructor and debug check
 *
 **************************************************************************/
NewClassAdParser::NewClassAdParser()
{
	return;
}

/**************************************************************************
 *
 * Function: NewClassAdParser Destructor
 * Purpose:  We don't actually have anything to destruct
 *
 **************************************************************************/
NewClassAdParser::~NewClassAdParser()
{
	return;
}

/**************************************************************************
 *
 * Function: ParseClassAd
 * Purpose:  Parse a ClassAd from a string buffer. 
 *
 **************************************************************************/
ClassAd *
NewClassAdParser::ParseClassAd(const char *buffer)
{
	CharDataSource source(buffer);
	
	return _ParseClassAd(source);
}

/**************************************************************************
 *
 * Function: ParseClassAd
 * Purpose:  Parse a ClassAd from a string buffer that we expect to 
 *           contain multiple ClassAds. Because of this, we return an
 *           integer (place) that you can pass to this function to read the
 *           next ClassAd. 
 *
 **************************************************************************/
ClassAd *
NewClassAdParser::ParseClassAds(const char *buffer, int *place)
{
	ClassAd       *classad;
	CharDataSource source(buffer + *place);
	
	classad = _ParseClassAd(source);
	*place = source.GetCurrentLocation();

	return classad;
}

/**************************************************************************
 *
 * Function: ParseClassAd
 * Purpose:  Parse a single ClassAd from a file.
 *
 **************************************************************************/
ClassAd *
NewClassAdParser::ParseClassAd(FILE *file)
{
	FileDataSource source(file);

	ClassAd *c = _ParseClassAd(source);

	return c;
}

/**************************************************************************
 *
 * Function: _ParseClassAd
 * Purpose:  This parses a New ClassAds ad from some source. This is a
 *           private function used by the other ParseClassAds. It uses a
 *           DataSource class so that it doesn't have to know about files
 *           or strings. Pretty much all of the parsing happens here. 
 * See Also: ReadToken()
 *
 **************************************************************************/
ClassAd *
NewClassAdParser::_ParseClassAd(DataSource &source)
{
	int character;
	bool done = false;
	bool in_string = false;
	bool in_classad = false;
	ClassAd *ad = NULL;
	MyString new_attr;
	int rc;

	ad = new ClassAd();
	if ( ad == NULL ) {
		goto _ParseClassAd_failed;
	}

	for ( character = source.ReadCharacter(); !done && character != EOF;
		  character = source.ReadCharacter() ) {

		// These characters require special treatment only if they're not
		// in a quoted string
		if ( !in_string ) {

			// Skip white-space that's not in a string
			if ( character == ' ' || character == '\t' || character == '\n' ||
				 character == '\r' ) {
				continue;
			}

			// We should only see this at the beginning of the ad
			if ( character == '[' ) {
				if ( in_classad ) {
					break;
				}
				in_classad = true;
				continue;
			}

			// End of the attribute/value pair. Insert it into the ad.
			if ( character == ';' ) {
				if ( new_attr.Length() != 0 ) {
					rc = ad->Insert( new_attr.Value() );
					if ( rc == FALSE ) {
						break;
					}
					new_attr = "";
				} else {
				}
				continue;
			}

			// End of the classad. Insert the last attribute/value pair
			// into the ad and break;
			if ( character == ']' ) {
				if ( new_attr.Length() != 0 ) {
					rc = ad->Insert( new_attr.Value() );
					if ( rc == FALSE ) {
						break;
					}
					new_attr = "";
				} else {
				}
				in_classad = false;
				done = true;
				continue;
			}

			if ( character == '"' ) {
				in_string = true;
			}

			new_attr += (char)character;

		} else {

			if ( character == '"' ) {
				in_string = false;
			}

			if ( character == '\\' ) {
				character = source.ReadCharacter();
				if ( character != '\\' && character != '"' ) {
					break;
				}
				if ( character == '"' ) {
					new_attr += '\\';
				}
			}

			new_attr += (char)character;

		}

	}

	if ( !done ) {
		goto _ParseClassAd_failed;
	}

	return ad;
 _ParseClassAd_failed:
	if ( ad != NULL ) {
		delete ad;
	}
	return NULL;
}

/*-------------------------------------------------------------------------
 *
 * NewClassAdUnparser
 *
 *-------------------------------------------------------------------------*/


/**************************************************************************
 *
 * Function: NewClassAdUnparser constructor
 * Purpose:  Set up defaults, do a debug check.
 *
 **************************************************************************/
NewClassAdUnparser::NewClassAdUnparser()
{
	_use_compact_spacing = true;
	_output_type = true;
	_output_target_type = true;
	return;
}

/**************************************************************************
 *
 * Function: NewClassAdUnparser destructor
 * Purpose:  Currently, nothing needs to happen here.
 *
 **************************************************************************/
NewClassAdUnparser::~NewClassAdUnparser()
{
	return;
}

/**************************************************************************
 *
 * Function: GetUseCompactSpacing
 * Purpose:  Tells you if we print out the classads compactly (one classad
 *           per line) or not.
 * THIS FUNCTIONALITY IS UNIMPLEMENTED! COMPACT SPACING IS ALWAYS USED!
 *
 **************************************************************************/
bool 
NewClassAdUnparser::GetUseCompactSpacing(void)
{
	return _use_compact_spacing;
}

/**************************************************************************
 *
 * Function: SetUseCompactSpacing
 * Purpose:  Allows the caller to decide if we should print out one
 *           classad per line (compact spacing), or print each attribute
 *           on a separate line (not compact spacing). 
 * THIS FUNCTIONALITY IS UNIMPLEMENTED! COMPACT SPACING IS ALWAYS USED!
 *
 **************************************************************************/
void 
NewClassAdUnparser::SetUseCompactSpacing(bool use_compact_spacing)
{
	_use_compact_spacing = use_compact_spacing;
	return;
}

/**************************************************************************
 *
 * Function: GetOutputType
 * Purpose:  Tells you if we print out the Type Attribute
 *
 **************************************************************************/
bool 
NewClassAdUnparser::GetOutputType(void)
{
	return _output_type;
}

/**************************************************************************
 *
 * Function: SetOutputType
 * Purpose:  Sets if we print out the Type Attribute or not
 *
 **************************************************************************/
void 
NewClassAdUnparser::SetOutputType(bool output_type)
{
	_output_type = output_type;
	return;
}

/**************************************************************************
 *
 * Function: GetOutputTargetType
 * Purpose:  Tells you if we print out the TargetType Attribute
 *
 **************************************************************************/
bool 
NewClassAdUnparser::GetOuputTargetType(void)
{
	return _output_target_type;
}

/**************************************************************************
 *
 * Function: SetOutputTargettype
 * Purpose:  Sets if we print out the TargetType Attribute or not
 *
 **************************************************************************/
void 
NewClassAdUnparser::SetOutputTargetType(bool output_target_type)
{
	_output_target_type = output_target_type;
	return;
}


/**************************************************************************
 *
 * Function: Unparse
 * Purpose:  Converts a ClassAd into a new ClassAds representation, and
 *           appends it to a MyString. Note that the exact appearance of the
 *           representation is governed by two attributes you can set:
 *           compact spacing and compact names.
 *
 **************************************************************************/
bool 
NewClassAdUnparser::Unparse(ClassAd *classad, MyString &buffer)
{
	ExprTree *expr;
	char *expr_str = NULL;

	if ( classad == NULL ) {
		return false;
	}

	buffer += "[ ";

	// First get the MyType and TargetType expressions 
	if (_output_type) {
		const char *mytype = classad->GetMyTypeName();
		if (*mytype != 0) {
			buffer += "MyType = \"";
			buffer += mytype;
			buffer += "\"; ";
		}
	}

	if (_output_target_type) {
		const char *targettype = classad->GetTargetTypeName();
		if (*targettype != 0) {
			buffer += "TargetType = \"";
			buffer += targettype;
			buffer += "\"; ";
		}
	}

	classad->ResetExpr();
	while( ( expr = classad->NextExpr() ) != NULL ) {

		buffer += ((Variable*)expr->LArg())->Name();

		buffer += " = ";

		expr->RArg()->PrintToNewStr( &expr_str );

		bool in_string = false;
		char *curr_pos = expr_str;
		while ( 1 ) {

			if ( !in_string ) {

				if ( *curr_pos == '\0' ) {
					break;
				}

				if ( *curr_pos == '"' ) {
					in_string = true;
				}

				buffer += *curr_pos;

			} else {

				if ( *curr_pos == '"' ) {
					in_string = false;
				}

				if ( *curr_pos == '\\' ) {
					buffer += "\\";
					if ( curr_pos[1] == '"' && curr_pos[2] != '\0' ) {
						// This backslash is escaping a double-quote
						curr_pos++;
					}
				}

				buffer += *curr_pos;

			}

			curr_pos++;
		}

			// TODO We should add a new-line here and possibly indent the
			//   next line if _use_compact_spacing is false.
		buffer += "; ";

		free( expr_str );
	}

	buffer += "]";
//	buffer += "\n";

	return true;
}

/*-------------------------------------------------------------------------
 *
 * FileDataSource
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: FileDataSource constructor
 * Purpose:  
 *
 **************************************************************************/
FileDataSource::FileDataSource(FILE *file)
{
	_file = file;
	return;
}

/**************************************************************************
 *
 * Function: FileDataSource destructor
 * Purpose:  
 *
 **************************************************************************/
FileDataSource::~FileDataSource()
{
	_file = NULL;
	return;
}

/**************************************************************************
 *
 * Function: ReadCharacter
 * Purpose:  Returns a single character
 *
 **************************************************************************/
int 
FileDataSource::ReadCharacter(void)
{
	return fgetc(_file);
}

/**************************************************************************
 *
 * Function: PushbackCharacter
 * Purpose:  Unreads a single character, so it can be read again later.
 *
 **************************************************************************/
void 
FileDataSource::PushbackCharacter(void)
{
	fseek(_file, -1, SEEK_CUR);
	return;
}

/**************************************************************************
 *
 * Function: AtEnd
 * Purpose:  Returns true if we are at the end of a file, false otherwise.
 *
 **************************************************************************/
bool
FileDataSource::AtEnd(void) const
{
	bool at_end;
	
		// TODO The semantics of AtEnd() differ between CharDataSource and
		//   FileDataSource. In FileDataSource, AtEnd() returns true if the
		//   caller has tried to read beyond the end of the input stream.
		//   In CharDataSource, AtEnd() returns true if the caller has read
		//   the last character of the input stream (but not necessarily
		//   read beyond it yet). This same inconsistency is present in the
		//   *XMLSource classes in xml_classads.C.
	at_end = (feof(_file) != 0);
	return at_end;
}

/*-------------------------------------------------------------------------
 *
 * CharDataSource
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: CharDataSource constructor
 * Purpose:  
 *
 **************************************************************************/
CharDataSource::CharDataSource(const char *string)
{
	_source_start = string;
	_current      = string;
	return;
}

/**************************************************************************
 *
 * Function: CharDataSource destructor
 * Purpose:  
 *
 **************************************************************************/
CharDataSource::~CharDataSource()
{
	return;
}
	
/**************************************************************************
 *
 * Function: ReadCharacter
 * Purpose:  Reads a single character
 *
 **************************************************************************/
int 
CharDataSource::ReadCharacter(void)
{
	int character;

	character = *_current;
	if (character == 0) {
		character = EOF;
	} else {
		_current++;
	}

	return character;
}

/**************************************************************************
 *
 * Function: PushbackCharacter
 * Purpose:  Unreads a single character, so it can be read again later.
 *
 **************************************************************************/
void 
CharDataSource::PushbackCharacter(void)
{
	if (_current > _source_start) {
		_current--;
	}
	return;
}


/**************************************************************************
 *
 * Function: GetCurrentLocation
 * Purpose:  Gets the current location within an CharDataSource. This is
 *           used for the string version of ParseClassAd() that returns
 *           the place in the string so that we can read the next ClassAd.
 *
 **************************************************************************/
int 
CharDataSource::GetCurrentLocation(void) const
{
	return _current - _source_start;
}

/**************************************************************************
 *
 * Function: AtEnd
 * Purpose:  Returns true if we are at the end of a file, false otherwise.
 *
 **************************************************************************/
bool
CharDataSource::AtEnd(void) const
{
	bool at_end;

		// TODO The semantics of AtEnd() differ between CharDataSource and
		//   FileDataSource. In FileDataSource, AtEnd() returns true if the
		//   caller has tried to read beyond the end of the input stream.
		//   In CharDataSource, AtEnd() returns true if the caller has read
		//   the last character of the input stream (but not necessarily
		//   read beyond it yet). This same inconsistency is present in the
		//   *XMLSource classes in xml_classads.C.
	at_end = (*_current == 0);
	return at_end;
}
