#ifndef __LEXER_H__
#define __LEXER_H__

#include "condor_io.h"		// for cedar
#include "grammar.h"		// for YYSTYPE
#include "stringSpace.h"

// the lexical analyzer class
class Lexer
{
	public:
		// ctor/dtor
		Lexer ();
		~Lexer ();

		// initialize methods
		bool initializeWithString		(const char *,int);	// immutable 
		bool initializeWithStringBuffer	(char *,int);		// mutable 
		bool initializeWithCedar 		(Sock &);			// CEDAR
		bool initializeWithFd 			(int);				// file descriptor
		bool initializeWithFile 		(FILE *);			// FILE structure

		// set the anticipated parse target
		void setTarget (int);

		// return the character string from the string space given the ID
		inline char *getCharString (int);

		// cleanup function --- purges strings from string space
		void finishedParse();
		
		// the 'extract token' function
		int	yylex (YYSTYPE *);

		// internal buffer for string, CEDAR and file token accumulation
		enum 	{ LEX_BUFFER_SIZE = 4096 };		// starting size
		char   	lexBuffer[LEX_BUFFER_SIZE];		// the buffer itself

		// miscellaneous auxillary functions
		int 	yyerror (char *);				// error printing
		static char *strLexToken (int);			// string rep'n of token

	private:
		// types of input streams to the scanner
		enum LexerInputSource 
		{
			LEXER_SRC_NONE, 
			LEXER_SRC_STRING, LEXER_SRC_STRING_BUFFER,
			LEXER_SRC_FILE,   LEXER_SRC_FILE_DESC,
			LEXER_SRC_CEDAR
		};

		// variables to describe and store the input source
		LexerInputSource lexInputSource;       	// input source type
		char    *lexInputStream;      			// input source
		int     lexInputLength;	   				// length of buffer

		// internal state of lexical analyzer
		int    	tokenType;              		// the integer id of the token
		char   	*lexTokenString;        		// pointer to marked character
		int    	markedPos;              		// index of marked character
		char   	savedChar;          			// stores character when cut
		int    	ch;                     		// the current character
		int    	pos;                    		// the cursor
		int		lexBufferCount;					// current offset in lexBuffer
		bool	inString;						// lexing a string constant

		StringSpace strings;					// identifiers/constants

		// internal lexing functions
		void 	wind (void);					// consume character from source
		void 	mark (void);					// mark()s beginning of a token
		void 	cut (void);						// delimits token
		void 	uncut (void);					// repatches input stream 

		// to tokenize the various tokens
		int 	tokenizeNumber (void);			// integer or real
		int 	tokenizeAlphaHead (void);		// identifiers/reserved strings
		int 	tokenizeStringLiteral (void);	// string constants
		int 	tokenizePunctOperator (void);	// punctuation and operators

		// miscellaneous
		Sock	*sock;							// the Cedar sock
		int		fd;								// the file descriptor
		FILE 	*file;							// the file structure
		int 	debug; 							// debug flag
		int 	parseTarget;					// current parse target

		YYSTYPE *yylval;
};


inline char *Lexer::
getCharString (int id)
{
	return (strings[id]);
}


#endif //__LEXER_H__
