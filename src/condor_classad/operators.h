/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
bool operateShortCircuit (OpKind, Value &, Value &);
void operate (OpKind, Value &, Value &, Value &);

#endif//__OPERATORS_H__
