//******************************************************************************
// parser.c
//
// Converts a string expression into an expression tree. The following function
// is implemented:
//
//     int Parse(char*, ExprTree*&)
//
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "exprtype.h"
#include "astbase.h"
#include "scanner.h"
#include "parser.h"

const int TABLESIZE = 20;

static	Token*	nextToken = NULL;
static	int		alreadyRead;

extern	void		Scanner(char*&, Token&);

int ParseExpr(char*& s, ExprTree*&, int&);

static	char*	StrCpy(char* str)
{
	char*	tmpStr = new char[strlen(str)+1];

	strcpy(tmpStr, str);
	return tmpStr;
}

////////////////////////////////////////////////////////////////////////////////
// ReadToken() reads in a token to the global variable for parse_... functions
// to use. Once a token is read by this function, it can not be looked at again.
////////////////////////////////////////////////////////////////////////////////
Token* ReadToken(char*& s)
{
    if(alreadyRead == TRUE) 
    {
		nextToken = new Token;
        Scanner(s, *nextToken);
    }
    alreadyRead = TRUE;
    return(nextToken);
}

////////////////////////////////////////////////////////////////////////////////
// LookToken 'look at' a token without 'read' it, so that the next token that's
// read in is the same token. A token can be looked at for many times.
////////////////////////////////////////////////////////////////////////////////
Token* LookToken(char*& s)
{
    if(alreadyRead == TRUE)
    {
		nextToken = new Token;
        Scanner(s, *nextToken);
    }
    alreadyRead = FALSE;
    return(nextToken);
}


