%token TYPE_NAME
%token CONST
%token IDENTIFIER
%token UNKNOWN
%token MAP
%token EXTRACT
%token PSEUDO
%token ARRAY
%token IN
%token OUT
%token BOTH
%token ALLOC
%token NOSUPP

%type <node> stub_spec param_list param simple_param map_param
%type <node> stub_body action_func_list
%type <node> action_param action_param_list action_func xfer_func alloc_func
%type <tok> TYPE_NAME CONST IDENTIFIER UNKNOWN MAP EXTRACT ARRAY opt_mult
%type <bool> opt_const opt_ptr opt_ptr_to_const opt_array
%type <bool> opt_reference
%type <param_mode> use_type

%{
#include <stdio.h>
#include <assert.h>
#include <assert.h>
#include "scanner.h"
extern int yyline;
struct node *
mk_func_node( char *type, char *name, struct node * p_list,
	int is_ptr, struct node *action_func_list );
struct node * mk_xfer_func( char *xdr_func, struct node *param_list,
	int in, int out );
struct node * mk_alloc_func( struct node *param_list );
struct node * mk_param_node( char *type, char *name,
	int is_const, int is_ptr, int is_const_ptr, int is_array,
	int is_in, int is_out );
struct node * mk_action_param_node( char *name, int is_ref, char *mult );
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
void Trace( char *msg );
char * find_type_name( char *param_name, struct node *param_list );
int has_out_params( struct node *action_func_list );
char *malloc( unsigned int );

#define MATCH 0 /* for strcmp() */

#define SWITCHES 1
#define SENDERS 2
#define RECEIVERS 3

int Supported = TRUE;
int	Mode = 0;
int ErrorEncountered = 0;
int IsExtracted = FALSE;
int IsPseudo = FALSE;


#if 0
input
	: /* empty */
	| input stub_list
	| input non_support_list
	;

#endif


%}

%%

input
	: stub_spec
	| non_support_list
	| input stub_spec
	| input non_support_list
	| error stub_body
	;

non_support_list
	: nosupp_begin stub_spec
		{
		Trace( "non_support_list( 1 )" );
		Supported = TRUE;
		}
	| nosupp_begin '{' stub_list '}'
		{
		Trace( "non_support_list( 2 )" );
		Supported = TRUE;
		}
	;

nosupp_begin
	: NOSUPP
		{
		Trace( "nosupp_begin" );
		Supported = FALSE;
		}
	;

stub_list
	: /* empty */
	| stub_spec stub_list
	| error stub_body
	;

stub_spec
	: TYPE_NAME opt_ptr IDENTIFIER '(' param_list ')' specials stub_body
		{
		  $$ = mk_func_node( $1.val, $3.val, $5, $2, $8 );
		  if( !ErrorEncountered ) {
			switch( Mode ) {
			  case SWITCHES:
				output_switch( $$ );
				break;
			  case SENDERS:
				output_sender( $$ );
				break;
			  case RECEIVERS:
				output_receiver( $$ );
				break;
			}
		  }
		}
	;

stub_body
	: ';'
		{
		Trace( "Empty stub body" );
		$$ = mk_list();
		}
	| '{' action_func_list '}'
		{
		Trace( "Non-Empty stub body" );
		$$ = $2;
		}
	;

action_func_list
	:	/* empty */
		{
		Trace( "Empty action_func_list" );
		$$ = mk_list();
		}
	|	action_func_list action_func
		{
		Trace( "added action_func to action_func_list" );
		$$ = insert_node( $1, $2 );
		}
	;

action_func
	: xfer_func
		{
		$$ = $1;
		}
	| alloc_func
		{
		$$ = $1;
		}
	;

xfer_func
	: use_type IDENTIFIER '(' action_param_list ')' ';'
		{
		Trace( "action_func" );
		$$ = mk_xfer_func( $2.val, $4, $1.in, $1.out );
		}
	;

alloc_func
	: ALLOC '(' action_param_list ')' ';'
		{
		Trace( "alloc_func" );
		$$ = mk_alloc_func( $3 );
		}
	;

