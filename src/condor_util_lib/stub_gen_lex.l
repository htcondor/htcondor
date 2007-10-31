%p 10000
%n 1500
%a 5200
%e 2500
%o 5000
%option noyywrap
%{
#include <string.h>
#include <ctype.h>
#include "scanner.h"
#include "y.tab.h"

#if defined(AIX32) && !defined(OBJ_DATA)
#	define OBJ_DATA(x) (x).obj	/* needed for gcc's toupper/tolower */
#endif

int		yyline = 1;
extern YYSTYPE yylval;

int grab(int type);

void display_token( int type );
void reset_yyline();


%}

%START C_Comment
STR [A-Za-z_][A-Za-z0-9_]*
WHITE [ \t\n]*
STRUCT struct[ \t\n]+
UNION union[ \t\n]+
UNSIGNED unsigned[ \t\n]+
%%
"/*"				BEGIN C_Comment;
<C_Comment>\n		yyline++;
<C_Comment>"*/"		BEGIN 0;
<C_Comment>.*		;
"//".*				;	/* C++ Style Comment */
^#.*				reset_yyline();
direct				return grab( DIRECT );
indirect			return grab( INDIRECT );
const				return grab( CONST );
void				return grab( TYPE_NAME );
char				return grab( TYPE_NAME );
short				return grab( TYPE_NAME );
int					return grab( TYPE_NAME );
long				return grab( TYPE_NAME );
float				return grab( TYPE_NAME );
{UNSIGNED}int		return grab( TYPE_NAME );
{UNSIGNED}long		return grab( TYPE_NAME );
{UNSIGNED}short		return grab( TYPE_NAME );
long{WHITE}int		return grab( TYPE_NAME );
long{WHITE}long		return grab( TYPE_NAME );
u_short				return grab( TYPE_NAME );
ulong_t				return grab( TYPE_NAME );
u_long				return grab( TYPE_NAME );
int64_t				return grab( TYPE_NAME );
size_t				return grab( TYPE_NAME );
ssize_t				return grab( TYPE_NAME );
off_t				return grab( TYPE_NAME );
offset_t			return grab( TYPE_NAME );
off64_t				return grab( TYPE_NAME );
dirent64_t			return grab( TYPE_NAME );
pid_t				return grab( TYPE_NAME );
mode_t				return grab( TYPE_NAME );
uid_t				return grab( TYPE_NAME );
gid_t				return grab( TYPE_NAME );
caddr_t				return grab( TYPE_NAME );
dev_t				return grab( TYPE_NAME );
sigset_t			return grab( TYPE_NAME );
fhandle_t			return grab( TYPE_NAME );
key_t				return grab( TYPE_NAME );
open_flags_t		return grab( TYPE_NAME );
fcntl_cmd_t			return grab( TYPE_NAME );
condor_signal_t		return grab( TYPE_NAME );
socklen_t			return grab( TYPE_NAME );
Psocklen_t			return grab( TYPE_NAME );
fd_set				return grab( TYPE_NAME );
nfds_t				return grab( TYPE_NAME );
time_t				return grab( TYPE_NAME );
MMAP_T				return grab( TYPE_NAME );
PROC				return grab( TYPE_NAME );
STARTUP_INFO		return grab( TYPE_NAME );
{STRUCT}stat		return grab( TYPE_NAME );
{STRUCT}statfs		return grab( TYPE_NAME );
{STRUCT}rusage		return grab( TYPE_NAME );
{STRUCT}msghdr		return grab( TYPE_NAME );
{STRUCT}sockaddr	return grab( TYPE_NAME );
{STRUCT}iovec		return grab( TYPE_NAME );
{STRUCT}sigaction	return grab( TYPE_NAME );
{STRUCT}itimerval	return grab( TYPE_NAME );
{STRUCT}timeval		return grab( TYPE_NAME );
{STRUCT}utimbuf		return grab( TYPE_NAME );
{STRUCT}timezone	return grab( TYPE_NAME );
{STRUCT}pollfd		return grab( TYPE_NAME );
{STRUCT}sigcontext	return grab( TYPE_NAME );
{STRUCT}sigstack	return grab( TYPE_NAME );
{STRUCT}rlimit		return grab( TYPE_NAME );
{STRUCT}export		return grab( TYPE_NAME );
{STRUCT}exportfsdata	return grab( TYPE_NAME );
{STRUCT}msqid_ds	return grab( TYPE_NAME );
{STRUCT}msgbuf		return grab( TYPE_NAME );
{STRUCT}sembuf		return grab( TYPE_NAME );
{STRUCT}utsname		return grab( TYPE_NAME );
{STRUCT}shmid_ds	return grab( TYPE_NAME );
{STRUCT}addressconf	return grab( TYPE_NAME );
{STRUCT}acl		return grab( TYPE_NAME );
{STRUCT}module		return grab( TYPE_NAME );
{STRUCT}stat64		return grab( TYPE_NAME );
{STRUCT}rlimit64	return grab( TYPE_NAME );
{STRUCT}dirent		return grab( TYPE_NAME );
{STRUCT}dirent64	return grab( TYPE_NAME );
{STRUCT}statfs64	return grab( TYPE_NAME );
{STRUCT}pollfd		return grab (TYPE_NAME );
{UNION}wait		return grab (TYPE_NAME );
ClassAd			return grab( TYPE_NAME );
"["{WHITE}"]"		return grab( ARRAY );
map					return grab( MAP );
map_name				return grab( MAP_NAME );
map_name_two				return grab( MAP_NAME_TWO );
file_table					return grab( FILE_TABLE );
in					return grab( IN );
out					return grab( OUT );
alloc				return grab( ALLOC );
null_alloc				return grab( NULL_ALLOC );
return				return grab( RETURN );
extract				return grab( EXTRACT );
dl_extract			return grab( DL_EXTRACT );
no_sys_chk			return grab( NO_SYS_CHK );
pseudo				return grab( PSEUDO );
not_supported		return grab( NOSUPP );
ignored				return grab( IGNORE );
local_name			return grab( LOCAL_NAME );
file_table_name			return grab( FILE_TABLE_NAME );
remote_name			return grab( REMOTE_NAME );
also_implements			return grab( ALSO_IMPLEMENTS );
sender_name			return grab( SENDER_NAME );
discard				return grab( DISCARD );
rdiscard			return grab( RDISCARD );
{STR}				return grab( IDENTIFIER );
"{"					return grab( '{' );
"}"					return grab( '}' );
"*"					return grab( '*' );
"&"					return grab( '&' );
"("					return grab( '(' );
")"					return grab( ')' );
","					return grab( ',' );
";"					return grab( ';' );
":"					return grab( ':' );
[ \t]+				;/* eat up whitespace */
\n					yyline += 1;	/* eat up newlines */
"..."					return grab( ELLIPSIS );
.					return grab( UNKNOWN );
%%

