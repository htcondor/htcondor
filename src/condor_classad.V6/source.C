#include "source.h"
#include "classad.h"
#include "lexer.h"

// the YACC/bison parser entry point
extern int yyparse (void *);
int yylex   (YYSTYPE *, void *);
int yyerror (char *);


// ctor
Source::
Source ()
{
	parseTarget = NO_TARGET;
}


// dtor
Source::
~Source ()
{
	memMan.finishedParse ();
	lexer.finishedParse ();
}


bool Source::
setSource (const char *buf, int len)
{
	return (lexer.initializeWithString (buf, len));
}


bool Source::
setSource (char *buf, int len)
{
	return (lexer.initializeWithStringBuffer (buf, len));
}


bool Source::
setSource (Sock &sock)
{
	return (lexer.initializeWithCedar (sock));
}


bool Source::
setSource (int fd)
{
	return (lexer.initializeWithFd (fd));
}


bool Source::
setSource (FILE *file)
{
	return (lexer.initializeWithFile (file));
}


bool Source::
parseClassAd (ClassAd &ad)
{
	int rval;

    // set anticipated target
    currentClassAd = &ad;
    parseTarget = CLASSAD_TARGET;
	lexer.setTarget (parseTarget);
   
    // parse
    rval = yyparse ((void *) this);

    // cleanup
    memMan.finishedParse ();
	lexer.finishedParse ();
	parseTarget = NO_TARGET;
    currentClassAd = NULL;

    // return true on success
    return (rval == 0);
}


bool Source::
parseExpression (ExprTree *&tree)
{
	int rval;

    // set anticipated target
    parseTarget = EXPRESSION_TARGET;
	lexer.setTarget (parseTarget);
    currentExprTree = NULL;		// will be filled by the parser
   
    // parse
    rval = yyparse ((void *) this);

    // cleanup
    memMan.finishedParse ();
	lexer.finishedParse ();
	parseTarget = NO_TARGET;

	// successful parse?
	if (rval == 0)
	{
		// success
		tree = currentExprTree;
		currentExprTree = NULL;
		return true;
	}

	currentExprTree = NULL;
    return (false);
}


bool Source::
parseClassAdList (ClassAdList &adList)
{
	int rval;

	// set anticipated target
	currentClassAdList = &adList;
	currentClassAd     = new ClassAd;

	parseTarget = CLASSAD_LIST_TARGET;
	lexer.setTarget (parseTarget);

	// parse
	rval = yyparse ((void *)this);

	// cleanup
	memMan.finishedParse ();
	lexer.finishedParse ();
	parseTarget = NO_TARGET;
	currentClassAdList = NULL;

	// return true on success
	return (rval == 0);
}


// call-backs
bool Source::
parsedExpressionTarget ()
{
	// tell the parser if it got what you expected
	return (parseTarget == EXPRESSION_TARGET);
}


bool Source::
parsedClassAdTarget ()
{
	// tell the parser if it got what you expected
	return (parseTarget == CLASSAD_TARGET);
}

bool Source::
parsedClassAdListTarget ()
{
	// tell the parser if it got what you expected
	return (parseTarget == CLASSAD_LIST_TARGET);
}


// the yylex function
int
yylex (YYSTYPE *lvalp, void *p)
{
    return (((Source*)p)->lexer.yylex(lvalp));
}


// the yyerror function
int
yyerror (char *)
{
    return 0;
}

