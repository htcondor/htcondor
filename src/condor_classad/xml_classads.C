/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

struct tag_name
{
	TagName  id; // Defined in the condor_xml_classads.h
	char     *short_name;
	char     *long_name;
};

#define NUMBER_OF_TAG_NAMES (sizeof(tag_names) / sizeof(struct tag_name))
static struct tag_name tag_names[] = 
{
	{tag_ClassAd,   "c",  "classad"   },
	{tag_Attribute, "a",  "attribute" },
	{tag_Number,    "n",  "number"    },
	{tag_String,    "s",  "string",   },
	{tag_Bool,      "b",  "bool"      },
	{tag_Undefined, "u",  "undefined" },
	{tag_Error,     "er", "error"     },
	{tag_Time,      "t",  "time"      },
	{tag_List,      "l",  "list"      },
	{tag_Expr,      "e",  "expr"      }
};

static void debug_check(void);

/*-------------------------------------------------------------------------
 *
 * ClassAdXMLParser
 *
 *-------------------------------------------------------------------------*/

ClassAdXMLParser::ClassAdXMLParser()
{
	debug_check();
	return;
}

ClassAdXMLParser::~ClassAdXMLParser()
{
	return;
}

ClassAd *
ClassAdXMLParser::ParseClassAd(const char *buffer)
{
	return NULL;
}

ClassAd *
ClassAdXMLParser::ParseClassAds(const char *buffer, int *place)
{
	return NULL;
}

/*-------------------------------------------------------------------------
 *
 * ClassAdXMLUnparser
 *
 *-------------------------------------------------------------------------*/


/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
ClassAdXMLUnparser::ClassAdXMLUnparser()
{
	debug_check();
	_use_compact_names = true;
	_use_compact_spacing = true;
	return;
}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
ClassAdXMLUnparser::~ClassAdXMLUnparser()
{
	return;
}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
bool 
ClassAdXMLUnparser::GetUseCompactNames(void)
{
	return _use_compact_names;
}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::SetUseCompactNames(bool use_compact_names)
{
	_use_compact_names = use_compact_names;
	return;
}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
bool 
ClassAdXMLUnparser::GetUseCompactSpacing(void)
{
	return _use_compact_spacing;
}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::SetUseCompactSpacing(bool use_compact_spacing)
{
	_use_compact_spacing = use_compact_spacing;
	return;
}

void ClassAdXMLUnparser::AddXMLFileHeader(MyString &buffer)
{
	buffer += "<?xml version=\"1.0\"?>\n";
	buffer += "<!DOCTYPE classad SYSTEM \"classads.dtd\">\n";
	buffer += "<classads>\n";
	return;

}

void ClassAdXMLUnparser::AddXMLFileFooter(MyString &buffer)
{
	buffer += "<?xml version=\"1.0\"?>\n";
	buffer += "<!DOCTYPE classad SYSTEM \"classads.dtd\">\n";
	buffer += "</classads>\n";
	return;

}

/**************************************************************************
 *
 * Function:
 * Purpose:  
 *
 **************************************************************************/
void 
ClassAdXMLUnparser::Unparse(ClassAd *classad, MyString &buffer)
{
	add_tag(buffer, tag_ClassAd, true);
	if (!_use_compact_spacing) {
		buffer += '\n';
	}
	
	// Loop through all expressions in the ClassAd
	ExprTree *expression;

	classad->ResetExpr();
	for (expression = classad->NextExpr(); 
		 expression != NULL; 
		 expression = classad->NextExpr()) {
		 
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

				char      number_string[20];
				char      *expr_string;
				MyString  fixed_string;

				switch (value_expr->MyType()) {
				case LX_INTEGER:
					sprintf(number_string, "%d", ((IntegerBase *)value_expr)->Value());
					add_tag(buffer, tag_Number, true);
					buffer += number_string;
					if (value_expr->unit == 'k') {
						buffer += " k";
					}
					add_tag(buffer, tag_Number, false);
					break;
				case LX_FLOAT:
					sprintf(number_string, "%f", ((FloatBase *)value_expr)->Value());
					add_tag(buffer, tag_Number, true);
					buffer += number_string;
					if (value_expr->unit == 'k') {
						buffer += " k";
					}
					add_tag(buffer, tag_Number, false);
					break;
				case LX_STRING:
					add_tag(buffer, tag_String, true);
					fix_characters(((StringBase *)value_expr)->Value(), fixed_string);
					buffer += fixed_string;
					fixed_string = "";
					add_tag(buffer, tag_String, false);
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
	}
	add_tag(buffer, tag_ClassAd, false);
	buffer += '\n';
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
	if (_use_compact_names) {
		buffer += tag_names[which_tag].short_name;
	} else {
		buffer += tag_names[which_tag].long_name;
	}
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
	if (_use_compact_names) {
		buffer += tag_names[tag_Attribute].short_name;
		buffer += " n=\"";
	} else {
		buffer += tag_names[tag_Attribute].long_name;
		buffer += " name=\"";
	}
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
	if (!_use_compact_spacing) {
		buffer += "    <";
	} else {
		buffer += '<';
	}
	if (_use_compact_names) {
		buffer += tag_names[tag_Bool].short_name;
		buffer += " v=\"";
	} else {
		buffer += tag_names[tag_Bool].long_name;
		buffer += " value=\"";
	}
	if (bool_expr->Value()) {
		buffer += "true";
	} else {
		buffer += "false";
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
	if (_use_compact_names) {
		buffer += tag_names[which_tag].short_name;
	} else {
		buffer += tag_names[which_tag].long_name;
	}
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
	char     *source,
	MyString &dest)
{
	while (*source != 0) {
		switch (*source) {
		case '&':   dest += "&amp;";   break;
		case '<':   dest += "&lt;";    break;
		case '>':   dest += "&gt;";    break;
		case '"':   dest += "&quot;";  break;
		case '\'':  dest += "&aposl";  break;
		default:
			dest += *source;
		}
		source++;
	}
	return;
}

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
	ASSERT(tag_names[tag_String].id    == tag_String);
	ASSERT(tag_names[tag_Bool].id      == tag_Bool);
	ASSERT(tag_names[tag_Undefined].id == tag_Undefined);
	ASSERT(tag_names[tag_Error].id     == tag_Error);
	ASSERT(tag_names[tag_Time].id      == tag_Time);
	ASSERT(tag_names[tag_List].id      == tag_List);
	ASSERT(tag_names[tag_Expr].id      == tag_Expr);
	return;
}