action_param_list
	: action_param
		{
		Trace( "action_param_list (1)" );
		$$ = insert_node( mk_list(), $1 );
		}
	| action_param_list ',' action_param
		{
		Trace( "action_param_list (2)" );
		$$ = append_node( $1, $3 );
		}
	;

action_param
	: opt_reference IDENTIFIER opt_mult
		{
		Trace( "action_param" );
		$$ = mk_action_param_node( $2.val, $1, $3.val );
		}
	;

opt_mult
	: '*' IDENTIFIER
		{
		Trace( "opt_mult" );
		$$.val = $2.val;
		}
	| /* null */
		{
		$$.val =  "";
		}
	;

opt_reference
	: '&'
		{ $$ = TRUE; }
	| /* null */
		{ $$ = FALSE; }
	;

specials
	: /* empty */
	| ':' special_list
	;

special_list
	: pseudo_or_extract
	| special_list ',' pseudo_or_extract 
	;

pseudo_or_extract
	:  PSEUDO
		{
		Trace( "pseudo_or_extract (1)" );
		IsPseudo = TRUE;
		}
	|  EXTRACT
		{
		Trace( "pseudo_or_extract (2)" );
		IsExtracted = TRUE;
		}
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
		Trace( "use_type (IN)" );
		$$.in = TRUE;
		$$.out = FALSE;
		}
	| OUT
		{
		Trace( "use_type (OUT)" );
		$$.in = FALSE;
		$$.out = TRUE;
		}
	| BOTH
		{
		Trace( "use_type (BOTH)" );
		$$.in = TRUE;
		$$.out = TRUE;
		}
	| /* null */
		{
		Trace( "use_type (NEITHER)" );
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

	if( ErrorEncountered ) {
		exit( 1 );
	} else {
		exit( 0 );
	}
}

yyerror( char * s )
{
	fprintf( stderr, "%s at \"%s\" on line %d\n", s, yylval.tok.val, yyline );
	ErrorEncountered = TRUE;

		/* make sure any resulting file won't compile */
	printf( "------------  Has Errors --------------\n" );
}

struct node *
mk_param_node( char *type, char *name,
	int is_const, int is_ptr, int is_const_ptr, int is_array,
	int is_in, int is_out )
{
	struct node	*answer;

	answer = (struct node *)calloc( 1, sizeof(struct node) );
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
mk_action_param_node( char *name, int is_ref, char *mult )
{
	struct node	*answer;
	char	*name_holder;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = ACTION_PARAM;
	if( mult[0] ) {
		name_holder = malloc( strlen(name) + strlen(mult) +4 );
		sprintf( name_holder, "%s * %s", name, mult );
		answer->id = name_holder;
	} else {
		answer->id = name;
	}
	answer->is_ref = is_ref;

	return answer;
}

struct node *
set_map_attr( struct node * simple )
{
	simple->is_mapped = TRUE;
	return simple;
}

struct node *
mk_func_node( char *type, char *name, struct node * p_list,
	int is_ptr, struct node *action_func_list )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = FUNC;
	answer->type_name = type;
	answer->id = name;
	answer->is_ptr = is_ptr;
	answer->extract = IsExtracted;
	IsExtracted = FALSE;
	answer->pseudo = IsPseudo;
	IsPseudo = FALSE;
	answer->param_list = p_list;
	answer->action_func_list = action_func_list;

	return answer;
}

struct node *
mk_xfer_func( char *xdr_func, struct node *param_list, int is_in, int is_out )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = XFER_FUNC;
	answer->XDR_FUNC = xdr_func;
	answer->param_list = param_list;
	answer->in_param = is_in;
	answer->out_param = is_out;

	return answer;
}

struct node *
mk_alloc_func( struct node *param_list )
{
	struct node	*answer;

	answer = (struct node *)malloc( sizeof(struct node) );
	answer->node_type = ALLOC_FUNC;
	answer->param_list = param_list;

	return answer;
}

