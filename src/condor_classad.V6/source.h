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

#ifndef __SOURCE_H__
#define __SOURCE_H__

#include <vector>
#include <iostream>
#include "lexer.h"

BEGIN_NAMESPACE( classad )

class ClassAd;
class ExprTree;
class ExprList;
class FunctionCall;

/// The parser object
class ClassAdParser
{
	public:
		/// Constructor
		ClassAdParser();

		/// Destructor
		~ClassAdParser();

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

		void SetDebug( bool d ) { lexer.SetDebug( d ); }
	private:
		// lexical analyser for parser
		Lexer	lexer;

		// mutually recursive parsing functions
		bool parseExpression( ExprTree*&, bool=false);
		bool parseClassAd( ClassAd&, bool=false);
		bool parseExprList( ExprList*&, bool=false);
		bool parseLogicalORExpression( ExprTree*& );
		bool parseLogicalANDExpression( ExprTree*& );
		bool parseInclusiveORExpression( ExprTree*& );
		bool parseExclusiveORExpression( ExprTree*& );
		bool parseANDExpression( ExprTree*& );
		bool parseEqualityExpression( ExprTree*& );
		bool parseRelationalExpression( ExprTree*& );
		bool parseShiftExpression( ExprTree*& );
		bool parseAdditiveExpression( ExprTree*& );
		bool parseMultiplicativeExpression( ExprTree*& );
		bool parseUnaryExpression( ExprTree*& );
		bool parsePostfixExpression( ExprTree*& );
		bool parsePrimaryExpression( ExprTree*& );
		bool parseArgumentList( std::vector<ExprTree*>& );

		bool shouldEvaluateAtParseTime(const std::string &functionName,
									   vector<ExprTree*> &argList);
		ExprTree *evaluateFunction(const std::string &functionName,
								  vector<ExprTree*> &argList);

};

std::istream & operator>>(std::istream &stream, ClassAd &ad);

END_NAMESPACE // classad

#endif//__SOURCE_H__
