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


#include <stdio.h>
#include <ctype.h>
#include <varargs.h>
#include <sys/types.h>
#include "except.h"
#include "debug.h"
#include "expr.h"
#include "clib.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#define SCAN_ERROR  \
	_LineNo = __LINE__; \
	_FileName = __FILE__; \
	scan_error
	

#define EVALUATION_ERROR  \
	_LineNo = __LINE__; \
	_FileName = __FILE__; \
	evaluation_error
	

#if defined(ALPHA)
#undef ALPHA
#endif
#define ALPHA(ch) (isalpha(ch)||ch=='_')

#define WHITE(ch) (isspace(ch))
#define QUOTE(ch) (ch=='"')
#define DIGIT(ch) (isdigit(ch))
#define PUNCT(ch) (ch=='<'||ch=='='||ch=='>'||ch=='('||ch==')'||ch=='|'||ch=='&'||ch=='!'||ch=='+'||ch=='-'||ch=='*'||ch=='/')
#define MATCH 0
#define MAX_RECURSION	50

extern FILE	*DebugFP;

int		Terse;
int		Silent;
int		_LineNo;
char	*_FileName;
int		HadError;
int		Depth;

char	*In, *Line;

char	*strdup(), *op_name();
ELEM	*get_number(), *get_int(), *get_float(), *get_string(), *get_name(),
		*get_punct(), *pop(), *elem_dup(), *integer_compare(),
		*float_compare(), *string_compare(), *eval(), *integer_arithmetic(),
		*float_arithmetic(), *unstack_elem(), *create_elem();
EXPR	*create_expr(), *scan(), *search_expr();
static do_op( ELEM *elem, STACK *stack );

EXPR *
scan( line )
char	*line;
{
	ELEM	*elem, *tmp, *get_elem();
	STACK	operand_stack, *op_stack = &operand_stack;
	EXPR	*expr;

	HadError = 0;

	expr = create_expr();
	init_stack( op_stack );
	In = Line = line;

	for(;;) {

		if( HadError ) {
			return NULL;
		}

		elem = get_elem();

		if( HadError ) {
			free_elem( elem );
			return NULL;
		}

		if( elem->type == ERROR ) {
			return NULL;
		}

		switch( elem->type ) {
			case ENDMARKER:
				for( tmp = pop(op_stack); tmp; tmp = pop(op_stack) ) {
					add_elem( tmp, expr );
				}
				add_elem( elem, expr );
				return expr;

			case FLOAT:
			case INT:
			case STRING:
			case BOOL:
			case NAME:
				add_elem( elem, expr );
				break;

			case RPAREN:
				free_elem( elem );
				for(;;) {
					tmp = pop( op_stack );
					if( tmp == NULL ) { 
						break;
					}
					if( tmp->type == LPAREN ) {
						free_elem( tmp );
						break;
					}
					add_elem( tmp, expr );
				}
				break;
			default:
				for(tmp=pop(op_stack); tmp; tmp = pop(op_stack) ) {
					if( expr_prio(tmp,IN_STACK) >= expr_prio(elem,IN_COMING) ) {
						add_elem( tmp, expr );
					} else {
						break;
					}
				}
				if( tmp ) {
					push( tmp, op_stack );
				}
				push( elem, op_stack );
				break;
		}
	}
}
		

/*
** Create and initialize an empty expression, and return a pointer to it.
*/
EXPR *
create_expr()
{
	EXPR	*answer;

	answer = (EXPR *)MALLOC( sizeof(EXPR) );
	answer->len = 0;
	answer->max_len = EXPR_INCR;
	answer->data = (ELEM **)MALLOC(
					(unsigned)(answer->max_len * sizeof(ELEM *)) );
	return answer;
}

/*
** Add an element to an expression.
*/
add_elem( elem, expr )
ELEM	*elem;
EXPR	*expr;
{
	if( expr->len == expr->max_len ) {
		expr->max_len += EXPR_INCR;
		expr->data =
			(ELEM **)REALLOC( (char *)expr->data,
			(unsigned)(expr->max_len * sizeof(ELEM *)) );
	}
	expr->data[expr->len++] = elem;
}

/*
** Free an expression including all the elements in it.
*/
free_expr( expr )
EXPR	*expr;
{
	int		i;

	for( i=0; i<expr->len; i++ ) {
		free_elem( expr->data[i] );
	}
	FREE( (char *)expr->data );
	FREE( (char *)expr );
}

init_stack( stack )
STACK	*stack;
{
	stack->top = -1;
}

push( elem, stack )
ELEM	*elem;
STACK	*stack;
{
	stack->data[ ++stack->top ] = elem;
}

ELEM *
pop( stack )
STACK	*stack;
{
	if( stack->top < 0 ) {
		return NULL;
	}
	return stack->data[ stack->top-- ];
}

empty_stack( stack )
STACK	*stack;
{
	return stack->top == -1;
}

