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

#ifndef __SOURCE_H__
#define __SOURCE_H__

#include "lexer.h"

BEGIN_NAMESPACE( classad )

class ClassAd;
class ExprTree;
class ExprList;
class FunctionCall;

/**
	Defines an abstraction for the input source from which expressions
	can be parsed.  The source may be pointed to a string, a file descriptor 
	or a FILE *.  Additionally, the ByteSource class may be extended for
	custom situations.
*/
class Source
{
	public:
		/// Constructor
		Source();

		/// Destructor
		~Source();

		// Set the stream source for the parse
		/** Points the source at a string.
			@param str String containing the expression
			@param len The length of the string.  If the parameter is not
				supplied, the source will be read until '\0' or EOF is
				encountered.
			@return false if the operation failed, true otherwise
		*/
		bool SetSource( const char *str, int len=-1 );

		/** Points the source at a FILE *.
			@param fp Pointer to the FILE structure from which the expression
				can be read
			@param len The length of the string.  If the parameter is not
				supplied, the source will be read until '\0' or EOF is
				encountered.
			@return false if the operation failed, true otherwise
		*/
		bool SetSource( FILE *fp, int len=-1 );

		/** Points the source at a file descriptor.
			@param fd The file descriptor from which the expression can be read
			@param len The length of the string.  If the parameter is not
				supplied, the source will be read until '\0' or EOF is
				encountered.
			@return false if the operation failed, true otherwise
		*/
		bool SetSource( int fd, int len=-1 );

		/** Points the source at a ByteSource object.
			@param b Pointer to a ByteSource object.
			@return false if the operation failed, true otherwise
			@see ByteSource
		*/
		bool SetSource( ByteSource* b );

		bool Reinitialize( ) { return( lexer.Reinitialize( ) ); }

		ByteSource *GetSource( ) { return( src ); }

		// parser entry points
		/** Parses a ClassAd from this Source object.
			@param ad Reference to ClassAd which will be populated with
				the named expressions coming in from the Source.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ClassAd was parsed successfully and no
				other tokens follow the ClassAd.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseClassAd( ClassAd &ad, bool full=false );

		/** Parses a ClassAd from this Source object.
			@param ad_ptr Pointer to ClassAd which will be populated with
				the named expressions coming in from the Source.  If this
				pointer is NULL a ClassAd will be created.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ClassAd was parsed successfully and no
				other tokens follow the ClassAd.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseClassAd( ClassAd*& ad_ptr, bool full=false );

		/** Parses an expression from this Source object.
			@param expr Reference to a ExprTree pointer, which will be pointed
				to the parsed expression.  The previous value of the pointer
				will be destroyed.
			@param full If this parameter is true, the parse is considered to
				succeed only if the expression was parsed successfully and no
				other tokens follow the expression.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseExpression(ExprTree*& expr, bool full=false);

		/** Parses an expression list from this Source object.
			@param exprList Reference to an expression list which will be 
				populated with the expressions coming in from the source.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ExprList was parsed successfully and no
				other tokens follow the ExprList.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseExprList( ExprList &exprList, bool full=false );

		/** Parses an expression list from this Source object.
			@param exprList_ptr Pointer to an expression list which will be 
				populated with the expressions coming in from the source.  If
				this pointer is NULL an ExprList object will be created.
			@param full If this parameter is true, the parse is considered to
				succeed only if the ExprList was parsed successfully and no
				other tokens follow the ExprList.
			@return true if the parse succeeded, false otherwise.
		*/
		bool ParseExprList( ExprList*& exprList_ptr, bool full=false );

		/** Sets the sentinel character for the source, which is treated
		 		like an EOF for the stream.  This feature is useful if, for
				example, the expression is expected to be terminated by an
				end of line character.  (Semantics of the regular EOF and '\0'
				characters is unchanged.)
			@param ch The sentinel character
		*/
		void SetSentinelChar( int ch );

		void SetDebug( bool d ) { lexer.SetDebug( d ); }
	private:
		// lexical analyser for parser
		Lexer	lexer;

		// mutually recursive parsing functions
		bool parseLogicalORExpression( ExprTree*& );
		bool parseLogicalANDExpression( ExprTree*& );
		bool parseInclusiveORExpression( ExprTree*& );
		bool parseExclusiveORExpression( ExprTree*& );
		bool parseANDExpression( ExprTree*& );
		bool parseEqualityExpression( ExprTree*& );
		bool parseRelationalExpression( ExprTree*& );
		bool parseShiftExpression( ExprTree*& );
		bool parseAdditiveExpression( ExprTree*& );
		bool parseMultiplicativeExpression( ExprTree*& );
		bool parseUnaryExpression( ExprTree*& );
		bool parsePostfixExpression( ExprTree*& );
		bool parsePrimaryExpression( ExprTree*& );
		bool parseArgumentList( FunctionCall* );

		ByteSource *src;
};

END_NAMESPACE // classad

#endif//__SOURCE_H__
