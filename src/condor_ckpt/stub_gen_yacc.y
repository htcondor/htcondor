%token TYPE_NAME
%token CONST
%token IDENTIFIER
%token UNKNOWN
%token MAP

%type <node> stub_spec param_list param simple_param map_param
%type <tok> TYPE_NAME CONST IDENTIFIER UNKNOWN

%{
#include <stdio.h>
#include <assert.h>
#include "scanner.h"
extern int CurLineNo;
struct node *mk_func_node( char *type_name, char *id, struct node *param_list );
struct node *mk_param_node( char *type_name, char *id, int is_ptr, int is_const );
struct node *mk_map_param( struct node * simple );
struct node *insert_node( struct node *list, struct node *new_elem );
void display_node( struct node * );
void output( struct node * );
void display_list( struct node * );
void output_param_decl( struct node * );
void output_call( char *call, char *id, struct node *list );
struct node *mk_list();
void copy_header( FILE *in_fp, FILE *out_fp );

#define IS_PTR 1
#define NOT_PTR 0
#define IS_CONST 1
#define NOT_CONST 0
%}

%%

input
	: /* empty */
	| input stub_spec
	;

stub_spec
	: TYPE_NAME IDENTIFIER '(' param_list ')' ';'
			{
			$$ = mk_func_node( $1.val, $2.val, $4 );
			output( $$ );
			}
	;

param_list
	: param
			{ $$ = insert_node( mk_list(), $1 ); }
	| param ',' param_list
			{ $$ = insert_node( $3, $1 ); }
	;

param
	: simple_param
			{ $$ = $1; }
	| map_param
			{ $$ = $1; }
	;

map_param
	: MAP '(' simple_param ')'
			{ $$ = mk_map_param( $3 ); }
	;

simple_param
	: TYPE_NAME IDENTIFIER
			{
			$$ = mk_param_node( $1.val, $2.val, NOT_PTR, NOT_CONST );
			}

	| TYPE_NAME '*' IDENTIFIER
			{
			$$ = mk_param_node( $1.val, $3.val, IS_PTR, NOT_CONST );
			}
	| CONST TYPE_NAME IDENTIFIER
			{
			$$ = mk_param_node( $2.val, $3.val, NOT_PTR, IS_CONST );
			}

	| CONST TYPE_NAME '*' IDENTIFIER
			{
			$$ = mk_param_node( $2.val, $4.val, IS_PTR, IS_CONST );
			}
	;
%%
main()
{
	copy_header( stdin, stdout );
	yyparse();
	exit( 0 );
}

yyerror( char * s )
{
	fprintf( stderr, "%s on line %d\n", s, CurLineNo );
	exit( 1 );
}

struct node *
mk_param_node( char *type, char *name, int is_ptr, int is_const )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = PARAM;
	answer->type_name = type;
	answer->id = name;
	answer->is_ptr = is_ptr;
	answer->is_const = is_const;
	answer->is_mapped = 0;

	answer->next = answer;
	answer->prev = answer;

	return answer;
}

struct node *
mk_map_param( struct node * simple )
{
	simple->is_mapped = 1;
	return simple;
}

struct node *
mk_func_node( char *type, char *name, struct node * p_list )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = FUNC;
	answer->type_name = type;
	answer->id = name;
	answer->next = p_list;

	return answer;
}

struct node *
insert_node( struct node * list, struct node * new_elem )
{

	new_elem->next = list->next;
	new_elem->prev = list;
	list->next->prev = new_elem;
	list->next = new_elem;

	return list;
}

void
output_param_node( struct node *n )
{
	assert( n->node_type == PARAM );

	if( n->is_const ) {
		printf( "const " );
	}
	if( n->is_ptr ) {
		printf( "%s *%s", n->type_name, n->id );
	} else {
		printf( "%s %s", n->type_name, n->id );
	}
}

