/******************************************************************************
  program: parser.h
******************************************************************************/

#ifndef _PARSER_H
#define _PARSER_H

#include "scanner.h"

#define TABLE_SIZE 20

static token next_t;
static int already_read;

extern "C" char *lookup_macro(char *, BUCKET *[], int);

Expression *parse(char *, Context *);
Expression *parse_expr(char **);
Expression *parse_x1(Expression *, char **);
Expression *parse_simple_expr(char **);
Expression *parse_x2(Expression *, char **);
Expression *parse_add_op(char **);
Expression *parse_x3(Expression *, char **);
Expression *parse_mult_op(char **);
Expression *parse_x4(Expression *, char **);
Expression *parse_factor(char **);

#endif /* _PARSER_H */
