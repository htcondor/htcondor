%pure_parser

%{
	// read the note in the below .h file on why we need this
	#include "parserFixes.h"
%}


%union 
{
	IntValue	intValue;
	RealValue	realValue;
	int			strValue;
	ArgValue	argValue;
	ListValue	listValue;

	int			treeId;
}


%{
	#include "source.h"
	#include "classad.h"
	#include "classadList.h"

	#define YYPARSE_PARAM	parm
	#define YYLEX_PARAM		parm


	// Debugging information (Note: x is of the format ("format", arg1, arg2))
	#ifdef 	DEBUG_PARSE
		#define	INFORM(x) { printf x ; }
	#else
		#define INFORM(x) ;
	#endif


	// Factoring common code motifs (Note: X is a type derived from ExprTree)
	#define OBTAIN_EXPR(X) \
				X  *tree = new X();\
				int	  id = ((Source*)parm)->memMan.doregister(tree);\
				if (!tree)	{ EXCEPT("Out of memory"); }

	#define PARSER 	((Source*)YYPARSE_PARAM)


	// for condor_debug
	static char _FileName_[] = "grammar.y";


	// declare for calls from yyparse()
	extern int yyerror (char *);
	extern int yylex   (YYSTYPE *, void *);
%}


%token  LEX_CLASSAD_LIST_TARGET
%token  LEX_CLASSAD_TARGET
%token  LEX_EXPRESSION_TARGET

%token  LEX_TOKEN_ERROR
%token  LEX_TOKEN_TOO_LONG
%token  LEX_INTEGER_VALUE
%token  LEX_REAL_VALUE
%token  LEX_BOOLEAN_VALUE
%token  LEX_STRING_VALUE
%token  LEX_UNDEFINED_VALUE
%token  LEX_ERROR_VALUE
%token  LEX_IDENTIFIER
%token  LEX_SCOPE_RESOLUTION
%token  LEX_MULTIPLY
%token  LEX_DIVIDE
%token  LEX_MODULUS
%token  LEX_PLUS
%token  LEX_MINUS
%token  LEX_BITWISE_AND
%token  LEX_BITWISE_OR
%token  LEX_BITWISE_NOT
%token  LEX_BITWISE_XOR
%token  LEX_LEFT_SHIFT
%token  LEX_LOGICAL_RIGHT_SHIFT
%token  LEX_ARITH_RIGHT_SHIFT
%token  LEX_LOGICAL_AND
%token  LEX_LOGICAL_OR
%token  LEX_LOGICAL_NOT
%token  LEX_LESS_THAN
%token  LEX_LESS_OR_EQUAL
%token  LEX_GREATER_THAN
%token  LEX_GREATER_OR_EQUAL
%token  LEX_EQUAL
%token  LEX_NOT_EQUAL
%token  LEX_META_EQUAL
%token  LEX_META_NOT_EQUAL
%token  LEX_BOUND_TO
%token  LEX_QMARK
%token  LEX_COLON
%token  LEX_COMMA
%token  LEX_SEMICOLON
%token  LEX_OPEN_BOX
%token  LEX_CLOSE_BOX
%token  LEX_OPEN_PAREN
%token  LEX_CLOSE_PAREN
%token	LEX_OPEN_BRACE
%token	LEX_CLOSE_BRACE
%token  LEX_BACKSLASH 

%type	<treeId>		Expression Constant AttributeReference
%type	<treeId>		FunctionCall 
%type	<treeId>		ExpressionList
%type   <argValue>		ArgumentList
%type	<listValue>		ListOfExpressions
%type	<treeId>		ParseTarget ClassAdList ClassAd
%type	<treeId>  		ListOfClassAds ListOfAttributes Attribute
%type	<intValue>		LEX_INTEGER_VALUE
%type	<realValue>		LEX_REAL_VALUE
%type 	<strValue> 		LEX_STRING_VALUE
%type	<strValue>		LEX_IDENTIFIER