/*
  Insert a new node at the beginning of a list.
*/
struct node *
insert_node( struct node * list, struct node * new_elem )
{

	new_elem->next = list->next;
	new_elem->prev = list;
	list->next->prev = new_elem;
	list->next = new_elem;

	return list;
}

/*
  Append a new node to the end of a list.
*/
struct node *
append_node( struct node * list, struct node * new_elem )
{

	new_elem->next = list;
	new_elem->prev = list->prev;
	list->prev->next = new_elem;
	list->prev = new_elem;

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

	printf( "		rval = REMOTE_syscall( CONDOR_%s", id );
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
	answer->node_type = DUMMY;
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
output_mapping( char *func_type_name, int is_ptr,  struct node *list )
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
		printf( "\tint use_local_access = FALSE;\n" );
		printf( "\n" );
	}

	for( n = list->next; n != list; n = n->next ) {
		if( n->is_mapped ) {
			printf( "	if( (user_%s=MapFd(%s)) < 0 ) {\n", n->id, n->id );
			printf( "		return (%s%s)-1;\n",
				func_type_name,
				is_ptr ? " *" : ""
			);
			printf( "	}\n" );
			printf( "\tif( LocalAccess(%s) ) {\n", n->id );
			printf( "\t\tuse_local_access = TRUE;\n" );
			printf( "\t}\n" );
		}
	}

	printf( "\n" );
	if( found_mapping ) {
		printf( "	if( LocalSysCalls() || use_local_access ) {\n" );
	} else {
		printf( "	if( LocalSysCalls() ) {\n" );
	}
}

/*
  Output code for one system call receiver.
*/
void
output_receiver( struct node *n )
{
	struct node *param_list = n->param_list;
	struct node *p, *q;
	struct node *var, *size;

	assert( n->node_type == FUNC );

	if( !n->pseudo ) {
		printf( "#if defined( SYS_%s )\n", n->id );
	}

	printf( "	case CONDOR_%s:\n", n->id );
	printf( "	  {\n" );

		/* output a local variable decl for each param of the sys call */
	for( p=param_list->next; p != param_list; p = p->next ) {
		printf( "\t\t%s %s%s%s;\n",
			p->type_name,
			p->is_ptr ? "*" : "",
			p->is_array ? "*" : "",
			p->id
		);
	}
	printf( "\t\tint terrno;\n" );
	printf( "\n" );


		/*
		Receive all call by value parameters - these must be IN parameters
		to the syscall, since they cannot return a value to the
		calling routine.
		*/
	for( p=param_list->next; p != param_list; p = p->next ) {
			/* assert( xdr_<type_name>(xdr_syscall,&<id>) ); */
		if( p->is_ptr || p->is_array ) {
			continue;
		}
		printf( "\t\tassert( xdr_%s(xdr_syscall,&%s) );\n",
			abbreviate(p->type_name),
			p->id 
		);
	}

		/*
		Allocate space for pointer type parameters.
		*/
	for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type != ALLOC_FUNC ) {
			continue;
		}
		var = p->param_list->next;
		size = var->next;
		printf( "\t\t%s = (%s)malloc( (unsigned)%s );\n",
			var->id,
			find_type_name( var->id, param_list ),
			size->id
		);
	}

		/*
		Receive other IN parameters - these are the ones with an
		IN action function defined in the template file.
		*/
	for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type != XFER_FUNC || p->in_param == FALSE ) {
			continue;
		}
		printf( "\t\tassert( %s(xdr_syscall,", p->XDR_FUNC );
		for( q=p->param_list->next; q->node_type != DUMMY; q = q->next ) {
			assert( q->node_type == ACTION_PARAM );
			printf( "%s%s%s", 
				q->is_ref ? "&" : "",
				q->id,
				q->next->node_type == DUMMY ? "" : ", "
			);
		}
		printf( ") );\n" );
	}

		/* Invoke the system call */
	printf( "\n" );
	printf( "\t\terrno = 0;\n" );
	if( !Supported  ) {
		printf( "\t\trval = CONDOR_NotSupported( CONDOR_%s );\n", n->id  );
	} else {
		printf( "\t\trval = %s%s( ",
			n->pseudo ? "pseudo_" : "",
			n->id
		);
		for( p=param_list->next; p != param_list; p = p->next ) {
			printf( "%s%s ",
				p->id,
				p->next == param_list ? "" : ","
			);
		}
		printf( ");\n" );
	}
	printf( "\t\tterrno = errno;\n" );
	printf( "\t\tdprintf( D_SYSCALLS, \"rval = %%d, errno = %%d\\n\", rval, terrno );\n" );
	printf( "\n" );


		/* Send system call return value, and errno if needed */
	printf( "\t\txdr_syscall->x_op = XDR_ENCODE;\n" );
	printf( "\t\tassert( xdr_int(xdr_syscall,&rval) );\n" );
	printf( "\t\tif( rval < 0 ) {\n" );
	printf( "\t\t\tassert( xdr_int(xdr_syscall,&terrno) );\n" );
	printf( "\t\t}\n" );

		/*
		Send out results in any OUT parameters - these are the ones with an
		OUT action function defined in the template file.
		*/
	if( has_out_params(n->action_func_list) ) {
		printf( "\t\tif( rval >= 0 ) {\n" );
		for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
			if( p->node_type != XFER_FUNC || p->out_param == FALSE ) {
				continue;
			}
			printf( "\t\t\tassert( %s(xdr_syscall,", p->XDR_FUNC );
			for( q=p->param_list->next; q->node_type != DUMMY; q = q->next ) {
				assert( q->node_type == ACTION_PARAM );
				printf( "%s%s%s", 
					q->is_ref ? "&" : "",
					q->id,
					q->next->node_type == DUMMY ? "" : ", "
				);
			}
			printf( ") );\n" );
		}
		printf( "\t\t}\n" );
	}

		/*
		DeAllocate space for pointer type parameters.
		*/
	for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type != ALLOC_FUNC ) {
			continue;
		}
		var = p->param_list->next;
		printf( "\t\tfree( (char *)%s );\n", var->id );
	}

	printf( "\t\tassert( xdrrec_endofrecord(xdr_syscall,TRUE) );;\n" );
	printf( "\t\treturn 0;\n" );


	printf( "\t}\n" );

	if( n->pseudo ) {
		printf( "\n" );
	} else {
		printf( "#endif\n\n" );
	}
}

