/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/* 

  This file implements config(), the function all daemons call to
  configure themselves.  It takes up to two arguments: a pointer to a
  ClassAd to fill up with the expressions in the config_file, and
  optionally a pointer to a string containing the hostname that should
  be inserted for $(HOSTNAME).

  When looking the global config file, config() checks the
  "CONDOR_CONFIG" environment variable to find its location.  If that
  doesn't exist, it looks in /etc/condor.  If the condor_config isn't
  there, it try's ~condor/.  If none of the above locations contain a
  config file, config() prints an error message and exits.

  The root config file is found in the same way, except no environment
  variable is checked.  

  In each "global" config file, a list of "local" config files can be
  specified.  Each file given in the list is read and processed in
  order.  These lists can be used to specify both platform-specific
  config files and machine-specific config files, in addition to a
  single, pool-wide, platform-independent config file.

*/

#include "condor_common.h"
#include "debug.h"
#include "clib.h"
#include "condor_sys.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "my_arch.h"
#include "condor_version.h"

extern "C" {
	
// Function prototypes
void real_config(ClassAd *classAd, char* host, int wantsQuiet);
int Read_config(char*, ClassAd*, BUCKET**, int, int);
int SetSyscalls(int);
char* find_global();
char* find_global_root();
char* find_file(const char*, const char*);
void init_tilde();
void fill_attributes();
void check_domain_attributes();
void init_config();
void clear_config();
void reinsert_specials(char*);
void process_file(char*, char*, char*, ClassAd*);
void process_locals( char*, char*, ClassAd*);
void check_params();

// External variables
extern int	ConfigLineNo;
extern char* mySubSystem;

// Global variables
BUCKET	*ConfigTab[TABLESIZE];
static char* tilde = NULL;

static char* _FileName_ = __FILE__;

// Function implementations

void
config_fill_ad( ClassAd* ad )
{
	char		buffer[512];
	char 		*tmp;
	char		*expr;
	StringList	reqdExprs;
	Source		src;
	ExprTree	*exprTree;
	
	if( !ad ) return;
	sprintf (buffer, "%s_EXPRS", mySubSystem);
	if( !( tmp = param (buffer) ) ) return;

	reqdExprs.initializeFromString (tmp);	
	free (tmp);

	reqdExprs.rewind();
	while ((tmp = reqdExprs.next())) {
		if( ( expr = param(tmp) ) == NULL ) continue;
		src.SetSource( expr );
		if( !src.ParseExpression( exprTree ) ) {
			EXCEPT( "Error parsing: %s\n", expr );
		}
		ad->Insert( tmp, exprTree );
		free (expr);
	}
	
	/* Insert the version into the ClassAd */
	ad->InsertAttr( ATTR_VERSION, CondorVersion() );
}


void
config( ClassAd* classAd, int wantsQuiet )
{
	real_config( classAd, NULL, wantsQuiet );
}


void
config_host( ClassAd* classAd, char* host )
{
	real_config( classAd, host, 0 );
}


void
real_config(ClassAd *classAd, char* host, int wantsQuiet)
{
	char		*config_file, *tmp;
	int			scm, rval;
	StringList	*local_files;

	static int first_time = TRUE;
	if( first_time ) {
		first_time = FALSE;
		init_config();
	} else {
			// Clear out everything in our config hash table so we can
			// rebuild it from scratch.
		clear_config();
	}

		/*
		  N.B. if we are using the yellow pages, system calls which are
		  not supported by either remote system calls or file descriptor
 		  mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
		*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

		// Try to find user "condor" in the passwd file.
	init_tilde();

		// Insert an entry for "tilde", (~condor)
	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
	} else {
			// What about tilde if there's no ~condor? 
	}

		// Insert some default values for attributes we want even if
		// they're not defined in the config files: ARCH and OPSYS.
		// We also want to insert the special "SUBSYSTEM" macro here. 
		// We do this now since if they are defined in the config
		// files, these values will get overridden.  However, we want
		// them defined to begin with so that people can use them in
		// the global config file to specify the location of
		// platform-specific config files, etc.  -Derek Wright 6/8/98
		// Moved all the domain-specific stuff to a seperate function
		// since we might not know our full hostname yet. -Derek 10/20/98
	fill_attributes();

		// Try to find the global config file
	if( ! (config_file = find_global()) ) {
		if( wantsQuiet ) {
			fprintf( stderr, "Condor error: can't find config file.\n" ); 
			exit( 1 );
		}
		fprintf(stderr,"\nNeither the environment variable CONDOR_CONFIG,\n" );
		fprintf(stderr,"/etc/condor/, nor ~condor/ contain a condor_config "
				"file.\n" );
		fprintf( stderr,"Either set CONDOR_CONFIG to point to a valid config "
				"file,\n" );
		fprintf( stderr,"or put a \"condor_config\" file in %s or %s.\n", 
				 "/etc/condor", "~condor/" );
		fprintf( stderr, "Exiting.\n\n" );
		exit( 1 );
	}

		// Read in the global file
	process_file( config_file, "global config file", NULL, classAd );
	free( config_file );

		// Insert entries for "hostname" and "full_hostname".  We do
		// this here b/c we need these macros defined so that we can
		// find the local config file if that's defined in terms of
		// hostname or something.  However, we do this after reading
		// the global config file so people can put the
		// DEFAULT_DOMAIN_NAME parameter somewhere if they need it. 
		// -Derek Wright <wright@cs.wisc.edu> 5/11/98
	if( host ) {
		insert( "hostname", host, ConfigTab, TABLESIZE );
	} else {
		insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );
	}
	insert( "full_hostname", my_full_hostname(), ConfigTab, TABLESIZE );

		// Also insert tilde since we don't want that over-written.
	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
	}

		// Read in the LOCAL_CONFIG_FILE as a string list and process
		// all the files in the order they are listed.
	process_locals( "LOCAL_CONFIG_FILE", host, classAd );
			
		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Try to find and read the global root config file
	if( config_file = find_global_root() ) {
		process_file( config_file, "global root config file", host, classAd );
		free( config_file );

			// Re-insert the special macros.  We don't want the user
			// to override them, since it's not going to work.
		reinsert_specials( host );

			// Read in the LOCAL_ROOT_CONFIG_FILE as a string list and
			// process all the files in the order they are listed.
		process_locals( "LOCAL_ROOT_CONFIG_FILE", host, classAd );
	}

		// Now that we're done reading files, if DEFAULT_DOMAIN_NAME
		// is set, we need to re-initilize my_full_hostname(). 
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		free( tmp );
		init_full_hostname();
	}

		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Make sure our FILESYSTEM_DOMAIN and UID_DOMAIN settings are
		// correct.
	check_domain_attributes();

		// If mySubSystem_EXPRS is set, insert those expressions into
		// the given classad.
	config_fill_ad( classAd );

		// We have to do some platform-specific checking to make sure
		// all the parameters we think are defined really are.  
	check_params();

	(void)SetSyscalls( scm );
}


void
process_file( char* file, char* name, char* host, ClassAd* classAd )
{
	int rval;
	if( access( file, R_OK ) != 0 ) {
		if( !host ) {
			fprintf( stderr, "ERROR: Can't read %s %s\n", 
					 name, file );
			exit( 1 );
		} 
	} else {
		rval = Read_config( file, classAd, ConfigTab, TABLESIZE,
							EXPAND_LAZY );
		if( rval < 0 ) {
			fprintf( stderr,
					 "Configuration Error Line %d while reading %s %s\n",
					 ConfigLineNo, name, file );
			exit( 1 );
		}
	}
}


// Param for given name, read it in as a string list, and process each
// config file listed there.
void
process_locals( char* param_name, char* host, ClassAd* classAd )
{
	StringList locals;
	char *file;

	file = param( param_name );
	if( file ) {
		locals.initializeFromString( file );
		free( file );
		locals.rewind();
		while( (file = locals.next()) ) {
			process_file( file, "config file", host, classAd );
		}
	}
}


// Try to find the "condor" user's home directory
void
init_tilde()
{
	if( tilde ) {
		free( tilde );
		tilde = NULL;
	}
#if !defined(WIN32)
	struct passwd *pw;
	if( (pw=getpwnam( "condor" )) ) {
		tilde = strdup( pw->pw_dir );
	} 
#endif
}


char*
get_tilde()
{
	init_tilde();
	return tilde;
}


char*
find_global()
{
	return find_file( "CONDOR_CONFIG", "condor_config" );
}


char*
find_global_root()
{
	return find_file( "CONDOR_CONFIG_ROOT", "condor_config.root" );
}


// Find location of specified file
char*
find_file(const char *env_name, const char *file_name)
{

	char	*config_file = NULL, *env;
	int		fd;

		// If we were given an environment variable name, try that first. 
	if( env_name && (env = getenv( env_name )) ) {
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
		if( (fd = open( config_file, O_RDONLY )) < 0 ) {
			free( config_file );
			config_file = NULL;
		} else {
			close( fd );
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
	return config_file;
}


void
fill_attributes()
{
		/* There are a few attributes that specify what platform we're
		   on that we want to insert values for even if they're not
		   defined in the config files.  These are ARCH and OPSYS,
		   which we compute with the my_arch() and my_opsys()
		   functions.  We also insert the subsystem here.  Moved all
		   the domain stuff to check_domain_attributes() on
		   10/20.  Also, since this is called before we read in any
		   config files, there's no reason to check to see if any of
		   these are already defined.  -Derek Wright */

	char *arch, *opsys;

	if( (arch = my_arch()) != NULL ) {
		insert( "ARCH", arch, ConfigTab, TABLESIZE );
	}

	if( (opsys = my_opsys()) != NULL ) {
		insert( "OPSYS", opsys, ConfigTab, TABLESIZE );
	}

	insert( "subsystem", mySubSystem, ConfigTab, TABLESIZE );
}


void
check_domain_attributes()
{
		/* Make sure the FILESYSTEM_DOMAIN and UID_DOMAIN attributes
		   are set to something reasonable.  If they're not already
		   defined, we default to our own full hostname.  Moved this
		   to its own function so we're sure we have our full hostname
		   by the time we call this. -Derek Wright 10/20/98 */

	char *uid_domain, *filesys_domain;
		   
	filesys_domain = param("FILESYSTEM_DOMAIN");
	if( !filesys_domain ) {
		filesys_domain = my_full_hostname();
		insert( "FILESYSTEM_DOMAIN", filesys_domain, ConfigTab, TABLESIZE );
	} else {
		free( filesys_domain );
	}

	uid_domain = param("UID_DOMAIN");
	if( !uid_domain ) {
		uid_domain = my_full_hostname();
		insert( "UID_DOMAIN", uid_domain, ConfigTab, TABLESIZE );
	} else {
		free( uid_domain );
	}
}


void
init_config()
{
	memset( (char *)ConfigTab, 0, (TABLESIZE * sizeof(BUCKET*)) ); 
}


void
clear_config()
{
	register 	int 	i;
	register 	BUCKET	*ptr;	
	register 	BUCKET	*tmp;	

	for( i=0; i<TABLESIZE; i++ ) {
		ptr = ConfigTab[i];
		while( ptr ) {
			tmp = ptr->next;
			FREE( ptr->value );
			FREE( ptr->name );
			FREE( ptr );
			ptr = tmp;
		}
		ConfigTab[i] = NULL;
	}
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
int
param_in_pattern( char *parameter, char *pattern )
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


void
reinsert_specials( char* host )
{
	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
	}
	if( host ) {
		insert( "hostname", host, ConfigTab, TABLESIZE );
	} else {
		insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );
	}
	insert( "full_hostname", my_full_hostname(), ConfigTab, TABLESIZE );
	insert( "subsystem", mySubSystem, ConfigTab, TABLESIZE );
}


