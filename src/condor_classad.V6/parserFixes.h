#ifndef __PARSERFIXES_H__
#define __PARSERFIXES_H__

// This file contains the definitions of elements of the YYSTYPE union
// that are themselves structures.  
//  +	The ListValue structure is used for expression lists, in which case the 
//		integer id of the tree must be passed for memory management, but the 
//		actual pointer to the (derived) ExprList class must be passed as 
//		well because we cannot cast backwards from the (base) (ExprTree *).
//  +	The ArgValue structure is used for argument lists, in which case the 
//		integer id of the tree must be passed for memory management, but the 
//		actual pointer to the (derived) ArgumentList class must be passed as 
//		well because we cannot cast backwards from the (base) (ExprTree *).
//  +	The IntValue structure is used to pass integer values along with
//		multiplicative factors such as k, m and g (for kilo, mega and giga).
//  +	The RealValue structure is used to pass real values along with
//		multiplicative factors such as k, m and g (for kilo, mega and giga).
// The definition of __GRAMMAR_H__ prevents exprTree.h from bringing in the
// definition of YYSTYPE (which has not yet been defined in the bison parser)

#define __GRAMMAR_H__
#include "exprTree.h"
#include "stringSpace.h"

struct ListValue
{
	ExprList		*exprList;
	int				treeId;
};

struct ArgValue
{
    ArgumentList 	*argList;
    int          	treeId;
};

struct IntValue
{
	int				value;
	NumberFactor	factor;
};

struct RealValue
{
	double			value;
	NumberFactor	factor;
};


#endif//__PARSERFIXES_H__
