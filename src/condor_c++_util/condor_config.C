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
  configure themselves.  It takes an optional argument which
  determines if config should be quiet or verbos on errors.  It
  defaults to verbose error reporting.

  There's also an entry point, config_host() where you pass in a
  string that should be filled in for HOSTNAME.  This is only used by
  special arguments to condor_config_val used by condor_init to
  bootstrap the installation process.

  When looking for the global config file, config() checks the
  "CONDOR_CONFIG" environment variable to find its location.  If that
  doesn't exist, it looks in /etc/condor.  If the condor_config isn't
  there, it tries ~condor/.  If none of the above locations contain a
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
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "condor_version.h"
#include "util_lib_proto.h"
#include "condor_scanner.h"		// for MAXVARNAME, etc
#include "my_username.h"
#ifdef WIN32
#	include "ntsysinfo.h"		// for WinNT getppid
#endif

extern "C" {
	
// Function prototypes
void real_config(char* host, int wantsQuiet);
int Read_config(char*, BUCKET**, int, int);
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
void process_file(char*, char*, char*);
void process_locals( char*, char*);
static int  process_runtime_configs();
void check_params();

// External variables
extern int	ConfigLineNo;
}  /* End extern "C" */

extern char* mySubSystem;

// Global variables
BUCKET	*ConfigTab[TABLESIZE];
static char* tilde = NULL;
extern DLL_IMPORT_MAGIC char **environ;


// Function implementations

void
config_fill_ad( ClassAd* ad )
{
	char 		buffer[1024];
	char 		*tmp;
	char		*expr;
	StringList	reqdExprs;
	
	if( !ad ) return;

	sprintf (buffer, "%s_EXPRS", mySubSystem);
	tmp = param (buffer);
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);

		reqdExprs.rewind();
		while ((tmp = reqdExprs.next())) {
			expr = param(tmp);
			if (expr == NULL) continue;
			sprintf (buffer, "%s = %s", tmp, expr);
			ad->Insert (buffer);
			free (expr);
		}	
	}
	
	sprintf (buffer, "%s_ATTRS", mySubSystem);
	tmp = param (buffer);
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);

		reqdExprs.rewind();
		while ((tmp = reqdExprs.next())) {
			expr = param(tmp);
			if (expr == NULL) continue;
			sprintf (buffer, "%s = %s", tmp, expr);
			ad->Insert (buffer);
			free (expr);
		}	
	}
	
	/* Insert the version into the ClassAd */
	sprintf(buffer,"%s=\"%s\"", ATTR_VERSION, CondorVersion() );
	ad->Insert(buffer);

	sprintf(buffer,"%s=\"%s\"", ATTR_PLATFORM, CondorPlatform() );
	ad->Insert(buffer);
}


void
config( int wantsQuiet )
{
	real_config( NULL, wantsQuiet );
}


void
config_host( char* host )
{
	real_config( host, 0 );
}


