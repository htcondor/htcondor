%{
#include <stdlib.h>
#include "scanner.h"
#include "y.tab.h"

int		yyline = 1;
int		CurLineNo;
extern YYSTYPE yylval;

int grab(int type);
char * strdup( char *orig );

%}

STR [A-Za-z_][A-Za-z0-9_]*
%%
int			return grab( TYPE_NAME );
long		return grab( TYPE_NAME );
void		return grab( TYPE_NAME );
size_t		return grab( TYPE_NAME );
off_t		return grab( TYPE_NAME );
pid_t		return grab( TYPE_NAME );
mode_t		return grab( TYPE_NAME );
uid_t		return grab( TYPE_NAME );
gid_t		return grab( TYPE_NAME );
caddr_t		return grab( TYPE_NAME );
char		return grab( TYPE_NAME );
"struct stat"		return grab( TYPE_NAME );
"struct statfs"		return grab( TYPE_NAME );
map			return grab( MAP );
{STR}		return grab( IDENTIFIER );
"*"			return grab( '*' );
"("			return grab( '(' );
")"			return grab( ')' );
","			return grab( ',' );
";"			return grab( ';' );
[ \t]+		;/* eat up whitespace */
\n			yyline += 1;	/* eat up newlines */
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
	CurLineNo = yyline;
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
