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
#ifndef __OPERATORS_H__
#define __OPERATORS_H__

#include "value.h"

// kinds of operators
enum OpKind 
{
	__NO_OP__,				// convenience

	__FIRST_OP__,

    __COMPARISON_START__	= __FIRST_OP__,
    LESS_THAN_OP 			= __COMPARISON_START__,
    LESS_OR_EQUAL_OP,		// (comparison)
    NOT_EQUAL_OP,			// (comparison)
    EQUAL_OP,				// (comparison)
	META_EQUAL_OP,			// (comparison)
	META_NOT_EQUAL_OP,		// (comparison)
    GREATER_OR_EQUAL_OP,	// (comparison)
    GREATER_THAN_OP,		// (comparison)
	__COMPARISON_END__ 		= GREATER_THAN_OP,

    __ARITHMETIC_START__,     
	UNARY_PLUS_OP 			= __ARITHMETIC_START__,
	UNARY_MINUS_OP,			// (arithmetic)
    ADDITION_OP,			// (arithmetic)
    SUBTRACTION_OP,			// (arithmetic)
    MULTIPLICATION_OP,		// (arithmetic)
    DIVISION_OP,			// (arithmetic)
    MODULUS_OP,				// (arithmetic)
	__ARITHMETIC_END__ 		= MODULUS_OP,
	
	__LOGIC_START__,
	LOGICAL_NOT_OP 			= __LOGIC_START__,
    LOGICAL_OR_OP,			// (logical)
    LOGICAL_AND_OP,			// (logical)
    __LOGIC_END__ 			= LOGICAL_AND_OP,

	__BITWISE_START__,
	BITWISE_NOT_OP 			= __BITWISE_START__,
	BITWISE_OR_OP,			// (bitwise)
	BITWISE_XOR_OP,			// (bitwise)
	BITWISE_AND_OP,			// (bitwise)
	LEFT_SHIFT_OP,			// (bitwise)
	LOGICAL_RIGHT_SHIFT_OP,	// (bitwise) -- unsigned shift
	ARITH_RIGHT_SHIFT_OP,	// (bitwise) -- signed shift
	__BITWISE_END__ 		= ARITH_RIGHT_SHIFT_OP,	

	__MISC_START__,
	PARENTHESES_OP 			= __MISC_START__,
	TERNARY_OP,				// (misc)
	__MISC_END__ 			= TERNARY_OP,

	__LAST_OP__				= __MISC_END__
};


// table of string representations of operators; indexed by OpKind
extern char *opString[];

// public access to operation function
void operate (OpKind, Value &, Value &);
void operate (OpKind, Value &, Value &, Value &);

#endif//__OPERATORS_H__
