#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

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

typedef struct {
	int		len;
	int		max_len;
	EXPR	**data;
} CONTEXT;


#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

	/* Element types */
static const LT			= 1;
static const LE			= 2;
static const GT			= 3;
static const GE			= 4;
static const EQ			= 5;
static const NE			= 6;
static const AND		= 7;
static const OR			= 8;
static const NOT		= 9;
static const PLUS		= 10;
static const MINUS		= 11;
static const MUL		= 12;
static const DIV		= 13;
static const GETS		= 14;
static const LPAREN		= 15;
static const RPAREN		= 16;
static const NAME		= 17;
static const STRING		= 18;
static const FLOAT		= 19;
static const INT		= 20;
static const BOOL		= 21;
static const ERROR		= 22;
static const ENDMARKER	= -1;

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || (__cplusplus)
EXPR * scan ( const char *line );
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

#if defined(__cplusplus)
}
#endif

#endif