void
real_config(char* host, int wantsQuiet)
{
	char		*config_file, *tmp;
	int			scm;

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
#ifndef WIN32
		fprintf(stderr,"/etc/condor/, nor ~condor/ contain a condor_config "
				"file.\n" );
#else
		fprintf(stderr,"nor the registry contains a path to a condor_config "
				"file.\n" );
#endif
		fprintf( stderr,"Either set CONDOR_CONFIG to point to a valid config "
				"file,\n" );
		fprintf( stderr,"or put a \"condor_config\" file in %s.\n", 
#ifndef WIN32
				 "/etc/condor or ~condor/" 
#else
				 "the registry at:\n HKEY_LOCAL_MACHINE\\Software\\Condor\\CONDOR_CONFIG"
#endif
				 );
		fprintf( stderr, "Exiting.\n\n" );
		exit( 1 );
	}

		// Read in the global file
	process_file( config_file, "global config file", NULL );
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
	process_locals( "LOCAL_CONFIG_FILE", host );
			
		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Now, insert any macros defined in the environment.  Note we do
		// this before the root config file!
	{
		char varname[MAXVARNAME];
		char *thisvar;
		int i,j;
		char *varvalue;

		for (i=0; environ[i]; i++) {
		
			if (strncmp(environ[i],"_condor_",8)!=0) 
				continue;

			thisvar = environ[i];

			varname[8] = '\0';
			for (j=0; thisvar[j] && thisvar[j] != '='; j++) {
				varname[j] = thisvar[j];
			}
			varname[j] = '\0';
			
			if ( varname[8] && (varvalue=getenv(varname)) ) {
				//dprintf(D_ALWAYS,"TODD at line %d insert var %s val %s\n",__LINE__,&(varname[8]), varvalue);

				if ( !strncmp( &( varname[8] ), "START_owner", 11 ) ) {
					char *tmp = (char *) malloc( strlen( varvalue ) 
								+ strlen( "Owner == \"   \"" ) );
					sprintf( tmp, "Owner == \"%s\"", varvalue );
					insert( "START", tmp, ConfigTab, TABLESIZE );
					free( tmp );
				}
				else {
					insert( &(varname[8]), varvalue, ConfigTab, TABLESIZE );
				}
			}
		}
	}

		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Try to find and read the global root config file
	if( config_file = find_global_root() ) {
		process_file( config_file, "global root config file", host );

			// Re-insert the special macros.  We don't want the user
			// to override them, since it's not going to work.
		reinsert_specials( host );

			// Read in the LOCAL_ROOT_CONFIG_FILE as a string list and
			// process all the files in the order they are listed.
		process_locals( "LOCAL_ROOT_CONFIG_FILE", host );
	}

		// Re-insert the special macros.  We don't want the user
		// to override them, since it's not going to work.
	reinsert_specials( host );

	if( process_runtime_configs() == 1 ) {
			// if we found runtime config files, we process the root
			// config file again
		if (config_file) {
			process_file( config_file, "global root config file", host );

				// Re-insert the special macros.  We don't want the user
				// to override them, since it's not going to work.
			reinsert_specials( host );

				// Read in the LOCAL_ROOT_CONFIG_FILE as a string list and
				// process all the files in the order they are listed.
			process_locals( "LOCAL_ROOT_CONFIG_FILE", host );
		}
	}

	if (config_file) free( config_file );

		// Now that we're done reading files, if DEFAULT_DOMAIN_NAME
		// is set, we need to re-initilize my_full_hostname(). 
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		free( tmp );
		init_full_hostname();
	}

		// Also, we should be safe to process the NETWORK_INTERFACE
		// parameter at this point, if it's set.
	init_ipaddr( TRUE );

		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Make sure our FILESYSTEM_DOMAIN and UID_DOMAIN settings are
		// correct.
	check_domain_attributes();

		// We have to do some platform-specific checking to make sure
		// all the parameters we think are defined really are.  
	check_params();

	(void)SetSyscalls( scm );
}


