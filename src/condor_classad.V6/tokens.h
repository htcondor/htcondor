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

#ifndef __TOKENS_H__
#define __TOKENS_H__

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
	LEX_COLON,
	LEX_COMMA,
	LEX_SEMICOLON,
	LEX_OPEN_BOX,
	LEX_CLOSE_BOX,
	LEX_OPEN_PAREN,
	LEX_CLOSE_PAREN,
	LEX_OPEN_BRACE,
	LEX_CLOSE_BRACE,
	LEX_BACKSLASH,
	LEX_ABSOLUTE_TIME_VALUE,
	LEX_RELATIVE_TIME_VALUE
};

#endif//__TOKENS_H__
