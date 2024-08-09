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


#include "classad/common.h"
#include "classad/xmlSource.h"
#include "classad/xmlLexer.h"
#include "classad/lexerSource.h"
#include "classad/classad.h"
#include "classad/source.h"
#include "classad/lexer.h"
#include "classad/util.h"
#include <assert.h>
#include <charconv>
#include <system_error>

using std::string;
using std::vector;
using std::pair;


namespace classad {

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
	ClassAd          *classad_out;
	StringLexerSource lexer_source(&buffer, offset);

	lexer.SetLexerSource(&lexer_source);
	classad_out = ParseClassAd(&classad);
	offset = lexer_source.GetCurrentLocation();

	return classad_out != NULL;
}

bool ClassAdXMLParser::
ParseClassAd( FILE *file, ClassAd &ad )
{
	ClassAd *classad_out;
	FileLexerSource lexer_source(file);

	lexer.SetLexerSource(&lexer_source);
	classad_out = ParseClassAd(&ad);
	return classad_out != NULL;
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
	StringLexerSource lexer_source(&buffer, offset);

	lexer.SetLexerSource(&lexer_source);
	classad = ParseClassAd();
	offset = lexer_source.GetCurrentLocation();

	return classad;
}

ClassAd *ClassAdXMLParser::
ParseClassAd(FILE *file)
{
	ClassAd *classad;
	FileLexerSource lexer_source(file);
	
	lexer.SetLexerSource(&lexer_source);
	classad = ParseClassAd();
	return classad;
}

ClassAd *ClassAdXMLParser::
ParseClassAd(ClassAd *classad_in)
{
	bool             in_classad;
	ClassAd          *classad = NULL;
	ClassAd          *local_ad = NULL;
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
					if ( classad_in ) {
						classad_in->Clear();
						classad = classad_in;
					} else {
						local_ad = new ClassAd();
						classad = local_ad;
					}
				} else {
					// We're done, return the ClassAd we got, if any.
                    in_classad = false;
					break;
				}
			}
		} else {
			if (token.token_type == XMLLexer::tokenType_Tag) {
			  if (token.tag_id   == XMLLexer::tagID_Attribute) {
			    if (token.tag_type == XMLLexer::tagType_Invalid) {
				  delete local_ad;
			      return NULL;
			    } else if( token.tag_type == XMLLexer::tagType_Start) {
					string attribute_name;
					ExprTree *tree;
					
					tree = ParseAttribute(attribute_name);
					if (tree != NULL) {
						classad->Insert(attribute_name, tree);
					}
					else {
					  delete local_ad;
					  return NULL;
					}
			    } else {
					lexer.ConsumeToken(NULL);
				}
              } else if (token.tag_id   == XMLLexer::tagID_ClassAd) {
                  lexer.ConsumeToken(NULL);
                  if (token.tag_type == XMLLexer::tagType_End) {
                      in_classad = false;
                      break;
                  } else {
                      // This is invalid, but we'll just ignore it.
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
			} else {
				lexer.ConsumeToken(NULL);
			}
		}
	}
	return classad;
}