void
process_file( char* file, char* name, char* host )
{
	int rval;
	if( access( file, R_OK ) != 0 ) {
		if( !host ) {
			fprintf( stderr, "ERROR: Can't read %s %s\n", 
					 name, file );
			exit( 1 );
		} 
	} else {
		rval = Read_config( file, ConfigTab, TABLESIZE, EXPAND_LAZY );
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
process_locals( char* param_name, char* host )
{
	StringList locals;
	char *file;

	file = param( param_name );
	if( file ) {
		locals.initializeFromString( file );
		free( file );
		locals.rewind();
		while( (file = locals.next()) ) {
			process_file( file, "config file", host );
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
#if defined(CONDOR_G)
	char	*home_dir;
#endif
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

#ifndef WIN32
	// Only look in /etc/condor and in ~condor on Unix.
#if defined(CONDOR_G)
    // For Condor-G, first check in the default install path, which is the
    // user's homedirectory/CondorG/etc/condor_config 
	if( ! config_file ) {
		struct passwd *pwent;      
		
		if( pwent = getpwuid( getuid() ) ) {
			home_dir = strdup( pwent->pw_dir );
	
			config_file = (char *)malloc((strlen(file_name) + strlen(home_dir) +
                            strlen("CondorG/etc/") + 3 ) * sizeof(char));	
			config_file[0] = '\0';
			strcat(config_file, home_dir);
			strcat(config_file, "/CondorG/etc/");
			strcat(config_file, file_name); 
			if( (fd = open( config_file, O_RDONLY)) < 0 ) {
				free( config_file );
				config_file = NULL;
			} else {
				close( fd );
			}
			free( home_dir);
		}
	}
#endif /* CONDOR_G */

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
#else	/* of ifndef WIN32 */
	// Only look in the registry on WinNT.
	HKEY handle;

	if ( !config_file && RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\Condor",
		0, KEY_READ, &handle) == ERROR_SUCCESS ) {
		// We have found a registry key for Condor, which
		// means this user has a pulse and has actually run the
		// installation program before trying to run Condor.  
		// This user deserves a tax credit.  

		// So now that we found the key, read it.
		char the_path[_POSIX_PATH_MAX];
		DWORD valType;
		DWORD valSize = _POSIX_PATH_MAX - 2;

		the_path[0] = '\0';
		if ( RegQueryValueEx(handle, env_name, 0, 
			&valType, (unsigned char *)the_path, &valSize) == ERROR_SUCCESS ) {

			// confirm it is a string value with something there
			if ( valType == REG_SZ && the_path[0] ) {
				// got it!  whoohooo!
				config_file = strdup(the_path);
				if( (fd = open( config_file, O_RDONLY)) < 0 ) {
					free( config_file );
					config_file = NULL;
				} else {
					close( fd );
				}
			}
		}
	}
#endif   /* of Win32 */

	return config_file;
}


void
fill_attributes()
{
		/* There are a few attributes that specify what platform we're
		   on that we want to insert values for even if they're not
		   defined in the config files.  These are ARCH and OPSYS,
		   which we compute with the sysapi_condor_arch() and sysapi_opsys()
		   functions.  We also insert the subsystem here.  Moved all
		   the domain stuff to check_domain_attributes() on
		   10/20.  Also, since this is called before we read in any
		   config files, there's no reason to check to see if any of
		   these are already defined.  -Derek Wright
		   Amended -Pete Keller 06/01/99 */

	const char *tmp;

	if( (tmp = sysapi_condor_arch()) != NULL ) {
		insert( "ARCH", tmp, ConfigTab, TABLESIZE );
	}

	if( (tmp = sysapi_uname_arch()) != NULL ) {
		insert( "UNAME_ARCH", tmp, ConfigTab, TABLESIZE );
	}

	if( (tmp = sysapi_opsys()) != NULL ) {
		insert( "OPSYS", tmp, ConfigTab, TABLESIZE );
	}

	if( (tmp = sysapi_uname_opsys()) != NULL ) {
		insert( "UNAME_OPSYS", tmp, ConfigTab, TABLESIZE );
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
param( const char *name )
{
	char *val = lookup_macro( name, ConfigTab, TABLESIZE );

	if( val == NULL || val[0] == '\0' ) {
		return( NULL );
	}
	return( expand_macro(val, ConfigTab, TABLESIZE) );
}


char *
macro_expand( const char *str )
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
	static unsigned int reinsert_pid = 0;
	static unsigned int reinsert_ppid = 0;
	char buf[40];

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

	// Insert login-name for our euid as "username"
	char *myusernm = my_username();
	insert( "username", myusernm, ConfigTab, TABLESIZE );
	free(myusernm);

	// Insert values for "pid" and "ppid".  Use static values since
	// this is expensive to re-compute on Windows.
	// Note: we have to resort to ifdef WIN32 crap even though 
	// DaemonCore can nicely give us this information.  We do this
	// because the config code is used by the tools as well as daemons.
	if (!reinsert_pid) {
#ifdef WIN32
		reinsert_pid = ::GetCurrentProcessId();
#else
		reinsert_pid = getpid();
#endif
	}
	sprintf(buf,"%u",reinsert_pid);
	insert( "pid", buf, ConfigTab, TABLESIZE );
	if ( !reinsert_ppid ) {
#ifdef WIN32
		CSysinfo system_hackery;
		reinsert_ppid = system_hackery.GetParentPID(reinsert_pid);
#else
		reinsert_ppid = getppid();
#endif
	}
	sprintf(buf,"%u",reinsert_ppid);
	insert( "ppid", buf, ConfigTab, TABLESIZE );
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
				 "or your model (%s)\n", sysapi_uname_arch() );
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

/* Begin code for runtime support for modifying a daemon's config file.
   See condor_daemon_core.V6/README.config for more details. */

static StringList PersistAdminList;

class RuntimeConfigItem {
public:
	RuntimeConfigItem() : admin(NULL), config(NULL) { }
	~RuntimeConfigItem() { if (admin) free(admin); if (config) free(config); }
	void initialize() { admin = config = NULL; }
	char *admin;
	char *config;
};

#include "extArray.h"
template class ExtArray<RuntimeConfigItem>;

static ExtArray<RuntimeConfigItem> rArray;

static char toplevel_runtime_config[_POSIX_PATH_MAX] = { '\0' };

static void
set_toplevel_runtime_config()
{
	if (!toplevel_runtime_config[0]) {
		char filename_parameter[50], *tmp;
		sprintf(filename_parameter, "%s_CONFIG", mySubSystem);
		tmp = param(filename_parameter);
		if (tmp) {
			sprintf(toplevel_runtime_config, "%s", tmp);
			free(tmp);
		} else {
			tmp = param("LOG");
			if (!tmp) {
				dprintf( D_ALWAYS, "Condor error: neither %s nor LOG is "
						 "specified in the configuration file.\n",
						 filename_parameter );
				exit( 1 );
			}
			sprintf(toplevel_runtime_config, "%s%c.config.%s", tmp,
					DIR_DELIM_CHAR,	mySubSystem);
			free(tmp);
		}
	}
}

/* 
** Caller is responsible for allocating admin and config with malloc.
** Caller should not free admin and config after the call.
*/
int
set_persistent_config(char *admin, char *config)
{
	char tmp_filename[_POSIX_PATH_MAX], filename[_POSIX_PATH_MAX];
	int fd;
	char *tmp;

	if (!admin || !admin[0]) {
		if (admin) free(admin);
		if (config) free(config);
		return -1;
	}

	// make sure toplevel config filename is set
	set_toplevel_runtime_config();

	if (config && config[0]) {	// (re-)set config
		// write new config to temporary file
		sprintf(filename, "%s.%s", toplevel_runtime_config, admin);
		sprintf(tmp_filename, "%s.tmp", filename);
		if ((fd = open(tmp_filename, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
			dprintf( D_ALWAYS, "open(%s) returns %d, errno %d in "
					 "set_persistent_config\n", tmp_filename,
					 fd, errno );
			free(admin);
			free(config);
			return -1;
		}
		if (write(fd, config, strlen(config)) != strlen(config)) {
			dprintf( D_ALWAYS, "write failed with errno %d in "
					 "set_persistent_config\n", errno );
			free(admin);
			free(config);
			return -1;
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close failed with errno %d in "
					 "set_persistent_config\n", errno );
			free(admin);
			free(config);
			return -1;
		}
		
		// commit config changes
		if (rotate_file(tmp_filename, filename) < 0) {
			dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with errno %d in "
					 "set_persistent_config\n", tmp_filename, filename,
					 errno );
			free(admin);
			free(config);
			return -1;
		}
	
		// update admin list in memory
		if (!PersistAdminList.contains(admin)) {
			PersistAdminList.append(admin);
		} else {
			free(admin);
			free(config);
			return 0;		// if no update is required, then we are done
		}

	} else {					// clear config

		// update admin list in memory
		PersistAdminList.remove(admin);
		if (config) {
			free(config);
			config = NULL;
		}
	}		

	// update admin list on disk
	sprintf(tmp_filename, "%s.tmp", toplevel_runtime_config);
	if ((fd = open(tmp_filename, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
		dprintf( D_ALWAYS, "open(%s) returns %d, errno %d in "
				 "set_persistent_config\n", tmp_filename,
				 fd, errno );
		free(admin);
		if (config) free(config);
		return -1;
	}
	const char param[] = "RUNTIME_CONFIG_ADMIN = ";
	if (write(fd, param, strlen(param)) != strlen(param)) {
		dprintf( D_ALWAYS, "write failed with errno %d in "
				 "set_persistent_config\n", errno );
		free(admin);
		if (config) free(config);
		return -1;
	}
	PersistAdminList.rewind();
	bool first_time = true;
	while (tmp = PersistAdminList.next()) {
		if (!first_time) {
			if (write(fd, ", ", 2) != 2) {
				dprintf( D_ALWAYS, "write failed with errno %d in "
						 "set_persistent_config\n", errno );
				free(admin);
				if (config) free(config);
				return -1;
			}
		} else {
			first_time = false;
		}
		if (write(fd, tmp, strlen(tmp)) != strlen(tmp)) {
			dprintf( D_ALWAYS, "write failed with errno %d in "
					 "set_persistent_config\n", errno );
			free(admin);
			if (config) free(config);
			return -1;
		}
	}
	if (write(fd, "\n", 1) != 1) {
		dprintf( D_ALWAYS, "write failed with errno %d in "
				 "set_persistent_config\n", errno );
		free(admin);
		if (config) free(config);
		return -1;
	}
	if (close(fd) < 0) {
		dprintf( D_ALWAYS, "close failed with errno %d in "
				 "set_persistent_config\n", errno );
		free(admin);
		if (config) free(config);
		return -1;
	}
	
	if (rotate_file(tmp_filename, toplevel_runtime_config) < 0) {
		dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with errno %d in "
				 "set_persistent_config\n", tmp_filename, filename, errno );
		free(admin);
		if (config) free(config);
		return -1;
	}

	// if we removed a config, then we should clean up by removing the file(s)
	if (!config || !config[0]) {
		sprintf(filename, "%s.%s", toplevel_runtime_config, admin);
		unlink(filename);
		if (PersistAdminList.number() == 0) {
			unlink(toplevel_runtime_config);
		}
	}

	free(admin);
	if (config) free(config);
	return 0;
}

int
set_runtime_config(char *admin, char *config)
{
	int i;

	if (!admin || !admin[0]) {
		if (admin) free(admin);
		if (config) free(config);
		return -1;
	}

	if (config && config[0]) {
		for (i=0; i <= rArray.getlast(); i++) {
			if (strcmp(rArray[i].admin, admin) == MATCH) {
				free(admin);
				free(rArray[i].config);
				rArray[i].config = config;
				return 0;
			}
		}
		rArray[i].admin = admin;
		rArray[i].config = config;
	} else {
		for (i=0; i <= rArray.getlast(); i++) {
			if (strcmp(rArray[i].admin, admin) == MATCH) {
				free(admin);
				if (config) free(config);
				free(rArray[i].admin);
				free(rArray[i].config);
				rArray[i] = rArray[rArray.getlast()];
				rArray[rArray.getlast()].initialize();
				rArray.truncate(rArray.getlast()-1);
				return 0;
			}
		}
	}

	return 0;
}

/* 
** returns 1 if runtime configs were processed; 0 if no runtime configs
** were defined, and -1 on error.  persistent configs are also processed
** by this function.
*/
static int
process_runtime_configs()
{
	char filename[_POSIX_PATH_MAX];
	char *tmp;
	int i, rval, fd;
	bool processed = false;

	set_toplevel_runtime_config();

	if( access( toplevel_runtime_config, R_OK ) == 0 &&
		PersistAdminList.number() == 0 ) {

		processed = true;

		rval = Read_config( toplevel_runtime_config, ConfigTab,
							TABLESIZE, EXPAND_LAZY );
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d while reading "
					 "top-level runtime config file: %s\n",
					 ConfigLineNo, toplevel_runtime_config );
			exit(1);
		}

		tmp = param ("RUNTIME_CONFIG_ADMIN");
		if (tmp) {
			PersistAdminList.initializeFromString(tmp);
			free(tmp);
		}
	}

	PersistAdminList.rewind();
	while ((tmp = PersistAdminList.next())) {
		processed = true;
		sprintf(filename, "%s.%s", toplevel_runtime_config, tmp);
		rval = Read_config( filename, ConfigTab, TABLESIZE,
							EXPAND_LAZY );
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d "
					 "while reading runtime config file: %s\n",
					 ConfigLineNo, filename );
			exit(1);
		}
	}

	tmpnam(filename);
	for (i=0; i <= rArray.getlast(); i++) {
		processed = true;
		if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
			dprintf( D_ALWAYS, "open(%s) returns %d, errno %d in "
					 "process_runtime_configs\n", filename,
					 fd, errno );
			exit(1);
		}
		if (write(fd, rArray[i].config, strlen(rArray[i].config))
			!= strlen(rArray[i].config)) {
			dprintf( D_ALWAYS, "write failed with errno %d in "
					 "process_runtime_configs\n", errno );
			exit(1);
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close failed with errno %d in "
					 "process_runtime_configs\n", errno );
			exit(1);
		}
		rval = Read_config( filename, ConfigTab, TABLESIZE,
							EXPAND_LAZY );
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d "
					 "while reading %s, runtime config: %s\n",
					 ConfigLineNo, filename, rArray[i].admin );
			exit(1);
		}
		unlink(filename);
	}

	return (int)processed;
}

/* End code for runtime support for modifying a daemon's config file. */
