#ifndef __COMMON_H__
#define __COMMON_H__

enum MmMode
{
	MEM_DUP,
	MEM_ADOPT_PRESERVE,
	MEM_ADOPT_DISPOSE,
	MEM_DISPOSE,
};

enum {
	EVAL_FAIL,
	EVAL_OK,
	EVAL_UNDEF,
	EVAL_ERROR
};

/// The modes in which an expression can be copied.
enum CopyMode 
{ 
	/** Default copy mode (EXPR_DEEP_COPY or EXPR_REF_COUNT)*/EXPR_DEFAULT_COPY,
	/** Make deep copies */ EXPR_DEEP_COPY, 
	/** Use reference counts */ EXPR_REF_COUNT 
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


#endif//__COMMON_H__
