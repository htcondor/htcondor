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

  NOT_KEYWORD
} LexemeType;

#endif
