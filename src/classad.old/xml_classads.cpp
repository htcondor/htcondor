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


#include "condor_common.h"
#include "condor_string.h"
#include "condor_xml_classads.h"

/*-------------------------------------------------------------------------
 *
 * Private Datatypes
 *
 * (Note that these are shared by the two classes (XML parser and unparser)
 *  so they don't belong in the class definition, yet they are certainly not 
 *  public.)
 *
 *-------------------------------------------------------------------------*/

enum XMLTokenType
{
	XMLToken_Tag,
	XMLToken_Text,
	XMLToken_Invalid
};

class XMLSource
{
public:
	XMLSource()
	{
		return;
	}
	virtual ~XMLSource()
	{
		return;
	}
	
	virtual int ReadCharacter(void) = 0;
	virtual void PushbackCharacter(void) = 0;
	virtual bool AtEnd(void) const = 0;
};

class FileXMLSource : public XMLSource
{
public:
	FileXMLSource(FILE *file);
	virtual ~FileXMLSource();
	
	virtual int ReadCharacter(void);
	virtual void PushbackCharacter(void);
	virtual bool AtEnd(void) const;

private:
	FILE *_file;
};

class CharXMLSource : public XMLSource
{
public:
	CharXMLSource(const char *string);
	virtual ~CharXMLSource();
	
	virtual int ReadCharacter(void);
	virtual void PushbackCharacter(void);
	virtual bool AtEnd(void) const;

	int GetCurrentLocation(void) const;
private:
	const char *_source_start;
	const char *_current;
};

class XMLToken
{
public: 
	XMLToken();
	~XMLToken();

	void         SetType(XMLTokenType type);
	XMLTokenType GetType(void) const;

	void         SetText(const char *text);
	void         GetText(char **text) const;

	void         SetTag(TagName tag);
	TagName      GetTag(void) const;

	void         SetTagIsEnd(bool is_end);
	bool         GetTagIsEnd(void);

	void         SetAttribute(const char *name, const char *value);
	bool         GetAttribute(MyString &name, MyString &value);

	void         Dump(void);
private:
	XMLTokenType  _type;
	TagName       _tag;
	bool          _is_end;
    char         *_text;
	char         *_attribute_name;
	char         *_attribute_value;
};

struct tag_name
{
	TagName  id; // Defined in the condor_xml_classads.h
	char     *name;
};

#define NUMBER_OF_TAG_NAMES (sizeof(tag_names) / sizeof(struct tag_name))
static struct tag_name tag_names[] = 
{
	{tag_ClassAds,  "classads", },
	{tag_ClassAd,   "c",   },
	{tag_Attribute, "a",   },
	// tag_Number is only for backwards compatibility--it's never generated
	{tag_Number,    "n",   }, 
	{tag_Integer,   "i",   },
	{tag_Real,      "r",   },
	{tag_String,    "s",   },
	{tag_Bool,      "b",   },
	{tag_Undefined, "un",  },
	{tag_Error,     "er",  },
	{tag_Time,      "t",   },
	{tag_List,      "l",   },
	{tag_Expr,      "e",   },
	{tag_NoTag,     "",    }
};

static void debug_check(void);
static XMLToken *ReadToken(XMLSource &source, bool must_be_text);
static TagName interpret_tagname(const char *tag_text);
static void fix_entities(const char *source, MyString &dest);

/*-------------------------------------------------------------------------
 *
 * ClassAdXMLParser
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: ClassAdXMLParser Constructor
 * Purpose:  Constructor and debug check
 *
 **************************************************************************/
ClassAdXMLParser::ClassAdXMLParser()
{
	debug_check();
	return;
}

/**************************************************************************
 *
 * Function: ClassAdXMLParser Destructor
 * Purpose:  We don't actually have anything to destruct
 *
 **************************************************************************/
