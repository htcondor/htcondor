#ifndef _STRING
#define _STRING

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || (__cplusplus)
int stricmp ( const char *s1, const char *s2 );
int strincmp ( const char *s1, const char *s2, int );
int mkargv ( int *argc, char *argv[], char *line );
void lower_case ( char *str );
int blankline ( const char *str );
char * getline ( FILE *fp );
#else
int stricmp();
int strincmp();
int mkargv();
void lower_case();
int blankline();
char * getline();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _STRING */