/*
  Output code for one system call sender.
*/
void
output_sender( struct node *n )
{
	struct node *param_list = n->param_list;
	struct node *p, *q;

	assert( n->node_type == FUNC );

	printf( "	case CONDOR_%s:\n", n->id );
	printf( "	  {\n" );

		/* output a local variable decl for each param of the sys call */
	for( p=param_list->next; p != param_list; p = p->next ) {
		printf( "\t\t%s %s%s%s;\n",
			p->type_name,
			p->is_ptr ? "*" : "",
			p->is_array ? "*" : "",
			p->id
		);
	}
	printf( "\n" );

		/* Set up system call number */
	printf( "\t\tCurrentSysCall = CONDOR_%s;\n\n", n->id  );

		/* Grab values of local variables using varargs routines */
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

		/* Send system call number */
	printf( "\t\txdr_syscall->x_op = XDR_ENCODE;\n" );
	printf( "\t\tassert(xdr_int(xdr_syscall,&CurrentSysCall) );\n" );

		/*
		Send all call by value parameters - these must be IN parameters
		to the syscall, since they cannot return a value to the
		calling routine.
		*/
	for( p=param_list->next; p != param_list; p = p->next ) {
			/* assert( xdr_<type_name>(xdr_syscall,&<id>) ); */
		if( p->is_ptr || p->is_array ) {
			continue;
		}
		printf( "\t\tassert( xdr_%s(xdr_syscall,&%s) );\n",
			abbreviate(p->type_name),
			p->id 
		);
	}

		/*
		Send other IN parameters - these are the ones with an
		IN action function defined in the template file.
		*/
	for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type != XFER_FUNC || p->in_param == FALSE ) {
			continue;
		}
		printf( "\t\tassert( %s(xdr_syscall,", p->XDR_FUNC );
		for( q=p->param_list->next; q->node_type != DUMMY; q = q->next ) {
			assert( q->node_type == ACTION_PARAM );
			printf( "%s%s%s", 
				q->is_ref ? "&" : "",
				q->id,
				q->next->node_type == DUMMY ? "" : ", "
			);
		}
		printf( ") );\n" );
	}

		/* Complete the XDR record, and flush */
	printf( "\t\tassert( xdrrec_endofrecord(xdr_syscall,TRUE) );\n\n" );

	printf( "\t\txdr_syscall->x_op = XDR_DECODE;\n" );
	printf( "\t\tassert( xdrrec_skiprecord(xdr_syscall) );\n" );
	printf( "\t\tassert( xdr_int(xdr_syscall,&rval) );\n" );
	printf( "\t\tif( rval < 0 ) {\n" );
	printf( "\t\t\tassert( xdr_int(xdr_syscall,&terrno) );\n" );
	printf( "\t\t\terrno = terrno;\n" );
	printf( "\t\t\tbreak;\n" );
	printf( "\t\t}\n" );

		/*
		Gather up results in any OUT parameters - these are the ones with an
		OUT action function defined in the template file.
		*/
	for( p=n->action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type != XFER_FUNC || p->out_param == FALSE ) {
			continue;
		}
		printf( "\t\tassert( %s(xdr_syscall,", p->XDR_FUNC );
		for( q=p->param_list->next; q->node_type != DUMMY; q = q->next ) {
			assert( q->node_type == ACTION_PARAM );
			printf( "%s%s%s", 
				q->is_ref ? "&" : "",
				q->id,
				q->next->node_type == DUMMY ? "" : ", "
			);
		}
		printf( ") );\n" );
	}

	printf( "\t\tbreak;\n" );


	printf( "\t}\n" );
	printf( "\n" );
}

