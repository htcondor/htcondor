#ifndef __COMMON_H__
#define __COMMON_H__

enum MmMode
{
	MEM_DUP,
	MEM_ADOPT_PRESERVE,
	MEM_ADOPT_DISPOSE,
	MEM_DISPOSE,
};

enum CopyMode 
{ 
	EXPR_DEFAULT_COPY,
	EXPR_DEEP_COPY, 
	EXPR_REF_COUNT 
};

enum NumberFactor 
{ 
    NO_FACTOR   = 1,
    K_FACTOR    = 1024,             // kilo
    M_FACTOR    = 1024*1024,        // mega
    G_FACTOR    = 1024*1024*1024    // giga
};

// kinds of operators
enum OpKind 
{
    __NO_OP__,              // convenience

    __FIRST_OP__,

    __COMPARISON_START__    = __FIRST_OP__,
    LESS_THAN_OP            = __COMPARISON_START__,
    LESS_OR_EQUAL_OP,       // (comparison)
    NOT_EQUAL_OP,           // (comparison)
    EQUAL_OP,               // (comparison)
    META_EQUAL_OP,          // (comparison)
	IS_OP					= META_EQUAL_OP,
    META_NOT_EQUAL_OP,      // (comparison)
	ISNT_OP					= META_NOT_EQUAL_OP,
    GREATER_OR_EQUAL_OP,    // (comparison)
    GREATER_THAN_OP,        // (comparison)
    __COMPARISON_END__      = GREATER_THAN_OP,

    __ARITHMETIC_START__,     
    UNARY_PLUS_OP           = __ARITHMETIC_START__,
    UNARY_MINUS_OP,         // (arithmetic)
    ADDITION_OP,            // (arithmetic)
    SUBTRACTION_OP,         // (arithmetic)
    MULTIPLICATION_OP,      // (arithmetic)
    DIVISION_OP,            // (arithmetic)
    MODULUS_OP,             // (arithmetic)
    __ARITHMETIC_END__      = MODULUS_OP,
    
    __LOGIC_START__,
    LOGICAL_NOT_OP          = __LOGIC_START__,
    LOGICAL_OR_OP,          // (logical)
    LOGICAL_AND_OP,         // (logical)
    __LOGIC_END__           = LOGICAL_AND_OP,

    __BITWISE_START__,
    BITWISE_NOT_OP          = __BITWISE_START__,
    BITWISE_OR_OP,          // (bitwise)
    BITWISE_XOR_OP,         // (bitwise)
    BITWISE_AND_OP,         // (bitwise)
    LEFT_SHIFT_OP,          // (bitwise)
    RIGHT_SHIFT_OP, 		// (bitwise) -- signed shift
    URIGHT_SHIFT_OP,   		// (bitwise) -- unsigned shift
    __BITWISE_END__         = URIGHT_SHIFT_OP, 

    __MISC_START__,
    PARENTHESES_OP          = __MISC_START__,
    SUBSCRIPT_OP,           // (misc) -- subscripting elements from lists
    TERNARY_OP,             // (misc)
    __MISC_END__            = TERNARY_OP,

    __LAST_OP__             = __MISC_END__
};


#endif//__COMMON_H__
