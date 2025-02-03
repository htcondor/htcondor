/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "classad/source.h"
#include "classad/jsonSource.h"
#include "classad/classad.h"
#include "classad/lexer.h"
#include "classad/util.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

/*--------------------------------------------------------------------
 *
 * Public Functions
 *
 *-------------------------------------------------------------------*/

ClassAdJsonParser::
ClassAdJsonParser ()
{
	lexer.SetJsonLex( true );
}

ClassAdJsonParser::
~ClassAdJsonParser ()
{
	lexer.FinishedParse ();
}


ExprTree *ClassAdJsonParser::
ParseExpression( const string &buffer, bool full)
{
	ExprTree          *tree;
	StringLexerSource lexer_source(&buffer);

	tree = NULL;

	if (lexer.Initialize(&lexer_source)) {
		if (!parseExpression(tree, full)) {
			if (tree) {
				delete tree;
				tree = NULL;
			}
		}
	}
	return tree;
}

ExprTree *ClassAdJsonParser::
ParseExpression( LexerSource *lexer_source, bool full )
{
	ExprTree          *tree;

	tree = NULL;

	if (lexer.Initialize(lexer_source)) {
		if (!parseExpression(tree, full)) {
			if (tree) {
				delete tree;
				tree = NULL;
			} 
		}
	}
	return tree;
}

ExprTree *ClassAdJsonParser::
ParseNextExpression(void)
{
    ExprTree *tree;

    tree = NULL;

    if (!lexer.WasInitialized()) {
        tree = NULL; 
    } else {
		if (!parseExpression(tree, false)) {
			if (tree) {
				delete tree;
				tree = NULL;
            }
		}
    }
    return tree;
}

/*--------------------------------------------------------------------------
 *
 * Parse: Fill In ClassAd
 *
 *--------------------------------------------------------------------------*/

bool ClassAdJsonParser::
ParseClassAd(const string &buffer, ClassAd &classad, bool full)
{
	bool              success;
	StringLexerSource lexer_source(&buffer);

	success = ParseClassAd(&lexer_source, classad, full);

	return success;
}

bool ClassAdJsonParser::
ParseClassAd(const string &buffer, ClassAd &classad, int &offset)
{
	bool              success = false;
	StringLexerSource lexer_source(&buffer, offset);

	success = ParseClassAd(&lexer_source, classad);
	offset = lexer_source.GetCurrentLocation();

	return success;
}

bool ClassAdJsonParser::
ParseClassAd(const char *buffer, ClassAd &classad, bool full)
{
	bool success;
	CharLexerSource lexer_source(buffer);

	success = ParseClassAd(&lexer_source, classad, full);
	
	return success;
}

bool ClassAdJsonParser::
ParseClassAd(const char *buffer, ClassAd &classad, int &offset)
{
	bool success = false;
	CharLexerSource lexer_source(buffer, offset);

	success = ParseClassAd(&lexer_source, classad);
	offset = lexer_source.GetCurrentLocation();

	return success;
}

bool ClassAdJsonParser::
ParseClassAd(FILE *file, ClassAd &classad, bool full)
{
	bool success;
	FileLexerSource lexer_source(file);

	success = ParseClassAd(&lexer_source, classad, full);

	return success;
}

bool ClassAdJsonParser::
ParseClassAd(LexerSource *lexer_source, ClassAd &classad, bool full)
{
	bool              success;

	success      = false;
	if (lexer.Initialize(lexer_source)) {
		success = parseClassAd(classad, full);
	}

	if (success) {
	} else {
		classad.Clear();
	}

	return success;
}

/*--------------------------------------------------------------------------
 *
 * Parse: Return ClassAd
 *
 *--------------------------------------------------------------------------*/