%right  LEX_QMARK LEX_COLON
%left 	LEX_LOGICAL_OR
%left 	LEX_LOGICAL_AND
%left	LEX_BITWISE_OR
%left	LEX_BITWISE_XOR
%left	LEX_BITWISE_AND 
%left	LEX_META_EQUAL  LEX_META_NOT_EQUAL
%left	LEX_EQUAL  LEX_NOT_EQUAL
%left	LEX_LESS_THAN  LEX_LESS_OR_EQUAL  LEX_GREATER_THAN  LEX_GREATER_OR_EQUAL
%left	LEX_LEFT_SHIFT  LEX_LOGICAL_RIGHT_SHIFT  LEX_ARITH_RIGHT_SHIFT
%left	LEX_PLUS  LEX_MINUS
%left	LEX_MULTIPLY  LEX_DIVIDE  LEX_MODULUS
%right	LEX_LOGICAL_NOT  LEX_BITWISE_NOT


%start	ParseTarget

%%

ParseTarget
	: LEX_CLASSAD_LIST_TARGET ClassAdList 
	{
		INFORM(("????: <ParseTarget> ::= <ClassAdList>\n"));

		PARSER->parsedClassAdListTarget();
	}

    | LEX_CLASSAD_TARGET ClassAd
    {
        INFORM(("????: <ParseTarget> ::= <ClassAd>\n"));

        // The attributes of the classad have been inserted into the
        // `currentClassAd` (if it exists).  Inform the parser object that we
        // got a classad target --- it will take care of any housekeeping that
        // needs to be done

        PARSER->parsedClassAdTarget ();
    }

    | LEX_EXPRESSION_TARGET Expression
    {
        INFORM(("????: <ParseTarget> ::= <Expression(%04d)>\n", $2));

        // Call back parser object and inform it that we parsed an expression
        // tree.  If the parser expected an expression parse target, it returns
        // 'true' on the call-back, and we unregister the tree and place it
        // at the `currenExprTree`.  If not, just forget about it ---
        // memMan.finishedParse() will be called by the parser object.

        if (PARSER->parsedExpressionTarget() && PARSER->currentExprTree == NULL)
        {
            PARSER->currentExprTree = PARSER->memMan.getExpr($2);
            PARSER->memMan.unregister ($2);
        }
    }
	;


ClassAdList
	: LEX_OPEN_BRACE LEX_CLOSE_BRACE
	{
		INFORM(("????: <ClassAdList> ::= {}\n"));
		delete PARSER->currentClassAd;
	}

	| LEX_OPEN_BRACE ListOfClassAds LEX_CLOSE_BRACE
	{
		INFORM(("????: <ClassAdList> ::= { <ListOfClassAds> }\n"));
		// the classad in currentClassAd is not used --- deallocate
		delete PARSER->currentClassAd;
	}
	;


ListOfClassAds
	: ClassAd
	{
		INFORM(("????: <ListOfClassAds> ::= <ClassAd>\n"));
		if (PARSER->currentClassAdList && PARSER->currentClassAd)
		{
			PARSER->currentClassAdList->append (PARSER->currentClassAd);

			// establish new classad for next in list
			PARSER->currentClassAd = new ClassAd;
		}
	}

	| ListOfClassAds ClassAd
	{
		INFORM(("????: <ListOfClassAds> ::= <ListOfClassAds> <ClassAd>\n"));
		if (PARSER->currentClassAdList && PARSER->currentClassAd)
		{
			// insert parsed classad into list
			PARSER->currentClassAdList->append (PARSER->currentClassAd);

			// establish new classad for next in list
			PARSER->currentClassAd = new ClassAd;
		}
	}
	;


ClassAd 
	: LEX_OPEN_BOX LEX_CLOSE_BOX
	{
		INFORM(("????: <ClassAd> ::= []\n"));
	}

	| LEX_OPEN_BOX ListOfAttributes LEX_CLOSE_BOX
	{
		INFORM(("????: <ClassAd> ::= [ <ListOfAttributes> ]\n"));
	}
	;


ListOfAttributes
	: Attribute
	{
		INFORM(("????: <ListOfAttributes> ::= <Attribute>\n"));
	}

	| ListOfAttributes LEX_SEMICOLON Attribute
	{
		INFORM(("????: <ListOfAttributes> ::= <ListOfAttributes>"
				" ; <Attribute>\n"));
	}
	;


