/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#if !defined(_EXPR_H)
#define _EXPR_H

	/* Element types */
#define LT		1
#define LE		2
#define GT		3
#define GE		4
#define EQ		5
#define NE		6
#define AND		7
#define OR		8
#define NOT		9
#define PLUS	10
#define MINUS	11
#define MUL		12
#define DIV		13
#define GETS	14
#define LPAREN	15
#define RPAREN	16
#define NAME	17
#define STRING	18
#define FLOAT	19
#define INT		20
#define BOOL	21
#define ERROR	22
#define MOD		23

#define ENDMARKER	-1

#define MAX_EXPR_LIST		512		/* Max elements in an expression */
#define MAX_STRING			1024	/* Max lenght of a name or string */

#define f_val val.float_val
#define i_val val.integer_val
#define s_val val.string_val
#define b_val val.integer_val

#ifndef CONDOR_TYPES_INCLUDED
typedef struct {
	int		type;
	union {
		int		integer_val;	/* Integer value */
		float	float_val;		/* Floating point value */
		char	*string_val;	/* String value */
	} val;
} ELEM;
#endif

#ifndef CONDOR_TYPES_INCLUDED
typedef struct {
	int		len;
	int		max_len;
	ELEM	**data;
} EXPR;
#endif
#define EXPR_INCR 25

typedef struct {
	int		top;
	ELEM	*data[MAX_EXPR_LIST];
} STACK;

#ifndef CONDOR_TYPES_INCLUDED
typedef struct {
	int		len;
	int		max_len;
	EXPR	**data;
} CONTEXT;
#endif

#define IN_STACK	1
#define IN_COMING	2

#endif	/* _EXPR_H */
