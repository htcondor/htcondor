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

#ifndef __COMMON_H__
#define __COMMON_H__

#if defined( WANT_NAMESPACES )
#define BEGIN_NAMESPACE( x ) namespace x {
#define END_NAMESPACE }
#else
#define BEGIN_NAMESPACE( x )
#define END_NAMESPACE
#endif

BEGIN_NAMESPACE( classad )

enum MmMode
{
	MEM_NONE	= 0,
	MEM_DISPOSE = 1,
	MEM_DUP		= 2
};

enum {
	EVAL_FAIL,
	EVAL_OK,
	EVAL_UNDEF,
	PROP_UNDEF,
	EVAL_ERROR,
	PROP_ERROR
};

/// The kinds of nodes in expression trees
enum NodeKind
{
	/** Literal node (string, integer, real, boolean, undefined, error) */
																LITERAL_NODE,
	/** Attribute reference node (attr, .attr, expr.attr) */    ATTRREF_NODE,
	/** Expression operation node (unary, binary, ternary) */   OP_NODE,
	/** Function call node */                                   FN_CALL_NODE,
	/** ClassAd node */                                         CLASSAD_NODE,
	/** Expression list node */                                 EXPR_LIST_NODE
};

/// The various kinds of values
enum ValueType
{
	/** The error value */              ERROR_VALUE,
	/** The undefined value */          UNDEFINED_VALUE,
	/** A boolean value (false, true)*/ BOOLEAN_VALUE,
	/** An integer value */             INTEGER_VALUE,
	/** A real value */                 REAL_VALUE,
	/** A relative time value */        RELATIVE_TIME_VALUE,
	/** An absolute time value */       ABSOLUTE_TIME_VALUE,
	/** A string value */               STRING_VALUE,
	/** A classad value */              CLASSAD_VALUE,
	/** An expression list value */     LIST_VALUE
};

/// The multiplicative factors for a numeric literal.
enum NumberFactor 
{ 
    /** No factor specified */ NO_FACTOR   = 1,
    /** Kilo factor */ 	K_FACTOR = 1024,
    /** Mega factor */ 	M_FACTOR = 1024*1024,
    /** Giga factor */ 	G_FACTOR = 1024*1024*1024
};

/// List of supported operators 
// The order in this enumeration should be synchronized with the opString[]
// array in operators.C
enum OpKind 
{
    /** No op */ __NO_OP__,              // convenience

    __FIRST_OP__,

    __COMPARISON_START__   	= __FIRST_OP__,
	/** @name Strict comparison operators */
	//@{
    /** Less than operator 	*/ LESS_THAN_OP            = __COMPARISON_START__,
    /** Less or equal 		*/	LESS_OR_EQUAL_OP,       // (comparison)
    /** Not equal 			*/ 	NOT_EQUAL_OP,           // (comparison)
    /** Equal 				*/	EQUAL_OP,               // (comparison)
    /** Greater or equal 	*/ 	GREATER_OR_EQUAL_OP,    // (comparison)
    /** Greater than 		*/	GREATER_THAN_OP,        // (comparison)
	//@}

	/** @name Non-strict comparison operators */
	//@{
    /** Meta-equal (same as IS)*/ META_EQUAL_OP,        // (comparison)
	/** Is 					*/ 	IS_OP= META_EQUAL_OP,	// (comparison)
    /** Meta-not-equal (same as ISNT) */META_NOT_EQUAL_OP,//(comparison)
	/** Isnt 				*/ 	ISNT_OP=META_NOT_EQUAL_OP,//(comparison)
	//@}
    __COMPARISON_END__     	= ISNT_OP,

    __ARITHMETIC_START__,     
	/** @name Arithmetic operators */
	//@{
    /** Unary plus 			*/	UNARY_PLUS_OP           = __ARITHMETIC_START__,
    /** Unary minus 		*/	UNARY_MINUS_OP,         // (arithmetic)
    /** Addition 			*/	ADDITION_OP,            // (arithmetic)
    /** Subtraction 		*/	SUBTRACTION_OP,         // (arithmetic)
    /** Multiplication 		*/	MULTIPLICATION_OP,      // (arithmetic)
    /** Division 			*/	DIVISION_OP,            // (arithmetic)
    /** Modulus 			*/	MODULUS_OP,             // (arithmetic)
	//@}
    __ARITHMETIC_END__      = MODULUS_OP,
    
    __LOGIC_START__,
	/** @name Logical operators */
	//@{
    /** Logical not 		*/	LOGICAL_NOT_OP          = __LOGIC_START__,
    /** Logical or 			*/	LOGICAL_OR_OP,          // (logical)
    /** Logical and 		*/	LOGICAL_AND_OP,         // (logical)
	//@}
    __LOGIC_END__           = LOGICAL_AND_OP,

    __BITWISE_START__,
	/** @name Bitwise operators */
	//@{
    /** Bitwise not 		*/	BITWISE_NOT_OP          = __BITWISE_START__,
    /** Bitwise or 			*/	BITWISE_OR_OP,          // (bitwise)
    /** Bitwise xor 		*/	BITWISE_XOR_OP,         // (bitwise)
    /** Bitwise and 		*/	BITWISE_AND_OP,         // (bitwise)
    /** Left shift 			*/	LEFT_SHIFT_OP,          // (bitwise)
    /** Right shift 		*/	RIGHT_SHIFT_OP, 		// (bitwise)
    /** Unsigned right shift */	URIGHT_SHIFT_OP,   		// (bitwise)
	//@}
    __BITWISE_END__         = URIGHT_SHIFT_OP, 

    __MISC_START__,
	/** @name Miscellaneous operators */
	//@{
    /** Parentheses 		*/	PARENTHESES_OP          = __MISC_START__,
    /** Subscript 			*/	SUBSCRIPT_OP,           // (misc)
    /** Conditional op 		*/	TERNARY_OP,             // (misc)
	//@}
    __MISC_END__            = TERNARY_OP,

    __LAST_OP__             = __MISC_END__
};


const char ATTR_COLLECTION_HINTS[] 	= "CollectionHints";
const char ATTR_CONTEXT			[] 	= "Context";
const char ATTR_DEEP_MODS		[] 	= "DeepMods";
const char ATTR_DELETE_AD		[] 	= "DeleteAd";
const char ATTR_DELETES			[] 	= "Deletes";
const char ATTR_KEY				[]	= "Key";
const char ATTR_NEW_AD			[]	= "NewAd";
const char ATTR_PROJECT_THROUGH	[]	= "ProjectThrough";
const char ATTR_RANK			[]	= "Rank";
const char ATTR_RANK_HINTS		[] 	= "RankHints";
const char ATTR_REPLACE			[] 	= "Replace";
const char ATTR_REQUIREMENTS	[]	= "Requirements";
const char ATTR_UPDATES			[] 	= "Updates";
const char ATTR_WANT_LIST		[]	= "WantList";
const char ATTR_WANT_PRELUDE	[]	= "WantPrelude";
const char ATTR_WANT_RESULTS	[]	= "WantResults";
const char ATTR_WANT_POSTLUDE	[]	= "WantPostlude";

END_NAMESPACE // classad

char* strnewp( const char* );

#endif//__COMMON_H__