Attribute
	: LEX_IDENTIFIER LEX_BOUND_TO Expression
	{
		INFORM(("????: <Attribute> ::= Id = <Expression(%04d)>\n", $3));

		// We should now insert this attribute into the `currentClassAd`
		// classad.  This classad may have been established in one of two
		// ways:  (1) the parser object may have set it up in anticipation of
		// a classad target, or (2) the classad list may have set one up when a
		// new classad is anticipated in the list.  We really don't care.  But
		// stay paranoid enough to check that a classad is actually established
		// for us.  If we do the insert, we unregister the tree, or else 
		// finishParse() will clean up

		if (PARSER->currentClassAd)
		{
			char *s = PARSER->lexer.getCharString($1);
			PARSER->currentClassAd->insert (s, PARSER->memMan.getExpr ($3));
			PARSER->memMan.unregister ($3);
		}
	}
	;


Expression
	: Constant
	{
		// the constant reduction has already processed this case
		// so there is no need to add another node
		INFORM(("%04d: <Expression> ::= <Constant(%04d)>\n", $1, $1));
		$$ = $1;
	}

	| AttributeReference
	{
		// the attribute reference reduction has already processed this case
		// so there is no need to add another node
		INFORM(("%04d: <Expression> ::= <AttributeReference(%04d)>\n", $1, $1));
		$$ = $1;
	}

	| ExpressionList
	{
		INFORM(("%04d: <Expression> ::= <ExpressionList(%04d)>\n", $1, $1));
		$$ = $1;
	}

    | Expression LEX_QMARK Expression LEX_COLON Expression
    {
        OBTAIN_EXPR(Operation);
        INFORM(("%04d: <Expression> ::= <Expression(%04d)> ? <Expression(%04d)>"
                " : <Expression(%04d)>\n", id, $1, $3, $5));
        tree->setOperation (TERNARY_OP, PARSER->memMan.getExpr($1),
                						PARSER->memMan.getExpr($3), 
										PARSER->memMan.getExpr($5));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);
        PARSER->memMan.unregister ($5);
        $$ = id;
    }

	| FunctionCall
	{
		// the function call reduction has already processed this case
		// so there is no need to add another node
		INFORM(("%04d: <Expression> ::= <FunctionCall(%04d)>\n", $1, $1));
		$$ = $1;
	}

	| LEX_OPEN_PAREN Expression	LEX_CLOSE_PAREN
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= ( <Expression(%04d)> ) \n", id, $2));
		tree->setOperation (PARENTHESES_OP, PARSER->memMan.getExpr($2));
		PARSER->memMan.unregister ($2);
		$$ = id;
	}


	| Expression LEX_LOGICAL_OR Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> || "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (LOGICAL_OR_OP, 	PARSER->memMan.getExpr($1), 
											PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression LEX_LOGICAL_AND Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> && "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (LOGICAL_AND_OP, PARSER->memMan.getExpr($1), 
											PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_BITWISE_OR  		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> | <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (BITWISE_OR_OP, 	PARSER->memMan.getExpr($1), 
											PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_BITWISE_XOR 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> ^ <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (BITWISE_XOR_OP, PARSER->memMan.getExpr($1),
											PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_BITWISE_AND 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> & <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (BITWISE_AND_OP, PARSER->memMan.getExpr($1), 
											PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_META_EQUAL	 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> =?= "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation(META_EQUAL_OP, 	PARSER->memMan.getExpr($1),
										 	PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_META_NOT_EQUAL		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> =!= "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (META_NOT_EQUAL_OP, 	PARSER->memMan.getExpr($1), 
												PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($1);
		PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_EQUAL       		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> == "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (EQUAL_OP,	PARSER->memMan.getExpr($1),
                                		PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);
		$$ = id;
	}

	| Expression 		LEX_NOT_EQUAL	 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> != "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation(NOT_EQUAL_OP,    PARSER->memMan.getExpr($1),
                                        	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_LESS_THAN			Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> < <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation(LESS_THAN_OP,    PARSER->memMan.getExpr($1),
                                            PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_LESS_OR_EQUAL 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> <= "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation(LESS_OR_EQUAL_OP,    PARSER->memMan.getExpr($1),
                                            	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_GREATER_THAN 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> > <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (GREATER_THAN_OP,	PARSER->memMan.getExpr($1),
                                            	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_GREATER_OR_EQUAL 	Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> >= "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (GREATER_OR_EQUAL_OP,    PARSER->memMan.getExpr($1),
                                            		PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_LEFT_SHIFT	 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> << "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (LEFT_SHIFT_OP,	PARSER->memMan.getExpr($1),
                                           	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_LOGICAL_RIGHT_SHIFT	 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> >> "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (LOGICAL_RIGHT_SHIFT_OP,	PARSER->memMan.getExpr($1),
                                            PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_ARITH_RIGHT_SHIFT 	Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> >>> "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation (ARITH_RIGHT_SHIFT_OP,	PARSER->memMan.getExpr($1),
                                            		PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression		LEX_PLUS				Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> + <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (ADDITION_OP,    PARSER->memMan.getExpr($1),
                                            PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression		LEX_MINUS				Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> - <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (SUBTRACTION_OP,	PARSER->memMan.getExpr($1),
                                           	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_MULTIPLY	 		Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> * <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation (MULTIPLICATION_OP,  PARSER->memMan.getExpr($1),
                                            	PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression 		LEX_DIVIDE	 			Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> / <Expression(%04d)>"
				"\n", id, $1, $3));
		tree->setOperation(DIVISION_OP, PARSER->memMan.getExpr($1),
                                        PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| Expression		LEX_MODULUS				Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= <Expression(%04d)> %% "
				"<Expression(%04d)>\n", id, $1, $3));
		tree->setOperation(MODULUS_OP, PARSER->memMan.getExpr($1),
                                       PARSER->memMan.getExpr($3));
        PARSER->memMan.unregister ($1);
        PARSER->memMan.unregister ($3);

		$$ = id;
	}

	| LEX_PLUS			Expression 		%prec	LEX_LOGICAL_NOT
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= + <Expression(%04d)>\n", id, $2));
		tree->setOperation (UNARY_PLUS_OP, PARSER->memMan.getExpr($2));
        PARSER->memMan.unregister ($2);
		$$ = id;
	}

	| LEX_MINUS			Expression  	%prec	LEX_LOGICAL_NOT
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= - <Expression(%04d)>\n", id, $2));
		tree->setOperation (UNARY_MINUS_OP, PARSER->memMan.getExpr($2));
        PARSER->memMan.unregister ($2);

		$$ = id;
	}

	| LEX_BITWISE_NOT 	Expression		%prec	LEX_LOGICAL_NOT
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= ~ <Expression(%04d)>\n", id, $2));
		tree->setOperation (BITWISE_NOT_OP, PARSER->memMan.getExpr($2));
        PARSER->memMan.unregister ($2);

		$$ = id;
	}

	| LEX_LOGICAL_NOT 	Expression
	{
		OBTAIN_EXPR(Operation);
		INFORM(("%04d: <Expression> ::= ! <Expression(%04d)>\n", id, $2));
		tree->setOperation (LOGICAL_NOT_OP, PARSER->memMan.getExpr($2));
        PARSER->memMan.unregister ($2);

		$$ = id;
	}
	;