ExprTree *ClassAdXMLParser::
ParseAttribute(
	string    &attribute_name)
{
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

        SwallowEndTag(XMLLexer::tagID_Attribute);
	}
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseThing(void)
{
	ExprTree         *tree;
	XMLLexer::Token  token;

    tree = NULL;
	lexer.PeekToken(&token);
	if (token.token_type == XMLLexer::tokenType_Tag) {
		switch (token.tag_id) {
		case XMLLexer::tagID_ClassAd:
			tree = ParseClassAd();
			break;
		case XMLLexer::tagID_List:
			tree = ParseList();
			break;
		case XMLLexer::tagID_Integer:
		case XMLLexer::tagID_Real:
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
		case XMLLexer::tagID_AbsoluteTime:
			tree = ParseAbsTime();
			break;
		case XMLLexer::tagID_RelativeTime:
			tree = ParseRelTime();
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
	ExprTree           *tree;
	ExprTree           *subtree;
	XMLLexer::Token    token;
	vector<ExprTree*>  expressions;

	tree = NULL;
	lexer.ConsumeToken(&token);

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
		if (tag_id == XMLLexer::tagID_Integer) {
			int64_t number;
			auto [pend, ec] = std::from_chars(token.text.data(), token.text.data() + token.text.size(), number, 10);
			if ( ec != std::errc()) {
				tree = Literal::MakeError();
			} else {
				tree = Literal::MakeInteger(number);
			}
		}
		else if (tag_id == XMLLexer::tagID_Real) {
			double real;
			char * pend;
			const char * pnum = token.text.c_str();
			real = strtod(pnum, &pend);
			if (pend == pnum) {
				tree = Literal::MakeError();
			} else {
				tree = Literal::MakeReal(real);
			}
		}
		else {        // its a string
			bool validStr = true;
			convert_escapes(token.text, validStr );
			if(!validStr) {  // invalid string because it had /0 escape sequence
				return NULL;
			} else {
				tree = Literal::MakeString(token.text);
			}
		}
	
	} else if (tag_id == XMLLexer::tagID_String) {
		// We were expecting text and got none, so we had
		// the empty string, which was skipped by the lexer.
		tree = Literal::MakeString("");
	}

    SwallowEndTag(tag_id);

	return tree;
}

ExprTree *ClassAdXMLParser::
ParseBool(void)
{
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_Bool);

	bool   value;
	string truth_value = token.attributes["v"];

	if (truth_value == "t" || truth_value == "T") {
		value = true;
	} else {
		value = false;
	}
	tree = Literal::MakeBool(value);

	if (token.tag_type == XMLLexer::tagType_Start) {
        SwallowEndTag(XMLLexer::tagID_Bool);
	}
	
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseUndefinedOrError(XMLLexer::TagID tag_id)
{
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == tag_id);

	if (tag_id == XMLLexer::tagID_Undefined) {
		tree = Literal::MakeUndefined();
	} else {
		tree = Literal::MakeError();
	}

	if (token.tag_type == XMLLexer::tagType_Start) {
        SwallowEndTag(tag_id);
	}

	return tree;
}

ExprTree *ClassAdXMLParser::
ParseAbsTime(void)
{
  	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_AbsoluteTime);
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Text) {
		lexer.ConsumeToken(&token);
		tree = Literal::MakeAbsTime(token.text);
	}
    SwallowEndTag(XMLLexer::tagID_AbsoluteTime);
	return tree;
}

ExprTree *ClassAdXMLParser::
ParseRelTime(void)
{
  	bool             have_token;
	ExprTree         *tree;
	XMLLexer::Token  token;

	tree = NULL;
	lexer.ConsumeToken(&token);
	assert(token.tag_id == XMLLexer::tagID_RelativeTime);
	have_token = lexer.PeekToken(&token);
	if (have_token && token.token_type == XMLLexer::tokenType_Text) {
		lexer.ConsumeToken(&token);
		tree = Literal::MakeRelTime(token.text);
	}
    SwallowEndTag(XMLLexer::tagID_RelativeTime);
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
		
		tree = parser.ParseExpression(token.text);
	}

    SwallowEndTag(XMLLexer::tagID_Expr);

	return tree;
}

void ClassAdXMLParser::
SwallowEndTag(
    XMLLexer::TagID tag_id)
{
    bool              have_token;
	XMLLexer::Token  token;

	have_token = lexer.PeekToken(&token);
	if (   have_token 
        && token.token_type == XMLLexer::tokenType_Tag
		&& token.tag_id     == tag_id
		&& token.tag_type   == XMLLexer::tagType_End) {
		lexer.ConsumeToken(&token);
	} else {
		// It's probably invalid XML, but we'll keep on going in 
		// the hope of figuring out something reasonable. 
	}
    return;
}

} // classad