void
config_insert( char* attrName, char* attrValue )
{
	if( ! (attrName && attrValue) ) {
		return;
	}
	insert( attrName, attrValue, ConfigTab, TABLESIZE );
}


void
check_params()
{
#if defined( HPUX )
		// Only on HPUX does this check matter...
	char* tmp;
	if( !(tmp = param("ARCH")) ) {
			// Arch isn't defined.  That means the user didn't define
			// it _and_ the special file we use that maps workstation
			// models to CPU types doesn't exist either.  Print a
			// verbose message and exit.  -Derek Wright 8/14/98
		fprintf( stderr, "ERROR: Condor must know if you are running " 
				 "on an HPPA1 or an HPPA2 CPU.\n" );
		fprintf( stderr, "Normally, we look in %s for your model.\n",
				 "/opt/langtools/lib/sched.models" );
		fprintf( stderr, "This file lists all HP models and the "
				 "corresponding CPU type.  However,\n" );
		fprintf( stderr, "this file does not exist on your machine " 
				 "or your model (%s)\n", my_uname_arch() );
		fprintf( stderr, "was not listed.  You should either explicitly "
				 "set the ARCH parameter\n" );
		fprintf( stderr, "in your config file, or install the "
				 "sched.models file.\n" );
		exit( 1 );
	} else {
		free( tmp );
	}
#endif
}


} /* extern "C" */
