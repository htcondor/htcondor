#ifndef _STRING
#define _STRING

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || (__cplusplus)
int mkargv ( int *argc, char *argv[], char *line );
void lower_case ( char *str );
int blankline ( const char *str );
char * getline ( FILE *fp );
char * ltrunc ( register char *str );
#else
int mkargv();
void lower_case();
int blankline();
char * getline();
char * ltrunc ();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _STRING */
