#ifndef __PARSER_H__
#define __PARSER_H__

#include "condor_io.h"
#include "lexer.h"

class ClassAd;
class ExprTree;
class ClassAdList;

enum {NO_TARGET, CLASSAD_TARGET, CLASSAD_LIST_TARGET, EXPRESSION_TARGET};

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

	private:
		// what do we want to parse?
		int	 parseTarget;

		// entry points from factory methods of classes 
		friend class ClassAd;
		friend class ExprTree;
		friend class ClassAdList;
		bool parseClassAd 			(ClassAd &);
		bool parseExpression		(ExprTree *&);
		bool parseClassAdList (ClassAdList &);

		// the following functions are invoked by the YACC/bison created parser.
		// these functions need access to private data to redirect the call to 
		// the appropriate method of the parser object --- we don't want to 
		// expose the accessed data as part of the interface.
		friend int  yylex (YYSTYPE *, void *);
		friend int  yyparse (void *);

		// call back functions from parser; called after parse
		bool parsedClassAdListTarget (void);
		bool parsedExpressionTarget  (void);
        bool parsedClassAdTarget     (void);

		// internal objects for the parse
		ParserMemoryManager	memMan;
		Lexer				lexer;

		// how the YACC/bison parser returns the result ...
		ClassAd 		*currentClassAd;
		ExprTree		*currentExprTree;
		ClassAdList 	*currentClassAdList;
};

int yylex   (YYSTYPE *, void *);
int yyerror (char *);

#endif//__PARSER_H__
