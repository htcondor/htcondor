// Includes 
#include "condor_common.h"
#include "escapes.h"
#include "lexer.h"

// for EXCEPT in condor_debug.h
static char _FileName_[] = __FILE__;

// macro for recognising octal digits
#define isodigit(x) ( (x) - '0' < 8 )

// ctor
Lexer::
Lexer ()
{
	// initialize input source variables
	lexInputSource = LEXER_SRC_NONE;
	lexInputStream = NULL;
	lexInputLength = 0;

	// initialize lexer state (token, etc.) variables
	tokenType = LEX_END_OF_INPUT;
	lexBufferCount = 0;
	savedChar = 0;
	ch = 0;
	pos = 0;	
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	// debug flag
	debug = false;
}


// dtor
Lexer::
~Lexer ()
{
}


// Initialization method:  Initialize with immutable string
//   +  Token will be accumulated in the lexBuffer
bool Lexer::
initializeWithString (char *buf, int buflen)
{
	// sanity check
	if (!buf || buflen < 1) return false;

	// set source characteristics
	lexInputSource = LEXER_SRC_STRING;
	lexInputStream = (char *) buf;
	lexInputLength = buflen;

	// token state initialization
	pos = 0;
	ch = lexInputStream[pos];
	lexBufferCount = 0;
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	return true;
}


// Initialization method:  Initialize with cedar
//   + The tokens will be accumulated in the lexBuffer
bool Lexer::
initializeWithCedar (Sock &s)
{
	// set source characteristics
	lexInputSource = LEXER_SRC_CEDAR;
	sock           = &s;

	// the following are not used for cedar sources
	lexInputLength = 0;		
	lexInputStream = NULL;

	// token state initialization
	pos = 0;
	lexBufferCount = 0;
	if (!sock->get_bytes (lexBuffer, 1)) return false;
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	// the first character
	ch = *lexBuffer;

	return true;
}


// Initialization method:  Initialize with file descriptor (similar to cedar)
//   + The tokens will be accumulated in the lex buffer
bool Lexer::
initializeWithFd (int desc)
{
	// set source characteristics
    lexInputSource = LEXER_SRC_FILE_DESC;
	fd             = desc;

	// the following are not used for file desc sources
    lexInputLength = 0;
    lexInputStream = NULL;

    // token state initialization
    pos = 0;
	lexBufferCount = 0;
	if (read (fd, lexBuffer, 1) < 0) return false;
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	// the first character
	ch = *lexBuffer;

    return true;
}


// Initialization method:  Initialize with file structure (similar to cedar)
//   + The tokens will be accumulated in the lex buffer
bool Lexer::
initializeWithFile (FILE *fp)
{
	// set source characteristics
    lexInputSource = LEXER_SRC_FILE;
	file           = fp;

	// the following are not used for file desc sources
    lexInputLength = 0;
    lexInputStream = NULL;

    // token state initialization
    pos = 0;
    lexBufferCount = 0;
	inString = false;
	tokenConsumed = true;
	accumulating = false;

	// the first character
	if ((ch = getc (fp)) == EOF) return false;

	// place the first character in the lexBuffer
	*lexBuffer = (char) ch;

    return true;
}



// FinishedParse:  This function implements the cleanup phase of a parse.
//   String valued tokens are entered into a string space, and maintained
//   with reference counting.  When a parse is finished, this space is flushed
//   out.
void Lexer::
finishedParse ()
{
	accumulating = false;
}


// Mark:  This function is called when the beginning of a token is detected
void Lexer::
mark (void)
{
	*lexBuffer = (char) ch;	
	lexBufferCount = 0;
	accumulating = true;
}


// Cut:  This function is called when the end of a token is detected
void  Lexer::
cut (void)
{
	lexBuffer[lexBufferCount] = '\0';
	accumulating = false;
}


// Wind:  This function is called when an additional character must be read
//        from the input source; the conceptual action is to move the cursor
void Lexer::
wind (void)
{
	char chr;

	// check the source
	switch (lexInputSource) {
		case LEXER_SRC_STRING:
			// common operations to both
			pos++;
			if (pos >= lexInputLength) { ch = EOF; return; }
			ch = lexInputStream[pos];
			break;

		// in the rest of the cases, the cursor is irrelevent --- just
		// accumulate the token in the lexBuffer
		case LEXER_SRC_CEDAR:
			if (!sock->get_bytes (&chr, 1)) { ch = EOF; return; }
			ch = chr;
			break;

		case LEXER_SRC_FILE:
			if ((ch = getc (file)) == EOF)	{ return; }
			break;

		case LEXER_SRC_FILE_DESC:
			if ((read (fd, &chr, 1)) < 0) { ch = EOF; return; }
			ch = chr;
			break;

		default:
			ch = EOF;
			return;
	}
	if( accumulating ) lexBuffer[++lexBufferCount] = ch;
}

			
TokenType Lexer::
consumeToken (TokenValue *lvalp)
{
	if (lvalp) *lvalp = yylval;

	// if a token has already been consumed, get another token
	if (tokenConsumed) peekToken (lvalp);

	tokenConsumed = true;
	return tokenType;
}