ClassAd *ClassAdJsonParser::
ParseClassAd(const string &buffer, bool full)
{
	ClassAd           *ad;
	StringLexerSource lexer_source(&buffer);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdJsonParser::
ParseClassAd(const string &buffer, int &offset)
{
	ClassAd           *ad = NULL;
	StringLexerSource lexer_source(&buffer, offset);

	ad = ParseClassAd(&lexer_source);
	offset = lexer_source.GetCurrentLocation();

	return ad;
}

ClassAd *ClassAdJsonParser::
ParseClassAd(const char *buffer, bool full)
{
	ClassAd          *ad;
	CharLexerSource  lexer_source(buffer);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdJsonParser::
ParseClassAd(const char *buffer, int &offset)
{
	ClassAd          *ad = NULL;
	CharLexerSource  lexer_source(buffer, offset);

	ad = ParseClassAd(&lexer_source);
	offset = lexer_source.GetCurrentLocation();

	return ad;
}

ClassAd *ClassAdJsonParser::
ParseClassAd(FILE *file, bool full)
{
	ClassAd         *ad;
	FileLexerSource lexer_source(file);

	ad = ParseClassAd(&lexer_source, full);

	return ad;
}

ClassAd *ClassAdJsonParser::
ParseClassAd(LexerSource *lexer_source, bool full)
{
	ClassAd  *ad;

	ad = new ClassAd;
	if (ad != NULL) {
		if (lexer.Initialize(lexer_source)) {
			if (!parseClassAd(*ad, full)) {
				if (ad) { 
					delete ad;
					ad = NULL;
				}
			}
		}
	}
	return ad;
}

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *-------------------------------------------------------------------*/

bool ClassAdJsonParser::
parseExpression( ExprTree *&tree, bool full )
{
	Lexer::TokenValue&	tv = lexer.PeekToken();
	
	switch( tv.type ) {
			
		// constants
		case Lexer::LEX_OPEN_BRACE:
			{
				ClassAd *newAd = new ClassAd;
				if( !newAd || !parseClassAd( *newAd ) ) {
					if( newAd ) delete newAd;
					tree = NULL;
					return false;
				}
				tree = newAd;
			}
			return true;

		case Lexer::LEX_OPEN_BOX:
			{
				ExprList *newList = NULL;
				if( !parseExprList( newList ) ) {
					if( newList ) delete newList;
					tree = NULL;
					return false;
				}
				tree = newList;
			}
			return true;

		case Lexer::LEX_UNDEFINED_VALUE:
			{
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeUndefined()) != NULL );
			}

		case Lexer::LEX_BOOLEAN_VALUE:
			{
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeBool(tv.boolValue)) != NULL );
			}

		case Lexer::LEX_INTEGER_VALUE:
			{
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeInteger(tv.intValue)) != NULL );
			}

		case Lexer::LEX_REAL_VALUE:
			{
				lexer.ConsumeToken( );
				return( (tree=Literal::MakeReal(tv.realValue)) != NULL );
			}

		case Lexer::LEX_STRING_VALUE:
			{
				lexer.ConsumeToken( );
				if ( tv.quotedExpr &&
					 strncasecmp( tv.strValue.c_str(), "/Expr(", 6 ) == 0 &&
					 strcmp( tv.strValue.c_str() + tv.strValue.length() - 2, ")/" ) == 0 ) {

					ClassAdParser parser;
					return ( (tree=parser.ParseExpression( tv.strValue.substr( 6, tv.strValue.length() - 8 ), true )) );
				}
				return( (tree=Literal::MakeString(tv.strValue)) != NULL );
			}

		default:
			tree = NULL;
			return false;
	}
}

