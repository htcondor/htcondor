/* type.h
 *  Interface for datatypes.
 */

#ifndef _TYPE_H
#define _TYPE_H


typedef enum
{
  /* Literals */
  LX_VARIABLE, LX_INTEGER, LX_FLOAT, LX_STRING, LX_BOOL,

  /* Operators */
  LX_AND, LX_OR,
  LX_LPAREN, LX_RPAREN,
  LX_MACRO, LX_EQ, LX_NEQ, LX_LT, LX_LE, LX_GT, LX_GE,
  LX_ADD, LX_SUB, LX_MULT, LX_DIV,
  LX_EOF,

  NOT_KEYWORD
} lexeme_type;

#endif /* _TYPE_H */
