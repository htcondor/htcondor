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
#include "classad_io.h"

namespace classad {

const int MAX_TOKEN_SIZE = 4096;

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
	int				usecs;
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
			usecs = 0;;
			strValue = NULL;
		}

		~TokenValue( ) {
			if( strValue ) delete [] strValue;
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

		void SetStringValue( char *str ) {
			if( strValue ) delete [] strValue;
			strValue = strnewp( str );
		}

		void SetAbsTimeValue( int asecs ) {
			secs = asecs;
		}

		void SetRelTimeValue( int rsecs, int rusecs=0 ) {
			secs = rsecs;
			usecs = rusecs;
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

		void GetStringValue( const char *&str, bool handOver=false ) {
			str = strValue;	
			if( handOver ) strValue = NULL;
		}	

		void GetAbsTimeValue( int& asecs ) {
			asecs = secs;
		}

		void GetRelTimeValue( int& rsecs, int& rusecs ) {
			rsecs = secs;
			rusecs= usecs;
		}

		void CopyFrom( TokenValue &tv ) {
			tt = tv.tt;
			factor = tv.factor;
			intValue = tv.intValue;
			realValue = tv.realValue;
			boolValue = tv.boolValue;
			secs = tv.secs;
			usecs = tv.usecs;
			if( strValue ) {
				delete [] strValue;
			}
			strValue = tv.strValue ? strnewp( tv.strValue ) : NULL;
		}
			
	private:
		TokenType tt;
		NumberFactor factor;
    	int intValue;
    	double realValue;
		bool boolValue;
    	const char *strValue;
		int	secs;
		int usecs;
};


// the lexical analyzer class
class Lexer
{
	public:
		// ctor/dtor
		Lexer ();
		~Lexer ();

		// initialize methods
		bool InitializeWithSource( ByteSource *src );

		// cleanup function --- purges strings from string space
		void FinishedParse();
		
		// the 'extract token' functions
		TokenType PeekToken( TokenValue* = 0 );
		TokenType ConsumeToken( TokenValue* = 0 );

		// internal buffer for token accumulation
		ExtArray<char> lexBuffer;					// the buffer itself

		// miscellaneous functions
		static char *strLexToken (int);				// string rep'n of token

		void SetDebug( bool d ) { debug = d; }

	private:
			// grant access to FunctionCall --- for tokenize{Abs,Rel}Time fns
		friend class FunctionCall;

		// internal state of lexical analyzer
		TokenType	tokenType;             		// the integer id of the token
		int    	markedPos;              		// index of marked character
		char   	savedChar;          			// stores character when cut
		int    	ch;                     		// the current character
		int		lexBufferCount;					// current offset in lexBuffer
		bool	inString;						// lexing a string constant
		bool	accumulating;					// are we in a token?
		int 	debug; 							// debug flag

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
		int		tokenizeTime( void );			// time (absolute and relative)
		int 	tokenizePunctOperator (void);	// punctuation and operators

		static bool	tokenizeRelativeTime(char*,int&,int&);// relative time
		static bool	tokenizeAbsoluteTime(char*,int&);// absolute time

		// input sources
		ByteSource	*src;
};

} // namespace classad

#endif //__LEXER_H__
