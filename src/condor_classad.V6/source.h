/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#ifndef __SOURCE_H__
#define __SOURCE_H__

#include <vector>
#include "lexer.h"

BEGIN_NAMESPACE( classad )

class ClassAd;
class ExprTree;
class ExprList;
class FunctionCall;

class ClassAdParser
{
	public:
		/// Constructor
		ClassAdParser();

		/// Destructor
		~ClassAdParser();

		/** Parses a ClassAd from this Source object.
			@param buffer Buffer containing the string representation of the
				classad.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ClassAd was parsed successfully and no
				other tokens follow the ClassAd.
			@return pointer to the ClassAd object if successful, or NULL 
				otherwise
		*/
		ClassAd *ParseClassAd( const string &buffer, bool full=false );
		bool ParseClassAd( const string &buffer, ClassAd &ad, bool full=false );

		/** Parses an expression from this Source object.
			@param expr Reference to a ExprTree pointer, which will be pointed
				to the parsed expression.  The previous value of the pointer
				will be destroyed.
			@param full If this parameter is true, the parse is considered to
				succeed only if the expression was parsed successfully and no
				other tokens follow the expression.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseExpression( const string &buffer, ExprTree*& expr, 
					bool full=false);
		ExprTree *ParseExpression( const string &buffer, bool full=false);

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
		bool parseArgumentList( vector<ExprTree*>& );
};

END_NAMESPACE // classad

#endif//__SOURCE_H__