ELEM	*
get_elem()
{
	ELEM	*answer;

	answer = create_elem();

	while( WHITE(*In) ) {
		In++;
	}

	if( !*In ) {
		answer->type = ENDMARKER;
		return answer;
	}

	if( DIGIT(*In) || *In == '-' || *In == '.' ) {
		return get_number( answer );
	}
		
	if( QUOTE(*In) ) {
		return get_string( answer );
	}

	if( ALPHA(*In) ) {
		return get_name( answer );
	}

	if( PUNCT(*In) ) {
		return get_punct( answer );
	}

	SCAN_ERROR( "Unrecognized character" );
	return answer;
}

ELEM	*
get_punct( elem )
ELEM	*elem;
{

	if( *In == '(' ) {
		elem->type = LPAREN;
		In++;
		return elem;
	}

	if( *In == ')' ) {
		elem->type = RPAREN;
		In++;
		return elem;
	}

	if( *In == '+' ) {
		elem->type = PLUS;
		In++;
		return elem;
	}

	if( *In == '-' ) {
		elem->type = MINUS;
		In++;
		return elem;
	}

	if( *In == '*' ) {
		elem->type = MUL;
		In++;
		return elem;
	}

	if( *In == '/' ) {
		elem->type = DIV;
		In++;
		return elem;
	}

	if( *In == '<' ) {
		In++;
		if( *In && *In == '=' ) {
			In++;
			elem->type = LE;
		} else {
			elem->type = LT;
		}
		return elem;
	}

	if( *In == '>' ) {
		In++;
		if( *In && *In == '=' ) {
			In++;
			elem->type = GE;
		} else {
			elem->type = GT;
		}
		return elem;
	}

	if( *In == '=' ) {
		In++;
		if( *In && *In == '=' ) {
			In++;
			elem->type = EQ;
		} else {
			elem->type = GETS;
		}
		return elem;
	}

	if( *In == '!' ) {
		In++;
		if( *In && *In == '=' ) {
			In++;
			elem->type = NE;
		} else {
			elem->type = NOT;
		}
		return elem;
	}

	if( *In == '|' ) {
		In++;
		if( *In && *In == '|' ) {
			elem->type = OR;
		} else {
			SCAN_ERROR( "Unrecognized punctuation" );
			return NULL;
		}
		In++;
		return elem;
	}

	if( *In == '&' ) {
		In++;
		if( *In && *In == '&' ) {
			elem->type = AND;
		} else {
			SCAN_ERROR( "Unrecognized punctuation" );
			return NULL;
		}
		In++;
		return elem;
	}
	SCAN_ERROR( "Unrecognized punctuation" );
	return NULL;
}

ELEM *
get_number( elem )
ELEM	*elem;
{
	char	*ptr;
	char	next = *(In + 1);

	if( *In == '-' && !DIGIT(next) && next != '.' ) {
		return get_punct( elem );		/* Binary MINUS */
	}

	for( ptr=In; *ptr; ptr++ ) {
		if( *ptr == '.' ) {
			return get_float( elem );
		}
		if( *ptr != '-' && !DIGIT(*ptr) ) {
			return get_int( elem );
		}
	}
	return get_int( elem );
}

ELEM	*
get_float( elem )
ELEM	*elem;
{
	char	*ptr;
	char	tmp;
	double	atof();

	ptr = In;
	if( *ptr == '-' )
		ptr++;
	for( ; DIGIT(*ptr) || *ptr == '.'; ptr++)
		;
	tmp = *ptr;
	*ptr = '\0';
	elem->type = FLOAT;
	elem->f_val = (float)atof(In);
	*ptr = tmp;
	In = ptr;
	return elem;
}

ELEM	*
get_int( elem )
ELEM	*elem;
{
	char	*ptr;
	char	tmp;

	ptr = In;
	if( *ptr == '-' )
		ptr++;
	for( ; DIGIT(*ptr); ptr++)
		;
	tmp = *ptr;
	*ptr = '\0';
	elem->type = INT;
	elem->i_val = atoi( In );
	*ptr = tmp;
	In = ptr;
	return elem;
}


ELEM	*
get_string( elem )
ELEM	*elem;
{
	char	*ptr;

	In++;
	for( ptr=In; *ptr && *ptr != '"'; ptr++ )
		;

	if( *ptr != '"' ) {
		SCAN_ERROR( "Quote not closed" );
		return elem;
	}

	*ptr = '\0';
	elem->type = STRING;
	elem->s_val = strdup( In );
	In = ptr + 1;
	*ptr = '"';
	return elem;
}


ELEM	*
get_name( elem )
ELEM	*elem;
{
	char	*ptr, tmp;

	for( ptr=In; ALPHA(*ptr); ptr++ )
		;
	tmp = *ptr;
	*ptr = '\0';
	elem->type = NAME;
	elem->s_val = strdup( In );
	*ptr = tmp;
	In = ptr;

	if( strcmp(elem->s_val,"T") == MATCH ) {
		FREE( elem->s_val );
		elem->type = BOOL;
		elem->b_val = 1;
	} else if( strcmp(elem->s_val,"F") == MATCH ) {
		FREE( elem->s_val );
		elem->type = BOOL;
		elem->b_val = 0;
	}
	return elem;
}

