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
extern const char *opString[];

// public access to operation function
void operate (OpKind, Value &, Value &);
bool operateShortCircuit (OpKind, Value &, Value &);
void operate (OpKind, Value &, Value &, Value &);

#endif//__OPERATORS_H__
