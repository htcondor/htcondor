%token TYPE
%token ID
%token STRUCT
%token UNKNOWN

%type <node> stub_spec param_list param
%type <tok> TYPE ID STRUCT UNKNOWN

%{
#include "scanner.h"
struct node *mk_func_node( char *type_name, char *id, struct node *param_list );
struct node *mk_param_node( char *type_name, char *id, int is_ptr );
struct node *insert_node( struct node *list, struct node *new_elem );
void display_node( struct node * );
void display_list( struct node * );
struct node *mk_list();
%}

%%

input
	: /* empty */
	| input stub_spec
			{ printf( "GOAL\n" ); }
	;

stub_spec
	: TYPE ID '(' param_list ')' ';'
			{
			$$ = mk_func_node( $1.val, $2.val, $4 );
			display_node( $$ ); printf( "\n" );
			}
	;

param_list
	: param
			{ $$ = insert_node( mk_list(), $1 ); }
	| param ',' param_list
			{ $$ = insert_node( $3, $1 ); }
	;

param
	: TYPE ID
			{
			$$ = mk_param_node( $1.val, $2.val, 0 );
			}

	| TYPE '*' ID
			{
			$$ = mk_param_node( $1.val, $3.val, 1 );
			}
	;
%%
main()
{
	yyparse();
}

yyerror( char * s )
{
	printf( "ERROR: %s\n", s );
}

struct node *
mk_param_node( char *type, char *name, int is_ptr )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = PARAM;
	answer->type_name = type;
	answer->id = name;
	answer->is_ptr = is_ptr;

	answer->next = answer;
	answer->prev = answer;

	return answer;
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
display_node( struct node *n )
{
	switch( n->node_type ) {
		case PARAM:
			if( n->is_ptr ) {
				printf( "%s *%s", n->type_name, n->id );
			} else {
				printf( "%s %s", n->type_name, n->id );
			}
			break;
		case FUNC:
			printf( "FUNC: %s %s ", n->type_name, n->id );
			display_list( n->next );
			break;
	}
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