display_elem( elem, log_fp )
ELEM	*elem;
FILE	*log_fp;
{
	if( Terse ) {
		display_elem_short( elem, log_fp );
	} else {
		display_elem_long( elem );
	}
}

display_elem_long( elem )
ELEM	*elem;
{
	int		type = elem->type;
	char	*op = op_name( type );

	switch( type ) {
		case LT:
		case LE:
		case EQ:
		case GT:
		case GE:
		case NE:
		case AND:
		case OR:
		case NOT:
		case PLUS:
		case MINUS:
		case MUL:
		case DIV:
		case GETS:
		case LPAREN:
		case ENDMARKER:
		case ERROR:
			dprintf( D_EXPR, "TYPE: %s\n", op );
			break;
		case NAME:
			dprintf( D_EXPR, "TYPE: %s	VALUE: \"%s\"\n", op, elem->s_val);
			break;
		case STRING:
			dprintf( D_EXPR, "TYPE: %s	VALUE: \"%s\"\n", op, elem->s_val);
			break;
		case FLOAT:
			dprintf( D_EXPR, "TYPE: %s	VALUE: %f\n", op, elem->f_val );
			break;
		case INT:
			dprintf( D_EXPR, "TYPE: %s	VALUE: %d\n", op, elem->i_val );
			break;
		case BOOL:
			dprintf( D_EXPR, "TYPE: %s	VALUE: %s\n", op,
											elem->b_val ? "TRUE" : "FALSE" );
			break;
		default:
			EXCEPT( "Found element of unknown type (%d)", type );
			break;
	}
}

display_elem_short( elem, log_fp )
ELEM	*elem;
FILE	*log_fp;
{
	int		type = elem->type;

	switch( type ) {
		case LT:
			(void)fputc( '<', log_fp );
			break;
		case LE:
			(void)fputc( '<', log_fp );
			(void)fputc( '=', log_fp );
			break;
		case EQ:
			(void)fputc( '=', log_fp );
			(void)fputc( '=', log_fp );
			break;
		case GT:
			(void)fputc( '>', log_fp );
			break;
		case GE:
			(void)fputc( '>', log_fp );
			(void)fputc( '=', log_fp );
			break;
		case NE:
			(void)fputc( '!', log_fp );
			(void)fputc( '=', log_fp );
			break;
		case AND:
			(void)fputc( '&', log_fp );
			(void)fputc( '&', log_fp );
			break;
		case OR:
			(void)fputc( '|', log_fp );
			(void)fputc( '|', log_fp );
			break;
		case NOT:
			(void)fputc( '!', log_fp );
			break;
		case PLUS:
			(void)fputc( '+', log_fp );
			break;
		case MINUS:
			(void)fputc( '-', log_fp );
			break;
		case MUL:
			(void)fputc( '*', log_fp );
			break;
		case DIV:
			(void)fputc( '/', log_fp );
			break;
		case GETS:
			(void)fputc( '=', log_fp );
			break;
		case LPAREN:
			(void)fputc( '(', log_fp );
			break;
		case RPAREN:
			(void)fputc( ')', log_fp );
			break;
		case ENDMARKER:
			(void)fputc( ';', log_fp );
			break;
		case ERROR:
			fprintf( log_fp, "(ERROR)" );
			break;
		case NAME:
			fprintf( log_fp,  "%s", elem->s_val );
			break;
		case STRING:
			fprintf( log_fp,  "%s", elem->s_val );
			break;
		case FLOAT:
			fprintf( log_fp,  "%f", elem->f_val );
			break;
		case INT:
			fprintf( log_fp,  "%d", elem->i_val );
			break;
		case BOOL:
			fprintf( log_fp,  "%c", elem->b_val ? 'T' : 'F' );
			break;
		default:
			debug_unlock();
			EXCEPT( "Found element of unknown type (%d)", type );
			break;
	}
}


struct prio {
	int		type;
	int		isp;
	int		icp;
} PrioTab[] = {
	GETS,	1,	1,
	LT,		5,	5,
	LE,		5,	5,
	EQ,		4,	4,
	GE,		5,	5,
	GT,		5,	5,
	NE,		4,	4,
	AND,	3,	3,
	OR,		2,	2,
	PLUS,	6,	6,
	MINUS,	6,	6,
	DIV,	7,	7,
	MUL,	7,	7,
	NOT,	8,	8,
	LPAREN,	0,	9,
	ENDMARKER,	0,	0,
};

expr_prio( elem, which )
ELEM	*elem;
int		which;
{
	struct prio	*p;

	for( p=PrioTab; p->type != ENDMARKER; p++ ) {
		if( p->type == elem->type ) {
			if( which == IN_STACK ) {
				return p->isp;
			} else {
				return p->icp;
			}
		}
	}
	EXCEPT( "Can't find priority for elem type %d\n", elem->type );
	/* NOTREACHED */
}

