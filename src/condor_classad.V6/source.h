#ifndef __SOURCE_H__
#define __SOURCE_H__

#include "condor_io.h"
#include "lexer.h"

class ClassAd;
class ExprTree;
class ExprList;
class FunctionCall;

class Source
{
	public:
		// ctor/dtor
		Source();
		~Source();

		// Set the stream source for the parse
		bool setSource (const char *, int);	// immutable strings
        bool setSource (char *, int);  		// mutable strings
        bool setSource (Sock &);    		// CEDAR
        bool setSource (int);           	// file descriptor
        bool setSource (FILE *);       		// FILE structure

		// parser entry points
		bool parseClassAd( ClassAd*, bool full=false );
		bool parseExpression(ExprTree*&, bool full=false);
		bool parseExprList( ExprList*, bool full=false );

	private:
		// lexical analyser for parser
		Lexer	lexer;

		// mutually recursive parsing functions
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
		bool parseArgumentList( FunctionCall* );
};

#endif//__SOURCE_H__
