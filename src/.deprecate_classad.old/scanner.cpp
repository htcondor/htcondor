/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

#ifdef USE_NEW_SCANNER
struct keywords
{
	char        *keyword;       // the text of the keyword we'll be matching against
	int         keyword_length; // the strlen of keyword
	LexemeType  type;           // the token type we should produce when scanning the keyword
	int         int_value;      // what we should use as the integer value.
};

#define NUMBER_OF_KEYWORDS (sizeof(keywords) / sizeof(struct keywords))
const struct keywords keywords[] = 
{
	{"ERROR",     5, LX_ERROR,     0},
	{"UNDEFINED", 9, LX_UNDEFINED, 0},
	{"TRUE",      4, LX_BOOL,      1},
	{"T",         1, LX_BOOL,      1},
	{"FALSE",     5, LX_BOOL,      0},
	{"F",         1, LX_BOOL,      0}
};

/* This funky hash table is used to reduce overhead in scan_keyword()
   when the input identifier can't possibly be a keyword.
*/
class _KeywordHash {
public:
	_KeywordHash() {
		int k;
			// mark all characters 1
		memset(keyword_hash,1,sizeof(keyword_hash));
			// mark all first characters of keywords 0 (both upper and lower)
		for(k=NUMBER_OF_KEYWORDS; k--; ) {
			char ch = keywords[k].keyword[0];
			keyword_hash[(unsigned char)tolower(ch)] = 0;
			keyword_hash[(unsigned char)toupper(ch)] = 0;
		}
	}
	inline bool notAKeyword(const char *str) {
		return keyword_hash[*(unsigned char *)str];
	}
private:
	char keyword_hash[256];
} KeywordHash;

static bool scan_keyword(const char *&input, Token &token);
static void scan_variable(const char *&input, Token &token);
static void scan_number(const char *&input, Token &token);
static void scan_string(const char *&input, Token &token);
static void scan_time(const char *&input, Token  &token);
static void scan_operator(const char *&input, Token &token);
#endif

////////////////////////////////////////////////////////////////////////////////
// Implementation of class Token
////////////////////////////////////////////////////////////////////////////////

Token::Token()
{
#ifdef USE_NEW_SCANNER
	strVal = (char *) malloc(1025);
	strValLength = 1024;
#endif
	reset();
}

