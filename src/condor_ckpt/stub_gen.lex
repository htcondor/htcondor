%{
#include "scanner.h"
extern YYSTYPE yylval;
# define TYPE 257
# define ID 258
# define STRUCT 259
# define UNKNOWN 259

int grab(int type);
char * strdup( char *orig );

%}

STR [A-Za-z_][A-Za-z0-9_]+
%%
int			return grab( TYPE );
void		return grab( TYPE );
size_t		return grab( TYPE );
off_t		return grab( TYPE );
pid_t		return grab( TYPE );
char		return grab( TYPE );
"struct stat"		return grab( TYPE );
{STR}		return grab( ID );
"*"			return grab( '*' );
"("			return grab( '(' );
")"			return grab( ')' );
","			return grab( ',' );
";"			return grab( ';' );
[ \t\n]+	;/* eat up whitespace */
.			return grab( UNKNOWN );
%%
yywrap()
{
	return feof(stdin);
}

int
grab( int type )
{
	yylval.tok.val = strdup( (char *)yytext );
	yylval.tok.tok_type = type;
	return type;
}

char *
strdup( char *orig )
{
	char *answer;
	char *malloc();

	answer = malloc( strlen(orig) + 1 );
	strcpy( answer, orig );
	return answer;
}
