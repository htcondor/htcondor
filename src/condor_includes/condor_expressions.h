#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#define MAX_EXPR_LIST	512
#define MAX_STRING		1024	/* Max length of a name or string */
#define EXPR_INCR 25
#define IN_STACK	1
#define IN_COMING	2

typedef struct {
	int		type;
	union {
		int		integer_val;	/* Integer value */
		float	float_val;		/* Floating point value */
		char	*string_val;	/* String value */
	} val;
} ELEM;

typedef struct {
	int		len;
	int		max_len;
	ELEM	**data;
} EXPR;

#if !defined(WIN32) // CONTEXT is defined in windows.h!  I hope we don't need these anymore...

typedef struct {
	int		len;
	int		max_len;
	EXPR	**data;
} CONTEXT;

#endif

typedef struct {
	int     top;
	ELEM    *data[MAX_EXPR_LIST];
} STACK;

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
#if !defined(WIN32) // these are built-in types in WIN32!
#define FLOAT	19
#define INT		20
#define BOOL	21
#endif
#if !defined(WIN32) // ERROR is a WIN32 macro!
#define ERROR	22
#endif
#define MOD		23
#define EXPRSTRING 24
#define ENDMARKER	-1

#define f_val val.float_val
#define i_val val.integer_val
#define s_val val.string_val
#define b_val val.integer_val

#if defined(__cplusplus)
extern "C" {
#endif

#if 0 // no more CONTEXTs

#if defined(__STDC__) || (__cplusplus)
EXPR * scan ( char *line );
CONTEXT * create_context ( void );
int store_stmt ( EXPR *expr, CONTEXT *context );
void free_elem ( ELEM *elem );
void free_context ( CONTEXT *context );
ELEM * eval ( char *name, CONTEXT *cont1, CONTEXT *cont2 );
#else
EXPR * scan();
CONTEXT * create_context();
int store_stmt();
void free_elem();
void free_context();
ELEM * eval();
#endif

#endif

#if defined(__cplusplus)
}
#endif

#endif
