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

  NOT_KEYWORD
} LexemeType;

#endif