/*
  Output code for one system call switch.
*/
void
output_switch( struct node *n )
{
	assert( n->node_type == FUNC );

	if( !n->pseudo ) {
		printf( "#if defined( SYS_%s )\n", n->id );
	}
	if( n->is_ptr ) {
		printf( "%s *\n", n->type_name );
	} else {
		printf( "%s\n", n->type_name );
	}
	printf( "%s", n->id );
	output_switch_decl( n->param_list );
	printf( "{\n" );
	printf( "	int	rval;\n" );
	output_mapping( n->type_name, n->is_ptr, n->param_list );
	if( n->extract ) {
		output_extracted_call( n->id, n->param_list );
	} else {
		output_local_call( n->id, n->param_list );
	}
	printf( "	} else {\n" );
	output_remote_call(  n->id, n->param_list );
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

	if( n->pseudo ) {
		printf( "\n" );
	} else {
		printf( "#endif\n\n" );
	}
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

void
Trace( char *msg )
{
#define TRACE 0
#if TRACE == 0		/* keep quiet */
	return;
#elif TRACE == 1	/* trace info to the terminal */
	fprintf( stderr, "Production: \"%s\"\n", msg );
#elif TRACE == 2	/* intermix trace info with generated code */
	printf( "Production: \"%s\"\n", msg );
#endif
}

char *
find_type_name( char *param_name, struct node *param_list )
{
	struct node	*p;
	static char answer[1024];

	for( p=param_list->next; p->node_type != DUMMY; p = p->next ) {
		if( strcmp(param_name,p->id) == MATCH ) {
			if( p->is_ptr || p->is_array ) {
				sprintf( answer, "%s *", p->type_name );
			} else {
				sprintf( answer, "%s", p->type_name );
			}
			return answer;
		}
	}
	assert( FALSE );
}


int
has_out_params( struct node *action_func_list )
{
	struct node	*p;

	for( p=action_func_list->next; p->node_type != DUMMY; p = p->next ) {
		if( p->node_type == XFER_FUNC && p->out_param == TRUE ) {
			return TRUE;
		}
	}
	return FALSE;
}