bool ClassAdJsonParser::
parseClassAd( ClassAd &ad , bool full )
{
	Lexer::TokenType 	tt;
	ExprTree			*tree = NULL;
	std::string			name;

	ad.Clear( );

	if( lexer.ConsumeToken().type != Lexer::LEX_OPEN_BRACE ) {
	    CondorErrno = ERR_PARSE_ERROR;
	    CondorErrMsg = "putative JSON did not begin with open brace";
	    return false;
	}
	tt = lexer.PeekTokenType();
	while( tt != Lexer::LEX_CLOSE_BRACE ) {
		// Get the name of the expression
		Lexer::TokenValue& tv = lexer.ConsumeToken();
        tt = tv.type;
        if( tt == Lexer::LEX_COMMA ) {
            // We allow empty expressions, so if someone give a double comma, it doesn't 
            // hurt. Technically it's not right, but we shouldn't make users pay the price for
            // a meaningless mistake. See condor-support #1881 for a user that was bitten by this.
            continue;
        }
		if( tt != Lexer::LEX_STRING_VALUE ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_STRING_VALUE " 
				" but got " + string( Lexer::strLexToken( tt ) );
			return false;
		}

		name = tv.strValue;

		// consume the intermediate ':'
		if( ( tt = lexer.ConsumeToken().type ) != Lexer::LEX_COLON ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_COLON " 
				" but got " + string( Lexer::strLexToken( tt ) );
			return false;
		}

		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) {
			return false;
		}

		// insert the attribute into the classad
		if( !ad.Insert( name, tree ) ) {
			delete tree;
			return false;
		}

		// the next token must be a ',' or a '}'
		tt = lexer.PeekTokenType();
		if( tt != Lexer::LEX_COMMA && tt != Lexer::LEX_CLOSE_BRACE ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing classad:  expected LEX_COMMA or "
				"LEX_CLOSE_BRACE but got " + string( Lexer::strLexToken( tt ) );
			return( false );
		}

        // Slurp up any extra commas. This does not duplicate the work at the top of the loop
        // because it accounts for the case where the last expression has extra commas,
        // while the first case accounts for optional beginning commas. 
		while( tt == Lexer::LEX_COMMA ) {
			lexer.ConsumeToken();
			tt = lexer.PeekTokenType();
		} 
	}

	lexer.ConsumeToken();

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken().type != Lexer::LEX_END_OF_INPUT ) ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing classad:  expected LEX_END_OF_INPUT for "
			"full parse but got " + string( Lexer::strLexToken( tt ) );
		return false;
	}

	return true;
}

bool ClassAdJsonParser::
parseExprList( ExprList *&list, bool full )
{
	Lexer::TokenType 	tt;
	ExprTree	*tree = NULL;
	vector<ExprTree*>	loe;

	if( ( tt = lexer.ConsumeToken().type ) != Lexer::LEX_OPEN_BOX ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing expression list:  expected LEX_OPEN_BOX"
			" but got " + string( Lexer::strLexToken( tt ) );
		return false;
	}
	tt = lexer.PeekTokenType();
	while( tt != Lexer::LEX_CLOSE_BOX ) {
		// parse the expression
		parseExpression( tree );
		if( tree == NULL ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing expression list:  expected "
				"LEX_CLOSE_BOX or LEX_COMMA but got "+
				string(Lexer::strLexToken(tt));
			vector<ExprTree*>::iterator i = loe.begin( );
			while(i != loe.end()) {
				delete *i;
				i++;
			}
			return false;
		}

		// insert the expression into the list
		loe.push_back( tree );

		// the next token must be a ',' or a ']'
		tt = lexer.PeekTokenType();
		if( tt == Lexer::LEX_COMMA )
			lexer.ConsumeToken();
		else
		if( tt != Lexer::LEX_CLOSE_BOX ) {
			CondorErrno = ERR_PARSE_ERROR;
			CondorErrMsg = "while parsing expression list:  expected "
				"LEX_CLOSE_BOX or LEX_COMMA but got "+
				string(Lexer::strLexToken(tt));
			vector<ExprTree*>::iterator i = loe.begin( );
			while(i != loe.end()) {
				delete *i;
				i++;
			}
			return false;
		}
	}

	lexer.ConsumeToken();

	if( !( list = ExprList::MakeExprList( loe ) ) ) {
		return( false );
	}

	// if a full parse was requested, ensure that input is exhausted
	if( full && ( lexer.ConsumeToken().type != Lexer::LEX_END_OF_INPUT ) ) {
		CondorErrno = ERR_PARSE_ERROR;
		CondorErrMsg = "while parsing expression list:  expected "
			"LEX_END_OF_INPUT for full parse but got "+
			string(Lexer::strLexToken(tt));
		if( list ) delete list;
		return false;
	}
	return true;
}

Lexer::TokenType ClassAdJsonParser::
PeekToken(void)
{
    if (lexer.WasInitialized()) {
        return lexer.PeekToken().type;
    } else {
        return Lexer::LEX_TOKEN_ERROR;
    }
}

Lexer::TokenType ClassAdJsonParser::
ConsumeToken(void)
{
    if (lexer.WasInitialized()) {
        return lexer.ConsumeToken().type;
    } else {
        return Lexer::LEX_TOKEN_ERROR;
    }
}


} // classad
