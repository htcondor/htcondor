%token TYPE_NAME
%token CONST
%token IDENTIFIER
%token UNKNOWN
%token MAP
%token EXTRACT
%token ARRAY
%token IN
%token OUT
%token BOTH

%type <node> stub_spec param_list param simple_param map_param
%type <tok> TYPE_NAME CONST IDENTIFIER UNKNOWN MAP EXTRACT ARRAY
%type <bool> opt_const opt_ptr opt_ptr_to_const opt_array opt_extract
%type <param_mode> use_type

%{
#include <stdio.h>
#include <assert.h>
#include <assert.h>
#include "scanner.h"
extern int yyline;
struct node *mk_func_node( char *type_name, char *id,
	struct node *param_list, int is_ptr, int extract );
struct node * mk_param_node( char *type, char *name,
	int is_const, int is_ptr, int is_const_ptr, int is_array,
	int is_in, int is_out );
struct node *set_map_attr( struct node * simple );
struct node *insert_node( struct node *list, struct node *new_elem );
void display_node( struct node * );
void output_switch( struct node * );
void display_list( struct node * );
void output_switch_decl( struct node * );
void output_local_call(  char *id, struct node *list );
void output_remote_call(  char *id, struct node *list );
void output_extracted_call(  char *id, struct node *list );
void output_param_list( struct node *list );
struct node *mk_list();
void copy_file( FILE *in_fp, FILE *out_fp );
FILE * open_file( char *name );
char * mk_upper();
char * abbreviate( char *type_name );

#define MATCH 0 /* for strcmp() */

#define SWITCHES 1
#define SENDERS 2
#define RECEIVERS 3

int	Mode = 0;

%}

%%

input
	: /* empty */
	| input stub_spec
	;

stub_spec
	: TYPE_NAME opt_ptr IDENTIFIER '(' param_list ')' opt_extract ';'
		{
			$$ = mk_func_node( $1.val, $3.val, $5, $2, $7 );
			switch( Mode ) {
			  case SWITCHES:
				output_switch( $$ );
				break;
			  case SENDERS:
				output_sender( $$ );
				break;
			  case RECEIVERS:
				fprintf( stderr, "Don't know how to output RECEIVERS - yet\n" );
				exit( 1 );
			}
		}
	;

opt_extract
	: ':' EXTRACT
		{ $$ = TRUE; }
	| /* null */
		{ $$ = FALSE; }
	;

param_list
	: param
			{ $$ = insert_node( mk_list(), $1 ); }
	| param ',' param_list
			{ $$ = insert_node( $3, $1 ); }
	| /* empty */
			{ $$ = mk_list(); }
	;

param
	: simple_param
			{ $$ = $1; }
	| map_param
			{ $$ = $1; }
	;

map_param
	: MAP '(' simple_param ')'
			{ $$ = set_map_attr( $3 ); }
	;

simple_param
	: use_type opt_const TYPE_NAME opt_ptr opt_ptr_to_const IDENTIFIER opt_array
			{
			$$ = mk_param_node(
				$3.val,			/* TYPE_NAME */
				$6.val,			/* IDENTIFIER */
				$2,				/* constant */
				$4,				/* pointer */
				$5,				/* pointer to const */
				$7,				/* array */
				$1.in,			/* in parameter */
				$1.out			/* out parameter */
			);
			}
	;

use_type
	: IN
		{
		$$.in = TRUE;
		$$.out = FALSE;
		}
	| OUT
		{
		$$.in = FALSE;
		$$.out = TRUE;
		}
	| BOTH
		{
		$$.in = TRUE;
		$$.out = TRUE;
		}
	| /* null */
		{
		$$.in = FALSE;
		$$.out = FALSE;
		}
	;

opt_const
	: CONST
		{
		$$ = TRUE;
		}
	| /* null */
		{
		$$ = FALSE;
		}
	;

opt_ptr_to_const
	: CONST
		{
		$$ = TRUE;
		}
	| /* null */
		{
		$$ = FALSE;
		}
	;

opt_array
	: ARRAY
		{
		$$ = TRUE;
		}
	| /* null */
		{
		$$ = FALSE;
		}
	;

opt_ptr
	: '*'
		{
		$$ = TRUE;
		}
	| /* null */
		{
		$$ = FALSE;
		}
	;

%%
char	*Prologue;
char	*Epilogue;
char	*ProgName;

usage()
{
	fprintf( stderr,
		"%s -m {switches,senders,receivers} [-p prologue] [-e epilogue]\n",
		ProgName
	);
}