display_expr( expr )
EXPR	*expr;
{
	int		i;
	FILE	*log_fp, *debug_lock();

	if( Terse ) {
		log_fp = debug_lock();
		for( i=0; i<expr->len; i++ ) {
			display_elem( expr->data[i], log_fp );
			if( i+1 < expr->len ) {
				(void)fputc( ' ', log_fp );
			}
		}
		(void)fputc( '\n', log_fp );
		debug_unlock();
	} else {
		dprintf( D_EXPR, "\nPostfix Expression\n" );
		for( i=0; i<expr->len; i++ ) {
			display_elem( expr->data[i], (FILE *) NULL );
		}
	}
}

ELEM	*
eval( name, cont1, cont2 )
char	*name;
CONTEXT	*cont1;
CONTEXT	*cont2;
{
	STACK	op_stack, *operand_stack = &op_stack;
	ELEM	*elem, *tmp, *result;
	int		i;
	EXPR	*expr;

		/* Built in function */
	if( strcmp("CurrentTime",name) == MATCH ) {
		result = create_elem();
		result->type = INT;
		result->i_val = time( (time_t *)0 );
		return result;
	}

	expr = search_expr( name, cont1, cont2 );
	if( !expr ) {
		EVALUATION_ERROR( "Can't find variable \"%s\"", name );
		return NULL;
	}

	HadError = 0;
	init_stack( operand_stack );

	for( i=1; i<expr->len; i++ ) {

		if( HadError ) {
			return NULL;
		}

		elem = elem_dup( expr->data[i] );
		switch( elem->type ) {
			case NAME:

				if( Depth++ > MAX_RECURSION ) {
					EVALUATION_ERROR(
						"Expression too complicated -- possible loop" );
					tmp = NULL;
				} else {
					tmp = eval( elem->s_val, cont1, cont2 );
				}
				Depth--;

				free_elem( elem );
				if( !tmp ) {
					return NULL;
				}
				push( tmp, operand_stack );
				break;
			case STRING:
			case FLOAT:
			case INT:
			case BOOL:
				push( elem, operand_stack );
				break;
			case LT:
			case LE:
			case GT:
			case GE:
			case NE:
			case EQ:
			case AND:
			case OR:
			case NOT:
			case PLUS:
			case MINUS:
			case MUL:
			case DIV:
				do_op( elem, operand_stack );
				free_elem( elem );
				break;
			case GETS:	/* Ignore for now */
				free_elem( elem );
				break;
			case ENDMARKER:
				free_elem( elem );
				result = pop( operand_stack );

				if( !result || !empty_stack(operand_stack) ) {
					EVALUATION_ERROR(
								"Number of operands doesn't match operators");
					return NULL;
				}
				return result;
			default:
				EXCEPT( "Found elem type %d in postfix expr\n", elem->type );
				break;
		}
	}
	EXCEPT( "Internal evaluation error" );
	/* NOTREACHED */
}

static
do_op( elem, stack )
ELEM	*elem;
STACK	*stack;
{
	switch( elem->type ) {
			case LT:
			case LE:
			case GT:
			case GE:
			case EQ:
			case NE:
				do_comparison_op( elem->type, stack );
				break;
			case AND:
			case OR:
			case NOT:
				do_logical_op( elem->type, stack );
				break;
			case PLUS:
			case MINUS:
			case MUL:
			case DIV:
				do_arithmetic_op( elem->type, stack );
				break;
			default:
				EXCEPT( "Unexpected element type (%d)", elem->type );
		}
}

do_logical_op( op, stack )
int		op;
STACK	*stack;
{
	ELEM	*elem_1, *elem_2;
	ELEM	*answer;

	answer = create_elem();
	answer->type = BOOL;

		/* Get and check right operand */
	elem_2 = unstack_elem( op, stack );
	if( !elem_2 ) {
		free_elem( answer );
		return;
	}
	if( elem_2->type != BOOL ) {
		EVALUATION_ERROR( "boolean value expected" );
		free_elem( elem_2 );
		free_elem( answer );
		return;
	}

		/* NOT only takes one operand, so do it here */
	if( op == NOT ) {
		answer->b_val = !elem_2->b_val;
		push( answer, stack );
		free_elem( elem_2 );
		return;
	}

		/* Get and check left operand */
	elem_1 = unstack_elem( op, stack );
	if( !elem_1 ) {
		free_elem( elem_2 );
		free_elem( answer );
		return;
	}
	if( elem_1->type != BOOL ) {
		EVALUATION_ERROR( "boolean value expected" );
		free_elem( elem_2 );
		free_elem( elem_1 );
		free_elem( answer );
		return;
	}

	switch( op ) {
		case AND:
			answer->b_val = elem_1->b_val && elem_2->b_val;
			break;
		case OR:
			answer->b_val = elem_1->b_val || elem_2->b_val;
			break;
		default:
			SCAN_ERROR( "unexpected operator" );
			return;
	}
	push( answer, stack );
	free_elem( elem_1 );
	free_elem( elem_2 );
}