AttributeReference
	: LEX_IDENTIFIER
	{
		char *s = PARSER->lexer.getCharString($1);

		OBTAIN_EXPR(AttributeReference);
		INFORM(("%04d: <AttributeReference> ::= Identifer(%s)\n", id, s));
		tree->setReference (NULL, s);
		$$ = id;
	}

	| LEX_IDENTIFIER LEX_SCOPE_RESOLUTION LEX_IDENTIFIER
	{
		char *s = PARSER->lexer.getCharString($1);
		char *t = PARSER->lexer.getCharString($3);

		OBTAIN_EXPR(AttributeReference);
		INFORM(("%04d: <AttributeReference> ::= %s.%s\n", id, s, t));
		tree->setReference (s, t);
		$$ = id;
	}
	;
	

Constant
	: LEX_INTEGER_VALUE
	{
		OBTAIN_EXPR(Constant);
		INFORM(("%04d: <Constant> ::= Integer\n", id));
		tree->setIntegerValue($1.value, $1.factor);
		$$ = id;
	}

	| LEX_REAL_VALUE
	{
		OBTAIN_EXPR(Constant);
		INFORM(("%04d: <Constant> ::= Real\n", id));
		tree->setRealValue($1.value, $1.factor);
		$$ = id;
	}

	| LEX_STRING_VALUE
	{
		char *s = PARSER->lexer.getCharString ($1);

		OBTAIN_EXPR(Constant);
		INFORM(("%04d: <Constant> ::= String(%s)\n", id, s));
		tree->setStringValue(s); 
		$$ = id;
	}

	| LEX_UNDEFINED_VALUE
	{
		OBTAIN_EXPR(Constant);
		INFORM(("%04d: <Constant> ::= Undefined\n", id));
		tree->setUndefinedValue();
		$$ = id;
	}

	| LEX_ERROR_VALUE
	{
		OBTAIN_EXPR(Constant);
		INFORM(("%04d: <Constant> ::= Error\n", id));
		tree->setErrorValue();
		$$ = id;
	}
	;


