//******************************************************************************
// scanner.C
//
// scanner() is used by parser to return the elements of an expression one at
// a time.
// 
//******************************************************************************

#include <stdlib.h> 
#include <stdio.h> 
#include <string.h>
#include <ctype.h> 
#include "exprtype.h"
#include "scanner.h" 

static int 	MAXVARNAME = 100;

////////////////////////////////////////////////////////////////////////////////
// Implementation of class Token
////////////////////////////////////////////////////////////////////////////////

Token::Token()
{
	length = 0;
	type = NOT_KEYWORD;
	strVal = NULL;
	isString = FALSE;
}

Token::~Token()
{
	if(isString)
	{
		delete []strVal;
	}
}

////////////////////////////////////////////////////////////////////////////////
// This function reads input from a string, returns a token;
////////////////////////////////////////////////////////////////////////////////
void Scanner(char*& s, Token& t)
{ 
    char	str[MAXVARNAME];
    char*	tmp;		// working variable

    // skip white space
    t.length = 0;
    while(*s == ' ')
    {
        s = s + 1;
		t.length++;
    }

    if(!strncmp(s, "NULL", 4) && !isalpha(*(s+4)) && *(s+4) != '_')
    // NULL
    {
        s = s + 4;
		t.length = t.length + 4;
        t.type = LX_NULL;
		return;
    }
    if(!strncmp(s, "TRUE", 4) && !isalpha(*(s+4)) && *(s+4) != '_')
    // TRUE
    {
        s = s + 4;
		t.length = t.length + 4;
        t.intVal = 1;
        t.type = LX_BOOL;
		return;
    }
    if(!strncmp(s, "FALSE", 5) && !isalpha(*(s+5)) && *(s+5) != '_')
    // FALSE
    {
        s = s + 5;
		t.length = t.length + 5;
        t.intVal = 0;
        t.type = LX_BOOL;
		return;
    }
    if(isalpha(*s) || *s == '_')
    // token is a variable
    {
        tmp = str;
        // prefix, if any.
		if(!strncmp(s, "MY.", 3) && (isalpha(*(s+3)) || *(s+3) == '_'))
		{
			for(int i=0; i<3; i++)
			{
				*tmp = *s;
				s++;
				tmp++;
				t.length++;
			}
		}
		else if(!strncmp(s, "TARGET.", 7) && (isalpha(*(s+7)) || *(s+7) == '_'))
		{
			for(int i=0; i<7; i++)
			{
				*tmp = *s;
				s++;
				tmp++;
				t.length++;
			}
		}
		// the rest of the variable.
        while(isalnum(*s) || *s == '_')
		{ 
			*tmp = *s;
			s++;
            tmp++;
			t.length++;
        }
        *tmp = '\0';
        t.strVal = new char[strlen(str) + 1];
		strcpy(t.strVal, str);
        t.type = LX_VARIABLE;
		t.isString = TRUE;
		return;
    }
    if(isdigit(*s))
    // token is an integer or a floating point number
    {
		// count the length of the number
        tmp = s;
        while(isdigit(*tmp))
		{
			tmp++;
			t.length++;
		}
        if(*tmp == '.')
        // token is a floating point number
        {
			t.length++;
			for(tmp++; isdigit(*tmp); tmp++) t.length++;
            t.floatVal = strtod(s, &s);
            t.type = LX_FLOAT; 
        }
		else
		// token is an integer number
		{
            t.intVal = strtol(s, &s, 10);
            t.type = LX_INTEGER; 
        }
		return;
    }
    if(*s == '"' || (*s == '\\' && *(s+1) == '"'))
    // token is a string
    {
        s++;
		tmp = str;
		t.length++;
        while(*s != '"' && *s != '\0')
		{
			*tmp = *s;
			s++;
            tmp++;
			t.length++;
        }
        if(*s == '\0')
		{
			 t.type = LX_ERROR;
			 t.length = 0;
			 return;
		}
        s++;
		t.length++;
        *tmp = '\0';
		t.strVal = new char[strlen(str) + 1];
		strcpy(t.strVal, str);
        t.type = LX_STRING; 
		t.isString = TRUE;
		return;
    }

    // token is an operator or a unit
	switch(*s)
	{
		case '(': t.type = LX_LPAREN;
			   s = s + 1;
			   t.length++;
			   break; 

		case ')': t.type = LX_RPAREN;
			   s = s + 1;
			   t.length++;
			   break; 

		case '+': t.type = LX_ADD;
			   s = s + 1;
			   t.length++;
			   break;

		case '-': t.type = LX_SUB;
			   s = s + 1;
			   t.length++;
			   break;

		case '*': t.type = LX_MULT;
			   s = s + 1;
			   t.length++;
			   break;

		case '/': t.type = LX_DIV;
			   s = s + 1;
			   t.length++;
			   break;

		case '<': s = s + 1;
			   t.length++;
			   if(*s == '=')
			   {
				   s = s + 1;
				   t.length++;
				   t.type = LX_LE;
			   }
			   else
			   {
				   t.type = LX_LT; 
			   }
			   break; 

		case '>': s = s + 1;
			   t.length++;
			   if(*s == '=')
			   {
				   s = s + 1;
				   t.length++;
				   t.type = LX_GE; 
			   }
			   else 
			   {
				   t.type = LX_GT; 
			   }
			   break; 

		case '&': s = s + 1;
			   t.length++;
			   if(*s == '&')
			   {
				   t.type = LX_AND;
				   s = s + 1;
				   t.length++;
			   }
			   else
			   {
				   t.type = LX_ERROR;
			   }
			   break;

		case '|': s = s + 1;
			   t.length++;
			   if(*s == '|')
			   {
					t.type = LX_OR;
					s = s + 1;
			   }
			   else
			   {
				   t.type = LX_ERROR;
			   }
			   break;

		case '\0':
		case '\n': t.type = LX_EOF;
				break;

		case '!': s = s + 1;
			   t.length++;
			   if(*s == '=')
			   {
				   t.type = LX_NEQ;
				   s = s + 1;
				   t.length++;
			   }
			   else
			   {
				   t.type = LX_ERROR;
			   }
			   break;

		case '=': s = s + 1;
			   t.length++;
			   if(*s == '=')
			   {
				   t.type = LX_EQ;
				   s = s + 1;
				   t.length++;
			   }
			   else
			   {
				   t.type = LX_ASSIGN;
			   }
			   break;

		case '$': t.type = LX_MACRO;
			   s = s + 1;
			   t.length++;
			   break;

		default:	t.type = LX_ERROR;
					break;
	} 
}
