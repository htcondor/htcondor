/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/*
** This file implements the default configuration table which is read from
** the config file.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include "trace.h"
#include "files.h"
#include "debug.h"
#include "except.h"
#include "clib.h"
#include "condor_sys.h"
#include "condor_config.h"

#ifndef LINT
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
#endif LINT

BUCKET	*ConfigTab[TABLESIZE];

char	*expand_macro(), *lookup_macro(), *param();

char *get_arch();
char *get_op_sys();
#if defined(__STDC__)
void insert_context( char *name, char *value, CONTEXT *context );
#else
void insert_context();
#endif


extern int	ConfigLineNo;

init_config()
{
	 memset( (char *)ConfigTab, 0,sizeof(ConfigTab) ); 
}

#if !defined(USER_NAME)
#	define USER_NAME "condor"
#endif

void config( char* a_out_name, CONTEXT* context )
{
	struct passwd	*pw, *getpwnam();
	char			*ptr;
	char			*config_file, *tilde;
	int				testing, rval;
	char			hostname[1024];
	int				scm;
	char			*arch, *op_sys;
	char		*temp = NULL;

	/*
	** N.B. if we are using the yellow pages, system calls which are
	** not supported by either remote system calls or file descriptor
	** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
	*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
	if( (pw=getpwnam(USER_NAME)) == NULL ) {
		printf( "Can't find user \"%s\" in passwd file!\n", USER_NAME );
		exit( 1 );
	}
	(void)endpwent();
	(void)SetSyscalls( scm );

	tilde = strdup( pw->pw_dir );
	insert( "tilde", tilde, ConfigTab, TABLESIZE );
	free( tilde );

	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		fprintf( stderr, "gethostname failed, errno = %d\n", errno );
		exit( 1 );
	}

	if( ptr=(char *)strchr((const char *)hostname,'.') )
		*ptr = '\0';
	insert( "hostname", hostname, ConfigTab, TABLESIZE );

	/* Different cases for default configuration or server configuration */
	/* weiru */
	/* Test versions end in _t, prog name gets passed in */
	for( ptr=a_out_name; *ptr; ptr++ );
	if( strcmp("_t",ptr-2) == 0 ) {
		testing = 1;
	}
	else {
		testing = 0;
	}

	if( testing ) {
		config_file = CONFIG_TEST;
	} else {
		config_file = CONFIG;
	}

	rval = read_config( pw->pw_dir, config_file, context,
						ConfigTab, TABLESIZE, EXPAND_LAZY );

	if( rval < 0 ) {
		fprintf( stderr,
			"Configuration Error Line %d while processing config file %s/%s ",
			ConfigLineNo, pw->pw_dir, config_file );
		perror( "" );
		exit( 1 );
	}

	if( (config_file=param("LOCAL_CONFIG_FILE")) ) {
		pw->pw_dir[0] = '\0';	/* Name specified in global config file */
	} else {
		if( testing ) {		/* Default to: "~condor/condor_config_t.local" */
			config_file = strdup( LOCAL_CONFIG_TEST );
		} else {			/* Default to: "~condor/condor_config.local" */
			config_file = strdup( LOCAL_CONFIG );
		}
	}

	(void) read_config( pw->pw_dir, config_file, context,
						ConfigTab, TABLESIZE, EXPAND_LAZY );
	free( config_file );

	/* If ARCH is not defined in config file, then try to get value
	   using uname().  Note that the config file parameter overrides
	   the uname() value.  -Jim B. */

	if ((arch = param("ARCH")) == NULL) {
	  arch = get_arch();
	}

	/* Always insert Arch into the context (overriding the Arch expression
	   in the config file).  This special case is needed because ARCH and
	   Arch differ only by case, triggering a bug which makes it
	   impossible to override ARCH in the LOCAL_CONFIG_FILE.  -Jim B. */
	insert( "ARCH", arch, ConfigTab, TABLESIZE );
	if( context ) {
	  insert_context( "Arch", arch, context );
	}

	/* If OPSYS is not defined in config file, then try to get value
	   using uname().  Note that the config file parameter overrides
	   the uname() value.  -Jim B. */
	if ((op_sys = param("OPSYS")) == NULL) {
	  op_sys = get_op_sys();
	}

	/* Same as with Arch above.  -Jim B. */
	insert( "OPSYS", op_sys, ConfigTab, TABLESIZE );
	if( context ) {
	  insert_context( "OpSys", op_sys, context );
	}
}