ExpressionList
	: LEX_OPEN_BRACE LEX_CLOSE_BRACE
	{
		OBTAIN_EXPR(ExprList);
		INFORM(("%04d: <ExprList> ::= { }\n", id));
		$$ = id;
	}

	| LEX_OPEN_BRACE ListOfExpressions LEX_CLOSE_BRACE
	{
		INFORM(("%04d: <ExprList> ::= { <ListOfExpressions(%04d)> }\n",$2,$2));
		$$ = $2.treeId;
	}
	;

ListOfExpressions
	: Expression
	{
		OBTAIN_EXPR(ExprList);
		INFORM(("%04d: <ListOfExpressions> ::= <Expression>\n"));
		tree->appendExpression(PARSER->memMan.getExpr($1));
		PARSER->memMan.unregister ($1);

		$$.exprList = tree;
		$$.treeId = id;
	}

	| ListOfExpressions LEX_COMMA Expression	
	{
		ExprList *list = $1.exprList;
		list->appendExpression(PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister($3);

		$$ = $1;
	}
	;

FunctionCall
	: LEX_IDENTIFIER LEX_OPEN_PAREN LEX_CLOSE_PAREN
	{
		char *s = PARSER->lexer.getCharString($1);

		OBTAIN_EXPR(FunctionCall);
		INFORM(("%04d: <FunctionCall> ::= Indentifier()\n", id));
		tree->setCall (s, NULL);
		$$ = id;
	}

	| LEX_IDENTIFIER LEX_OPEN_PAREN ArgumentList LEX_CLOSE_PAREN
	{
		char *s = PARSER->lexer.getCharString($1);

		OBTAIN_EXPR(FunctionCall);
		INFORM(("%04d: <FunctionCall> ::= Identifier( <ArgumentList(%04d)> )\n",
				id, ($3).treeId));
		tree->setCall (s, ($3).argList);
		PARSER->memMan.unregister (($3).treeId);
		$$ = id;
	}
	;


ArgumentList
	: Expression
	{	
		ArgValue args;

		OBTAIN_EXPR(ArgumentList);
		INFORM(("%04d: <ArgumentList> ::= <Expression(%04d)>\n", id, $1));
		tree->appendArgument(PARSER->memMan.getExpr($1));
		PARSER->memMan.unregister ($1);
		args.argList = tree;
		args.treeId = id;
		$$ = args;
	}

	| ArgumentList LEX_COMMA Expression
	{
		INFORM(("%04d: <ArgumentList> ::= <ArgumentList(%04d)>, "
					"<Expression(%04d)>\n", ($1).treeId, ($1).treeId, $3));
		($1).argList->appendArgument(PARSER->memMan.getExpr($3));
		PARSER->memMan.unregister ($3);
		$$ = $1;
	}
	;

%%
