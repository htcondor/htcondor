/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
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
#include "xmlSource.h"
#include "xmlLexer.h"
#include "classad.h"
#include "source.h"

using namespace std;

BEGIN_NAMESPACE( classad )

ClassAdXMLParser::
ClassAdXMLParser ()
{
	return;
}


ClassAdXMLParser::
~ClassAdXMLParser ()
{
	return;
}

bool ClassAdXMLParser::
ParseClassAd( const string &buffer, ClassAd &classad)
{
	int offset = 0;
	return ParseClassAd(buffer, classad, offset);
}

bool ClassAdXMLParser::
ParseClassAd( const string &buffer, ClassAd &classad, int &offset)
{
	return false;
}


ClassAd *ClassAdXMLParser::
ParseClassAd( const string &buffer)
{
	int offset = 0;

	return ParseClassAd(buffer, offset);
}

ClassAd *ClassAdXMLParser::
ParseClassAd( const string &buffer, int &offset)
{
	ClassAd          *classad;

	lexer.SetLexText(buffer);
	lexer.SetOffset(offset);
	classad = ParseClassAd();
	offset = lexer.GetOffset();

	return classad;
}

ClassAd *ClassAdXMLParser::
ParseClassAd(void)
{
	bool             in_classad;
	ClassAd          *classad;
	XMLLexer::Token  token;

	classad = NULL;
	in_classad = false;

	while (lexer.PeekToken(&token)) {
		if (!in_classad) {
			lexer.ConsumeToken(NULL);
			if (   token.token_type == XMLLexer::tokenType_Tag 
				&& token.tag_id     == XMLLexer::tagID_ClassAd) {
				
				// We have a ClassAd tag
				if (token.tag_type   == XMLLexer::tagType_Start) {
					in_classad = true;
					classad = new ClassAd();
				} else {
					// We're done, return the ClassAd we got, if any.
					break;
				}
			}
		} else {
			if (   token.token_type == XMLLexer::tokenType_Tag) {
				if (   token.tag_id   == XMLLexer::tagID_Attribute
					   && token.tag_type == XMLLexer::tagType_Start) {
					string attribute_name;
					ExprTree *tree;
					
					tree = ParseAttribute(attribute_name);
					if (tree != NULL) {
						classad->Insert(attribute_name, tree);
					}
				} else if (   token.tag_id != XMLLexer::tagID_XML
							  && token.tag_id != XMLLexer::tagID_XMLStylesheet
							  && token.tag_id != XMLLexer::tagID_Doctype
							  && token.tag_id != XMLLexer::tagID_ClassAds) {
					// We got a non-attribute, non-xml thingy within a 
					// ClassAd. That must be an error, but we'll just skip
					// it in the hopes of recovering.
					lexer.ConsumeToken(NULL);
					break;
				}
			}
		}
	}
	return classad;
}