int config_from_server( dir, a_out_name, context )
char*	dir; 
char    *a_out_name;
CONTEXT *context;
{
    struct passwd   *pw, *getpwnam();
    char            *ptr;
    char            *config_file, *tilde;
    int             testing, rval;
    char            hostname[1024];
    int             scm;
    char            *arch, *op_sys;

    /*
    ** N.B. if we are using the yellow pages, system calls which are
    ** not supported by either remote system calls or file descriptor
    ** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
    */
    scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
    if( (pw=getpwnam(USER_NAME)) == NULL ) {
        printf( "Can't find user \"%s\" in passwd file!\n", USER_NAME );
        exit( 1 );
    }
    (void)endpwent();
    (void)SetSyscalls( scm );

    tilde = strdup( pw->pw_dir );
    insert( "tilde", tilde, ConfigTab, TABLESIZE );
    free( tilde );

    if( gethostname(hostname,sizeof(hostname)) < 0 ) {
        fprintf( stderr, "gethostname failed, errno = %d\n", errno );
        exit( 1 );
    }

    if( ptr=(char *)strchr((const char *)hostname,'.') )
        *ptr = '\0';
    insert( "hostname", hostname, ConfigTab, TABLESIZE );

    /* Different cases for default configuration or server configuration */
    /* weiru */
	rval = read_config(dir, SERVER_CONFIG, context, ConfigTab, TABLESIZE,
					   EXPAND_LAZY);
	if(rval < 0)
	{
		return -1;
	}

	/* try to get a runtime value for our machine architecture,
           if we can't figure it out we default to value from config file */
    if( (arch = get_arch()) != NULL ) {
        insert( "ARCH", arch, ConfigTab, TABLESIZE );
        if( context ) {
            insert_context( "Arch", arch, context );
        }
    }

        /* Try to get a runtime value for our operating system,
           if we can't figure it out we default to value from config file */
    if( (op_sys = get_op_sys()) != NULL ) {
        insert( "OPSYS", op_sys, ConfigTab, TABLESIZE );
        if( context ) {
            insert_context( "OpSys", op_sys, context );
        }
    }
	return 0; 
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
char *
param( name )
char	*name;
{
	char *val = lookup_macro( name, ConfigTab, TABLESIZE );

	if( val == NULL ) {
		return( NULL );
	}
	return( expand_macro(val, ConfigTab, TABLESIZE) );
}

char *
macro_expand( str )
char *str;
{
	return( expand_macro(str, ConfigTab, TABLESIZE) );
}

/*
** Return non-zero iff the named configuration parameter contains the given
** pattern.  
*/
param_in_pattern( parameter, pattern )
char	*parameter;
char	*pattern;
{
	char	*argv[512];
	int		argc;
	char	*tmp;

		/* Look up the parameter and break its value into an argv */
	/* tmp = strdup( param(parameter) ); */
	tmp = param(parameter);
	mkargv( &argc, argv, tmp );

		/* Search for the given pattern */
	for( argc--; argc >= 0; argc-- ) {
		if( !strcmp(pattern,argv[argc]) ) {
			FREE( tmp );
			return 1;
		}
	}
	FREE( tmp );
	return 0;
}

/* uname() is POSIX, so this should work on all platforms.  -Jim */
#include <sys/utsname.h>

char *
get_arch()
{
	static struct utsname buf;

	if( uname(&buf) < 0 ) {
		return NULL;
	}

	return buf.machine;
}

char *
get_op_sys()
{
	static char	answer[1024];
	struct utsname buf;

	if( uname(&buf) < 0 ) {
		return NULL;
	}

	strcpy( answer, buf.sysname );
	strcat( answer, buf.release );
	return answer;
}

void
insert_context( name, value, context )
char *name;
char *value;
CONTEXT *context;
{
	char	line[ 1024 ];
	EXPR	*expr, *scan();

	(void)sprintf(line, "%s = \"%s\"", name, value);

	expr = scan( line );
	if( expr == NULL ) {
		EXCEPT("Can't parse expression \"%S\"", line );
	}

	store_stmt( expr, context );
}

void Config(char* MyName)
{
	struct passwd*		pwd;
	char*				config_location;
	int   free_location = 1;

	/* Weiru */
	if((pwd = getpwnam("condor")) == NULL)
	{
		EXCEPT( "condor not in passwd file" );
	}
	if(read_config(pwd->pw_dir, MASTER_CONFIG, NULL, ConfigTab, TABLESIZE,
				   EXPAND_LAZY) < 0)
	{
		config(MyName, NULL);
	}
	else
	{
   		config_location = param("CONFIG_FILE_LOCATION");
   		if(!config_location)
   		{
   			config_location = pwd->pw_dir;
			free_location = 0;
   		}
		if(config_from_server(config_location, MyName, NULL) < 0)
	    	{
			config(MyName, NULL);
		}
		if(free_location) {
			free (config_location); 
		}
	}
}	