do_comparison_op( op, stack )
int		op;
STACK	*stack;
{
	ELEM	*elem_1, *elem_2;

		/* Get and check right operand */
	elem_2 = unstack_elem( op, stack );
	if( !elem_2 ) {
		return;
	}

		/* Get and check left operand */
	elem_1 = unstack_elem( op, stack );
	if( !elem_1 ) {
		free_elem( elem_2 );
		return;
	}

	if( elem_1->type == INT && elem_2->type == INT ) {
		push( integer_compare(op,elem_1->i_val,elem_2->i_val), stack );
	} else if( elem_1->type == FLOAT && elem_2->type == INT ) {
		push( float_compare(op,elem_1->f_val,(float)elem_2->i_val), stack );
	} else if( elem_1->type == INT && elem_2->type == FLOAT ) {
		push( float_compare(op,(float)elem_1->i_val,elem_2->f_val), stack );
	} else if( elem_1->type == FLOAT && elem_2->type == FLOAT) {
		push( float_compare(op,elem_1->f_val,elem_2->f_val), stack );
	} else if( elem_1->type == STRING && elem_2->type == STRING ) {
		push( string_compare(op,elem_1->s_val,elem_2->s_val), stack );
	} else {
		EVALUATION_ERROR( "Comparison of incompatible types %d and %d",
											elem_1->type, elem_2->type );
	}
	free_elem( elem_1 );
	free_elem( elem_2 );
}

ELEM	*
integer_compare( op, v1, v2 )
int		op;
int		v1;
int		v2;
{
	ELEM	*answer;

	answer = create_elem();
	answer->type = BOOL;

	switch( op ) {
		case LT:
			answer->b_val = v1 < v2;
			break;
		case LE:
			answer->b_val = v1 <= v2;
			break;
		case EQ:
			answer->b_val = v1 == v2;
			break;
		case GE:
			answer->b_val = v1 >= v2;
			break;
		case GT:
			answer->b_val = v1 > v2;
			break;
		case NE:
			answer->b_val = v1 != v2;
			break;
		default:
			EXCEPT( "Unexpecteded operator %d\n", op );
	}
	return answer;
}

ELEM	*
float_compare( op, f1, f2 )
int		op;
float	f1;
float	f2;
{
	ELEM	*answer;

	answer = create_elem();
	answer->type = BOOL;

	switch( op ) {
		case LT:
			answer->b_val = f1 < f2;
			break;
		case LE:
			answer->b_val = f1 <= f2;
			break;
		case EQ:
			answer->b_val = f1 == f2;
			break;
		case GE:
			answer->b_val = f1 >= f2;
			break;
		case GT:
			answer->b_val = f1 > f2;
			break;
		case NE:
			answer->b_val = f1 != f2;
			break;
		default:
			EXCEPT( "Unexpecteded operator %d\n", op );
	}
	return answer;
}

ELEM	*
string_compare( op, s1, s2 )
int		op;
char	*s1;
char	*s2;
{
	ELEM	*answer;

	answer = create_elem();
	answer->type = BOOL;

	switch( op ) {
		case LT:
			answer->b_val = strcmp(s1,s2) < 0;
			break;
		case LE:
			answer->b_val = strcmp(s1,s2) <= 0;
			break;
		case EQ:
			answer->b_val = strcmp(s1,s2) == 0;
			break;
		case GE:
			answer->b_val = strcmp(s1,s2) >= 0;
			break;
		case GT:
			answer->b_val = strcmp(s1,s2) > 0;
			break;
		case NE:
			answer->b_val = strcmp(s1,s2) != 0;
			break;
		default:
			EXCEPT( "Unexpecteded operator %d\n", op );
	}
	return answer;
}

ELEM	*
elem_dup( elem )
ELEM	*elem;
{
	ELEM	*answer;

	answer = create_elem();

	switch( elem->type ) {
		case NAME:
		case STRING:
			answer->type = elem->type;
			answer->s_val = strdup( elem->s_val );
			break;
		default:
			bcopy( (char *)elem, (char *)answer, sizeof(ELEM) );
			break;
	}
	return answer;
}