main( int argc, char *argv[] )
{
	char	*arg;
	FILE	*fp;

	ProgName = *argv;
	for( argv++; arg = *argv; argv++ ) {
		if( arg[0] = '-' ) {
			switch( arg[1] ) {
			  case 'm':
				arg = *(++argv);
				if( strcmp("switches",arg) == 0 ) {
					Mode = SWITCHES;
				} else if( strcmp("senders",arg) == 0 ) {
					Mode = SENDERS;
				} else if( strcmp("receivers",arg) == 0 ) {
					Mode = RECEIVERS;
				} else {
					usage();
				}
				break;
			  case 'p':
				Prologue = *(++argv);
				break;
			  case 'e':
				Epilogue = *(++argv);
				break;
			  default:
				usage();
			}
		} else {
			usage();
		}
	}

	if( !Mode ) {
		usage();
	}

		/* Copy the prologue */
	if( fp = open_file(Prologue) ) {
		copy_file( fp, stdout );
	}

		/* Process the template file */
	yyparse();

		/* Copy the epilogue */
	if( fp = open_file(Epilogue) ) {
		copy_file( fp, stdout );
	}

	exit( 0 );
}

yyerror( char * s )
{
	fprintf( stderr, "%s on line %d\n", s, yyline );
	exit( 1 );
}

struct node *
mk_param_node( char *type, char *name,
	int is_const, int is_ptr, int is_const_ptr, int is_array,
	int is_in, int is_out )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = PARAM;
	answer->type_name = type;
	answer->id = name;

	answer->is_const = is_const;
	answer->is_const_ptr = is_const_ptr;
	answer->is_ptr = is_ptr;
	answer->is_array = is_array;
	answer->in_param = is_in;
	answer->out_param = is_out;

	answer->is_mapped = FALSE;	/* will set later if needed */

	answer->next = answer;
	answer->prev = answer;

	return answer;
}

struct node *
set_map_attr( struct node * simple )
{
	simple->is_mapped = TRUE;
	return simple;
}

struct node *
mk_func_node( char *type, char *name, struct node * p_list, int is_ptr, int extract )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = FUNC;
	answer->type_name = type;
	answer->id = name;
	answer->is_ptr = is_ptr;
	answer->extract = extract;
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

int
is_empty_list( struct node *list )
{
	return list == list->next;
}

void
output_vararg( struct node *n )
{
	assert( n->node_type == PARAM );
	printf( "\t\t%s = va_arg( ap, %s );\n", n->id, n->type_name );
}

