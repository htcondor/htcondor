// Includes 
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "caseSensitivity.h"
#include "lexer.h"

// for EXCEPT in condor_debug.h
static char _FileName_[] = __FILE__;

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
	lexTokenString = NULL;
	lexBufferCount = 0;
	savedChar = 0;
	ch = 0;
	pos = 0;	
	inString = false;
	tokenConsumed = true;

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
initializeWithString (const char *buf, int buflen)
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
	lexTokenString = NULL;
	inString = false;
	tokenConsumed = true;

	return true;
}


// Initialization method:  Initialize with mutable string
//   + The token will be cut in place
bool Lexer::
initializeWithStringBuffer (char *buf, int buflen)
{
	// sanity check
    if (!buf || buflen < 1) return false;

	// set source characteristics
	lexInputSource = LEXER_SRC_STRING_BUFFER; 
	lexInputLength = buflen;
	lexInputStream = buf;

	// token state initialization
	pos = 0;
	ch  = lexInputStream[pos];
	lexBufferCount = 0;
	lexTokenString = NULL;
	inString = false;
	tokenConsumed = true;

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
	lexTokenString = NULL;
	if (!sock->get_bytes (lexBuffer, 1)) return false;
	inString = false;
	tokenConsumed = true;

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
    lexTokenString = NULL;
	if (read (fd, lexBuffer, 1) < 0) return false;
	inString = false;
	tokenConsumed = true;

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
    lexTokenString = NULL;
	inString = false;
	tokenConsumed = true;

	// the first character
	if ((ch = getc (fp)) == EOF) return false;

	// place the first character in the lexBuffer
	*lexBuffer = (char) ch;

    return true;
}



// FinishedParse:  This function implements the cleanup phase of a parse.
//   String valued tokens are entered into a string space, and maintained
//   with reference counting.  When a parse is finished, this space is flushed
//   out.  Functionally, this is a memory manager for strings in the same
//   spirit as the ParserMemoryManager is for expressions.
void Lexer::
finishedParse ()
{
	strings.purge ();
}


// Mark:  This function is called when the beginning of a token is detected
//   1. If tokens are accumulated in the lexBuffer, set the lexTokenString to
//      the beginning of the lexBuffer
//   2. Otherwise, the token string is constructed in the input stream itself
void Lexer::
mark (void)
{
	switch (lexInputSource) {
		// tokens accumulated in place
		case LEXER_SRC_STRING_BUFFER:
			lexTokenString = lexInputStream + pos;
			break;

		// tokens accumulated in the lexBuffer
		case LEXER_SRC_STRING:
		case LEXER_SRC_CEDAR:
		case LEXER_SRC_FILE:
		case LEXER_SRC_FILE_DESC:
			lexTokenString = lexBuffer;
			*lexBuffer = (char) ch;	
			lexBufferCount = 0;
			break;

		default:
			// should not reach here
			EXCEPT ("ClassAd Lexer:  Should not reach here");
	}
}


// Cut:  This function is called when the end of a token is detected
//   1. If tokens are accumulated in the lexBuffer, terminate the string in the
//      lexBuffer.
//   2. Otherwise, tokens are constructed in-place.  Save the character under
//      the cursor, and replace it with a NULL character.
void  Lexer::
cut (void)
{
	switch (lexInputSource) {
		// tokens accumulated in place
		case LEXER_SRC_STRING_BUFFER:
			savedChar = lexInputStream[pos];	
			lexInputStream[pos] = '\0';
			break;

		// tokens accumulated in the lexBuffer
		case LEXER_SRC_STRING:
		case LEXER_SRC_CEDAR:
		case LEXER_SRC_FILE:
		case LEXER_SRC_FILE_DESC:
			lexTokenString[lexBufferCount] = '\0';
			break;

		default:
			// should not reach here
			EXCEPT ("ClassAd Lexer:  Should not reach here");
	}
}


