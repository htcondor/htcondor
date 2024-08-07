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


#ifndef __CLASSAD_LEXER_H__
#define __CLASSAD_LEXER_H__

#include "classad/common.h"
#include "classad/value.h"
#include "classad/lexerSource.h"

namespace classad {

const int EMPTY = -2;

// the lexical analyzer class
class Lexer
{
	public:
		enum TokenType
		{
			LEX_TOKEN_ERROR,
			LEX_END_OF_INPUT,
			LEX_TOKEN_TOO_LONG,
			LEX_INTEGER_VALUE,
			LEX_REAL_VALUE,
			LEX_BOOLEAN_VALUE,
			LEX_STRING_VALUE,
			LEX_UNDEFINED_VALUE,
			LEX_ERROR_VALUE,
			LEX_IDENTIFIER,
			LEX_SELECTION,
			LEX_MULTIPLY,
			LEX_DIVIDE,
			LEX_MODULUS,
			LEX_PLUS,
			LEX_MINUS,
			LEX_BITWISE_AND,
			LEX_BITWISE_OR,
			LEX_BITWISE_NOT,
			LEX_BITWISE_XOR,
			LEX_LEFT_SHIFT,
			LEX_RIGHT_SHIFT,
			LEX_URIGHT_SHIFT,
			LEX_LOGICAL_AND,
			LEX_LOGICAL_OR,
			LEX_LOGICAL_NOT,
			LEX_LESS_THAN,
			LEX_LESS_OR_EQUAL,
			LEX_GREATER_THAN,
			LEX_GREATER_OR_EQUAL,
			LEX_EQUAL,
			LEX_NOT_EQUAL,
			LEX_META_EQUAL,
			LEX_META_NOT_EQUAL,
			LEX_BOUND_TO,
			LEX_QMARK,
			LEX_ELVIS,
			LEX_COLON,
			LEX_COMMA,
			LEX_SEMICOLON,
			LEX_OPEN_BOX,
			LEX_CLOSE_BOX,
			LEX_OPEN_PAREN,
			LEX_CLOSE_PAREN,
			LEX_OPEN_BRACE,
			LEX_CLOSE_BRACE
		};

		struct TokenValue
		{
			std::string		strValue;
			long long		intValue{0};
			double 			realValue{0.0};
			TokenType 		type{LEX_TOKEN_ERROR};
			bool 			boolValue{false};
			bool			quotedExpr{false};
		};

		// ctor/dtor
		Lexer ();
		~Lexer ();

		// initialize methods
		bool Initialize(LexerSource *source);
		bool Reinitialize(void);
        
        bool WasInitialized(void) const;

		bool SetOldClassAdLex( bool do_old );
		bool SetJsonLex( bool do_json );

		// cleanup function --- purges strings from string space
		void FinishedParse();
		
		// the 'extract token' functions
		TokenType PeekTokenType() { return PeekToken().type; }
		TokenValue& PeekToken() { if (tokenConsumed) { LoadToken(); } return yylval; }
		void LoadToken();
		TokenValue& ConsumeToken();
		TokenType getLastTokenType() { return tokenType; } // return the type last token the lexer saw when it stopped.

		// internal buffer for token accumulation
		std::string lexBuffer;					    // the buffer itselfw

		// miscellaneous functions
		static const char *strLexToken (int);		// string rep'n of token

		// set debug flag 
		void SetDebug( bool d ) { debug = d; }

	private:
			// grant access to FunctionCall --- for tokenize{Abs,Rel}Time fns
		friend class FunctionCall;
		friend class ClassAdXMLParser;

        // The copy constructor and assignment operator are defined
        // to be private so we don't have to write them, or worry about
        // them being inappropriately used. The day we want them, we can 
        // write them. 
        Lexer(const Lexer &)            { return;       }
        Lexer &operator=(const Lexer &) { return *this; }

		// internal state of lexical analyzer
        bool        initialized;
		TokenType	tokenType;             		// the integer id of the token
		LexerSource *lexSource;
		int    		markedPos;              	// index of marked character
		char   		savedChar;          		// stores character when cut
		int    		ch;                     	// the current character
		bool		inString;					// lexing a string constant
		bool		accumulating;				// are we in a token?
		bool		jsonLex;
		bool		oldClassAdLex;
		int 		debug; 						// debug flag

		// cached last token
		TokenValue 	yylval;						// the token itself
		bool		tokenConsumed;				// has the token been consumed?

		// internal lexing functions
		// Wind:  This function is called when we're done with the current character
		//        and want to either dispose of it or add it to the current token.
		//        By default, we also read the next character from the input source,
		//        though this can be suppressed (when the caller knows we're at the
		//        end of a token.
		void wind (bool fetch = true) {
				if(ch == EOF) return;
				if (accumulating && ch != -2) {
					lexBuffer += ch;
				}
				if (fetch) {
					ch = lexSource->ReadCharacter();
				} else {
					ch = -2;
				}
			}


		void 		mark(void);					// mark()s beginning of a token
		void 		cut(void);					// delimits token
		void		fetch();					// fetch next character if ch is empty

		// to tokenize the various tokens
		int 		tokenizeNumber (void);		// integer or real
		int 		tokenizeAlphaHead (void);	// identifiers/reserved strings
		int 		tokenizePunctOperator(void);// punctuation and operators
		int         tokenizeString(char delim);//string constants
		int         tokenizeStringOld(char delim);//string constants
};

} // classad

#endif //__CLASSAD_LEXER_H__