int
grab( int type )
{
#undef DEBUGGING
#if	defined(DEBUGGING)
	display_token( type );
#endif

	yylval.tok.val = malloc( strlen(yytext) + 1 );
	strcpy( yylval.tok.val, yytext );
	yylval.tok.tok_type = type;
	return type;
}

void
reset_yyline()
{
	char *ptr;

	for( ptr=(char *)yytext; !isdigit(*ptr); ptr++ )
		;
	yyline = atoi( ptr ) - 1;
}

void
display_token( int type )
{
	fprintf( stderr, "Token: " );
	switch( type ) {
	  case CONST:
		fprintf( stderr, "CONST" );
		break;
	  case TYPE_NAME:
		fprintf( stderr, "TYPE_NAME (%s)", (char *)yytext );
		break;
	  case ARRAY:
		fprintf( stderr, "ARRAY" );
		break;
	  case IDENTIFIER:
		fprintf( stderr, "ID (%s)", (char *)yytext );
		break;
	  case MAP:
		fprintf( stderr, "MAP" );
		break;
	  case IN:
		fprintf( stderr, "IN" );
		break;
	  case OUT:
		fprintf( stderr, "OUT" );
		break;
	  case BOTH:
		fprintf( stderr, "BOTH" );
		break;
	  case NOSUPP:
		fprintf( stderr, "NOSUPP" );
		break;
	  case IGNORE:
		fprintf( stderr, "IGNORE" );
		break;
	  case '(':
		fprintf( stderr, "(" );
		break;
	  case ')':
		fprintf( stderr, ")" );
		break;
	  case '{':
		fprintf( stderr, "{" );
		break;
	  case '}':
		fprintf( stderr, "}" );
		break;
	  case ',':
		fprintf( stderr, "," );
		break;
	  case '*':
		fprintf( stderr, "*" );
		break;
	  case '&':
		fprintf( stderr, "&" );
		break;
	  case ';':
		fprintf( stderr, ";" );
		break;
	  default:
		fprintf( stderr, "UNKNOWN (%s)", (char *)yytext );
		break;
	}
	fprintf( stderr, "\n" );
}