// peekToken() returns the same token till consumeToken() is called
TokenType Lexer::
peekToken (TokenValue *lvalp)
{
	if (!tokenConsumed) {
		if( lvalp ) *lvalp = yylval;
		return tokenType;
	}

	// set the token to unconsumed
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
				while( ch && ch != '\n' ) {
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
				return( tokenType );
			}
		} else {
			break; // out of while( 1 ) loop
		}
	}

	// check if this is the end of the input
	if (ch == 0 || ch == EOF) {
		tokenType = LEX_END_OF_INPUT;
		return tokenType;
	}

	// check the first character of the token
	if (isdigit( ch ) || ch == '.' ) {
		// tokenizeNumber() also takes care of the selection operator
		tokenizeNumber();	
	} else if (isalpha (ch) || ch == '_') {
		tokenizeAlphaHead ();
	} else if (ch == '\"') {
		tokenizeStringLiteral ();
	} else {
		tokenizePunctOperator ();
	}

	if (debug) {
		printf ("%s\n", strLexToken(tokenType));
	}

	if (lvalp) *lvalp = yylval;

	return tokenType;
}	


// Tokenize number constants:
//   1.  Integers:  0[0-7]+ | 0[xX][0-9a-fA-F]+ | [0-9]+
//   2.  Reals   :  [0-9]*\.[0-9]* (e|E) [+-]? [0-9]+
int Lexer::
tokenizeNumber (void)
{
	enum { NONE, INTEGER, REAL };
	int		numberType = NONE;
	NumberFactor f;
	int		integer;
	double	real;
	int 	och;

	och = ch;
	mark( );
	wind( );

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

	char *endptr;
	if( numberType == INTEGER ) {
		cut( );
		integer = (int) strtol( lexBuffer, &endptr, 0 );
	} else if( numberType == REAL ) {
		cut( );
		real = strtod( lexBuffer, &endptr );
	} else {
		EXCEPT( "Should not reach here" );
	}

	if( *endptr != '\0' ) {
		EXCEPT( "strto{l,d}() signalled an error" );
	}

	switch( toupper( ch ) ) {
		case 'K': f = K_FACTOR; wind( ); break;
		case 'M': f = M_FACTOR; wind( ); break;
		case 'G': f = G_FACTOR; wind( ); break;
		default:
			f = NO_FACTOR;
	}

	if( numberType == INTEGER ) {
		yylval.intValue.value = integer;
		yylval.intValue.factor = f;
		tokenType = LEX_INTEGER_VALUE;
	} else {
		yylval.realValue.value = real;
		yylval.realValue.factor = f;
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
		yylval.strValue = lexBuffer;
		
		return tokenType;
	}	

	// check if the string is one of the reserved words; Case insensitive
	cut ();
	if (strcasecmp(lexBuffer, "true") == 0) {
		tokenType = LEX_BOOLEAN_VALUE;
		yylval.boolValue = true;
	} else if (strcasecmp(lexBuffer, "false") == 0) {
		tokenType = LEX_BOOLEAN_VALUE;
		yylval.boolValue = false;
	} else if (strcasecmp(lexBuffer, "undefined") == 0) {
		tokenType = LEX_UNDEFINED_VALUE;
	} else if (strcasecmp(lexBuffer, "error") == 0) {
		tokenType = LEX_ERROR_VALUE;
	} else if (strcasecmp(lexBuffer, "is") == 0 ) {
		tokenType = LEX_META_EQUAL;
	} else if (strcasecmp(lexBuffer, "isnt") == 0) {
		tokenType = LEX_META_NOT_EQUAL;
	} else {
		// token is a character only identifier
		tokenType = LEX_IDENTIFIER;
		yylval.strValue = lexBuffer;
	}

	return tokenType;
}


// tokenizeStringLiteral:  Scans strings of the form " ... "
int Lexer::
tokenizeStringLiteral (void)
{
	int oldCh = 0;

	// need to mark() after the quote
	inString = true;
	wind ();
	mark ();

	// consume the string literal; read upto " ignoring \"
	while( ( ch > 0 ) && ( ch != '\"' || ch == '\"' && oldCh == '\\' ) ) {
		wind( );
		oldCh = ch;
	}

	if( ch == '\"' ) {
		cut( );
		wind( );	// skip over the close quote
		collapse_escapes( lexBuffer );
		yylval.strValue = lexBuffer;
		tokenType = LEX_STRING_VALUE;
	} else {
		// loop quit due to ch == 0 or ch == EOF
		tokenType = LEX_TOKEN_ERROR;
	}

	return tokenType;
}


// tokenizePunctOperator:  Tokenize puncutation and operators 
int Lexer::
tokenizePunctOperator (void)
{
	// save character; may need to lookahead
	int oldch = ch;
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
					tokenType = LEX_META_NOT_EQUAL;
					wind ();

					// ensure trailing '=' of the '=!=' combination
					if (ch != '=')
						tokenType = LEX_TOKEN_ERROR;

					wind ();
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
char *Lexer::
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

		default:
				return "** Unknown **";
	}
}
