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
// condor_exprtype.h
//
// Cai, Weiru
//
// condor literal types.
//
//******************************************************************************

#ifndef _EXPRTYPE_H_
#define _EXPRTYPE_H_

#if !defined(WANT_OLD_CLASSADS)

#include "compat_classad.h"
#include "compat_classad_list.h"
#include "compat_classad_util.h"
using namespace compat_classad;

#else


#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE 
#define FALSE 0
#endif

#define UNKNOWNVAR -1

typedef enum
{
  // Literals
  LX_VARIABLE,
  LX_INTEGER,
  LX_FLOAT,
  LX_STRING,
  LX_BOOL,
  LX_NULL,
  LX_UNDEFINED,
  LX_ERROR,

  // Operators
  LX_ASSIGN,
  LX_AGGADD,
  LX_AGGEQ,
  LX_AND,
  LX_OR,
  LX_LPAREN,
  LX_RPAREN,
  LX_MACRO,
  LX_META_EQ,
  LX_META_NEQ,
  LX_EQ,
  LX_NEQ,
  LX_LT,
  LX_LE,
  LX_GT,
  LX_GE,
  LX_ADD,
  LX_SUB,
  LX_MULT,
  LX_DIV,
  LX_EOF,

  LX_EXPR,

  LX_TIME,

  LX_FUNCTION,
  LX_SEMICOLON,
  LX_COMMA,

  NOT_KEYWORD
} LexemeType;

#endif /* !defined(WANT_OLD_CLASSADS) */

#endif /* _EXPRTYPE_H_ */