void
output_param_node( struct node *n )
{
	assert( n->node_type == PARAM );

	if( n->is_const ) {
		printf( "const " );
	}

	printf( "%s ", n->type_name );

	if( n->is_ptr ) {
		printf( "*" );
	}

	if( n->is_const_ptr ) {
		printf( " const " );
	}

	printf( "%s", n->id );

	if( n->is_array ) {
		printf( "[]" );
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
output_switch_decl( struct node *dum )
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
output_local_call( char *id, struct node *list )
{
	struct node	*p;

	printf( "		rval = syscall( SYS_%s", id );
	if( !is_empty_list(list) ) {
		printf( ", " );
	}
	output_param_list( list );
	printf( " );\n" );
}

void
output_remote_call( char *id, struct node *list )
{
	struct node	*p;

	printf( "		rval = REMOTE_syscall( SYS_%s", id );
	if( !is_empty_list(list) ) {
		printf( ", " );
	}
	output_param_list( list );
	printf( " );\n" );
}

void
output_extracted_call( char *id, struct node *list )
{
	struct node	*p;

	printf( "		rval = %s(", mk_upper(id) );
	if( !is_empty_list(list) ) {
		printf( " " );
	}
	output_param_list( list );
	printf( " );\n" );
}

void
output_param_list( struct node *list )
{
	struct node	*p;

	for( p=list->next; p != list; p = p->next ) {
		if( p->is_mapped ) {
			printf( "user_%s", p->id );
		} else {
			printf( "%s", p->id );
		}
		if( p->next != list ) {
			printf( ", " );
		}
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
  Output code for one system call sender.
*/
void
output_sender( struct node *n )
{
	struct node *param_list = n->next;
	struct node *p;

	assert( n->node_type == FUNC );

	printf( "#if defined( SYS_%s )\n", n->id );
	printf( "	case SYS_%s:\n", n->id );
	printf( "	  {\n" );

	for( p=param_list->next; p != param_list; p = p->next ) {

			/* type_name **id; - '*' and '*' are optional */
		printf( "\t\t%s %s%s%s;\n",
			p->type_name,
			p->is_ptr ? "*" : "",
			p->is_array ? "*" : "",
			p->id
		);
	}
	printf( "\n" );

	printf( "\t\tCurrentSysCall = CONDOR_%s;\n\n", n->id  );

	for( p=param_list->next; p != param_list; p = p->next ) {
			/* id = va_arg( ap, type_name * ); - '*' is optional */
		printf( "\t\t%s = va_arg( ap, %s %s%s);\n",
			p->id,
			p->type_name,
			p->is_ptr ? "*" : "",
			p->is_array ? "*" : ""
		);
	}
	printf( "\n" );

	printf( "\t\txdr_syscall->x_op = XDR_ENCODE;\n" );
	printf( "\t\tassert(xdr_int(xdr_syscall,&CurrentSysCall) );\n" );

	for( p=param_list->next; p != param_list; p = p->next ) {
			/* assert( xdr_<type_name>(xdr_syscall,&<id>) ); */
		printf( "\t\tassert( xdr_%s(xdr_syscall,&%s) );\n",
			abbreviate(p->type_name),
			p->id 
		);
	}
	printf( "\n" );

	printf( "\t\txdr_syscall->x_op = XDR_DECODE;\n" );
	printf( "\t\tassert( xdrrec_skiprecord(xdr_syscall) );\n" );
	printf( "\t\tassert( xdr_int(xdr_syscall,&rval) );\n" );
	printf( "\t\tif( rval < 0 ) {\n" );
	printf( "\t\t\tassert( xdr_int(xdr_syscall,&terrno) );\n" );
	printf( "\t\t\terrno = terrno;\n" );
	printf( "\t\t}\n" );
	printf( "\t\tbreak;\n" );


	printf( "\t}\n" );
	printf( "#endif\n\n" );
}

/*
  Output code for one system call switch.
*/
void
output_switch( struct node *n )
{
	assert( n->node_type == FUNC );

	printf( "#if defined( SYS_%s )\n", n->id );
	if( n->is_ptr ) {
		printf( "%s *\n", n->type_name );
	} else {
		printf( "%s\n", n->type_name );
	}
	printf( "%s", n->id );
	output_switch_decl( n->next );
	printf( "{\n" );
	printf( "	int	rval;\n" );
	output_mapping( n->type_name, n->next );
	printf( "\n" );
	printf( "	if( LocalSysCalls() ) {\n" );
	if( n->extract ) {
		output_extracted_call( n->id, n->next );
	} else {
		output_local_call( n->id, n->next );
	}
	printf( "	} else {\n" );
	output_remote_call(  n->id, n->next );
	printf( "	}\n" );
	if( strcmp(n->type_name,"void") != 0 ) {
		printf( "\n" );
		if( strcmp(n->type_name,"int") == 0 ) {
			printf( "	return rval;\n" );
		} else if(n->is_ptr) {
			printf( "	return (%s *)rval;\n", n->type_name );
		} else {
			printf( "	return (%s)rval;\n", n->type_name );
		}
	}
	printf( "}\n" );
	printf( "#endif\n\n" );
}

FILE *
open_file( char *name )
{
	FILE	*fp;

	if( name ) {
		if( (fp = fopen(name,"r")) == NULL) {
			perror( name );
			exit( 1 );
		}
		return fp;
	}
	return NULL;
}

void
copy_file( FILE *in_fp, FILE *out_fp )
{
	char	buf[1024];


	while( fgets(buf,sizeof(buf),in_fp) ) {
		yyline += 1;
		fputs(buf,out_fp);
	}
}

char *
mk_upper( char *str )
{
	char	*src, *dst;
	static char buf[1024];

	src = str;
	dst = buf;
	while( *src ) {
		if( islower(*src) ) {
			*dst++ = toupper(*src++);
		} else {
			*dst++ = *str++;
		}
	}
	*dst = '\0';
	return buf;
}

char *
abbreviate( char *type_name )
{
	if( strncmp("struct",type_name,6) == MATCH ) {
		return type_name + 7;
	} else if( strcmp("long int",type_name) == MATCH ) {
		return "long";
	} else if( strcmp("unsigned int",type_name) == MATCH ) {
		return "u_int";
	} else if( strcmp("unsigned long",type_name) == MATCH ) {
		return "u_long";
	} else {
		return type_name;
	}
}