void
display_node( struct node *n )
{
	switch( n->node_type ) {
		case PARAM:
			if( n->is_mapped ) {
				printf( "MAP(" );
			}
			if( n->is_const ) {
				printf( "const " );
			}
			if( n->is_ptr ) {
				printf( "%s *%s", n->type_name, n->id );
			} else {
				printf( "%s %s", n->type_name, n->id );
			}
			if( n->is_mapped ) {
				printf( ")" );
			}
			break;
		case FUNC:
			printf( "FUNC: %s %s ", n->type_name, n->id );
			display_list( n->next );
			break;
	}
}

void
output_param_decl( struct node *dum )
{
	struct node	*p;

	printf( "(" );
	for( p=dum->next; p != dum; p = p->next ) {
		printf( " " );
		output_param_node( p );
		if( p->next != dum ) {
			printf( "," );
		} else {
			printf( " " );
		}
	}
	printf( ")\n" );
}

void
output_call( char *call, char *id, struct node *list )
{
	struct node	*p;

	printf( "		rval = %s( SYS_%s, ", call, id );
	for( p=list->next; p != list; p = p->next ) {
		if( p->is_mapped ) {
			printf( "user_%s", p->id );
		} else {
			printf( "%s", p->id );
		}
		if( p->next != list ) {
			printf( ", " );
		} else {
			printf( " " );
		}
	}
	printf( ");\n" );
}

void
display_list( struct node *dum )
{
	struct node	*p;

	printf( "(" );
	for( p=dum->next; p != dum; p = p->next ) {
		printf( " " );
		display_node( p );
		if( p->next != dum ) {
			printf( "," );
		} else {
			printf( " " );
		}
	}
	printf( ")\n" );
}

struct node *
mk_list()
{
	struct node *answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->next = answer;
	answer->prev = answer;
	answer->type_name = "(dummy)";
	answer->id = "(dummy)";
	return answer;
}

/*
  Look through the list of parameters and see if any are fd's which need
  to be mapped.  If so output the appropriate declaration and mapping
  code.
*/
void
output_mapping( char *func_type_name, struct node *list )
{
	struct node	*n;
	int	found_mapping = 0;

	for( n = list->next; n != list; n = n->next ) {
		if( n->is_mapped ) {
			printf( "	%s	user_%s;\n", n->type_name, n->id );
			found_mapping = 1;
		}
	}

	if( found_mapping ) {
		printf( "\n" );
	}

	for( n = list->next; n != list; n = n->next ) {
		if( n->is_mapped ) {
			printf( "	if( (user_%s=MapFd(%s)) < 0 ) {\n", n->id, n->id );
			printf( "		return (%s)-1;\n", func_type_name );
			printf( "	}\n" );
		}
	}
}

/*
  Output code for one system call stub.
*/
void
output( struct node *n )
{
	assert( n->node_type == FUNC );

	printf( "#if defined( SYS_%s )\n", n->id );
	printf( "%s\n", n->type_name );
	printf( "%s", n->id );
	output_param_decl( n->next );
	printf( "{\n" );
	printf( "	int	rval;\n" );
	output_mapping( n->type_name, n->next );
	printf( "\n" );
	printf( "	if( LocalSysCalls() ) {\n" );
	output_call( "syscall", n->id, n->next );
	printf( "	} else {\n" );
	output_call( "REMOTE_syscall", n->id, n->next );
	printf( "	}\n\n" );
	if( strcmp(n->type_name,"int") == 0 ) {
		printf( "	return rval;\n" );
	} else {
		printf( "	return (%s)rval;\n", n->type_name );
	}
	printf( "}\n" );
	printf( "#endif\n\n" );
}

/*
  The header of the template file is to be copied character for character to
  the ".c" file we are building.  The header is terminated by a line
  containing only "%%".
*/
void
copy_header( FILE *in_fp, FILE *out_fp )
{
	char	buf[1024];

	while( fgets(buf,sizeof(buf),in_fp) ) {
		if( strcmp("%%\n",buf) == 0 ) {
			return;
		}
		fputs(buf,out_fp);
	}
}