do_arithmetic_op( op, stack )
int		op;
STACK	*stack;
{
	ELEM	*elem_1, *elem_2;

	elem_2 = unstack_elem( op, stack );
	elem_1 = unstack_elem( op, stack );

		/* Kludge to allow boolean (viewed as 1 or 0),
		to be added to ints or floats -- grot */
	if( elem_1->type == BOOL ) {
		elem_1->type = INT;
	}
	if( elem_2->type == BOOL ) {
		elem_2->type = INT;
	}

	if( elem_1->type == INT && elem_2->type == INT ) {
		push( integer_arithmetic(op,elem_1->i_val,elem_2->i_val), stack );
	} else if( elem_1->type == FLOAT && elem_2->type == INT ) {
		push( float_arithmetic(op,elem_1->f_val,(float)elem_2->i_val), stack );
	} else if( elem_1->type == INT && elem_2->type == FLOAT ) {
		push( float_arithmetic(op,(float)elem_1->i_val,elem_2->f_val), stack );
	} else if( elem_1->type == FLOAT && elem_2->type == FLOAT) {
		push( float_arithmetic(op,elem_1->f_val,elem_2->f_val), stack );
	} else {
		EXCEPT( "Arithmetic on operand types %d and %d\n",
											elem_1->type, elem_2->type );
	}
	free_elem( elem_1 );
	free_elem( elem_2 );
}

scan_error( str )
char	*str;
{
	int		count;
	int		i;
	char	buf[ BUFSIZ ];

	if( Silent ) {
		return;
	}

	dprintf( D_EXPR, "\n%s", Line );
	count = In - Line;
	for( i=0; i<count; i++ ) {
		buf[i] = ' ';
	}
	buf[i++] = '^';
	buf[i] = '\0';
	dprintf( D_EXPR, buf );
	dprintf( D_EXPR, "Syntax error: " );
	dprintf( D_EXPR | D_NOHEADER, "%s\n", str );
	dprintf( D_EXPR, "Discovered at line %d in file %s\n",
													_LineNo, _FileName );
	HadError++;
}

ELEM	*
integer_arithmetic( op, v1, v2 )
int		op;
int		v1;
int		v2;
{
	ELEM	*answer;

	answer = create_elem();
	answer->type = INT;

	switch( op ) {
		case PLUS:
			answer->i_val = v1 + v2;
			break;
		case MINUS:
			answer->i_val = v1 - v2;
			break;
		case DIV:
			answer->i_val = v1 / v2;
			break;
		case MUL:
			answer->i_val = v1 * v2;
			break;
		default:
			EXCEPT( "Unexpecteded operator %d\n", op );
	}
	return answer;
}

ELEM	*
float_arithmetic( op, v1, v2 )
int		op;
float	v1;
float	v2;
{
	ELEM	*answer;

	answer = create_elem();
	answer->type = FLOAT;

	switch( op ) {
		case PLUS:
			answer->f_val = v1 + v2;
			break;
		case MINUS:
			answer->f_val = v1 - v2;
			break;
		case DIV:
			answer->f_val = v1 / v2;
			break;
		case MUL:
			answer->f_val = v1 * v2;
			break;
		default:
			EXCEPT( "Unexpecteded operator %d\n", op );
	}
	return answer;
}

struct {
	int		op;
	char	*name;
} OpName[] = {
	LT,			"LT",
	LE,			"LE",
	EQ,			"EQ",
	GT,			"GT",
	GE,			"GE",
	NE,			"NE",
	AND,		"AND",
	OR,			"OR",
	NOT,		"NOT",
	PLUS,		"PLUS",
	MINUS,		"MINUS",
	MUL,		"MUL",
	DIV,		"DIV",
	GETS,		"GETS",
	LPAREN,		"LPAREN",
	RPAREN,		"RPAREN",
	NAME,		"NAME",
	STRING,		"STRING",
	FLOAT,		"FLOAT",
	INT,		"INT",
	BOOL,		"BOOL",
	ERROR,		"ERROR",
	ENDMARKER,	"ENDMARKER",
	0,			0,
};

char *
op_name( op )
int		op;
{
	int		i;

	for( i=0; OpName[i].op; i++ ) {
		if( OpName[i].op == op ) {
			return OpName[i].name;
		}
	}
	EXCEPT( "Can't find op name for elem type (%d)\n", op );
	/* NOTREACHED */
}


free_elem( elem )
ELEM	*elem;
{
	if( elem->type == STRING || elem->type == NAME ) {
		FREE( elem->s_val );
	}
	FREE( (char *)elem );
}

/*
** Take an element off the stack, and if it's a name, look up its value.
*/
ELEM *
unstack_elem( op, stack )
int		op;
STACK	*stack;
{
	ELEM	*answer;

	answer = pop( stack );

	if( !answer ) {
		EVALUATION_ERROR( "Missing operand for %s", op_name(op) );
		return NULL;
	}

	return answer;
}


ELEM	*
create_elem()
{
	ELEM	*answer;

	answer = (ELEM *)MALLOC( sizeof(ELEM) );
	answer->type = ERROR;
	answer->i_val = 0;
	return answer;
}

store_stmt( expr, context )
EXPR	*expr;
CONTEXT	*context;
{
	int		i;
	char	*name;

	if( expr->data[0]->type != NAME ) {
		dprintf( D_ALWAYS, "First element in statement not a NAME" );
		return FALSE;
	}
	name = expr->data[0]->s_val;

	for( i=0; i<context->len; i++ ) {
		if( context->data[i]->data[0]->type != NAME ) {
			dprintf( D_ALWAYS,
				"Bad machine context, first elem in expr [%d] is type %d",
				i, context->data[i]->data[0]->type );
			return FALSE;
		}
		if( strcmp(name,context->data[i]->data[0]->s_val) == MATCH ) {
			free_expr( context->data[i] );
			context->data[i] = expr;
			return TRUE;
		}
	}
	return add_stmt( expr, context );
}