ClassAdXMLParser::~ClassAdXMLParser()
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
ClassAdXMLParser::ParseClassAd(const char *buffer)
{
	CharXMLSource source(buffer);
	
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
ClassAdXMLParser::ParseClassAds(const char *buffer, int *place)
{
	ClassAd       *classad;
	CharXMLSource source(buffer + *place);
	
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
ClassAdXMLParser::ParseClassAd(FILE *file)
{
	FileXMLSource source(file);

	ClassAd *c = _ParseClassAd(source);

	return c;
}

/**************************************************************************
 *
 * Function: _ParseClassAd
 * Purpose:  This parses a ClassAd from an XML source. This is a private
 *           function used by the other ParseClassAds. It uses an XMLSource
 *           class so that it doesn't have to know about files or strings.
 *           Pretty much all of the parsing happens here. 
 * See Also: ReadToken()
 *
 **************************************************************************/
ClassAd *
ClassAdXMLParser::_ParseClassAd(XMLSource &source)
{
	XMLToken  *token;
	ClassAd   *classad;
	bool      in_classad;
	bool      in_attribute;
	bool      done;
    bool      must_be_text;
	TagName   attribute_type;
	MyString  attribute_name;
	MyString  attribute_value;

	classad = new ClassAd();

	in_classad   = false;
	in_attribute = false;
	done         = false;
    must_be_text = false;

	attribute_type = tag_NoTag;
	
	while (!done && (token = ReadToken(source, must_be_text)) != NULL) {
		bool         is_end_tag;
		TagName      tag_name;
		XMLTokenType token_type;

        must_be_text = false;
		//token->Dump();
		is_end_tag = token->GetTagIsEnd();
		token_type = token->GetType();
		tag_name = token->GetTag();

		if (token_type == XMLToken_Text) {
			if (in_attribute && attribute_type != tag_NoTag 
				&& attribute_name.Length() > 0) {
				
				bool      add_to_classad = true;
				MyString  to_insert;
				char      *token_text_raw;
				MyString  token_text("");
				
				to_insert = attribute_value;
				to_insert += " = ";
				
				token->GetText(&token_text_raw);
				fix_entities(token_text_raw, token_text);
				delete[] token_text_raw;
				
				switch (attribute_type) {
				case tag_String:
					// Type and Target need to be added
					// specially, because stupid old ClassAds
					// handles them differently. *sigh*
					if (attribute_value == "MyType") {
						classad->SetMyTypeName(token_text.Value());
						add_to_classad = false;
					} else if (attribute_value == "TargetType") {
						classad->SetTargetTypeName(token_text.Value());
						add_to_classad = false;
					} else {
						if (token_text[0] != '"') {
							to_insert += '"';
						}
						to_insert += token_text;
						if (token_text[token_text.Length()-1] != '"') {
							to_insert += '"';
						}
					}
					break;
				case tag_Time:
					if (token_text[0] != '\'') {
							to_insert += '\'';
					}
					to_insert += token_text;
					if (token_text[token_text.Length()-1] != '\'') {
						to_insert += '\'';
					}
					break;
				case tag_Bool:
					// We should never have text in a bool, because
					// it should always be defined within the tag
					// itself. 
					add_to_classad = false;
					break;
					// tag_Number is only for backwards compatibility--it's never generated
				case tag_Number:
				case tag_Integer:
				case tag_Expr:
				case tag_Real:
					to_insert += token_text;
					break;
				case tag_Undefined:
					to_insert += "UNDEFINED";
					break;
				case tag_Error:
					to_insert += "ERROR";
					break;
				case tag_List:
				default:
					add_to_classad = false;
					break;
				}
				if (add_to_classad) {
					classad->Insert(to_insert.Value());
				}
			}
		}

		// We ignore stuff that is clearly wrong:
		//   * Attributes not in a ClassAd
		//   * Attribute values not in an attribute
		if (!in_classad && tag_name != tag_ClassAd) {
			delete token;
			continue;
		} else if (tag_name > tag_Attribute && !in_attribute) {
			delete token;
			continue;
		}

		TagName current_tag = token->GetTag();
		switch (current_tag) {
		case tag_ClassAds:
			// We just ignore it. 
			break;
		case tag_ClassAd:
			if (is_end_tag) {
				if (in_classad) {
					done = true;
				}
				in_classad = false;
				break;
			} else {
				in_classad = true;
			}
			break;
		case tag_Attribute:
			if (is_end_tag) {
				in_attribute = false;
				attribute_name = "";
				break;
			} else {
				in_attribute = true;
				attribute_type = tag_NoTag;
				token->GetAttribute(attribute_name, attribute_value);
				if (attribute_name != "n") {
					attribute_name = "";
					attribute_value = "";
				}
			}
		case tag_Bool:
			attribute_type = tag_Bool;
			{
				MyString  to_insert;
				
				to_insert = attribute_value;
				to_insert += " = ";

				MyString bool_attribute_name, bool_attribute_value;
				token->GetAttribute(bool_attribute_name, bool_attribute_value);
				if (bool_attribute_name == "v") {
					if (bool_attribute_value == "t") {
						to_insert += "TRUE";
					} else {
						to_insert += "FALSE";
					}
				}
				classad->Insert(to_insert.Value());
			}
			break;
		case tag_Number:
		case tag_Integer:
		case tag_Real:
		case tag_Undefined:
		case tag_Error:
		case tag_Time:
		case tag_List:
		case tag_Expr:
			attribute_type = current_tag;
			break;
		case tag_String:
			attribute_type = current_tag;
			if (!is_end_tag) {
                must_be_text = true;
            }
            break;
		case tag_NoTag:
		default:
			break;
		}
		delete token;
	}

	return classad;
}

/*-------------------------------------------------------------------------
 *
 * ClassAdXMLUnparser
 *
 *-------------------------------------------------------------------------*/


/**************************************************************************
 *
 * Function: ClassAdXMLUnparser constructor
 * Purpose:  Set up defaults, do a debug check.
 *
 **************************************************************************/
ClassAdXMLUnparser::ClassAdXMLUnparser()
{
	debug_check();
	_use_compact_spacing = true;
	_output_type = true;
	_output_target_type = true;
	return;
}

/**************************************************************************
 *
 * Function: ClassAdXMLUnparser destructor
 * Purpose:  Currently, nothing needs to happen here.
 *
 **************************************************************************/
ClassAdXMLUnparser::~ClassAdXMLUnparser()
{
	return;
}

/**************************************************************************
 *
 * Function: GetUseCompactSpacing
 * Purpose:  Tells you if we print out XML compactly (one classad per line)
 *           or not.
 *
 **************************************************************************/
bool 
ClassAdXMLUnparser::GetUseCompactSpacing(void)
{
	return _use_compact_spacing;
}

/**************************************************************************
 *
 * Function: SetUseCompactSpacing
 * Purpose:  Allows the caller to decide if we should print out one XML
 *           classad per line (compact spacing), or print each attribute
 *           on a separate line (not compact spacing). 
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::SetUseCompactSpacing(bool use_compact_spacing)
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
ClassAdXMLUnparser::GetOutputType(void)
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
ClassAdXMLUnparser::SetOutputType(bool output_type)
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
ClassAdXMLUnparser::GetOuputTargetType(void)
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
ClassAdXMLUnparser::SetOutputTargetType(bool output_target_type)
{
	_output_target_type = output_target_type;
	return;
}


/**************************************************************************
 *
 * Function: AddXMLFileHeader
 * Purpose:  Print the stuff that should appear at the beginning of an
 *           XML file that contains a series of ClassAds.
 *
 **************************************************************************/
void ClassAdXMLUnparser::AddXMLFileHeader(MyString &buffer)
{
	buffer += "<?xml version=\"1.0\"?>\n";
	buffer += "<!DOCTYPE classads SYSTEM \"classads.dtd\">\n";
	buffer += "<classads>\n";
	return;

}

/**************************************************************************
 *
 * Function: AddXMLFileFooter
 * Purpose:  Print the stuff that should appear at the end of an XML file
 *           that contains a series of ClassAds.
 *
 **************************************************************************/
void ClassAdXMLUnparser::AddXMLFileFooter(MyString &buffer)
{
	buffer += "</classads>\n";
	return;

}

/**************************************************************************
 *
 * Function: Unparse
 * Purpose:  Converts a ClassAd into an XML representation, and appends
 *           it to a MyString. Note that the exact appearance of the
 *           representation is governed by two attributes you can set:
 *           compact spacing and compact names.
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::Unparse(ClassAd *classad, MyString &buffer, StringList *attr_white_list)
{
	ExprTree *expression;

	add_tag(buffer, tag_ClassAd, true);
	if (!_use_compact_spacing) {
		buffer += '\n';
	}
	
	// First get the MyType and TargetType expressions 
	const char *mytype, *mytarget;

	if (_output_type && (!attr_white_list || attr_white_list->contains_anycase("MyType")) ) {
		mytype = classad->GetMyTypeName();
		if (*mytype != 0) {
			MyString  type_expr_string("MyType = \"");
			ExprTree  *type_expr;
			
			type_expr_string += mytype;
			type_expr_string += '\"';
			Parse(type_expr_string.Value(), type_expr);
			Unparse(type_expr, buffer);
			delete type_expr;
		}
	}

	if (_output_target_type && (!attr_white_list || attr_white_list->contains_anycase("TargetType"))) {
		mytarget = classad->GetTargetTypeName();
		if (*mytarget != 0) {
			MyString  target_expr_string("TargetType = \"");
			ExprTree  *target_expr;
			
			target_expr_string += mytarget;
			target_expr_string += '\"';
			Parse(target_expr_string.Value(), target_expr);
			Unparse(target_expr, buffer);
			delete target_expr;
		}
	}

	// Then loop through all the other expressions in the ClassAd
	classad->ResetExpr();
	for (expression = classad->NextExpr(); 
		 expression != NULL; 
		 expression = classad->NextExpr()) {
		if( expression->invisible ) {
			continue;
		}
		if( attr_white_list && !attr_white_list->contains_anycase(((VariableBase*)expression->LArg())->Name()) ) {
			continue; // not in white list
		}
		Unparse(expression, buffer);
	}
	add_tag(buffer, tag_ClassAd, false);
	buffer += '\n';
	return;
}

/**************************************************************************
 *
 * Function: Unparse
 * Purpose:  Converts a single expression into an XML representation,
 *           and appends it to a MyString. It is used by Unparse(ClassAd *...)
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::Unparse(ExprTree *expression, MyString &buffer)
{
	// If it isn't an assignment, it's a malformed ClassAd
	if (expression->MyType() == LX_ASSIGN) {
		ExprTree *name_expr;
		ExprTree *value_expr;
		
		name_expr  = expression->LArg();
		value_expr = expression->RArg();
		
		// If the left-hand side of the assignment isn't a variable,
		// it's malformed.
		if (name_expr->MyType() == LX_VARIABLE) {
			const char *name = ((VariableBase *)name_expr)->Name();
			
			add_attribute_start_tag(buffer, name);
			
			MyString  number_string;
			char      *expr_string;
			int       int_number;
            double    double_number;
			MyString  fixed_string;
			
			switch (value_expr->MyType()) {
			case LX_INTEGER:
				int_number = ((IntegerBase *)value_expr)->Value();
				if (value_expr->unit == 'k') {
					int_number *= 1024;
				}
				number_string.sprintf("%d", int_number);
				add_tag(buffer, tag_Integer, true);
				buffer += number_string;
				add_tag(buffer, tag_Integer, false);
				break;
			case LX_FLOAT:
				double_number = ((FloatBase *)value_expr)->Value();
				if (value_expr->unit == 'k') {
					double_number *= 1024;
				}
                number_string.sprintf("%1.15E", double_number);
				add_tag(buffer, tag_Real, true);
				buffer += number_string;
				add_tag(buffer, tag_Real, false);
				break;
			case LX_STRING:
				add_tag(buffer, tag_String, true);
				fix_characters(((StringBase *)value_expr)->Value(), 
							   fixed_string);
				buffer += fixed_string;
				fixed_string = "";
				add_tag(buffer, tag_String, false);
				break;
			case LX_TIME:
				add_tag(buffer, tag_Time, true);
				fix_characters(((ISOTimeBase *)value_expr)->Value(), 
							   fixed_string);
				buffer += fixed_string;
				fixed_string = "";
				add_tag(buffer, tag_Time, false);
				break;
			case LX_BOOL:
				add_bool_start_tag(buffer, (BooleanBase *)value_expr);
				break;
			case LX_UNDEFINED:
				add_empty_tag(buffer, tag_Undefined);
				break;
			case LX_ERROR:
				add_empty_tag(buffer, tag_Error);
				break;
			default:
				add_tag(buffer, tag_Expr, true);
				value_expr->PrintToNewStr(&expr_string);
				fix_characters(expr_string, fixed_string);
				free(expr_string);
				buffer += fixed_string;
				fixed_string = "";
				add_tag(buffer, tag_Expr, false);
				break;
			}
			add_tag(buffer, tag_Attribute, false);
			if (!_use_compact_spacing) {
				buffer += "\n";
			}
		}
	}
	return;
}
	
/**************************************************************************
 *
 * Function: add_tag
 * Purpose:  Adds a plain begin or end tag without any attributes.
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::add_tag(
	MyString  &buffer, 
	TagName   which_tag, 
	bool      start_tag)
{
	buffer += '<';
	if (!start_tag) {
		buffer += '/';
	}
	buffer += tag_names[which_tag].name;
	buffer += '>';
	return;
}

/**************************************************************************
 *
 * Function: add_attribute_start_tag
 * Purpose:  Adds a start tag for our attribute element, which looks like:
 *           <attribute name="foo">
 *
 **************************************************************************/
void
ClassAdXMLUnparser::add_attribute_start_tag(
	MyString   &buffer, 
	const char *name)
{
	if (!_use_compact_spacing) {
		buffer += "    <";
	} else {
		buffer += '<';
	}

	buffer += tag_names[tag_Attribute].name;
	buffer += " n=\"";
	buffer += name;
	buffer += "\">";
	return;
}

/**************************************************************************
 *
 * Function: add_bool_start_tag
 * Purpose:  Adds a start tag for booleans, which looks like: 
 *           <bool value="true">
 *
 **************************************************************************/
void
ClassAdXMLUnparser::add_bool_start_tag(
	MyString     &buffer, 
	BooleanBase  *bool_expr)
{
	buffer += '<';
	buffer += tag_names[tag_Bool].name;
	buffer += " v=\"";
	if (bool_expr->Value()) {
		buffer += "t";
	} else {
		buffer += "f";
	}
	buffer += "\"/>";
	return;
}

/**************************************************************************
 *
 * Function: add_empty_tag
 * Purpose:  Adds an empty tag, which looks like: <name/>
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::add_empty_tag(
	MyString  &buffer, 
	TagName   which_tag)
{
	buffer += '<';
	buffer += tag_names[which_tag].name;
	buffer += "/>";
	return;
}

/**************************************************************************
 *
 * Function: fix_characters
 * Purpose:  This fills in a string (dest) with text identical to the 
 *           source string except that the five standard XML named entities
 *           replace their normal equivalents. For example, if we had a
 *           less-than symbol in an expression, it would be interpreted
 *           as starting a new XML tag, so we replace it with &lt; .
 *
 **************************************************************************/
void ClassAdXMLUnparser::fix_characters(
	const char *source,
	MyString   &dest) // XML attribute, that is.
{
	while (*source != 0) {
		switch (*source) {
		case '&':   dest += "&amp;";   break;
		case '<':   dest += "&lt;";    break;
		case '>':   dest += "&gt;";    break;
		// We skip these cases: they are only necessary in 
		// XML attributes, and this won't happen in the old ClassAds.
		// (Our only relevant attributes are names in the attribute tag,
		// and these won't contain quotes--or ampersands or brackets, 
		// for that matter)
		//case '"': 	dest += "&quot;";  break; 
		//case '\'':  dest += "&apos;";  break;
		default:
			dest += *source;
		}
		source++;
	}
	return;
}

/*-------------------------------------------------------------------------
 *
 * FileXMLSource
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: FileXMLSource constructor
 * Purpose:  
 *
 **************************************************************************/
FileXMLSource::FileXMLSource(FILE *file)
{
	_file = file;
	return;
}

/**************************************************************************
 *
 * Function: FileXMLSource destructor
 * Purpose:  
 *
 **************************************************************************/
FileXMLSource::~FileXMLSource()
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
FileXMLSource::ReadCharacter(void)
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
FileXMLSource::PushbackCharacter(void)
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
FileXMLSource::AtEnd(void) const
{
	bool at_end;
	
	at_end = (feof(_file) != 0);
	return at_end;
}

/*-------------------------------------------------------------------------
 *
 * CharXMLSource
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: CharXMLSource constructor
 * Purpose:  
 *
 **************************************************************************/
CharXMLSource::CharXMLSource(const char *string)
{
	_source_start = string;
	_current      = string;
	return;
}

/**************************************************************************
 *
 * Function: CharXMLSource destructor
 * Purpose:  
 *
 **************************************************************************/
CharXMLSource::~CharXMLSource()
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
CharXMLSource::ReadCharacter(void)
{
	int character;

	character = *_current;
	if (character == 0) {
		character = -1; 
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
CharXMLSource::PushbackCharacter(void)
{
	if (_current > _source_start) {
		_current--;
	}
	return;
}


/**************************************************************************
 *
 * Function: GetCurrentLocation
 * Purpose:  Gets the current location within an CharXMLSource. This is
 *           used for the string version of ParseClassAd() that returns
 *           the place in the string so that we can read the next ClassAd.
 *
 **************************************************************************/
int 
CharXMLSource::GetCurrentLocation(void) const
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
CharXMLSource::AtEnd(void) const
{
	bool at_end;

	at_end = (*_current == 0);
	return at_end;
}

/*-------------------------------------------------------------------------
 *
 * XMLToken 
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: XMLToken constructor
 * Purpose:  Sets up a single XML Token
 *
 **************************************************************************/
XMLToken::XMLToken()
{
	_type            = XMLToken_Invalid;
	_tag             = tag_NoTag;
	_is_end          = false;
	_text            = NULL;
	_attribute_name  = NULL;
	_attribute_value = NULL;
};

/**************************************************************************
 *
 * Function: XMLToken
 * Purpose:  Destroys the token, freeing up the memory we allocated.
 *
 **************************************************************************/
XMLToken::~XMLToken()
{
	_type = XMLToken_Invalid;
	_tag  = tag_NoTag;
	_is_end = false;
	if (_text != NULL) {
		delete[] _text;
	}
	if (_attribute_name != NULL) {
		delete[] _attribute_name;
	} 
	if (_attribute_value != NULL) {
		delete[] _attribute_value;
	}
	return;
}

/**************************************************************************
 *
 * Function: SetType
 * Purpose:  Sets the type of the token (text, tag, or invalid)
 *
 **************************************************************************/
void 
XMLToken::SetType(XMLTokenType type)
{
	_type = type;
}

/**************************************************************************
 *
 * Function: GetType
 * Purpose:  Gets the type of the token (text, tag, or invalid)
 *
 **************************************************************************/
XMLTokenType 
XMLToken::GetType(void) const
{
	return _type;
}

/**************************************************************************
 *
 * Function: SetText
 * Purpose:  Sets the text of a token. This is used for tokens that
 *           are not tags. 
 *
 **************************************************************************/
void XMLToken::SetText(const char *text)
{
	if (_text != NULL) {
		delete [] _text;
	}
	_text = strnewp(text);
	return;
}

/**************************************************************************
 *
 * Function: GetText
 * Purpose:  Gets the text of a token. This is used for tokens that
 *           are not tags. 
 *
 **************************************************************************/
void 
XMLToken::GetText(char **text) const
{
	if (text != NULL && _text != NULL) {
		*text = strnewp(_text);
	}
	return;
}

/**************************************************************************
 *
 * Function: SetTag
 * Purpose:  Sets which tag a token is.
 *
 **************************************************************************/
void
XMLToken::SetTag(TagName tag)
{
	_tag = tag;
	return;
}

/**************************************************************************
 *
 * Function: GetTag
 * Purpose:  Gets which tag a token is. 
 *
 **************************************************************************/
TagName 
XMLToken::GetTag(void) const
{
	return _tag;
}

/**************************************************************************
 *
 * Function: SetTagIsEnd
 * Purpose:  If a tag in an end tag (like </attribute> or <bool value="true"/>,
 *           use this to indicate so. 
 *
 **************************************************************************/
void
XMLToken::SetTagIsEnd(bool is_end)
{
	_is_end = is_end;
	return;
}

/**************************************************************************
 *
 * Function: GetTagIsEnd
 * Purpose:  Returns true if a tag is an end tag (like </attribute> or 
 *           <bool value="true"/>/
 *
 **************************************************************************/
bool
XMLToken::GetTagIsEnd(void)
{
	return _is_end;
}


/**************************************************************************
 *
 * Function: SetAttribute
 * Purpose:  Sets an attribute name and value. Don't confuse this with 
 *           the attribute element. <attribute name="blah"> is an element
 *           named "attribute" with an attribute named "name". 
 *
 **************************************************************************/
void 
XMLToken::SetAttribute(const char *name, const char *value)
{
	// Recall that strnewp is like strdup(), but it uses
	// the C++ new. It's defined in condor_c++_util/strnewp.C
	if (name != NULL) {
		if (_attribute_name != NULL) {
			delete[] _attribute_name;
		}
		_attribute_name = strnewp(name);
	}
	if (value != NULL) {
		if (_attribute_value != NULL) {
			delete[] _attribute_value;
		}
		_attribute_value = strnewp(value);
	}
	return;
}

/**************************************************************************
 *
 * Function: GetAttribute
 * Purpose:  See SetAttribute. 
 *
 **************************************************************************/
bool
XMLToken::GetAttribute(MyString &name, MyString &value)
{
	bool have_attribute = false;

	if (_attribute_name == NULL || _attribute_value == NULL) {
		name  = "";
		value = "";
	} else {
		name  = _attribute_name;
		value = _attribute_value;
		have_attribute = true;
	}

	return have_attribute;
}

/**************************************************************************
 *
 * Function: Dump
 * Purpose:  Print a token for debugging purposes.
 *
 **************************************************************************/
void
XMLToken::Dump(void)
{
	printf("Token (Type=");
	switch (_type) {
	case XMLToken_Tag:
		printf("\"Tag\", ");
		break;
	case XMLToken_Text:
		printf("\"Text\", ");
		break;
	case XMLToken_Invalid:
		printf("\"Invalid\", ");
		break;
	default: 
		printf("\"Unknown\", ");
		break;
	}

	if (_type == XMLToken_Tag) {
		printf("IsEnd = %s, Tag = %s",
			   _is_end ? "true" : "false",
			   tag_names[_tag].name);
		if (_attribute_name && _attribute_value) {
			printf(", %s = %s", _attribute_name, _attribute_value);
		}
	} else if (_type == XMLToken_Text){
		if (_text) {
			printf("Text = %s", _text);
		} else {
			printf("<empty>");
		}
	}
	printf(")\n");
	
	return;
};

/*-------------------------------------------------------------------------
 *
 * Extra Functions
 *
 *-------------------------------------------------------------------------*/

/**************************************************************************
 *
 * Function: debug_check
 * Purpose:  This is simply a bunch of asserts to ensure that we haven't
 *           screwed up our tables with tag names.
 *
 **************************************************************************/
static void debug_check(void)
{
	ASSERT(NUMBER_OF_TAG_NAME_ENUMS    == NUMBER_OF_TAG_NAMES);
	ASSERT(tag_names[tag_ClassAd].id   == tag_ClassAd);
	ASSERT(tag_names[tag_Attribute].id == tag_Attribute);
	ASSERT(tag_names[tag_Number].id    == tag_Number);
	ASSERT(tag_names[tag_Integer].id   == tag_Integer);
	ASSERT(tag_names[tag_Real].id      == tag_Real);
	ASSERT(tag_names[tag_String].id    == tag_String);
	ASSERT(tag_names[tag_Bool].id      == tag_Bool);
	ASSERT(tag_names[tag_Undefined].id == tag_Undefined);
	ASSERT(tag_names[tag_Error].id     == tag_Error);
	ASSERT(tag_names[tag_Time].id      == tag_Time);
	ASSERT(tag_names[tag_List].id      == tag_List);
	ASSERT(tag_names[tag_Expr].id      == tag_Expr);
	return;
}

/**************************************************************************
 *
 * Function: ReadToken
 * Purpose:  Reads a token from an XML source and gives it back. It returns
 *           NULL if there are no more tokens to read. 
 * Note:     One might argue that this should be inside the XMLToken class. 
 *           I wrote this function just after reading a Scott Meyers article
 *           about encapsulation, and it talks about keeping classes minimal. 
 *           I'm not sure if I agree or not, so this is a bit of an 
 *           experiment. 
 *
 **************************************************************************/
static XMLToken *ReadToken(XMLSource &source, bool must_be_text)
{
	int       character;
	XMLToken  *token;
	MyString  text;

	if (source.AtEnd()) {
		token = NULL;
	} else {
		token = new XMLToken();
		
		// First we skip any whitespace
		while (1) {
			character = source.ReadCharacter();
			if (character == EOF) {
				break;
			}
            if (must_be_text && character == '<') {
                break;
            }
			text += ((char) character);
			if (!isspace(character)) {
				break;
			}
		}
		
		// Now we determine if it is a tag or some text (PCDATA)
		if (character == '<' && !must_be_text) {
			// It's a tag. 
			token->SetType(XMLToken_Tag);
			text = "";
			
			// Skip whitespace, then pull out the complete tag.
			// Also check for /, indicating end tag.
			character = source.ReadCharacter();
			if (character == '/') {
				token->SetTagIsEnd(true);
				character = source.ReadCharacter();
			}
			while (isspace(character) && character != EOF) {
				character = source.ReadCharacter();
			}
			while (character != EOF && character != '>') {
				text += ((char) character);
				character = source.ReadCharacter();
			}
			
			// Now that we have the tag, we need to:
			// 1) Figure out if it's an ending tag.
			// 2) find the name of the tag.
			// 3) find the attribute in the tag. (We only allow
			//    one attribute for now.)
			// Note that the text contains everything except for the
			// angle brackets. 
			if (text[text.Length() - 1] == '/') {
				token->SetTagIsEnd(true);
			}
			
			MyString tag_name;
			MyString attribute_name;
			MyString attribute_value;
			int      length;
			length = text.Length();
			// Get tag name
			int i;
			for (i = 0; i < length && !isspace(text[i]) && text[i] != '/'; i++) {
				tag_name += text[i];
			}
			// Skip space
			for ( ; i < length && isspace(text[i]); i++) {
				;
			}
			// Get attribute name
			for ( ; 
				 i < length && !isspace(text[i]) && text[i] != '/' && text[i] != '='; 
				 i++) {
				attribute_name += text[i];
			}
			if (text[i] == '=') {
				i++;
			}
			// Get attribute value
			for ( ;
				 i < length && !isspace(text[i]) && text[i] != '/'; 
				 i++) {
				if (text[i] != '"') {
					attribute_value += text[i];
				}
			}
			token->SetTag(interpret_tagname(tag_name.Value()));
			if (attribute_name.Length() > 0 && attribute_value.Length() > 0) {
				token->SetAttribute(attribute_name.Value(),
									attribute_value.Value());
			}
		} else {
			token->SetType(XMLToken_Text);

            if (must_be_text && character == '<') {
                source.PushbackCharacter();
            }
			
			// We read all of the text up to next '<', adding it to 
			// the text we've been tracking. 
			while (1) {
				character = source.ReadCharacter();
				if (character == EOF) {
					break;
				} else if (character == '<') {
					source.PushbackCharacter();
					break;
				} else {
					text += ((char) character);
				}
			}
			token->SetText(text.Value());
		}
	}
	return token;
}
	
/**************************************************************************
 *
 * Function: interpret_tagname
 * Purpose:  Given a tag name like "attribute", convert it to our enum 
 *           value, like tag_Attribute.
 *
 **************************************************************************/
static TagName interpret_tagname(const char *tag_text)
{
	int      tag_index;
	TagName  which_tag;

	which_tag = tag_NoTag;
	for (tag_index = 0; tag_index < (int) NUMBER_OF_TAG_NAMES; tag_index++) {
		if (strcmp(tag_text, tag_names[tag_index].name) == 0) {
			which_tag = tag_names[tag_index].id;
			break;
		}
	}
	
	return which_tag;
}

/**************************************************************************
 *
 * Function: entities
 * Purpose:  Given a string, produce a new string that changes XML entities
 *           into their corresponding character. For example, &amp; is an
 *           ampersand. 
 *
 **************************************************************************/
static void fix_entities(const char *source, MyString &dest)
{
	while (*source != 0) {
		if (*source != '&') {
			dest += *source;
			source++;
		} else {
			if (!strncmp(source, "&amp;", 5)) {
				dest += '&';
				source += 5;
			} else if (!strncmp(source, "&lt;", 4)) {
				dest += '<';
				source += 4;
			} else if (!strncmp(source, "&gt;", 4)) {
				dest += '>';
				source += 4;
			} else if (!strncmp(source, "&quot;", 6)) {
				dest += '"';
				source += 6;
			} else if (!strncmp(source, "&apos;", 6)) {
				dest += '\'';
				source += 6;
			} else {
				dest += *source;
				source++;
			}
		}
	}

	return;
}
