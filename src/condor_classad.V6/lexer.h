#ifndef __LEXER_H__
#define __LEXER_H__

#include "condor_io.h"		// for cedar
#include "stringSpace.h"
#include "common.h"
#include "tokens.h"

struct IntValue {
    int             value;
    NumberFactor    factor;
};

struct RealValue {
    double          value;
    NumberFactor    factor;
};

union TokenValue
{
    IntValue    intValue;
    RealValue   realValue;
	bool		boolValue;
    int         strValue;
};


// the lexical analyzer class
class Lexer
{
	public:
		// ctor/dtor
		Lexer ();
		~Lexer ();

		// initialize methods
		bool initializeWithString(char *,int);		// string 
		bool initializeWithCedar (Sock &);			// CEDAR
		bool initializeWithFd 	(int);				// file descriptor
		bool initializeWithFile (FILE *);			// FILE structure

		// return the character string from the string space given the ID
		inline char *getCharString (int);

		// cleanup function --- purges strings from string space
		void finishedParse();
		
		// the 'extract token' functions
		TokenType peekToken (TokenValue * = NULL);
		TokenType consumeToken (TokenValue * = NULL);

		// internal buffer for string, CEDAR and file token accumulation
		enum 	{ LEX_BUFFER_SIZE = 4096 };		// starting size
		char   	lexBuffer[LEX_BUFFER_SIZE];		// the buffer itself

		// miscellaneous functions
		static char *strLexToken (int);			// string rep'n of token

	private:
		// types of input streams to the scanner
		enum LexerInputSource {
			LEXER_SRC_NONE, LEXER_SRC_STRING, LEXER_SRC_FILE,   
			LEXER_SRC_FILE_DESC, LEXER_SRC_CEDAR
		};

		// variables to describe and store the input source
		LexerInputSource lexInputSource;       	// input source type
		char    *lexInputStream;      			// input source
		int     lexInputLength;	   				// length of buffer

		// internal state of lexical analyzer
		TokenType	tokenType;             		// the integer id of the token
		int    	markedPos;              		// index of marked character
		char   	savedChar;          			// stores character when cut
		int    	ch;                     		// the current character
		int    	pos;                    		// the cursor
		int		lexBufferCount;					// current offset in lexBuffer
		bool	inString;						// lexing a string constant
		bool	accumulating;					// are we in a token?
		int 	debug; 							// debug flag

		StringSpace strings;					// identifiers/constants

		// cached last token
		TokenValue yylval;						// the token itself
		bool	tokenConsumed;					// has the token been consumed?

		// internal lexing functions
		void 	wind (void);					// consume character from source
		void 	mark (void);					// mark()s beginning of a token
		void 	cut (void);						// delimits token

		// to tokenize the various tokens
		int 	tokenizeNumber (void);			// integer or real
		int 	tokenizeAlphaHead (void);		// identifiers/reserved strings
		int 	tokenizeStringLiteral (void);	// string constants
		int 	tokenizePunctOperator (void);	// punctuation and operators

		// input sources
		Sock	*sock;							// the Cedar sock
		int		fd;								// the file descriptor
		FILE 	*file;							// the file structure
};


inline char *Lexer::
getCharString (int id)
{
	return (strings[id]);
}


#endif //__LEXER_H__