CONTEXT *
create_context()
{
	CONTEXT	*answer;

	answer = (CONTEXT *)MALLOC( sizeof(CONTEXT) );
	answer->len = 0;
	answer->max_len = EXPR_INCR;
	answer->data = (EXPR **)MALLOC(
							(unsigned)(answer->max_len * sizeof(EXPR *)) );
	return answer;
}

free_context( context )
CONTEXT	*context;
{
	int		i;

	for( i=0; i<context->len; i++ ) {
		free_expr( context->data[i] );
	}
	FREE( (char *)context->data );
	FREE( (char *)context );
}

add_stmt( expr, context )
EXPR	*expr;
CONTEXT	*context;
{
	if( context->len == context->max_len ) {
		context->max_len += EXPR_INCR;
		context->data = (EXPR **)REALLOC( (char *)context->data,
							(unsigned)(context->max_len * sizeof(EXPR *)) );
	}
	context->data[context->len++] = expr;
	return TRUE;
}

display_context( context )
CONTEXT	*context;
{
	int		i;

	for( i=0; i<context->len; i++ ) {
		dprintf( D_EXPR, "Stmt %2d:", i );
		if( !Terse ) {
			dprintf( D_EXPR, "\n" );
		}
		display_expr( context->data[i] );
	}
}

EXPR *
search_expr( name, cont1, cont2 )
char	*name;
CONTEXT	*cont1;
CONTEXT	*cont2;
{
	int		i;

	if( cont1 ) {
		for( i=0; i<cont1->len; i++ ) {
			if( strcmp(name,cont1->data[i]->data[0]->s_val) == MATCH ) {
				return cont1->data[i];
			}
		}
	}

	if( cont2 ) {
		for( i=0; i<cont2->len; i++ ) {
			if( strcmp(name,cont2->data[i]->data[0]->s_val) == MATCH ) {
				return cont2->data[i];
			}
		}
	}
	return NULL;
}

EXPR	*
build_expr( name, val )
char	*name;
ELEM	*val;
{
	EXPR	*answer;
	ELEM	*elem;

	answer = create_expr();

	elem = create_elem();
	elem->type = NAME;
	elem->s_val = strdup( name );
	add_elem( elem, answer );

	add_elem( elem_dup(val), answer );

	elem = create_elem();
	elem->type = GETS;
	add_elem( elem, answer );

	elem = create_elem();
	elem->type = ENDMARKER;
	add_elem( elem, answer );

	return answer;
}

evaluate_bool( name, answer, context1, context2 )
char	*name;
int		*answer;
CONTEXT	*context1;
CONTEXT	*context2;
{
	ELEM	*elem;

	elem = eval( name, context1, context2 );

	if( !elem ) {
		if( !Silent ) {
			dprintf( D_EXPR, "Expression \"%s\" can't evaluate\n", name );
		}
		return -1;
	}

	if( elem->type != BOOL ) {
		free_elem( elem );
		dprintf( D_EXPR,
			"Expression \"%s\" expected type boolean, but was %s",
											name, op_name(elem->type) );
		return -1;
	}
	*answer = elem->b_val;
	free_elem( elem );
	dprintf( D_EXPR, "evaluate_bool(\"%s\") returns %s\n",
										name, *answer ? "TRUE" : "FALSE" );
	return 0;
}

evaluate_float( name, answer, context1, context2 )
char	*name;
float	*answer;
CONTEXT	*context1;
CONTEXT	*context2;
{
	ELEM	*elem;

	elem = eval( name, context1, context2 );

	if( !elem ) {
		if( !Silent ) {
			dprintf( D_EXPR, "Expression \"%s\" can't evaluate\n", name );
		}
		return -1;
	}

	if( elem->type != INT && elem->type != FLOAT ) {
		free_elem( elem );
		dprintf( D_EXPR,
			"Expression \"%s\" expected type float, but was %s",
											name, op_name(elem->type) );
		return -1;
	}
	if( elem->type == FLOAT ) {
		*answer = elem->f_val;
	} else {
		*answer = (float)elem->i_val;
	}
	free_elem( elem );
	dprintf( D_EXPR, "evaluate_float(\"%s\") returns %f\n",
										name, *answer );
	return 0;
}

evaluate_int( name, answer, context1, context2 )
char	*name;
int		*answer;
CONTEXT	*context1;
CONTEXT	*context2;
{
	ELEM	*elem;

	elem = eval( name, context1, context2 );

	if( !elem ) {
		if( !Silent ) {
			dprintf( D_EXPR, "Expression \"%s\" can't evaluate\n", name );
		}
		return -1;
	}

	if( elem->type != INT ) {
		free_elem( elem );
		dprintf( D_EXPR,
			"Expression \"%s\" expected type int, but was %s",
											name, op_name(elem->type) );
		return -1;
	}
	*answer = elem->i_val;
	free_elem( elem );
	dprintf( D_EXPR, "evaluate_int(\"%s\") returns %d\n",
										name, *answer );
	return 0;
}