Token::~Token()
{
#ifdef USE_NEW_SCANNER
	if (strVal != NULL) {
		free(strVal);
	}
	strValLength = 0;
#endif
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

#ifdef USE_NEW_SCANNER 

void Scanner(const char *& input, Token& token)
{ 
    // skip white space
    token.length = 0;
    while (isspace(*input)) {
        input++;
		token.length++;
    }

    if (isalpha(*input) || *input == '_' || *input == '.' ) {
		if (!scan_keyword(input, token)) {
			scan_variable(input, token);
			token.isString = TRUE; // scan_variable is not a friend, can't set this
		}
	} else if (isdigit(*input)) {
		scan_number(input, token);
	} else if (*input == '"') {
		scan_string(input, token);
		token.isString = TRUE; // scan_string is not a friend, can't set this
	} else if (*input == '\'') { 
		scan_time(input, token);
		token.isString = TRUE;
	} else {
		scan_operator(input, token);
	}
	return;
}

static bool scan_keyword(
    const char   *&input,
	Token  &token)
{
	int  keyword_index;
	bool is_keyword;

	if( KeywordHash.notAKeyword(input) ) {
		return false;
	}
		// might still not be a keyword, but at least there is a chance...

	is_keyword = false;
	
	for(keyword_index = 0; keyword_index < (int) NUMBER_OF_KEYWORDS; keyword_index++) {
		const struct keywords *keyword;
		
		keyword = &(keywords[keyword_index]);
		if (!strncasecmp(input, keyword->keyword, keyword->keyword_length)
			&& !isalpha(*(input + keyword->keyword_length))
			&& !isdigit(*(input + keyword->keyword_length))
			&& *(input + keyword->keyword_length) != '_') {

			input        += keyword->keyword_length;
			token.length += keyword->keyword_length;
			token.type    = keyword->type;
			token.intVal  = keyword->int_value;
			
			is_keyword = true;
			break;
		}
	}
	return is_keyword;
}

static void scan_variable(
    const char   *&input,
    Token  &token)
{
	int  variable_length;
	const char *s;

	variable_length = 0;
	s = input;
	
	// First, we count how many characters the variable is. 
	while (isalnum(*s) || *s == '_' || *s == '.') {
		variable_length++;
		s++;
	}

	// Then we make sure we have space to store it.
	if (variable_length > token.strValLength) {
		free(token.strVal);
		token.strVal = (char *) malloc(variable_length + 1);
		token.strValLength = variable_length;
	}

	// Finally, we copy the variable. 
	strncpy(token.strVal, input, variable_length);
	token.strVal[variable_length] = 0;

	input          += variable_length;
	token.length   += variable_length;
	token.type      = LX_VARIABLE;

	//token.isString  = TRUE;
	return;
}

static void scan_number(
    const char   *&input,
	Token  &token)
{
	const char *digit_text;

	digit_text = input;

	// Count how long the number is
	while(isdigit(*digit_text)) {
		digit_text++;
		token.length++;
	}

	// Check if it's a floating point value
	if(*digit_text == '.') {
		token.length++;
		
		// It is a float, so count the fractional digits as well.
		for(digit_text++; isdigit(*digit_text); digit_text++) {
			token.length++;
		}

		// Convert the float
		token.floatVal = (float) strtod(input, (char **)&input);
		token.type = LX_FLOAT; 
	} else {
		// It's just a plain integer
		token.intVal = strtol(input, (char **)&input, 10);
		token.type = LX_INTEGER; 
	}
	return;
}

static void scan_string(
    const char   *&input,
	Token  &token)
{

	int   string_length;
	const char *s;

	// skip the initial quote mark 
	input++;
	token.length++;

	s = input;
	string_length = 0;

	// First count the length of the string. 
	while (*s != '"' && *s != '\0') {
		// These quoting rules are basically wrong.
		// However, they are required for backwards compatibility.
		// A quote may be escaped with a backslash.
		// Backslash does not escape any other character.
		// Exception: \" at the end of a string or a line is a backslash, not a quote.
		if(*s == '\\' && *(s+1) == '"' && *(s+2) != '\0' && *(s+2) != '\n' && *(s+2) != '\r' ) {
			s++;
			if(!*s) break;
		}
		s++;
		string_length++;
	}

	// Check if the string is not properly terminated with a quote mark. 
	if (*s == '\0') {
		token.type = LX_ERROR;
		token.length = 0;			 
	} else {
		// Make sure we have space to store the string.
		if (string_length > token.strValLength) {
			free(token.strVal);
			token.strVal = (char *) malloc(string_length + 1);
			token.strValLength = string_length;
		}

		// Copy the string. Because of escaped quotes, we have to 
		// copy the string without the benefit of strncpy
		char *dest;
		dest = token.strVal;
        while(*input != '"' && *input != '\0')
		{
			// See explanation above.
	                if(*input == '\\' && *(input+1) == '"' && *(input+2) != '\0' && *(input+2) != '\n' && *(input+2) != '\r' ) {
				input++;
				token.length++; 
			} 
			*dest = *input;
			input++;
            dest++;
			token.length++;
        }

		token.strVal[string_length] = 0;

		// Update our tracking.
		input++;        // for the final quote
		token.length++; // for the final quote
		token.type = LX_STRING;
	}
	return;
}

// We can time, which is just a string.
static void scan_time(
    const char   *&input,
	Token  &token)
{
	int   string_length;
	const char *s;

	// skip the initial quote mark 
	input++;
	token.length++;

	s = input;
	string_length = 0;

	// First count the length of the string. 
	while (*s != '\'' && *s != '\0') {
		s++;
		string_length++;
	}

	// Check if the string is not properly terminated with a quote mark. 
	if (*s == '\0') {
		token.type = LX_ERROR;
		token.length = 0;			 
	} else {
		// Make sure we have space to store the string.
		if (string_length > token.strValLength) {
			free(token.strVal);
			token.strVal = (char *) malloc(string_length + 1);
			token.strValLength = string_length;
		}

		// Copy the string.
		char *dest;
		dest = token.strVal;
        while(*input != '\'' && *input != '\0')
		{
			*dest = *input;
			input++;
            dest++;
			token.length++;
        }

		token.strVal[string_length] = 0;

		// Update our tracking.
		input++;        // for the final quote
		token.length++; // for the final quote
		token.type = LX_TIME;
	}
	return;
}

static void scan_operator(
    const char   *&input,
    Token  &token)
{
	switch(*input) {
	case '(': 
		token.type = LX_LPAREN;
		input = input + 1;
		token.length++;
		break; 
		
	case ')': 
		token.type = LX_RPAREN;
		input = input + 1;
		token.length++;
		break; 

	case '+': 
		token.type = LX_ADD;
		input = input + 1;
		token.length++;
		break;

	case '-': 
		token.type = LX_SUB;
		input = input + 1;
		token.length++;
		break;

	case '*': 
		token.type = LX_MULT;
		input = input + 1;
		token.length++;
		break;
		
	case '/': 
		token.type = LX_DIV;
		input = input + 1;
		token.length++;
		break;
		
	case '<': 
		input = input + 1;
		token.length++;
		if(*input == '=') {
			input = input + 1;
			token.length++;
			token.type = LX_LE;
		} else {
			token.type = LX_LT; 
		}
		break; 
		
	case '>': 
		input = input + 1;
		token.length++;
		if(*input == '=') {
			input = input + 1;
			token.length++;
			token.type = LX_GE; 
		} else {
			token.type = LX_GT; 
		}
		break; 
		
	case '&': 
		input = input + 1;
		token.length++;
		if(*input == '&') {
			token.type = LX_AND;
			input = input + 1;
			token.length++;
		} else {
			token.type = LX_ERROR;
		}
		break;

	case '|': 
		input = input + 1;
		token.length++;
		if(*input == '|') {
			token.type = LX_OR;
			input = input + 1;
		} else {
			token.type = LX_ERROR;
		}
		break;

	case '\0':
	case '\n': 
		token.type = LX_EOF;
		break;

	case '!': 
		input = input + 1;
		token.length++;
		if(*input == '=') {
			token.type = LX_NEQ;
			input = input + 1;
			token.length++;
		} else {
			token.type = LX_ERROR;
		}
		break;
		
	case '=': 
		input = input + 1;
		token.length++;
		if(*input == '=') {
			token.type = LX_EQ;
			input = input + 1;
			token.length++;
		} else if (*input == '?' && *(input+1) == '=') {
			token.type = LX_META_EQ;
			input = input + 2;
			token.length += 2;
		} else if (*input == '!' && *(input+1) == '=') {
			token.type = LX_META_NEQ;
			input = input + 2;
			token.length += 2;
		} else {
			token.type = LX_ASSIGN;
		}
		break;

	case '$': 
		token.type = LX_MACRO;
		input = input + 1;
		token.length++;
		break;

	case ',':
		token.type = LX_COMMA;
		input = input + 1;
		token.length++;
		break;

	case ';':
		token.type = LX_SEMICOLON;
		input = input + 1;
		token.length++;
		break;
			
	default:
		token.type = LX_ERROR;
		break;
	}
	return;
}

#else /* USE_NEW_SCANNER is not defined */
void Scanner(const char *& s, Token& t)
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
#endif /* USE_EXTENSIBLE_ARRAY is not defined */
