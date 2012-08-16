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


// Includes 
#include "classad/common.h"
#include "classad/lexer.h"
#include "classad/util.h"
#include "classad/classad.h"

using namespace std;

namespace classad {

// ctor
Lexer::
Lexer ()
{
	// initialize lexer state (token, etc.) variables
	tokenType = LEX_END_OF_INPUT;
	lexBufferCount = 0;
	savedChar = 0;
	ch = 0;
	inString = false;
	tokenConsumed = true;
	accumulating = false;
    initialized = false;

	// debug flag
	debug = false;
	return;
}


// dtor
Lexer::
~Lexer ()
{
}


// Initialization method:  Initialize with immutable string
//   +  Token will be accumulated in the lexBuffer
bool Lexer::
Initialize(LexerSource *source)
{
	lexSource = source;
	ch = lexSource->ReadCharacter();

	// token state initialization
	lexBuffer = ch;
	lexBufferCount = 0;
	inString = false;
	tokenConsumed = true;
	accumulating = false;
    initialized = true;
	return true;
}


bool Lexer::
Reinitialize(void)
{
	ch = lexSource->ReadCharacter();
	// token state initialization
	lexBuffer = ch;
	lexBufferCount = 0;
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	return true;
}

bool Lexer::
WasInitialized(void)
{
    return initialized;
}

// FinishedParse:  This function implements the cleanup phase of a parse.
//   String valued tokens are entered into a string space, and maintained
//   with reference counting.  When a parse is finished, this space is flushed
//   out.
void Lexer::
FinishedParse ()
{
	accumulating = false;
	return;
}


// Mark:  This function is called when the beginning of a token is detected
void Lexer::
mark (void)
{
	lexBuffer = ch;
	lexBufferCount = 0;
	accumulating = true;
	return;
}


// Cut:  This function is called when the end of a token is detected
void  Lexer::
cut (void)
{
	if(lexBufferCount < lexBuffer.length())
	{
		lexBuffer[lexBufferCount] = '\0';
	}
	accumulating = false;
	return;
}


// Wind:  This function is called when an additional character must be read
//        from the input source; the conceptual action is to move the cursor
void Lexer::
wind (void)
{
	if(ch == -1) return;
	ch = lexSource->ReadCharacter();
	++lexBufferCount;
	if( ch == -1 ) return;
	if( accumulating ) {
		lexBuffer += ch; 
	}
	return;
}

			
Lexer::TokenType Lexer::
ConsumeToken (TokenValue *lvalp)
{
	if (lvalp) lvalp->CopyFrom( yylval );

	// if a token has already been consumed, get another token
	if (tokenConsumed) PeekToken (lvalp);

	if (debug) {
		printf ("Consume: %s\n", strLexToken(tokenType));
	}

	tokenConsumed = true;
	return tokenType;
}


// peekToken() returns the same token till consumeToken() is called
Lexer::TokenType Lexer::
PeekToken (TokenValue *lvalp)
{
	if (!tokenConsumed) {
		if( lvalp ) lvalp->CopyFrom( yylval );
		return tokenType;
	}

	// Set the token to unconsumed
	tokenConsumed = false;
	
	// consume white space
	while( 1 ) {
		if( isspace( ch ) ) {
			wind( );
			continue;
		} else if( ch == '/' ) {
			mark( );
			wind( );
			if( ch == '/' ) {
				// a c++ style comment
				while( ch > 0 && ch != '\n' ) {
					wind( );
				}
			} else if( ch == '*' ) {
				// a c style comment
				int oldCh;
				ch = '\n';
				do {
					oldCh = ch;
					wind( );
				} while( oldCh != '*' || ch != '/' );
				wind( );
			} else {
				// just a division operator
				cut( );
				tokenType = LEX_DIVIDE;
				yylval.SetTokenType( tokenType );
				return( tokenType );
			}
		} else {
			break; // out of while( 1 ) loop
		}
	}

	// check if this is the end of the input
	if (ch == 0 || ch == EOF) {
		tokenType = LEX_END_OF_INPUT;
		yylval.SetTokenType( tokenType );
		return tokenType;
	}

	// check the first character of the token
	if ( ch == '-' ) {
		// Depending on the last token we saw, a minus may be the start
		// of an integer or real token. tokenizeNumber() does the right
		// thing if there is no subsequent integer or real.
		switch ( tokenType ) {
		case LEX_INTEGER_VALUE:
		case LEX_REAL_VALUE:
		case LEX_BOOLEAN_VALUE:
		case LEX_STRING_VALUE:
		case LEX_UNDEFINED_VALUE:
		case LEX_ERROR_VALUE:
		case LEX_IDENTIFIER:
		case LEX_SELECTION:
		case LEX_CLOSE_BOX:
		case LEX_CLOSE_PAREN:
		case LEX_CLOSE_BRACE:
		case LEX_BACKSLASH:
		case LEX_ABSOLUTE_TIME_VALUE:
		case LEX_RELATIVE_TIME_VALUE:
			tokenizePunctOperator();
			break;
		default:
			tokenizeNumber();
			break;
		}
	} else if (isdigit( ch ) || ch == '.' ) {
		// tokenizeNumber() also takes care of the selection operator
		tokenizeNumber();	
	} else if (isalpha (ch) || ch == '_') {
		tokenizeAlphaHead ();
	} else if (ch == '\"') {
		tokenizeString('\"'); // its a string literal
	} else if( ch == '\'' ) {
		tokenizeString('\''); // its a quoted attribute
	} 

	else {
		tokenizePunctOperator ();
	}

	if (debug) {
		printf ("Peek: %s\n", strLexToken(tokenType));
	}

	if (lvalp) lvalp->CopyFrom( yylval );

	yylval.SetTokenType( tokenType );
	return tokenType;
}	


// Tokenize number constants:
//   1.  Integers:  [-] 0[0-7]+ | 0[xX][0-9a-fA-F]+ | [0-9]+
//   2.  Reals   :  [-] [0-9]*\.[0-9]* (e|E) [+-]? [0-9]+
int Lexer::
tokenizeNumber (void)
{
	enum { NONE, INTEGER, REAL };
	int		numberType = NONE;
	Value::NumberFactor f;
	long long integer=0;
	double	real=0;
	int 	och;

	och = ch;
	mark( );
	wind( );

	if ( och == '-' ) {
		// This may be a negative number or the unary minus operator
		// The subsequent two characters will tell us which.
		if ( isdigit( ch ) ) {
			// It looks like a negative number, keep reading.
			och = ch;
			wind();
		} else if ( ch == '.' ) {
			// This could be a real number or an attribute reference
			// starting with dot. Look at the second character.
			int ch2 = lexSource->ReadCharacter();
			if ( ch2 >= 0 ) {
				lexSource->UnreadCharacter();
			}
			if ( !isdigit( ch2 ) ) {
				// It's not a real number, return a minus token.
				cut();
				tokenType = LEX_MINUS;
				return tokenType;
			}
			// It looks like a negative real, keep reading.
		} else {
			// It's not a number, return a minus token.
			cut();
			tokenType = LEX_MINUS;
			return tokenType;
		}
	}

	if( och == '0' ) {
		// number is octal, hex or real
		if( tolower( ch ) == 'x' ) {
			// get hex digits only; parse hex-digit+
			numberType = INTEGER;
			wind( );
			if( !isxdigit( ch ) ) {
				cut( );
				tokenType = LEX_TOKEN_ERROR;
				return( tokenType ) ;
			}
			while( isxdigit( ch ) ) {
				wind( );
			}
		} else {
			// get octal or real
			numberType = INTEGER;
			while( isdigit( ch ) ) {
				wind( );
				if( !isodigit( ch ) ) {
					// not an octal number
					numberType = REAL;
				}
			}
			if( ch == '.' || tolower( ch ) == 'e' ) {
				numberType = REAL;
			} else if( numberType == REAL ) {
				// non-octal digits, but not a real (no '.' or 'e')
				// so, illegal octal constant
				cut( );
				tokenType = LEX_TOKEN_ERROR;
				return( tokenType );
			}
		}
	} else if( isdigit( och ) ) {
		// decimal or real; get digits
		while( isdigit( ch ) ) {
			wind( );
		}
		numberType = ( ch=='.' || tolower( ch )=='e' ) ? REAL : INTEGER;
	} 

	if( och == '.' || ch == '.' ) {
		// fraction part of real or selection operator
		if( ch == '.' ) wind( );
		if( isdigit( ch ) ) {
			// real; get digits after decimal point
			numberType = REAL;
			while( isdigit( ch ) ) {
				wind( );
			}
		} else {
			if( numberType != NONE ) {
				// initially like a number, but no digit following the '.'
				cut( );
				tokenType = LEX_TOKEN_ERROR;
				return( tokenType );
			}
			// selection operator
			cut( );
			tokenType = LEX_SELECTION;
			return( tokenType );
		}
	}

	// if we are tokenizing a real, the (optional) exponent part is left
	//   i.e., [eE][+-]?[0-9]+
	if( numberType == REAL && tolower( ch ) == 'e' ) {
		wind( );
		if( ch == '+' || ch == '-' ) wind( );
		if( !isdigit( ch ) ) {
			cut( );
			tokenType = LEX_TOKEN_ERROR;
			return( tokenType );
		}
		while( isdigit( ch ) ) {
			wind( );
		}
	}

	if( numberType == INTEGER ) {
		cut( );
		long long l;
		int base = 0;
		if ( _useOldClassAdSemantics ) {
			// Old ClassAds don't support octal or hexidecimal
			// representations for integers.
			base = 10;
		}
#ifdef WIN32
		l = _strtoi64( lexBuffer.c_str(), NULL, base );
#else
		l = strtoll( lexBuffer.c_str(), NULL, base );
#endif
		integer = l;
	} else if( numberType == REAL ) {
		cut( );
		real = strtod( lexBuffer.c_str(), NULL );
	} else {
		/* If we've reached this point, we have a serious programming
		 * error: tokenizeNumber should only be called if we are
		 * lexing a number or a selection, and we didn't find a number
		 * or a selection. This should really never happen, so we
		 * bomb if it does. It should be reported as a bug.
		 */
			CLASSAD_EXCEPT("Should not reach here");
	}

	switch( toupper( ch ) ) {
		case 'B': f = Value::B_FACTOR; wind( ); break;	
		case 'K': f = Value::K_FACTOR; wind( ); break;
		case 'M': f = Value::M_FACTOR; wind( ); break;
		case 'G': f = Value::G_FACTOR; wind( ); break;
		case 'T': f = Value::T_FACTOR; wind( ); break;
		default:
			f = Value::NO_FACTOR;
	}

	if( numberType == INTEGER ) {
		yylval.SetIntValue( integer, f );
		yylval.SetTokenType( LEX_INTEGER_VALUE );
		tokenType = LEX_INTEGER_VALUE;
	} else {
		yylval.SetRealValue( real, f );
		yylval.SetTokenType( LEX_REAL_VALUE );
		tokenType = LEX_REAL_VALUE;
	}

	return( tokenType );
}


// Tokenize alpha head: (character sequences beggining with an alphabet)
//   1.  Reserved character sequences:  true, false, error, undefined
//   2.  Identifier                  :  [a-zA-Z_][a-zA-Z0-9_]*
int Lexer::
tokenizeAlphaHead (void)
{
	mark( );
	while (isalpha (ch)) {
		wind ();
	}

	if (isdigit (ch) || ch == '_') {
		// The token is an identifier; consume the rest of the token
		wind ();
		while (isalnum (ch) || ch == '_') {
			wind ();
		}
		cut ();

		tokenType = LEX_IDENTIFIER;
		yylval.SetStringValue( lexBuffer.c_str( ) );
		
		return tokenType;
	}	

	// check if the string is one of the reserved words; Case insensitive
	cut ();
	if (strcasecmp(lexBuffer.c_str(), "true") == 0) {
		tokenType = LEX_BOOLEAN_VALUE;
		yylval.SetBoolValue( true );
	} else if (strcasecmp(lexBuffer.c_str(), "false") == 0) {
		tokenType = LEX_BOOLEAN_VALUE;
		yylval.SetBoolValue( false );
	} else if (strcasecmp(lexBuffer.c_str(), "undefined") == 0) {
		tokenType = LEX_UNDEFINED_VALUE;
	} else if (strcasecmp(lexBuffer.c_str(), "error") == 0) {
		tokenType = LEX_ERROR_VALUE;
	} else if (strcasecmp(lexBuffer.c_str(), "is") == 0 ) {
		tokenType = LEX_META_EQUAL;
	} else if (strcasecmp(lexBuffer.c_str(), "isnt") == 0) {
		tokenType = LEX_META_NOT_EQUAL;
	} else {
		// token is a character only identifier
		tokenType = LEX_IDENTIFIER;
		yylval.SetStringValue( lexBuffer.c_str() );
	}

	return tokenType;
}



// tokenizeStringLiteral:  Scans strings of the form " ... " or '...' 
// based on whether the argument passed was '\"' or '\''
int Lexer::
tokenizeString(char delim)
{
	bool stringComplete = false;

	// need to mark() after the quote
	inString = true;
	wind ();
	mark ();
	
	while (!stringComplete) {
		bool oddBackWhacks = false;
		int oldCh = 0;
		// consume the string literal; read upto " ignoring \"
		while( ( ch > 0 ) && ( ch != delim || ( ch == delim && oldCh == '\\' && oddBackWhacks ) ) ) {
			if( !oddBackWhacks && ch == '\\' ) {
				oddBackWhacks = true;
			}
			else {
				oddBackWhacks = false;
			}
			oldCh = ch;
			wind( );
		}
		
		if( ch == delim ) {
			int tempch = ' ';
			// read past the whitespace characters
			while (isspace(tempch)) {
				tempch = lexSource->ReadCharacter();
			}
			if (tempch != delim) {  // a new token exists after the string
                if (tempch != -1) {
                    lexSource->UnreadCharacter();
                }
				stringComplete = true;
			} else {    // the adjacent string is to be concatenated to the existing string
				lexBuffer.erase(lexBufferCount--); // erase the lagging '\"'
				wind();
			}
		}
		else {
			// loop quit due to ch == 0 or ch == EOF
			tokenType = LEX_TOKEN_ERROR;
			return tokenType;
		}    
	}
	cut( );
	wind( );	// skip over the close quote
	bool validStr = true; // to check if string is valid after converting escape
	convert_escapes(lexBuffer, validStr);
	yylval.SetStringValue( lexBuffer.c_str( ) );
	if (validStr) {
		if(delim == '\"') {
			tokenType = LEX_STRING_VALUE;
		}
		else {
			tokenType = LEX_IDENTIFIER;
		}
	}
	else {
		tokenType = LEX_TOKEN_ERROR; // string conatins a '\0' character inbetween
	}
	
	return tokenType;
}


// tokenizePunctOperator:  Tokenize puncutation and operators 
int Lexer::
tokenizePunctOperator (void)
{
	// save character; may need to lookahead
	int oldch = ch;
	int extra_lookahead;

	mark( );
	wind ();
	switch (oldch) {
		// these cases don't need lookaheads
		case '.':
			tokenType = LEX_SELECTION;
			break;


		case '*':	
			tokenType = LEX_MULTIPLY;		
			break;


		case '/':	
			tokenType = LEX_DIVIDE;		
			break;


		case '%':	
			tokenType = LEX_MODULUS;		
			break;


		case '+':	
			tokenType = LEX_PLUS;			
			break;


		case '-':	
			tokenType = LEX_MINUS;			
			break;


		case '~':	
			tokenType = LEX_BITWISE_NOT;	
			break;


		case '^':	
			tokenType = LEX_BITWISE_XOR;	
			break;


		case '?':	
			tokenType = LEX_QMARK;			
			break;


		case ':':	
			tokenType = LEX_COLON;			
			break;


		case ';':	
			tokenType = LEX_SEMICOLON;		
			break;


		case ',':
			tokenType = LEX_COMMA;
			break;


		case '[':	
			tokenType = LEX_OPEN_BOX;		
			break;


		case ']':	
			tokenType = LEX_CLOSE_BOX;		
			break;


		case '(':	
			tokenType = LEX_OPEN_PAREN;	
			break;


		case ')':	
			tokenType = LEX_CLOSE_PAREN;	
			break;


		case '{':
			tokenType = LEX_OPEN_BRACE;
			break;


		case '}':
			tokenType = LEX_CLOSE_BRACE;
			break;


		// the following cases need lookaheads

		case '&':
			tokenType = LEX_BITWISE_AND;
			if (ch == '&') {
				tokenType = LEX_LOGICAL_AND;
				wind ();
			}
			break;


		case '|':
			tokenType = LEX_BITWISE_OR;
			if (ch == '|') {
				tokenType = LEX_LOGICAL_OR;
				wind ();
			}
			break;


		case '<':
			tokenType = LEX_LESS_THAN;
			switch (ch) {
				case '=':
					tokenType = LEX_LESS_OR_EQUAL;
					wind ();
					break;

				case '<':
					tokenType = LEX_LEFT_SHIFT;
					wind ();
					break;

				default:
					// just the '<' --- no need to do anything
					break;
			}
			break;


		case '>':
			tokenType = LEX_GREATER_THAN;
			switch (ch) {
				case '=':
					tokenType = LEX_GREATER_OR_EQUAL;
					wind ();
					break;

				case '>':
					tokenType = LEX_RIGHT_SHIFT;
					wind ();
					if (ch == '>') {
						tokenType = LEX_URIGHT_SHIFT;
						wind ();
					}
					break;
				
				default:
					// just the '>' --- no need to do anything
					break;
			}
			break;


		case '=':
			tokenType = LEX_BOUND_TO;
			switch (ch) {
				case '=':
					tokenType = LEX_EQUAL;
					wind ();
					break;

				case '?':
					tokenType = LEX_META_EQUAL;
					wind ();

					// ensure the trailing '=' of the '=?=' combination
					if (ch != '=') 
					{
						tokenType = LEX_TOKEN_ERROR;
						return tokenType;
					}

					wind ();
					break;

				case '!':
					extra_lookahead = lexSource->ReadCharacter();
					lexSource->UnreadCharacter();
					if (extra_lookahead == '=') {
						tokenType = LEX_META_NOT_EQUAL;
						wind();
						wind();
					}
					break;

				default:
					// just the '=' --- no need to do anything
					break;
			}
			break;


		case '!':
			tokenType = LEX_LOGICAL_NOT;
			switch (ch) {
				case '=':
					tokenType = LEX_NOT_EQUAL;
					wind ();
					break;

				default:
					// just the '!' --- no need to do anything
					break;
			}
			break;


		default:
			tokenType = LEX_TOKEN_ERROR;
			return tokenType;
	}

	// cut the token and return 
	cut ();

	return tokenType;
}


// strLexToken:  Return string representation of token type
const char *Lexer::
strLexToken (int tokenValue)
{
	switch (tokenValue) {
		case LEX_END_OF_INPUT:           return "LEX_END_OF_INPUT";
		case LEX_TOKEN_ERROR:            return "LEX_TOKEN_ERROR";
		case LEX_TOKEN_TOO_LONG:         return "LEX_TOKEN_TOO_LONG";

		case LEX_INTEGER_VALUE:          return "LEX_INTEGER_VALUE";
		case LEX_REAL_VALUE:             return "LEX_REAL_VALUE";
		case LEX_BOOLEAN_VALUE:          return "LEX_BOOLEAN_VALUE";
		case LEX_STRING_VALUE:           return "LEX_STRING_VALUE";
		case LEX_UNDEFINED_VALUE:        return "LEX_UNDEFINED_VALUE";
		case LEX_ERROR_VALUE:            return "LEX_ERROR_VALUE";

		case LEX_IDENTIFIER:             return "LEX_IDENTIFIER";
		case LEX_SELECTION:       		 return "LEX_SELECTION";

		case LEX_MULTIPLY:               return "LEX_MULTIPLY";
		case LEX_DIVIDE:                 return "LEX_DIVIDE";
		case LEX_MODULUS:                return "LEX_MODULUS";
		case LEX_PLUS:                   return "LEX_PLUS";
		case LEX_MINUS:                  return "LEX_MINUS";

		case LEX_BITWISE_AND:            return "LEX_BITWISE_AND";
		case LEX_BITWISE_OR:             return "LEX_BITWISE_OR";
		case LEX_BITWISE_NOT:            return "LEX_BITWISE_NOT";
		case LEX_BITWISE_XOR:            return "LEX_BITWISE_XOR";

		case LEX_LEFT_SHIFT:             return "LEX_LEFT_SHIFT";
		case LEX_RIGHT_SHIFT:    		 return "LEX_RIGHT_SHIFT";
		case LEX_URIGHT_SHIFT:      	 return "LEX_URIGHT_SHIFT";

		case LEX_LOGICAL_AND:            return "LEX_LOGICAL_AND";
		case LEX_LOGICAL_OR:             return "LEX_LOGICAL_OR";
		case LEX_LOGICAL_NOT:            return "LEX_LOGICAL_NOT";

		case LEX_LESS_THAN:              return "LEX_LESS_THAN";
		case LEX_LESS_OR_EQUAL:          return "LEX_LESS_OR_EQUAL";
		case LEX_GREATER_THAN:           return "LEX_GREATER_THAN";
		case LEX_GREATER_OR_EQUAL:       return "LEX_GREATER_OR_EQUAL";
		case LEX_EQUAL:                  return "LEX_EQUAL";
		case LEX_NOT_EQUAL:              return "LEX_NOT_EQUAL";
		case LEX_META_EQUAL:             return "LEX_META_EQUAL";
		case LEX_META_NOT_EQUAL:         return "LEX_META_NOT_EQUAL";

		case LEX_BOUND_TO:               return "LEX_BOUND_TO";

		case LEX_QMARK:                  return "LEX_QMARK";
		case LEX_COLON:                  return "LEX_COLON";
		case LEX_SEMICOLON:              return "LEX_SEMICOLON";
		case LEX_COMMA:					 return "LEX_COMMA";
		case LEX_OPEN_BOX:               return "LEX_OPEN_BOX";
		case LEX_CLOSE_BOX:              return "LEX_CLOSE_BOX";
		case LEX_OPEN_PAREN:             return "LEX_OPEN_PAREN";
		case LEX_CLOSE_PAREN:            return "LEX_CLOSE_PAREN";
		case LEX_OPEN_BRACE: 			 return "LEX_OPEN_BRACE";
		case LEX_CLOSE_BRACE: 			 return "LEX_CLOSE_BRACE";
		case LEX_BACKSLASH:              return "LEX_BACKSLASH";
	    case LEX_ABSOLUTE_TIME_VALUE:    return "LEX_ABSOLUTE_TIME_VALUE";
	    case LEX_RELATIVE_TIME_VALUE:    return "LEX_RELATIVE_TIME_VALUE";

		default:
				return "** Unknown **";
	}
}

} // classad
