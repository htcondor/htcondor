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

#ifndef __LEXER_H__
#define __LEXER_H__

#include "stringSpace.h"
#include "extArray.h"
#include "common.h"
#include "tokens.h"

BEGIN_NAMESPACE( classad )

struct IntValue {
    int             value;
    NumberFactor    factor;
};

struct RealValue {
    double          value;
    NumberFactor    factor;
};

struct AbsTimeValue {
	int				secs;
};

struct RelTimeValue {
	int				secs;
};

class TokenValue
{
	public:
		TokenValue( ) {
			tt = LEX_TOKEN_ERROR;
			factor = NO_FACTOR;
    		intValue = 0;
    		realValue = 0.0;
			boolValue = false;
			secs = 0;
		}

		~TokenValue( ) {
		}

		void SetTokenType( TokenType t ) {
			tt = t;
		}

		void SetIntValue( int i, NumberFactor f) {
			intValue = i;
			factor = f;
		}

		void SetRealValue( double r, NumberFactor f ) {
			realValue = r;
			factor = f;
		}

		void SetBoolValue( bool b ) {
			boolValue = b;
		}

		void SetStringValue( const string &str) {
			strValue = str;
		}

		void SetAbsTimeValue( time_t asecs ) {
			secs = asecs;
		}

		void SetRelTimeValue( time_t rsecs ) {
			secs = rsecs;
		}

		TokenType GetTokenType( ) {
			return tt;
		}

		void GetIntValue( int& i, NumberFactor& f) {
			i = intValue;
			f = factor;
		}

		void GetRealValue( double& r, NumberFactor& f ) {
			r = realValue;
			f = factor;
		}

		void GetBoolValue( bool& b ) {
			b = boolValue;
		}

		void GetStringValue( string &str ) {
			str = strValue;	
		}	

		void GetAbsTimeValue( time_t& asecs ) {
			asecs = secs;
		}

		void GetRelTimeValue( time_t& rsecs ) {
			rsecs = secs;
		}

		void CopyFrom( TokenValue &tv ) {
			tt = tv.tt;
			factor = tv.factor;
			intValue = tv.intValue;
			realValue = tv.realValue;
			boolValue = tv.boolValue;
			secs = tv.secs;
			strValue = tv.strValue;
		}
			
	private:
		TokenType 		tt;
		NumberFactor 	factor;
    	int 			intValue;
    	double 			realValue;
		bool 			boolValue;
    	string 			strValue;
		time_t			secs;
};


// the lexical analyzer class
class Lexer
{
	public:
		// ctor/dtor
		Lexer ();
		~Lexer ();

		// initialize methods
		bool Initialize( const string &buf );
		bool Reinitialize( );

		// cleanup function --- purges strings from string space
		void FinishedParse();
		
		// the 'extract token' functions
		TokenType PeekToken( TokenValue* = 0 );
		TokenType ConsumeToken( TokenValue* = 0 );

		// internal buffer for token accumulation
		ExtArray<char> lexBuffer;					// the buffer itself

		// miscellaneous functions
		static char *strLexToken (int);				// string rep'n of token

		// set debug flag 
		void SetDebug( bool d ) { debug = d; }

	private:
			// grant access to FunctionCall --- for tokenize{Abs,Rel}Time fns
		friend class FunctionCall;

		// internal state of lexical analyzer
		TokenType	tokenType;             		// the integer id of the token
		const char  *parseBuffer;				// the buffer being parsed
		int			offset;						// offset in parse buffer
		int    		markedPos;              	// index of marked character
		char   		savedChar;          		// stores character when cut
		int    		ch;                     	// the current character
		int			lexBufferCount;				// current offset in lexBuffer
		bool		inString;					// lexing a string constant
		bool		accumulating;				// are we in a token?
		int 		debug; 						// debug flag

		// cached last token
		TokenValue 	yylval;						// the token itself
		bool		tokenConsumed;				// has the token been consumed?

		// internal lexing functions
		void 		wind(void);					// consume character from source
		void 		mark(void);					// mark()s beginning of a token
		void 		cut(void);					// delimits token

		// to tokenize the various tokens
		int 		tokenizeNumber (void);		// integer or real
		int 		tokenizeAlphaHead (void);	// identifiers/reserved strings
		int 		tokenizeStringLiteral(void);// string constants
		int			tokenizeTime( void );		// time (absolute and relative)
		int 		tokenizePunctOperator(void);// punctuation and operators

		static bool	tokenizeRelativeTime(char*,time_t&);// relative time
		static bool	tokenizeAbsoluteTime(char*,time_t&);// absolute time
};

END_NAMESPACE // classad

#endif //__LEXER_H__
