/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
//******************************************************************************
// parser.c
//
// Converts a string expression into an expression tree. The following function
// is implemented:
//
//     int Parse(const char*, ExprTree*&)
//
//******************************************************************************
#include "condor_common.h"
#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_scanner.h"
#include "condor_string.h"

Token &nextToken()
{
	static Token next_token;
	return next_token;
}

static	int		alreadyRead;

extern	void		Scanner(char*&, Token&);

int ParseExpr(char*& s, ExprTree*&, int&);

#ifndef USE_STRING_SPACE_IN_CLASSADS
static	char*	StrCpy(const char* str)
{
	char*	tmpStr = new char[strlen(str)+1];

	strcpy(tmpStr, str);
	return tmpStr;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// ReadToken() reads in a token to the global variable for parse_... functions
// to use. Once a token is read by this function, it can not be looked at again.
////////////////////////////////////////////////////////////////////////////////
Token* 
ReadToken(char*& s)
{
    if(alreadyRead == TRUE) 
    {
		nextToken().reset();
        Scanner(s, nextToken());
    }
    alreadyRead = TRUE;
    return(&nextToken());
}

////////////////////////////////////////////////////////////////////////////////
// LookToken 'look at' a token without 'read' it, so that the next token that's
// read in is the same token. A token can be looked at for many times.
////////////////////////////////////////////////////////////////////////////////
Token* 
LookToken(char*& s)
{
    if(alreadyRead == TRUE)
    {
		nextToken().reset();
        Scanner(s, nextToken());
    }
    alreadyRead = FALSE;
    return(&nextToken());
}


////////////////////////////////////////////////////////////////////////////////
// Match() reads in a token, see if its type matches the argument t. If it does,
// then return TRUE; Otherwise return FALSE.
////////////////////////////////////////////////////////////////////////////////
int 
Match(LexemeType t, char*& s, int& count)
{
    Token*	token;

    token = ReadToken(s);
    count = count + token->length;
    if(t == token->type)
    {
		nextToken().reset();
        return TRUE;
    }
	nextToken().reset();
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// intermediate parse functions.
///////////////////////////////////////////////////////////////////////////////

int
ParseFunction(char *functionName, char*& s, ExprTree*& newTree, int& count)
{
	int       parse_succeeded;
	Token     *t;
	Function  *function;
#ifdef USE_STRING_SPACE_IN_CLASSADS
	newTree = new Function(functionName);
#else
	newTree = new Function(StrCpy(functionName));
#endif

	function = (Function *) newTree;

	t = ReadToken(s); // That's the right paren
	count += t->length;

	parse_succeeded = FALSE;

	t = LookToken(s);
	if (t->type == LX_RPAREN) {
		ReadToken(s);
	} else {
		while (1) {
			ExprTree *argument;
			if (ParseExpr(s, argument, count)) {
				function->AppendArgument(argument);
				
				Token *next_token;
				
				next_token = LookToken(s);
				if (next_token->type == LX_RPAREN) {
					ReadToken(s);
					count += next_token->length;
					parse_succeeded = TRUE;
					break;
				} else if (next_token->type == LX_SEMICOLON) {
					ReadToken(s);
					count += next_token->length;
					continue;
				} else {
					parse_succeeded = FALSE;
					break;
				}
			} else {
				parse_succeeded = FALSE;
				break;
			}
		}
	}

	return parse_succeeded;
}

int 
ParseFactor(char*& s, ExprTree*& newTree, int& count)
{
    Token*		t     = LookToken(s); // next token
	ExprTree	*expr = NULL;
 
    switch(t->type)
    {
        case LX_VARIABLE :

            t = ReadToken(s);

#ifdef USE_STRING_SPACE_IN_CLASSADS
			newTree = new Variable(t->strVal);
#else
            newTree = new Variable(StrCpy(t->strVal));
#endif
    		count = count + t->length;

			{
				Token *next_token;

				next_token = LookToken(s);
				if (next_token->type == LX_LPAREN) {
					// Oops, we didn't really want a variable
					char *function_name;
					function_name = strnewp(((Variable *)newTree)->Name());
					delete newTree;
					newTree = NULL;
					ParseFunction(function_name, s, newTree, count);
					delete [] function_name;
					break;
				}
			}
            break;

		case LX_UNDEFINED:
	
			t = ReadToken(s);
			newTree = new Undefined();
			count = count + t->length;
			break;

		case LX_ERROR:
	
			t = ReadToken(s);
			newTree = new Error();
			count = count + t->length;
			break;

        case LX_INTEGER :

            t = ReadToken(s);
            newTree = new Integer(t->intVal);
    		count = count + t->length;
            break;

        case LX_FLOAT :

            t = ReadToken(s);
            newTree = new Float(t->floatVal);
    		count = count + t->length;
            break;

        case LX_STRING :

            t = ReadToken(s);
#ifdef USE_STRING_SPACE_IN_CLASSADS
            newTree = new String(t->strVal);
#else
            newTree = new String(StrCpy(t->strVal));
#endif
    		count = count + t->length;
            break;

	    case LX_TIME:
			t = ReadToken(s);
#ifdef USE_STRING_SPACE_IN_CLASSADS
            newTree = new ISOTime(t->strVal);
#else
            newTree = new ISOTime(StrCpy(t->strVal));
#endif
    		count = count + t->length;
			break;

        case LX_BOOL :

            t = ReadToken(s);
            newTree = new ClassadBoolean(t->intVal);
    		count = count + t->length;
            break;

        case LX_LPAREN :

            Match(LX_LPAREN, s, count);
            if(ParseExpr(s, newTree, count)) {
                if(!Match(LX_RPAREN, s, count)) {
					return FALSE;
				} else {
					// parsed parenthesised expr correctly
					ExprTree *ptree = newTree;
					newTree = new AddOp( NULL, ptree );
				}
			} else {
				return FALSE;
			}
			return TRUE;

        case LX_SUB :	// unary minus

            Match(LX_SUB, s, count);
			if(ParseExpr(s, expr, count))
			{
				if( expr->MyType() == LX_INTEGER ) {
					newTree = new Integer( - ((IntegerBase*)expr)->Value() );
					delete expr;
					return TRUE;
				} else if( expr->MyType() == LX_FLOAT ) {
					newTree = new Float( - ((FloatBase*)expr)->Value() );
					delete expr;
					return TRUE;
				}
				newTree = new SubOp(NULL, expr);
				return TRUE;
			}
			else
			{
				return FALSE;
			}	
            break;

        default :

			// I don't know if this is correct, but this is way it was.
			//  --Rajesh
			newTree = new Error;
			return FALSE;
    }

    return TRUE;
}
  
int 
ParseX4(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*			t       = LookToken(s);	// next token
    ExprTree*		subTree = NULL;
    ExprTree*		rArg    = NULL;

    switch(t->type)
    {
        case LX_MULT:

            Match(LX_MULT, s, count);
            if(ParseFactor(s, rArg, count))
			{
                subTree = new MultOp(lArg, rArg);
			}
			else
			{
                newTree = new MultOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_DIV :

            Match(LX_DIV, s, count);
            if(ParseFactor(s, rArg, count))
			{
                subTree = new DivOp(lArg, rArg);
			}
			else
			{
                newTree = new DivOp(lArg, rArg);
				return FALSE;
			}
            break;

        default :

			newTree = lArg;
            return TRUE;
    }

    return ParseX4(subTree, s, newTree, count);
}

int 
ParseMultOp(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg = NULL;

    if(ParseFactor(s, lArg, count))
    {
		return ParseX4(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}

int 
ParseX3(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*			t       = LookToken(s);
    ExprTree*		subTree = NULL;
    ExprTree*		rArg    = NULL;

    switch(t->type)
    {
        case LX_ADD :

            Match(LX_ADD, s, count);
            if(ParseMultOp(s, rArg, count))
			{
                subTree = new AddOp(lArg, rArg);
			}
			else
			{
                newTree = new AddOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_SUB :

            Match(LX_SUB, s, count);
            if(ParseMultOp(s, rArg, count))
			{
                subTree = new SubOp(lArg, rArg);
			}
			else
			{
                newTree = new SubOp(lArg, rArg);
				return FALSE;
			}
            break;

       default :

			newTree = lArg;
            return TRUE;
    }

    return ParseX3(subTree, s, newTree, count);
}

int 
ParseAddOp(char*& s, ExprTree*& newTree, int& count)
{
    Token*		t;
    ExprTree*	lArg = NULL;
    char*		tmp;

    if(ParseMultOp(s, lArg, count))
    {
        return ParseX3(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
    
    t = LookToken(s);
    if(t->type == LX_VARIABLE)
    {
		tmp = t->strVal;
		if(!strcmp(tmp, "K") || !strcmp(tmp, "k"))
		{
			ReadToken(s);
			newTree->unit = 'k';
		}
		if(!strcmp(tmp, "M") || !strcmp(tmp, "m"))
		{
			ReadToken(s);
		}
    }

    return TRUE;
}

int 
ParseX2p5(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t       = LookToken(s);
    ExprTree*	subTree = NULL;
    ExprTree*	rArg    = NULL;

    switch(t->type) {
        case LX_LT:
            Match(LX_LT, s, count);
            if(ParseAddOp(s, rArg, count)) {
				subTree = new LtOp(lArg, rArg);
			} else {
				newTree = new LtOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_LE :
            Match(LX_LE, s, count);
            if(ParseAddOp(s, rArg, count)) {
                subTree = new LeOp(lArg, rArg);
			} else {
                newTree = new LeOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_GT:
            Match(LX_GT, s, count);
            if(ParseAddOp(s, rArg, count)) {
                subTree = new GtOp(lArg, rArg);
			} else {
                newTree = new GtOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_GE :
            Match(LX_GE, s, count);
            if(ParseAddOp(s, rArg, count)) {
                subTree = new GeOp(lArg, rArg);
			} else {
                newTree = new GeOp(lArg, rArg);
				return FALSE;
			}
            break;

        default :
			newTree = lArg;
            return TRUE;
    }

    return ParseX2p5(subTree, s, newTree, count);
}


int 
ParseEqualityOp(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg = NULL;

    if(ParseAddOp(s, lArg, count)) {
        return ParseX2p5(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}


int 
ParseX2(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t       = LookToken(s);
    ExprTree*	subTree = NULL;
    ExprTree*	rArg    = NULL;

    switch(t->type) {
        case LX_META_EQ :
            Match(LX_META_EQ, s, count);
            if(ParseEqualityOp(s, rArg, count)) {
                subTree = new MetaEqOp(lArg, rArg);
			} else {
                newTree = new MetaEqOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_META_NEQ :
            Match(LX_META_NEQ, s, count);
            if(ParseEqualityOp(s, rArg, count)) {
                subTree = new MetaNeqOp(lArg, rArg);
			} else {
                newTree = new MetaNeqOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_EQ :
            Match(LX_EQ, s, count);
            if(ParseEqualityOp(s, rArg, count)) {
                subTree = new EqOp(lArg, rArg);
			} else {
                newTree = new EqOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_NEQ :
            Match(LX_NEQ, s, count);
            if(ParseEqualityOp(s, rArg, count)) {
                subTree = new NeqOp(lArg, rArg);
			} else {
                newTree = new NeqOp(lArg, rArg);
				return FALSE;
			}
            break;

        default :
			newTree = lArg;
            return TRUE;
    }

    return ParseX2(subTree, s, newTree, count);
}


int 
ParseSimpleExpr(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg = NULL;

    if(ParseEqualityOp(s, lArg, count)) {
        return ParseX2(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}


int 
ParseX1p5(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t       = LookToken(s);
    ExprTree*	subTree = NULL;
    ExprTree*	rArg    = NULL;

    if( t->type == LX_AND ) {
        Match(LX_AND, s, count);
        if(ParseSimpleExpr(s, rArg, count)) {
            subTree = new AndOp(lArg, rArg);
		} else {
            newTree = new AndOp(lArg, rArg);
			return FALSE;
		}
	} else {
		newTree = lArg;
        return TRUE;
    }

    return ParseX1p5(subTree, s, newTree, count);
}


int 
ParseAndExpr(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg = NULL;

    if(ParseSimpleExpr(s, lArg, count)) {
        return ParseX1p5(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}


int 
ParseX1(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t       = LookToken(s);
    ExprTree*	subTree = NULL;
    ExprTree*	rArg    = NULL;

    if( t->type == LX_OR ) {
        Match(LX_OR, s, count);
        if(ParseAndExpr(s, rArg, count)) {
            subTree = new OrOp(lArg, rArg);
		} else {
            newTree = new OrOp(lArg, rArg);
			return FALSE;
		}
	} else {
		newTree = lArg;
        return TRUE;
    }

    return ParseX1(subTree, s, newTree, count);
}

int 
ParseExpr(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg = NULL;

    if(ParseAndExpr(s, lArg, count)) {
        return ParseX1(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}

int 
ParseAssignExpr(char*& s, ExprTree*& newTree, int& count)
{
    Token*	t;
    ExprTree* 	lArg = NULL;
    ExprTree* 	rArg = NULL;

    if(ParseExpr(s, lArg, count))
    {
		t = LookToken(s);
		if(t->type == LX_ASSIGN)
		{
			Match(LX_ASSIGN, s, count);
			if(ParseExpr(s, rArg, count))
			{
				newTree = new AssignOp(lArg, rArg);
				t = LookToken(s);
				if(t->type == LX_EOF)
				{
					return TRUE;
				}
				return FALSE;
			}
			else
			{
				newTree = new AssignOp(lArg, rArg);
				return FALSE;
			}
		}
		else if(t->type == LX_EOF)
		{
			newTree = lArg;
			return TRUE;
		}
    }
    newTree = lArg;
    return FALSE;
}

int 
Parse(const char* s, ExprTree*& tree)
{
	char   *str;
	char   *tmp;
	int    count;

	tree = NULL;

	// Make a copy of the string we're parsing.  Unlike the old
	// version (see below) we don't bite the dust when the string is
	// really long. But I'm unsure why the original author of this
	// code chose to copy the string at all. Is it really necessary? I
	// suspect not, but I don't want to risk breaking all of Condor by
	// changing it.--Alain 10-Sep-2001
	str = (char *) malloc(strlen(s) + 1);
	tmp = str;
	strcpy(str, s);

	count = 0;
    alreadyRead = TRUE;
    if(ParseAssignExpr(tmp, tree, count))
    {
		count = 0;
    } else if (tree != NULL) {
		delete tree;
		tree = NULL;
	}
	nextToken().reset();
	free(str);
    return count;
}

// Old version of Parse()
#if 0
int 
Parse(const char* s, ExprTree*& tree)
{
	char		str[ATTRLIST_MAX_EXPRESSION];
    char*		tmp = str;	// parsing indicator
    int			count = 0;	// count characters which has been read

	strcpy(str, s);
    alreadyRead = TRUE;
    if(ParseAssignExpr(tmp, tree, count))
    {
		nextToken().reset();
		return 0;
    }
	nextToken().reset();
    return count;
}
#endif

int 
Parse(char* s, ExprTree*& tree)
{
	return Parse((const char *)s, tree);
}