////////////////////////////////////////////////////////////////////////////////
// Match() reads in a token, see if its type matches the argument t. If it does,
// then return TRUE; Otherwise return FALSE.
////////////////////////////////////////////////////////////////////////////////
int Match(LexemeType t, char*& s, int& count)
{
    Token*	token;

    token = ReadToken(s);
    count = count + token->length;
    if(t == token->type)
    {
		delete token;
        return TRUE;
    }
	delete token;
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// intermediate parse functions.
///////////////////////////////////////////////////////////////////////////////

int ParseFactor(char*& s, ExprTree*& newTree, int& count)
{
    Token*		t = LookToken(s);	// next token
	char*		tmpStr;
 
    switch(t->type)
    {
        case LX_VARIABLE :

            t = ReadToken(s);
            newTree = new Variable(StrCpy(t->strVal));
    		count = count + t->length;
			delete t;
            break;

        case LX_INTEGER :

            t = ReadToken(s);
            newTree = new Integer(t->intVal);
    		count = count + t->length;
			delete t;
            break;

        case LX_FLOAT :

            t = ReadToken(s);
            newTree = new Float(t->floatVal);
    		count = count + t->length;
			delete t;
            break;

        case LX_STRING :

            t = ReadToken(s);
            newTree = new String(StrCpy(t->strVal));
    		count = count + t->length;
			delete t;
            break;

        case LX_BOOL :

            t = ReadToken(s);
            newTree = new Boolean(t->intVal);
    		count = count + t->length;
			delete t;
            break;

        case LX_NULL :

            t = ReadToken(s);
            newTree = new Null;
    		count = count + t->length;
			delete t;
            break;

        case LX_LPAREN :

            Match(LX_LPAREN, s, count);
            if(ParseExpr(s, newTree, count))
			{
                if(!Match(LX_RPAREN, s, count))
				{
					return FALSE;
				}
			}
			else
			{
				return FALSE;
			}
			return TRUE;

        case LX_SUB :	// unary minus

            Match(LX_SUB, s, count);
            t = ReadToken(s);
            if(t->type == LX_INTEGER)
			{
				newTree = new Integer(-t->intVal);
			}
			else if(t->type == LX_FLOAT)
			{
				newTree = new Float(-t->floatVal);
			}
    		count = count + t->length;
			delete t;
            break;

        default :

			newTree = new Error;
			return FALSE;
    }

    return TRUE;
}
  
int ParseX4(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*			t = LookToken(s);	// next token
    ExprTree*		subTree;
    ExprTree*		rArg;

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

int ParseMultOp(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg;

    if(ParseFactor(s, lArg, count))
    {
		return ParseX4(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}

int ParseX3(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*			t = LookToken(s);
    ExprTree*		subTree;
    ExprTree*		rArg;

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

int ParseAddOp(char*& s, ExprTree*& newTree, int& count)
{
    Token*		t;
    ExprTree*	lArg;
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
			delete t;
			newTree->unit = 'k';
		}
		if(!strcmp(tmp, "M") || !strcmp(tmp, "m"))
		{
			ReadToken(s);
			delete t;
		}
    }

    return TRUE;
}

int ParseX2(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t = LookToken(s);
    ExprTree*	subTree;
    ExprTree*	rArg;

    switch(t->type)
    {
        case LX_EQ :

            Match(LX_EQ, s, count);
            if(ParseAddOp(s, rArg, count))
			{
                subTree = new EqOp(lArg, rArg);
			}
			else
			{
                newTree = new EqOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_NEQ :

            Match(LX_NEQ, s, count);
            if(ParseAddOp(s, rArg, count))
			{
                subTree = new NeqOp(lArg, rArg);
			}
			else
			{
                newTree = new NeqOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_LT:

            Match(LX_LT, s, count);
            if(ParseAddOp(s, rArg, count))
			{
				subTree = new LtOp(lArg, rArg);
			}
			else
			{
				newTree = new LtOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_LE :

            Match(LX_LE, s, count);
            if(ParseAddOp(s, rArg, count))
			{
                subTree = new LeOp(lArg, rArg);
			}
			else
			{
                newTree = new LeOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_GT:

            Match(LX_GT, s, count);
            if(ParseAddOp(s, rArg, count))
			{
                subTree = new GtOp(lArg, rArg);
			}
			else
			{
                newTree = new GtOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_GE :

            Match(LX_GE, s, count);
            if(ParseAddOp(s, rArg, count))
			{
                subTree = new GeOp(lArg, rArg);
			}
			else
			{
                newTree = new GeOp(lArg, rArg);
				return FALSE;
			}
            break;

        default :

			newTree = lArg;
            return TRUE;
    }

    return ParseX2(subTree, s, newTree, count);
}

int ParseSimpleExpr(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg;

    if(ParseAddOp(s, lArg, count))
    {
        return ParseX2(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}

int ParseX1(ExprTree* lArg, char*& s, ExprTree*& newTree, int& count)
{
    Token*		t = LookToken(s);
    ExprTree*	subTree;
    ExprTree*	rArg;

    switch(t->type)
    {
        case LX_AND :

            Match(LX_AND, s, count);
            if(ParseSimpleExpr(s, rArg, count))
			{
                subTree = new AndOp(lArg, rArg);
			}
			else
			{
                newTree = new AndOp(lArg, rArg);
				return FALSE;
			}
            break;

        case LX_OR :

            Match(LX_OR, s, count);
            if(ParseSimpleExpr(s, rArg, count))
			{
                subTree = new OrOp(lArg, rArg);
			}
			else
			{
                newTree = new OrOp(lArg, rArg);
				return FALSE;
			}
            break;

        default :

			newTree = lArg;
            return TRUE;
    }

    return ParseX1(subTree, s, newTree, count);
}

int ParseExpr(char*& s, ExprTree*& newTree, int& count)
{
    ExprTree*	lArg;

    if(ParseSimpleExpr(s, lArg, count))
    {
        return ParseX1(lArg, s, newTree, count);
    }
    newTree = lArg;
    return FALSE;
}

int ParseAssignExpr(char*& s, ExprTree*& newTree, int& count)
{
    Token*	t;
    ExprTree* 	lArg;
    ExprTree* 	rArg;

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

int Parse(char* s, ExprTree*& tree)
{
    char*		tmp = s;	// parsing indicator
    int			count = 0;	// count characters which has been read

    alreadyRead = TRUE;
    if(ParseAssignExpr(tmp, tree, count))
    {
		delete nextToken;
		return 0;
    }
	delete nextToken;
    return count;
}
