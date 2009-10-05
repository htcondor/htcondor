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

// parse an assignment expression: name = expression
int Parse(const char*, ExprTree*&, int *pos = NULL);

// parse an rval (i.e. anything that could appear on the rhs of an assignment)
// if error occurs, ExprTree is NULL and return value is
// position at which error occurred
int ParseClassAdRvalExpr(const char*, ExprTree*&, int *pos = NULL);

#endif
