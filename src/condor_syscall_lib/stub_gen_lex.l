%{
#include <stdlib.h>
#include "scanner.h"
#include "y.tab.h"

int		yyline = 1;
extern YYSTYPE yylval;

int grab(int type);
char * strdup( char *orig );
void display_token( int type );

%}

STR [A-Za-z_][A-Za-z0-9_]*
WHITE [ \t\n]*
STRUCT struct[ \t\n]+
UNSIGNED unsigned[ \t\n]+
%%
const				return grab( CONST );
void				return grab( TYPE_NAME );
char				return grab( TYPE_NAME );
short				return grab( TYPE_NAME );
int					return grab( TYPE_NAME );
long				return grab( TYPE_NAME );
{UNSIGNED}int		return grab( TYPE_NAME );
{UNSIGNED}long		return grab( TYPE_NAME );
long{WHITE}int		return grab( TYPE_NAME );
ulong_t				return grab( TYPE_NAME );
size_t				return grab( TYPE_NAME );
ssize_t				return grab( TYPE_NAME );
off_t				return grab( TYPE_NAME );
pid_t				return grab( TYPE_NAME );
mode_t				return grab( TYPE_NAME );
uid_t				return grab( TYPE_NAME );
gid_t				return grab( TYPE_NAME );
caddr_t				return grab( TYPE_NAME );
dev_t				return grab( TYPE_NAME );
sigset_t			return grab( TYPE_NAME );
fhandle_t			return grab( TYPE_NAME );
key_t				return grab( TYPE_NAME );
fd_set				return grab( TYPE_NAME );
{STRUCT}stat		return grab( TYPE_NAME );
{STRUCT}statfs		return grab( TYPE_NAME );
{STRUCT}rusage		return grab( TYPE_NAME );
{STRUCT}msghdr		return grab( TYPE_NAME );
{STRUCT}sockaddr	return grab( TYPE_NAME );
{STRUCT}iovec		return grab( TYPE_NAME );
{STRUCT}sigaction	return grab( TYPE_NAME );
{STRUCT}itimerval	return grab( TYPE_NAME );
{STRUCT}timeval		return grab( TYPE_NAME );
{STRUCT}timezone	return grab( TYPE_NAME );
{STRUCT}pollfd		return grab( TYPE_NAME );
{STRUCT}sockaddr	return grab( TYPE_NAME );
{STRUCT}sigcontext	return grab( TYPE_NAME );
{STRUCT}sigstack	return grab( TYPE_NAME );
{STRUCT}rlimit		return grab( TYPE_NAME );
{STRUCT}exportfsdata	return grab( TYPE_NAME );
{STRUCT}msqid_ds	return grab( TYPE_NAME );
{STRUCT}msgbuf		return grab( TYPE_NAME );
{STRUCT}sembuf		return grab( TYPE_NAME );
{STRUCT}utsname		return grab( TYPE_NAME );
{STRUCT}shmid_ds	return grab( TYPE_NAME );
{STRUCT}addressconf	return grab( TYPE_NAME );
"["{WHITE}"]"		return grab( ARRAY );
map					return grab( MAP );
extract				return grab( EXTRACT );
{STR}				return grab( IDENTIFIER );
"*"					return grab( '*' );
"("					return grab( '(' );
")"					return grab( ')' );
","					return grab( ',' );
";"					return grab( ';' );
":"					return grab( ':' );
[ \t]+				;/* eat up whitespace */
\n					yyline += 1;	/* eat up newlines */
.					return grab( UNKNOWN );
%%
yywrap()
{
	return feof(stdin);
}

int
grab( int type )
{
#undef DEBUGGING
#if	defined(DEBUGGING)
	display_token( type );
#endif

	yylval.tok.val = strdup( (char *)yytext );
	yylval.tok.tok_type = type;
	return type;
}

char *
strdup( char *orig )
{
	char *answer;

	answer = malloc( strlen(orig) + 1 );
	strcpy( answer, orig );
	return answer;
}

void
display_token( int type )
{
	switch( type ) {
	  case CONST:
		fprintf( stderr, "Grabbed CONST\n" );
		break;
	  case TYPE_NAME:
		fprintf( stderr, "Grabbed TYPE_NAME (%s)\n", (char *)yytext );
		break;
	  case ARRAY:
		fprintf( stderr, "Grabbed ARRAY\n" );
		break;
	  case IDENTIFIER:
		fprintf( stderr, "Grabbed ID (%s)\n", (char *)yytext );
		break;
	  case MAP:
		fprintf( stderr, "Grabbed MAP\n" );
		break;
	  case '(':
		fprintf( stderr, "Grabbed (\n" );
		break;
	  case ')':
		fprintf( stderr, "Grabbed )\n" );
		break;
	  case ',':
		fprintf( stderr, "Grabbed ,\n" );
		break;
	  case '*':
		fprintf( stderr, "Grabbed *\n" );
		break;
	  case ';':
		fprintf( stderr, "Grabbed ;\n" );
		break;
	  default:
		fprintf( stderr, "Grabbed (something)\n" );
		break;
	}
}
