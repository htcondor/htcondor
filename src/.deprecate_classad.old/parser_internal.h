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

#ifndef _PARSER_INTERNAL_H
#define _PARSER_INTERVAL_H

#include "condor_ast.h"

// This is an internal-only parsing function.

// Parse an assignment expression, i.e. variable = expression
// On success, 0 is returned and tree is set to the resulting ExprTree.
// On failure, non-0 is returned, tree is set to NULL, and if pos is
// non-NULL, the location it points to is set to the position at which
// the error occurred.
int Parse(const char*s, ExprTree*&tree, int *pos = NULL);

#endif
