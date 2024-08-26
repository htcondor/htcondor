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


#ifndef __CLASSAD_JSON_SOURCE_H__
#define __CLASSAD_JSON_SOURCE_H__

#include <vector>
#include "classad/lexer.h"

namespace classad {

class ClassAd;
class ExprTree;
class ExprList;
class FunctionCall;

/// This reads %ClassAd strings from various sources and converts them into a ClassAd.
/// It can read from C++ strings, C strings, FILEs, and streams.
class ClassAdJsonParser
{
	public:
		/// Constructor
		ClassAdJsonParser();

		/// Destructor
		~ClassAdJsonParser();

		/** Parse a ClassAd 
			@param buffer Buffer containing the string representation of the
				classad.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ClassAd was parsed successfully and no
				other tokens follow the ClassAd.
			@return pointer to the ClassAd object if successful, or NULL 
				otherwise
		*/
		ClassAd *ParseClassAd(const std::string &buffer, bool full=false);
		ClassAd *ParseClassAd(const std::string &buffer, int &offset);
		ClassAd *ParseClassAd(const char *buffer, bool full=false);
		ClassAd *ParseClassAd(const char *buffer, int &offset);
		ClassAd *ParseClassAd(FILE *file, bool full=false);
		ClassAd *ParseClassAd(std::istream &stream, bool full=false);

		ClassAd *ParseClassAd(LexerSource *lexer_source, bool full=false);

		/** Parse a ClassAd 
			@param buffer Buffer containing the string representation of the
				classad.
			@param ad The classad to be populated
			@param full If this parameter is true, the parse is considered to
				succeed only if the ClassAd was parsed successfully and no
				other tokens follow the ClassAd.
			@return true on success, false on failure
		*/
		bool ParseClassAd(const std::string &buffer, ClassAd &ad, bool full=false);
		bool ParseClassAd(const std::string &buffer, ClassAd &classad, int &offset);
		bool ParseClassAd(const char *buffer, ClassAd &classad, bool full=false);
		bool ParseClassAd(const char *buffer, ClassAd &classad, int &offset);
		bool ParseClassAd(FILE *file, ClassAd &classad, bool full=false);
		bool ParseClassAd(std::istream &stream, ClassAd &classad, bool full=false);

		bool ParseClassAd(LexerSource *lexer_source, ClassAd &ad, bool full=false);

		/** Parse an expression 
			@param expr Reference to a ExprTree pointer, which will be pointed
				to the parsed expression.  The previous value of the pointer
				will be destroyed.
			@param full If this parameter is true, the parse is considered to
				succeed only if the expression was parsed successfully and no
				other tokens follow the expression.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseExpression( const std::string &buffer, ExprTree*& expr, 
					bool full=false);

		bool ParseExpression( LexerSource *lexer_source, ExprTree*& expr, 
					bool full=false);

		/** Parse an expression
			@param buffer Buffer containing the string representation of the
				expression.
			@param full If this parameter is true, the parse is considered to
				succeed only if the expression was parsed successfully and no
				other tokens are left.
			@return pointer to the expression object if successful, or NULL 
				otherwise
		*/
		ExprTree *ParseExpression( const std::string &buffer, bool full=false);

		ExprTree *ParseExpression( LexerSource *lexer_source, bool full=false);

        ExprTree *ParseNextExpression(void);

		Lexer::TokenType PeekToken(void);
		Lexer::TokenType ConsumeToken(void);
		Lexer::TokenType getLastTokenType() { return lexer.getLastTokenType(); }

	private:
		// lexical analyser for parser
		Lexer	lexer;

		// mutually recursive parsing functions
		bool parseExpression( ExprTree*&, bool=false);
		bool parseClassAd( ClassAd&, bool=false);
		bool parseExprList( ExprList*&, bool=false);
};

} // classad

#endif//__CLASSAD_JSON_SOURCE_H__
