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
//******************************************************************************
// scanner.C
//
// scanner() is used by parser to return the elements of an expression one at
// a time.
// 
//******************************************************************************

#include "condor_common.h"
#include "condor_exprtype.h"
#include "condor_scanner.h" 

////////////////////////////////////////////////////////////////////////////////
// Implementation of class Token
////////////////////////////////////////////////////////////////////////////////

Token::Token()
{
	reset();
}

void
Token::reset()
{
	length = 0;
	type = NOT_KEYWORD;
	floatVal = 0.0;
	intVal = 0;
	strVal[0] = '\0';
	isString = FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// This function reads input from a string, returns a token;
////////////////////////////////////////////////////////////////////////////////
void Scanner(char*& s, Token& t)
{ 
    char	str[MAXVARNAME];
    char	*tmp;		// working variables

    // skip white space
    t.length = 0;
    while(isspace(*s))
    {
        s = s + 1;
		t.length++;
    }

	tmp = str;

    if(isalpha(*s) || *s == '_' || *s=='.' ) 
	{
		// first check for keywords

		switch (*s) {

		case 'e':
		case 'E':
			if(!strncasecmp(s, "ERROR", 5) && !isalpha(*(s+5)) && *(s+5) != '_')
			{
				s = s + 5;
				t.length = t.length + 5;
				t.type = LX_ERROR;
				return;
			}
			break;

		case 'u':
		case 'U':
			if(!strncasecmp(s, "UNDEFINED", 9) && !isalpha(*(s+9)) && *(s+9) != '_')
			{
				s = s + 9;
				t.length = t.length + 9;
				t.type = LX_UNDEFINED;
				return;
			}
			break;

		case 'T':
		case 't':
			if(!strncasecmp(s, "TRUE", 4) && !isalpha(*(s+4)) && *(s+4) != '_')
			// TRUE
			{
				s = s + 4;
				t.length = t.length + 4;
				t.intVal = 1;
				t.type = LX_BOOL;
				return;
			}
			if(!strncasecmp(s, "T", 1) && !isalpha(*(s+1)) && *(s+1) != '_')
			// also TRUE
			{
				s = s + 1;
				t.length = t.length + 1;
				t.intVal = 1;
				t.type = LX_BOOL;
				return;
			}
			break;

		case 'f':
		case 'F':
			if(!strncasecmp(s, "FALSE", 5) && !isalpha(*(s+5)) && *(s+5) != '_')
			// FALSE
			{
				s = s + 5;
				t.length = t.length + 5;
				t.intVal = 0;
				t.type = LX_BOOL;
				return;
			}
			if(!strncasecmp(s, "F", 1) && !isalpha(*(s+1)) && *(s+1) != '_')
			// also FALSE
			{
				s = s + 1;
				t.length = t.length + 1;
				t.intVal = 0;
				t.type = LX_BOOL;
				return;
			}
			break;
		}	// end of switch on first letter

		// read in the rest of the variable.
        while(isalnum(*s) || *s == '_' || *s == '.' )
		{ 
			*tmp = *s;
			s++;
            tmp++;
			t.length++;
        }
        *tmp = '\0';
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
            t.floatVal = (float)strtod(s, &s);
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

    if(*s == '"')
    // token is a string
    {
        s++;
		char tmp2[ATTRLIST_MAX_EXPRESSION];		
		tmp = tmp2;
		t.length++;
        while(*s != '"' && *s != '\0')
		{
			// note: short-circuit evaluation will prevent SEGV if we're 
			// at the end of the string in below conditional.
			// make certain s+2 != \0 so that on NT we can have strings
			// that are pathnames that end with a '\'.
			if(*s == '\\' && *(s+1) == '"' && *(s+2) != '\0')
			{
				s++;
				t.length++; 
			} 
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
		strcpy(t.strVal, tmp2);
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
			   if (*s == '?' && *(s+1) == '=')
			   {
					t.type = LX_META_EQ;
					s = s + 2;
					t.length += 2;
			   }
			   else
			   if (*s == '!' && *(s+1) == '=')
			   {
					t.type = LX_META_NEQ;
					s = s + 2;
					t.length += 2;
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