evaluate_string( name, answer, context1, context2 )
char	*name;
char	**answer;
CONTEXT	*context1;
CONTEXT	*context2;
{
	ELEM	*elem;

	elem = eval( name, context1, context2 );

	if( !elem ) {
		if( !Silent ) {
			dprintf( D_EXPR, "Expression \"%s\" can't evaluate\n", name );
		}
		return -1;
	}

	if( elem->type != STRING ) {
		free_elem( elem );
		dprintf( D_EXPR,
			"Expression \"%s\" expected type string, but was %s",
											name, op_name(elem->type) );
		return -1;
	}
	*answer = strdup( elem->s_val );
	free_elem( elem );
	dprintf( D_EXPR, "evaluate_string(\"%s\") returns \"%s\"\n",
										name, *answer );
	return 0;
}

#ifdef LINT
/* VARARGS */
evaluation_error(foo)
char	*foo;
{ printf( foo ); }
#else LINT
/* VARARGS */
evaluation_error(va_alist)
va_dcl
{
	va_list pvar;
	char *fmt;
	char	buf[ BUFSIZ ];

	if( Silent ) {
		return;
	}

	va_start(pvar);

	fmt = va_arg(pvar, char *);


#if vax || i386 || bobcat || ibm032
	{
		FILE _strbuf;
		int *argaddr = &va_arg(pvar, int);

		_strbuf._flag = _IOWRT+_IOSTRG;
		_strbuf._ptr  = buf;
		_strbuf._cnt  = BUFSIZ;
		_doprnt( fmt, argaddr, &_strbuf );
		putc('\0', &_strbuf);
	}
#else vax || i386 || bobcat || ibm032
	vsprintf( buf, fmt, pvar );
#endif vax || i386 || bobcat || ibm032

	dprintf( D_EXPR, "Evaluation error: %s\n", buf );
	HadError++;
}
#endif LINT

#ifdef notdef
read_exprs( name, context )
char	*name;
CONTEXT	*context;
{
	FILE	*fp;
	char	line[1024];
	EXPR	*expr, *scan();

	if( (fp=fopen(name,"r")) == NULL ) {
		EXCEPT( "fopen(\"%s\",\"r\")", name );
	}

	while( fgets(line,sizeof(line),fp) ) {
		if( blankline(line) ) {
			continue;
		}
		
		if( (expr=scan(line)) == NULL ) {
			EXCEPT( "Syntax Error in \"%s\"", name );
		}

		store_stmt( expr, context );

	}
	(void)fclose( fp );
}
#endif notdef

/*
* these are added for condor flocking : dhruba 
*/
append_stmt(expr, context)
EXPR    *expr;
CONTEXT *context;
{
    int     i;
    char    *name;

    if( expr->data[0]->type != NAME ) {
        dprintf( D_ALWAYS, "First element in statement not a NAME" );
        return FALSE;
    }
    name = expr->data[0]->s_val;

    for( i=0; i<context->len; i++ ) 
	{
        if( context->data[i]->data[0]->type != NAME ) 
		{
            dprintf( D_ALWAYS,
                "Bad machine context, first elem in expr [%d] is type %d",
                i, context->data[i]->data[0]->type );
            return FALSE;
        }
		if( strcmp(name,context->data[i]->data[0]->s_val) == MATCH )
		{
			/* found where to append */
			concatenate_expr(context->data[i], expr,AND);
			return  TRUE;
		}
	}
	return FALSE;
}

concatenate_expr(exp1, exp2,operator)
EXPR	*exp1, *exp2;
int     operator;
{
	int 	i;
	ELEM	*elem;
	if ( ( elem = (ELEM*) malloc(sizeof(ELEM))) == NULL )
	{
		EXCEPT(" Error in malloc in util lib\n");
	}
	elem->type = operator;

	for ( i=1; i < exp2->len-2; i++)
		add_elem_before_last( exp2->data[i], exp1);
	add_elem_before_last( elem, exp1);
}
		
/*
* adds the element in the last-but-one slot of the data array 
*/
add_elem_before_last( elem, expr )
ELEM	*elem;
EXPR	*expr;
{
	if( expr->len == expr->max_len ) {
		expr->max_len += EXPR_INCR;
		expr->data =
			(ELEM **)REALLOC( (char *)expr->data,
			(unsigned)(expr->max_len * sizeof(ELEM *)) );
	}
	expr->data[expr->len]   = expr->data[expr->len-1];
	expr->data[expr->len-1] = expr->data[expr->len-2];
	expr->data[expr->len-2] = elem;
	expr->len++;
}
		

	    