// Uncut:  Called at the beginning of the yylex() function to restore the
//         input stream due to any previous cut operation.
//  1.  If yylex() is being called for the first time, we should not perform
//      the uncut().  This condition is detected by checking the lexTokenString
//      pointer.
//  2.  Reset the lexTokenString pointer to indicate that we are no longer
//      accumulating a token (can reduce copying to the lexBuffer).
//  3.  If we are uncut()ting after a string literal, also consume the trailing
//      close quote
void Lexer::
uncut (void)
{
	// Check if uncut is being called for the first time (i.e., before any
	// cut), or if the tokens are being accumulated in the lex buffer.  If so, 
	// do *not* perform the uncut.
	if (lexTokenString && lexInputSource == LEXER_SRC_STRING_BUFFER) {
		lexInputStream[pos] = savedChar;
	}

	// Are we uncut()ting a string literal?  If so, consume the close quote
	if (lexTokenString && inString && ch == '\"') {
		lexTokenString = NULL;
		lexBufferCount = 0;
		inString = false;
		wind ();
	}

	// reinitialize the lexBuffer state
	lexTokenString = NULL;
	lexBufferCount = 0;
}


// Wind:  This function is called when an additional character must be read
//        from the input source; the conceptual action is to move the cursor
//  1.  If we are accumulating tokens in the lexBuffer, we must copy a character
//      to the lexBuffer.  However, we need to do this only if we are in the
//      midst of a token.
//  2.  Otherwise, the tokens are being accumulated in place, and we only need
//      to wind the pointer.
//  3.  For external sources (like cedar, FILE and file desc), we do not have
//      to keep track of the cursor
void Lexer::
wind (void)
{
	char chr;

	// are we accumulating a token?
	if (lexTokenString) {
		// check the source
		switch (lexInputSource) {
			case LEXER_SRC_STRING:
			case LEXER_SRC_STRING_BUFFER:
				// common operations to both
				pos++;
				if (pos >= lexInputLength) { ch = EOF; return; }
				ch = lexInputStream[pos];

				// for immutable strings, the character must be copied to 
				// the lexBuffer
				if (lexInputSource == LEXER_SRC_STRING)
					lexBuffer[++lexBufferCount] = ch;
			
				return;


			// in the rest of the cases, the cursor is irrelevent --- just
			// accumulate the token in the lexBuffer
			case LEXER_SRC_CEDAR:
				if (!sock->get_bytes (&chr, 1)) { ch = EOF; return; }
				ch = chr;
				lexBuffer[++lexBufferCount] = chr;
				break;

			case LEXER_SRC_FILE:
				if ((ch = getc (file)) == EOF)	{ return; }
				lexBuffer[++lexBufferCount] = (char) ch;
				break;

			case LEXER_SRC_FILE_DESC:
				if ((read (fd, &chr, 1)) < 0) { ch = EOF; return; }
				ch = chr;
				lexBuffer[++lexBufferCount] = chr;
				break;

			default:
				EXCEPT ("ClassAd Lexer:  Should not get here");
		}
	} else {
		// we are not accumulating a token;  this means that we do not have to
		// perform any copying and count updating for input sources that require
		// token accumulation in the lexBuffer
		switch (lexInputSource) {
			case LEXER_SRC_STRING:
			case LEXER_SRC_STRING_BUFFER:
				pos++;
				if (pos >= lexInputLength) { ch = EOF; return; }
				ch = lexInputStream[pos];
				return;

			// in the rest of the cases, the cursor is irrelevent
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
				EXCEPT ("ClassAd Lexer:  Should not get here");
		}
	}
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
	
	// the last token was cut, so restore the buffer
	uncut ();

	// consume white space
	while (isspace (ch)) {
		wind ();
	}

	// check if this is the end of the input
	if (ch == 0 || ch == EOF) {
		tokenType = LEX_END_OF_INPUT;
		return tokenType;
	}

	// this is the start of the token
	mark ();

	// check the first character of the token
	if (isdigit (ch)) {
		tokenizeNumber ();	
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
//   1.  Integers:  [0-9]+
//   2.  Reals   :  [0-9]+\.[0-9]*
// At some point, we should probably include scientific notation.
int Lexer::
tokenizeNumber (void)
{
	NumberFactor f = NO_FACTOR;
	int uch;

	// the token is either integer or real value
	while (isdigit (ch)) {
		wind ();
	}

	// if the next character is a '.', the token is a real otherwise
	// treat the token as an integer and return
	if (ch != '.') {
		// whether or not there's a multiplicative factor, we don't want the
		// character to be in the tokenString --- just being nice to atoi()
		cut ();

		f = NO_FACTOR;
		uch = toupper(ch);

		// check if there is a multiplicative factor
		if (uch == 'K' || uch == 'M' || uch == 'G') {
			wind ();
			if (uch == 'K') f = K_FACTOR;
			if (uch == 'M') f = M_FACTOR;
			if (uch == 'G') f = G_FACTOR;
		}

		// token is an integer
		tokenType = LEX_INTEGER_VALUE;
		yylval.intValue.value  = atoi (lexTokenString);
		yylval.intValue.factor = f;
		uncut ();
		return tokenType;
	} else {
		// token is a real ... consume the '.'
		wind ();

		// read the fraction part
		while (isdigit (ch)) {
			wind ();
		}

		// whether or not there's a multiplicative factor, we don't want the
		// character to be in the tokenString --- just being nice to atof()
		cut ();

		uch = toupper(ch);
		f = NO_FACTOR;

		// check if there is a multiplicative factor
		if (uch == 'K' || uch == 'M' || uch == 'G') {
			wind ();
			if (uch == 'K') f = K_FACTOR;
			if (uch == 'M') f = M_FACTOR;
			if (uch == 'G') f = G_FACTOR;
		}

		tokenType = LEX_REAL_VALUE;
		yylval.realValue.value  = atof (lexTokenString);
		yylval.realValue.factor = f;

		uncut ();
		return tokenType;
	}

	// should not get here
	return LEX_TOKEN_ERROR;

}


// Tokenize alpha head: (character sequences beggining with an alphabet)
//   1.  Reserved character sequences:  true, false, error, undefined
//   2.  Identifier                  :  [a-zA-Z_][a-zA-Z0-9_]*
int Lexer::
tokenizeAlphaHead (void)
{
	// true or false or undefined or error or identifier
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
		yylval.strValue = strings.getCanonical(lexTokenString);
		
		return tokenType;
	}	

	// check if the string is one of the reserved words
	cut ();
	if (CLASSAD_RESERVED_STRCMP(lexTokenString, "true") == 0) {
		tokenType = LEX_INTEGER_VALUE;
		yylval.intValue.value = 1;
		yylval.intValue.factor = NO_FACTOR;
	} else if (CLASSAD_RESERVED_STRCMP(lexTokenString, "false") == 0) {
		tokenType = LEX_INTEGER_VALUE;
		yylval.intValue.value = 0;
		yylval.intValue.factor = NO_FACTOR;
	} else if (CLASSAD_RESERVED_STRCMP(lexTokenString, "undefined") == 0) {
		tokenType = LEX_UNDEFINED_VALUE;
	} else if (CLASSAD_RESERVED_STRCMP(lexTokenString, "error") == 0) {
		tokenType = LEX_ERROR_VALUE;
	} else if (CLASSAD_RESERVED_STRCMP(lexTokenString, "is") == 0 ) {
		tokenType = LEX_META_EQUAL;
	} else if (CLASSAD_RESERVED_STRCMP(lexTokenString, "isnt") == 0) {
		tokenType = LEX_META_NOT_EQUAL;
	} else {
		// token is a character only identifier
		tokenType = LEX_IDENTIFIER;
		yylval.strValue = strings.getCanonical (lexTokenString);
	}

	return tokenType;
}


// tokenizeStringLiteral:  Scans strings of the form " ... "
//   +  We should add support for escape sequences sometime
int Lexer::
tokenizeStringLiteral (void)
{
	// need to mark() after the quote
	inString = true;
	wind ();
	mark ();

	// consume the string literal
	while (ch != '\"' && ch > 0) {
		wind ();
	}

	// check the termination condition
	if (ch == '\"') {
		// correctly got the end of string delimiter
		cut ();
		tokenType = LEX_STRING_VALUE;
		yylval.strValue = strings.getCanonical(lexTokenString);

		return tokenType;
	}

	// loop quit due to ch == 0 or ch == EOF
	tokenType = LEX_TOKEN_ERROR;

	return tokenType;
}


// tokenizePunctOperator:  Tokenize puncutation and operators 
int Lexer::
tokenizePunctOperator (void)
{
	// save character; may need to lookahead
	int oldch = ch;

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
					tokenType = LEX_ARITH_RIGHT_SHIFT;
					wind ();
					if (ch == '>') {
						tokenType = LEX_LOGICAL_RIGHT_SHIFT;
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
		case LEX_LOGICAL_RIGHT_SHIFT:    return "LEX_LOGICAL_RIGHT_SHIFT";
		case LEX_ARITH_RIGHT_SHIFT:      return "LEX_ARITH_RIGHT_SHIFT";

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