ExprTree *ClassAdXMLParser::
ParseAttribute(
	string    &attribute_name)
{
	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_Attribute);

	if (token.tag_type != XMLLexer::tagType_Start) {
		attribute_name = "";
	} else {
		attribute_name = token.attributes["n"];
		if (attribute_name != "") {
			tree = ParseThing();
		}

		have_token = lexer.PeekToken(&token);
		if (have_token && token.token_type == XMLLexer::tokenType_Tag
			&& token.tag_id == XMLLexer::tagID_Attribute
			&& token.tag_type == XMLLexer::tagType_End) {
			lexer.ConsumeToken(&token);
		} else {
			// It's probably invalid XML, but we'll keep on going in 
			// the hope of figuring out something reasonable. 
		}
	}
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseThing(void)
{
	ExprTree         *tree;
	XMLLexer::Token  token;

	lexer.PeekToken(&token);
	if (token.token_type == XMLLexer::tokenType_Tag) {
		switch (token.tag_id) {
		case XMLLexer::tagID_ClassAd:
			tree = ParseClassAd();
			break;
		case XMLLexer::tagID_List:
			tree = ParseList();
			break;
		case XMLLexer::tagID_Number:
		case XMLLexer::tagID_String:
			tree = ParseNumberOrString(token.tag_id);
			break;
		case XMLLexer::tagID_Bool:
			tree = ParseBool();
			break;
		case XMLLexer::tagID_Undefined:
		case XMLLexer::tagID_Error:
			tree = ParseUndefinedOrError(token.tag_id);
			break;
		case XMLLexer::tagID_Time:
			tree = ParseTime();
			break;
		case XMLLexer::tagID_Expr:
			tree = ParseExpr();
			break;
		default:
			break;
		}
	}
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseList(void)
{
	bool               have_token;
	ExprTree           *tree;
	ExprTree           *subtree;
	XMLLexer::Token    token;
	vector<ExprTree*>  expressions;

	tree = NULL;
	have_token = lexer.ConsumeToken(&token);
	assert(have_token && token.tag_id == XMLLexer::tagID_List);

	while (lexer.PeekToken(&token)) {
		if (   token.token_type == XMLLexer::tokenType_Tag
			&& token.tag_type   == XMLLexer::tagType_End
			&& token.tag_id     == XMLLexer::tagID_List) {
			// We're at the end of the list
			lexer.ConsumeToken(NULL);
			break;
		} else if (   token.token_type == XMLLexer::tokenType_Tag
				   && token.tag_type != XMLLexer::tagType_End) {
			subtree = ParseThing();
			expressions.push_back(subtree);
		}
	}

	tree = ExprList::MakeExprList(expressions);

	return tree;
}

ExprTree *ClassAdXMLParser::
ParseNumberOrString(XMLLexer::TagID tag_id)
{
	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	// Get start tag
	tree = NULL;
	have_token = lexer.ConsumeToken(&token);
	assert(have_token && token.tag_id == tag_id);

	// Get text of number or string
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Text) {
		lexer.ConsumeToken(&token);
		Value value;
		if (tag_id == XMLLexer::tagID_Number) {
			if (strchr(token.text.c_str(), '.')) {
				// This is a floating point number
				float number;
				sscanf(token.text.c_str(), "%g", &number);
				value.SetRealValue(number);
			} else {
				// This is an integer.
				int number;
				sscanf(token.text.c_str(), "%d", &number);
				value.SetIntegerValue(number);
			}
		} else {
			value.SetStringValue(token.text);
		}
		tree = Literal::MakeLiteral(value);
	}

	// Get end tag
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Tag
		&& token.tag_id == tag_id
		&& token.tag_type == XMLLexer::tagType_End) {
		lexer.ConsumeToken(&token);
	} else {
		// It's probably invalid XML, but we'll keep on going in 
		// the hope of figuring out something reasonable. 
	}

	return tree;
}

ExprTree *ClassAdXMLParser::
ParseBool(void)
{
	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_Bool);

	Value  value;
	string truth_value = token.attributes["v"];

	if (truth_value == "t" || truth_value == "T") {
		value.SetBooleanValue(true);
	} else {
		value.SetBooleanValue(false);
	}
	tree = Literal::MakeLiteral(value);

	if (token.tag_type == XMLLexer::tagType_Start) {
		// They had a start token, so swallow the
		// end token.
		have_token = lexer.PeekToken(&token);
		if (have_token && token.token_type == XMLLexer::tokenType_Tag
			&& token.tag_id == XMLLexer::tagID_Bool
			&& token.tag_type == XMLLexer::tagType_End) {
			lexer.ConsumeToken(&token);
		} else {
			// It's probably invalid XML, but we'll keep on going in 
			// the hope of figuring out something reasonable. 
		}
	}
	
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseUndefinedOrError(XMLLexer::TagID tag_id)
{
	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == tag_id);

	Value value;
	if (tag_id == XMLLexer::tagID_Undefined) {
		value.SetUndefinedValue();
	} else {
		value.SetErrorValue();
	}
	tree = Literal::MakeLiteral(value);

	if (token.tag_type == XMLLexer::tagType_Start) {
		// They had a start token, so swallow the
		// end token.
		have_token = lexer.PeekToken(&token);
		if (have_token && token.token_type == XMLLexer::tokenType_Tag
			&& token.tag_id == tag_id
			&& token.tag_type == XMLLexer::tagType_End) {
			lexer.ConsumeToken(&token);
		} else {
			// It's probably invalid XML, but we'll keep on going in 
			// the hope of figuring out something reasonable. 
		}
	}

	return tree;
}

ExprTree *ClassAdXMLParser::
ParseTime(void)
{
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_Time);

	// We haven't yet finished this functionality.
	assert(0); 

	return tree;
	
}

ExprTree *ClassAdXMLParser::
ParseExpr(void)
{
	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	// Get start tag
	tree = NULL;
	have_token = lexer.ConsumeToken(&token);
	assert(have_token && token.tag_id == XMLLexer::tagID_Expr);

	// Get text of expression
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Text) {
		lexer.ConsumeToken(&token);
		ClassAdParser  parser;
		
		tree = parser.ParseExpression(token.text, true);
	}

	// Get end tag
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Tag
		&& token.tag_id == XMLLexer::tagID_Expr
		&& token.tag_type == XMLLexer::tagType_End) {
		lexer.ConsumeToken(&token);
	} else {
		// It's probably invalid XML, but we'll keep on going in 
		// the hope of figuring out something reasonable. 
	}

	return tree;
}

END_NAMESPACE // classad
