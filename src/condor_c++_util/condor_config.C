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
** Origional author:  Anand Narayanan
** 
** Totally re-written and stream-lined on 7/10/97 by Derek Wright
** 
*/ 

/* 
  This file implements config(), function all daemons should call to
  configure themselves.  It takes just one argument: a pointer to the
  ClassAd to fill up with the expressions in the config_file.
  config() simply calls real_config() with the right parameters for
  the default behavior.  real_config() is also called by
  config_master(), which is used to configure the condor_master
  process with different file names.  

  In general, the config functions check an environment variable to
  find the location of the global config files.  If that doesn't
  exist, it looks in /etc/condor.  If the files aren't there, it try's
  ~condor/.  If none of the above locations contain a config file, the
  functions print error messages and exit
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include "debug.h"
#include "clib.h"
#include "condor_sys.h"
#include "condor_config.h"
#include "util_lib_proto.h"
#include "condor_attributes.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Function prototypes
int real_config(const char*, const char*, const char*, ClassAd*);
int Read_config(char*, ClassAd*, BUCKET**, int, int);
char *get_arch();
char *get_op_sys();
int SetSyscalls(int);
#if defined(LINUX) || defined(HPUX9)
    int gethostname(char*, unsigned int);
#else
    int gethostname(char*, int);
#endif


// External variables
extern BUCKET	*ConfigTab[];
extern int	ConfigLineNo;


// Global variables
BUCKET	*ConfigTab[TABLESIZE];


// Function implementations
void 
config(ClassAd *classAd)
{
	if( real_config("CONDOR_CONFIG", "condor_config", 
					"condor_config.local", classAd) ) {
		fprintf( stderr, "\nNeither the environment variable CONDOR_CONFIG,\n" );
		fprintf( stderr, "/etc/condor/, nor ~condor/ contain a condor_config file.\n" );
		fprintf( stderr, "Either set CONDOR_CONFIG to point to a valid config file,\n" );
		fprintf( stderr, "or put a \"condor_config\" file in %s or %s.\n", 
				 "/etc/condor", "~condor/" );
		fprintf( stderr, "Exiting.\n\n" );
		exit( 1 );
	}
}

void
config_master(ClassAd *classAd)
{
	int rval = real_config("CONDOR_CONFIG_MASTER", "condor_config.master", 
						   "condor_config.master.local", (ClassAd*)classAd);

	if( rval ) {
			// Trying to find things in with the .master names failed,
			// try the regular config files.  
		rval = real_config("CONDOR_CONFIG", "condor_config", 
						   "condor_config.local", classAd);
		if( rval ) {
				// Everything failed, give up.
			fprintf( stderr, "\nNeither the environment variables %s nor\n",
					 "CONDOR_CONFIG_MASTER" );
			fprintf( stderr, "%s, nor %s, nor %s contain\n",
					 "CONDOR_CONFIG", "/etc/condor/", "~condor/" );
			fprintf( stderr, "a %s or %s file.  Either set\n",
					 "condor_config", "condor_config.master" );
			fprintf( stderr, "%s or %s to point to a\n",	
					 "CONDOR_CONFIG", "CONDOR_CONFIG_MASTER" );
			fprintf( stderr, "valid config file, or put a \"%s\" file in\n", 
					 "condor_config" );
			fprintf( stderr, "/etc/condor or ~condor/ and try again.\n" );
			fprintf( stderr, "Exiting.\n\n" );
			exit( 1 );
		}
	}
}

int
real_config(const char *env_name, const char *file_name,
			const char *local_name, ClassAd *classAd)
{
	struct passwd	*pw, *getpwnam();
	char			*ptr;
	int				rval, fd;
	char			hostname[1024];
	int				scm;
	char			*arch, *op_sys;
	char			*env, *config_file = NULL, *tilde = NULL;
  
		/*
		  N.B. if we are using the yellow pages, system calls which are
		  not supported by either remote system calls or file descriptor
 		  mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
		*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	if( (pw=getpwnam( "condor" )) ) {
		tilde = strdup( pw->pw_dir );
	} 

		// Find location of condor_config file

	if( (env = getenv( env_name )) ) {
		config_file = strdup( env );
		if( (fd = open( config_file, O_RDONLY)) < 0 ) {
			fprintf( stderr, "File specified in %s environment ", env_name );
			fprintf( stderr, "variable:\n\"%s\" does not exist.", config_file ); 
			fprintf( stderr, "  Trying fall back options...\n" ); 
			free( config_file );
			config_file = NULL;
		} else {
			close( fd );
		}
	}
	if( ! config_file ) {
			// try /etc/condor/file_name
		config_file = (char *)malloc( (strlen(file_name) + 14) * sizeof(char) ); 
		config_file[0] = '\0';
		strcat( config_file, "/etc/condor/" );
		strcat( config_file, file_name );
		if( (fd = open( config_file, O_RDONLY )) > 0 ) {
			close( fd );
		} else {
			free( config_file );
			config_file = NULL;
		}
	}
	if( ! config_file && tilde ) {
			// try ~condor/file_name
		config_file = (char *)malloc( 
						 (strlen(tilde) + strlen(file_name) + 2) * sizeof(char) ); 
		config_file[0] = '\0';
		strcat( config_file, tilde );
		strcat( config_file, "/" );
		strcat( config_file, file_name );
		if( (fd = open( config_file, O_RDONLY)) < 0 ) {
			free( config_file );
			config_file = NULL;
		} else {
			close( fd );
		}
	}

	if( ! config_file ) {
		return(1);
	}

		// Build a hash table with entries for "tilde" and
		// "hostname". Note that "hostname" ends up as just the
		// machine name w/o the . separators

	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
		free( tilde );
		tilde = NULL;
	} else {
			// What about tilde if there's no ~condor? 
	}
  
	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		fprintf( stderr, "gethostname failed, errno = %d\n", errno );
		exit( 1 );
	}
  
	if( ptr=(char *)strchr((const char *)hostname,'.') )
		*ptr = '\0';
	insert( "hostname", hostname, ConfigTab, TABLESIZE );

		// Actually read in the file

	rval = Read_config( config_file, classAd, ConfigTab, TABLESIZE,
						EXPAND_LAZY );
	if( rval < 0 ) {
		fprintf( stderr,
				 "Configuration Error Line %d while processing config file %s ",
				 ConfigLineNo, config_file );
		perror( "" );
		exit( 1 );
	}
	free( config_file );
	
		// Try to find the local config file

	config_file=param( "LOCAL_CONFIG_FILE" );
	if( ! config_file && tilde ) {
			// If there's no LOCAL_CONFIG_FILE in the global config
			// file, try ~condor/local_name
		config_file = (char *)malloc( 
						 (strlen(tilde) + strlen(local_name) + 2) * sizeof(char) ); 
		config_file[0] = '\0';
		strcat( config_file, tilde );
		strcat( config_file, "/" );
		strcat( config_file, local_name );
		if( (fd = open( config_file, O_RDONLY)) < 0 ) {
			free( config_file );
			config_file = NULL;
		} else {
			close( fd );
		}
	}

	if( config_file) {
		rval = Read_config( config_file, classAd, ConfigTab, TABLESIZE, EXPAND_LAZY ); 

		if( rval < 0 ) {
			fprintf( stderr,
					 "Configuration Error Line %d while processing config file %s ",
					 ConfigLineNo, config_file );
			perror( "" );
			exit( 1 );
		}
		free( config_file );
	}

  /* If ARCH is not defined in config file, then try to get value
     using uname().  Note that the config file parameter overrides
     the uname() value.  -Jim B. */
  /* Jim's insertion into context changed to insertion into
     classAd -> N Anand */
  
	arch = param("ARCH");
	if( arch == NULL) {
		if( (arch = get_arch()) != NULL ) {
			insert( "ARCH", arch, ConfigTab, TABLESIZE );
			if(classAd)	{
				char line[80];
				sprintf( line, "%s=%s", ATTR_ARCH, arch );
				classAd->Insert(line);
			}
		}
	} else {
		free( arch );
	}

		/* If OPSYS is not defined in config file, then try to get value
		   using uname().  Note that the config file parameter overrides
		   the uname() value.  -Jim B. */
		/* Jim's insertion into context changed to insertion into
		   classAd -> N Anand */
	op_sys = param("OPSYS");
	if( op_sys == NULL ) {
		if( (op_sys = get_op_sys()) != NULL ) {
			insert( "OPSYS", op_sys, ConfigTab, TABLESIZE );
			if(classAd) {
				char line[80];
				sprintf( line, "%s=%s", ATTR_OPSYS, op_sys );
				classAd->Insert(line);
			}
		}
	} else {
		free( op_sys );
	}

	(void)endpwent();
	(void)SetSyscalls( scm );
	return 0;
}

void
init_config()
{
	 memset( (char *)ConfigTab, 0,sizeof(ConfigTab) ); 
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
char *
param( char *name )
{
	char *val = lookup_macro( name, ConfigTab, TABLESIZE );

	if( val == NULL ) {
		return( NULL );
	}
	return( expand_macro(val, ConfigTab, TABLESIZE) );
}

char *
macro_expand( char *str )
{
	return( expand_macro(str, ConfigTab, TABLESIZE) );
}

/*
** Return non-zero iff the named configuration parameter contains the given
** pattern.  
*/
boolean( char *parameter, char *pattern )
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

#if defined(__cplusplus)
}
#endif
