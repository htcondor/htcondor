//******************************************************************************
// parser.h
//
// Parse() converts a string expression into an expression tree. If an error
// is encountered, the partially parsed tree is preserved and the number of
// characters parsed is returned; Otherwise 0 is returned.
//
//******************************************************************************

#ifndef _PARSER_H
#define _PARSER_H

#include "condor_ast.h"

int Parse(const char*, ExprTree*&);
int Parse(char *, ExprTree *&);

#endif
