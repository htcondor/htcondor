/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
#include "my_username.h"
#ifdef WIN32
#	include "ntsysinfo.h"		// for WinNT getppid
#	include <locale.h>
#endif
#include "directory.h"			// for StatInfo
#include "condor_scanner.h"		// for MAXVARNAME, etc
#include "condor_distribution.h"
#include "condor_environ.h"
#include "HashTable.h"
#include "extra_param_info.h"
#include "condor_uid.h"

extern "C" {
	
// Function prototypes
void real_config(char* host, int wantsQuiet);
int Read_config(const char*, BUCKET**, int, int, bool,
				ExtraParamTable* = NULL);
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
void process_file(char*, char*, char*, int);
void process_locals( char*, char*);
static int  process_dynamic_configs();
void check_params();

// External variables
extern int	ConfigLineNo;
}  /* End extern "C" */

extern char* mySubSystem;

// Global variables
BUCKET	*ConfigTab[TABLESIZE];
static ExtraParamTable *extra_info = NULL;
static char* tilde = NULL;
extern DLL_IMPORT_MAGIC char **environ;
static bool have_config_file = true;

MyString global_config_file;
MyString global_root_config_file;
MyString local_config_files;

// Function implementations

void
config_fill_ad( ClassAd* ad )
{
	char 		*tmp;
	char		*expr;
	StringList	reqdExprs;
	MyString 	buffer;
	
	if( !ad ) return;

	buffer.sprintf( "%s_EXPRS", mySubSystem );
	tmp = param( buffer.Value() );
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);

		reqdExprs.rewind();
		while ((tmp = reqdExprs.next())) {
			expr = param(tmp);
			if (expr == NULL) continue;
			buffer.sprintf( "%s = %s", tmp, expr );
			ad->Insert( buffer.Value() );
			free( expr );
			expr = NULL;
		}	
	}
	
	buffer.sprintf( "%s_ATTRS", mySubSystem );
	tmp = param( buffer.Value() );
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);

		reqdExprs.rewind();
		while ((tmp = reqdExprs.next())) {
			expr = param(tmp);
			if (expr == NULL) continue;
			buffer.sprintf( "%s = %s", tmp, expr );
			ad->Insert( buffer.Value() );
			free( expr );
			expr = NULL;
		}	
	}
	
	/* Insert the version into the ClassAd */
	buffer.sprintf( "%s=\"%s\"", ATTR_VERSION, CondorVersion() );
	ad->Insert( buffer.Value() );

	buffer.sprintf( "%s=\"%s\"", ATTR_PLATFORM, CondorPlatform() );
	ad->Insert( buffer.Value() );
}


void
config( int wantsQuiet )
{
#ifdef WIN32
	setlocale( LC_ALL, "English" );
#endif
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
		extra_info->AddInternalParam("tilde");

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

	char* env = getenv( EnvGetName(ENV_CONFIG) );
	if( env && stricmp(env, "ONLY_ENV") == MATCH ) {
			// special case, no config file desired
		have_config_file = false;
	}

	if( have_config_file && ! (config_file = find_global()) ) {
		if( wantsQuiet ) {
			fprintf( stderr, "%s error: can't find config file.\n",
					 myDistro->GetCap() ); 
			exit( 1 );
		}
		fprintf(stderr,"\nNeither the environment variable %s_CONFIG,\n",
				myDistro->GetUc() );
#	  if defined UNIX
		fprintf(stderr,"/etc/%s/, nor ~%s/ contain a %s_config file.\n",
				myDistro->Get(), myDistro->Get(), myDistro->Get() );
#	  elif defined WIN32
		fprintf(stderr,"nor the registry contains a path to a %s_config "
				"file.\n", myDistro->Get() );
#	  else
#		error "Unknown O/S"
#	  endif
		fprintf( stderr,"Either set %s_CONFIG to point to a valid config "
				"file,\n", myDistro->GetUc() );
#	  if defined UNIX
		fprintf( stderr,"or put a \"%s_config\" file in /etc/%s or ~%s/\n",
				 myDistro->Get(), myDistro->Get(), myDistro->Get() );
#	  elif defined WIN32
		fprintf( stderr,"or put a \"%s_config\" file in the registry at:\n"
				 " HKEY_LOCAL_MACHINE\\Software\\%s\\%s_CONFIG",
				 myDistro->Get(), myDistro->Get(), myDistro->GetUc() );
#	  else
#		error "Unknown O/S"
#	  endif
		fprintf( stderr, "Exiting.\n\n" );
		exit( 1 );
	}

		// Read in the global file
	if( have_config_file ) {
		process_file( config_file, "global config file", NULL, true );
		global_config_file = config_file;
		free( config_file );
	}

		// Insert entries for "hostname" and "full_hostname".  We do
		// this here b/c we need these macros defined so that we can
		// find the local config file if that's defined in terms of
		// hostname or something.  However, we do this after reading
		// the global config file so people can put the
		// DEFAULT_DOMAIN_NAME parameter somewhere if they need it. 
		// -Derek Wright <wright@cs.wisc.edu> 5/11/98
	if( host ) {
		insert( "hostname", host, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("hostname");
	} else {
		insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("hostname");
	}
	insert( "full_hostname", my_full_hostname(), ConfigTab, TABLESIZE );
	extra_info->AddInternalParam("full_hostname");

		// Also insert tilde since we don't want that over-written.
	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("tilde");
	}

		// Read in the LOCAL_CONFIG_FILE as a string list and process
		// all the files in the order they are listed.
	process_locals( "LOCAL_CONFIG_FILE", host );
			
		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Now, insert any macros defined in the environment.  Note we do
		// this before the root config file!
	for( int i = 0; environ[i]; i++ ) {
		char magic_prefix[MAX_DISTRIBUTION_NAME + 3];	// case-insensitive
		strcpy( magic_prefix, "_" );
		strcat( magic_prefix, myDistro->Get() );
		strcat( magic_prefix, "_" );
		int prefix_len = strlen( magic_prefix );

		// proceed only if we see the magic prefix
		if( strncasecmp( environ[i], magic_prefix, prefix_len ) != 0 ) {
			continue;
		}

		char *varname = strdup( environ[i] );
		if( !varname ) {
			EXCEPT( "Out of memory in %s:%s\n", __FILE__, __LINE__ );
		}

		// isolate variable name by finding & nulling the '='
		int equals_offset = strchr( varname, '=' ) - varname;
		varname[equals_offset] = '\0';
		// isolate value by pointing to everything after the '='
		char *varvalue = varname + equals_offset + 1;
//		assert( !strcmp( varvalue, getenv( varname ) ) );
		// isolate Condor macro_name by skipping magic prefix
		char *macro_name = varname + prefix_len;

		// special macro START_owner needs to be expanded (for the
		// glide-in code) [which should probably be fixed to use
		// the general mechanism and set START itself --pfc]
		if( !strcmp( macro_name, "START_owner" ) ) {
			char *pretext = "Owner == \"";
			char *posttext = "\"";
			char *tmp = (char *)malloc( strlen( pretext ) + strlen( varvalue )
										+ strlen( posttext ) );
			sprintf( tmp, "%s%s%s", pretext, varvalue, posttext );
			insert( "START", tmp, ConfigTab, TABLESIZE );
			extra_info->AddEnvironmentParam("START");
			free( tmp );
		}
		// ignore "_CONDOR_" without any macro name attached
		else if( macro_name[0] != '\0' ) {
			insert( macro_name, varvalue, ConfigTab, TABLESIZE );
			extra_info->AddEnvironmentParam(macro_name);
		}

		free( varname );
	}

		// Re-insert the special macros.  We don't want the user to 
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Try to find and read the global root config file
	if( (config_file = find_global_root()) ) {
		global_root_config_file = config_file;
		process_file( config_file, "global root config file", host, true );

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

	if( process_dynamic_configs() == 1 ) {
			// if we found dynamic config files, we process the root
			// config file again
		if (config_file) {
			process_file( config_file, "global root config file", host, true );

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
process_file( char* file, char* name, char* host, int required )
{
	int rval;
	if( access( file, R_OK ) != 0 ) {
		if( !required) { return; }

		if( !host ) {
			fprintf( stderr, "ERROR: Can't read %s %s\n", 
					 name, file );
			exit( 1 );
		} 
	} else {
		rval = Read_config( file, ConfigTab, TABLESIZE, EXPAND_LAZY, 
							false, extra_info );
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
	char *tmp;
	int local_required;
	
	local_required = true;	
    tmp = param( "REQUIRE_LOCAL_CONFIG_FILE" );
    if( tmp ) {
		if( tmp[0] == 'f' || tmp[0] == 'F' ) {
			local_required = false;
		}
		free( tmp );
    }

	file = param( param_name );
	if( file ) {
		locals.initializeFromString( file );
		free( file );
		locals.rewind();
		while( (file = locals.next()) ) {
			process_file( file, "config file", host, local_required );
			if (local_config_files.Length() > 0) {
				local_config_files += " ";
			}
			local_config_files += file;
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
# if defined UNIX
	struct passwd *pw;
	if( (pw=getpwnam( myDistro->Get() )) ) {
		tilde = strdup( pw->pw_dir );
	} 
# endif
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
	char	file[_POSIX_PATH_MAX];
	sprintf( file, "%s_config", myDistro->Get() );
	return find_file( EnvGetName( ENV_CONFIG), file );
}


char*
find_global_root()
{
	char	file[_POSIX_PATH_MAX];
	sprintf( file, "%s_config.root", myDistro->Get() );
	return find_file( EnvGetName( ENV_CONFIG_ROOT ), file );
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
		StatInfo si( config_file );
		switch( si.Error() ) {
		case SIGood:
			if( si.IsDirectory() ) {
				fprintf( stderr, "File specified in %s environment "
						 "variable:\n\"%s\" is a directory.  "
						 "Please specify a file.\n", env_name,
						 config_file );  
				free( config_file );
				exit( 1 );
			}
				// Otherwise, we're happy
			return config_file;
			break;
		case SINoFile:
			fprintf( stderr, "File specified in %s environment "
					 "variable:\n\"%s\" does not exist.\n",
					 env_name, config_file );  
			free( config_file );
			exit( 1 );
			break;
		case SIFailure:
			fprintf( stderr, "Cannot stat file specified in %s "
					 "environment variable:\n\"%s\", errno: %d\n", 
					 env_name, config_file, si.Errno() );
			free( config_file );
			exit( 1 );
			break;
		}
	}

# ifdef UNIX
	// Only look in /etc/condor, ~condor, and $GLOBUS_LOCATION/etc on Unix.

	if( ! config_file ) {
		// try /etc/condor/file_name
		MyString	str( "/etc/" );
		str += myDistro->Get();
		str += "/";
		str += file_name;
		config_file = strdup( str.Value() );
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
    // For Condor-G, also check off of GLOBUS_LOCATION.
	if( ! config_file ) {
		char *globus_location;      
	
		if( (globus_location = getenv("GLOBUS_LOCATION")) ) {
	
			config_file = (char *)malloc( ( strlen(globus_location) +
                                            strlen("/etc/") +   
                                            strlen(file_name) + 3 ) 
                                          * sizeof(char) );	
			config_file[0] = '\0';
			strcat(config_file, globus_location);
			strcat(config_file, "/etc/");
			strcat(config_file, file_name); 
			if( (fd = open( config_file, O_RDONLY)) < 0 ) {
				free( config_file );
				config_file = NULL;
			} else {
				close( fd );
			}
		}
	} 
# elif defined WIN32	// ifdef UNIX
	// Only look in the registry on WinNT.
	HKEY	handle;
	char	regKey[256];

	sprintf( regKey, "Software\\%s", myDistro->GetCap() );
	if ( !config_file && RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKey,
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

				if ( strncmp(config_file, "\\\\", 2 ) == 0 ) {
					// UNC Path, so run a 'net use' on it first.
					NETRESOURCE nr;
					nr.dwType = RESOURCETYPE_DISK;
					nr.lpLocalName = NULL;
					nr.lpRemoteName = dirname(config_file);
					nr.lpProvider = NULL;
					
					if ( NO_ERROR != WNetAddConnection2(
										&nr,   /* NetResource */
										NULL,  /* password (default) */
										NULL,  /* username (default) */
										0      /* flags (none) */
						) ) {

						if ( GetLastError() == ERROR_INVALID_PASSWORD ) {
							// try again with an empty password
							WNetAddConnection2(
										&nr,   /* NetResource */
										"",    /* password (none) */
										NULL,  /* username (default) */
										0      /* flags (none) */
							);
						}

						// whether it worked or not, we're gonna continue.
						// The goal of running the WNetAddConnection2() is 
						// to make a mapping to the UNC path. For reasons
						// I don't fully understand, some sites need the 
						// mapping, and some don't. If it works, great; if 
						// not, try the open() anyways, and at worst we'll
						// fail fast and the user can fix their file server.
					}

					if (nr.lpRemoteName) {
						free(nr.lpRemoteName);
					}
				}

				if( (fd = open( config_file, O_RDONLY)) < 0 ) {
					free( config_file );
					config_file = NULL;
				} else {
					close( fd );
				}
			}
		}

		RegCloseKey(handle);
	}
# else
#	error "Unknown O/S"
# endif		/* ifdef UNIX / Win32 */

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
		extra_info->AddInternalParam("ARCH");
	}

	if( (tmp = sysapi_uname_arch()) != NULL ) {
		insert( "UNAME_ARCH", tmp, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("UNAME_ARCH");
	}

	if( (tmp = sysapi_opsys()) != NULL ) {
		insert( "OPSYS", tmp, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("OPSYS");
	}

	if( (tmp = sysapi_uname_opsys()) != NULL ) {
		insert( "UNAME_OPSYS", tmp, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("UNAME_OPSYS");
	}

	insert( "subsystem", mySubSystem, ConfigTab, TABLESIZE );
	extra_info->AddInternalParam("subsystem");
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
		extra_info->AddInternalParam("FILESYSTEM_DOMAIN");
	} else {
		free( filesys_domain );
	}

	uid_domain = param("UID_DOMAIN");
	if( !uid_domain ) {
		uid_domain = my_full_hostname();
		insert( "UID_DOMAIN", uid_domain, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("UID_DOMAIN");
	} else {
		free( uid_domain );
	}
}


void
init_config()
{
	memset( (char *)ConfigTab, 0, (TABLESIZE * sizeof(BUCKET*)) ); 
	extra_info = new ExtraParamTable();
	return;
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
	if (extra_info != NULL) {
		delete extra_info;
		extra_info = new ExtraParamTable();
	}
	global_config_file       = "";
	global_root_config_file  = "";
	local_config_files         = "";
	return;
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

	// Ok, now expand it out...
	val = expand_macro( val, ConfigTab, TABLESIZE );

	// If it returned an empty string, free it before returning NULL
	if( val == NULL ) {
		return NULL;
	} else if ( val[0] == '\0' ) {
		free( val );
		return( NULL );
	} else {
		return val;
	}
}

/*
** Return the integer value associated with the named paramter.
** If the value is not defined or not a valid integer, then
** return the default_value argument.  The min_value and max_value
** arguments are optional and default to MININT and MAXINT.
*/

int
param_integer( const char *name, int default_value,
			   int min_value, int max_value )
{
	int result;
	int fields;
	char *string;

	ASSERT( name );
	string = param( name );
	if( ! string ) {
		dprintf( D_FULLDEBUG, "%s is undefined, using default value of %d\n",
				 name, default_value );
		return default_value;
	}

	fields = sscanf( string, "%d", &result );

	if( fields != 1 ) {
		dprintf( D_FULLDEBUG, "WARNING: %s not an integer (\"%s\"), using "
				 "default value of %d\n", name, string, default_value );
		result = default_value;
	}
	else if( result < min_value ) {
		dprintf( D_FULLDEBUG, "WARNING: %s too low (%d), using minimum "
				 "value of %d\n", name, result, min_value );
		result = min_value;
	}
	else if( result > max_value ) {
		dprintf( D_FULLDEBUG, "WARNING: %s too high (%d), using maximum "
				 "value of %d\n", name, result, max_value );
		result = max_value;
	}
	free( string );
	return result;
}

/*
** Return the boolean value associated with the named paramter.
** The parameter value is expected to be set to the string
** "TRUE" or "FALSE" (no quotes, case insensitive).
** If the value is not defined or not a valid, then
** return the default_value argument.
*/

bool
param_boolean( const char *name, const bool default_value )
{
	bool result;
	char *string;

	ASSERT( name );
	string = param( name );
	if( ! string ) {
		dprintf( D_FULLDEBUG, "%s is undefined, using default value of %s\n",
				 name, default_value ? "True" : "False" );
		return default_value;
	}

	switch ( string[0] ) {
		case 'T':
		case 't':
		case '1':
			result = true;
			break;
		case 'F':
		case 'f':
		case '0':
			result = false;
			break;
		default:
		    dprintf( D_FULLDEBUG, "WARNING: %s not a boolean (\"%s\"), using "
					 "default value of %s\n", name, string,
					 default_value ? "True" : "False" );
			result = default_value;
			break;
	}

	free( string );
	
	return result;
}


char *
macro_expand( const char *str )
{
	return( expand_macro(str, ConfigTab, TABLESIZE) );
}

/*
** Same as param_boolean but for C -- returns 0 or 1
** The parameter value is expected to be set to the string
** "TRUE" or "FALSE" (no quotes, case insensitive).
** If the value is not defined or not a valid, then
** return the default_value argument.
*/
int
param_boolean_int( const char *name, int default_value )
{
    bool default_bool;

    default_bool = default_value == 0 ? false : true;
    return param_boolean(name, default_bool) ? 1 : 0;
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

// Note that the line_number can be -1 if the filename isn't a real
// filename, but something like <Internal> or <Environment>
bool param_get_location(
	const char *parameter,
	MyString  &filename,
	int       &line_number)
{
	bool found_it;

	if (parameter != NULL && extra_info != NULL) {
		found_it = extra_info->GetParam(parameter, filename, line_number);
	} else {
		found_it = false;
	}
	return found_it;
}

void
reinsert_specials( char* host )
{
	static unsigned int reinsert_pid = 0;
	static unsigned int reinsert_ppid = 0;
	static bool warned_no_user = false;
	char buf[40];

	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
		extra_info->AddInternalParam("tilde");
	}
	if( host ) {
		insert( "hostname", host, ConfigTab, TABLESIZE );
	} else {
		insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );
	}
	insert( "full_hostname", my_full_hostname(), ConfigTab, TABLESIZE );
	insert( "subsystem", mySubSystem, ConfigTab, TABLESIZE );
	extra_info->AddInternalParam("hostname");
	extra_info->AddInternalParam("full_hostname");
	extra_info->AddInternalParam("subsystem");

	// Insert login-name for our real uid as "username".  At the time
	// we're reading in the config file, the priv state code is not
	// initialized, so our euid will always be the same as our ruid.
	char *myusernm = my_username();
	if( myusernm ) {
		insert( "username", myusernm, ConfigTab, TABLESIZE );
		free(myusernm);
		myusernm = NULL;
		extra_info->AddInternalParam("username");
	} else {
		if( ! warned_no_user ) {
			dprintf( D_ALWAYS, "ERROR: can't find username of current user! "
					 "BEWARE: $(USERNAME) will be undefined\n" );
			warned_no_user = true;
		}
	}

	// Insert values for "pid" and "ppid".  Use static values since
	// this is expensive to re-compute on Windows.
	// Note: we have to resort to ifdef WIN32 junk even though 
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
	extra_info->AddInternalParam("pid");
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
	insert( "ip_address", my_ip_string(), ConfigTab, TABLESIZE );
	extra_info->AddInternalParam("ppid");
	extra_info->AddInternalParam("ip_address");
}


void
config_insert( const char* attrName, const char* attrValue )
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
		fprintf( stderr, "ERROR: %s must know if you are running "
				 "on an HPPA1 or an HPPA2 CPU.\n",
				 myDistro->Get() );
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

static MyString toplevel_persistent_config;

/*
  we want these two bools to be global, and only initialized on
  startup, so that folks can't play tricks and change these
  dynamically.  for example, if a site enables runtime but not
  persistent configs, we can't allow someone to set
  "ENABLE_PERSISTENT_CONFIG" with a condor_config_val -rset.
  therefore, we only read these once, before we look at any of the
  dynamic config files, to make sure we're happy.  this means it
  requires a restart to change any of these, but i think that's a
  reasonable burden on admins, considering the potential security
  implications.  -derek 2006-03-17
*/ 
static bool enable_runtime;
static bool enable_persistent;

static void
init_dynamic_config()
{
	static bool initialized = false;

	if( initialized ) {
			// already have a value, we're done
		return;
	}

	enable_runtime = param_boolean( "ENABLE_RUNTIME_CONFIG", false );
	enable_persistent = param_boolean( "ENABLE_PERSISTENT_CONFIG", false );
	initialized = true;

	if( !enable_persistent ) {
			// we don't want persistent configs, leave the toplevel blank
		return;
	}

	char* tmp;

		// if we're using runtime config, try a subsys-specific config
		// knob for the root location
	MyString filename_parameter;
	filename_parameter.sprintf( "%s_CONFIG", mySubSystem );
	tmp = param( filename_parameter.Value() );
	if( tmp ) {
		toplevel_persistent_config = tmp;
		free( tmp );
		return;
	}

	tmp = param( "PERSISTENT_CONFIG_DIR" );

	if( !tmp ) {
		if( strcmp(mySubSystem,"SUBMIT")==0 || 
			strcmp(mySubSystem,"TOOL")==0 ||
			! have_config_file )
		{
				/* 
				   we are just a tool, not a daemon.
				   or, we were explicitly told we don't have
				   the usual config files.
				   thus it is not imperative that we find what we
				   were looking for...
				*/
			return;
		} else {
				// we are a daemon.  if we fail, we must exit.
			fprintf( stderr, "%s error: ENABLE_PERSISTENT_CONFIG is TRUE, "
					 "but neither %s nor PERSISTENT_CONFIG_DIR is "
					 "specified in the configuration file\n",
					 myDistro->GetCap(), filename_parameter.Value() );
			exit( 1 );
		}
	}
	toplevel_persistent_config.sprintf( "%s%c.config.%s", tmp,
										DIR_DELIM_CHAR, mySubSystem );
	free(tmp);
}


/* 
** Caller is responsible for allocating admin and config with malloc.
** Caller should not free admin and config after the call.
*/

#define ABORT \
	if(admin) { free(admin); } \
	if(config) { free(config); } \
	set_priv(priv); \
	return -1

int
set_persistent_config(char *admin, char *config)
{
	int fd, rval;
	char *tmp;
	MyString filename;
	MyString tmp_filename;
	priv_state priv;

	if (!admin || !admin[0] || !enable_persistent) {
		if (admin)  { free(admin);  }
		if (config) { free(config); }
		return -1;
	}

	// make sure toplevel config filename is set
	init_dynamic_config();
	if( ! toplevel_persistent_config.Length() ) {
		EXCEPT( "Impossible: programmer error: toplevel_persistent_config "
				"is 0-length, but we already initialized, enable_persistent "
				"is TRUE, and set_persistent_config() has been called" );
	}

	priv = set_root_priv();
	if (config && config[0]) {	// (re-)set config
			// write new config to temporary file
		filename.sprintf( "%s.%s", toplevel_persistent_config.Value(), admin );
		tmp_filename.sprintf( "%s.tmp", filename.Value() );
		do {
			unlink( tmp_filename.Value() );
			fd = open( tmp_filename.Value(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
		} while (fd == -1 && errno == EEXIST); 
		if( fd < 0 ) {
			dprintf( D_ALWAYS, "open(%s) returned %d '%s' (errno %d) in "
					 "set_persistent_config()\n", tmp_filename.Value(),
					 fd, strerror(errno), errno );
			ABORT;
		}
		if (write(fd, config, strlen(config)) != (ssize_t)strlen(config)) {
			dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			ABORT;
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			ABORT;
		}
		
			// commit config changes
		if (rotate_file(tmp_filename.Value(), filename.Value()) < 0) {
			dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' "
					 "(errno %d) in set_persistent_config()\n",
					 tmp_filename.Value(), filename.Value(),
					 strerror(errno), errno );
			ABORT;
		}
	
		// update admin list in memory
		if (!PersistAdminList.contains(admin)) {
			PersistAdminList.append(admin);
		} else {
			free(admin);
			free(config);
			set_priv(priv);
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
	tmp_filename.sprintf( "%s.tmp", toplevel_persistent_config.Value() );
	do {
		unlink( tmp_filename.Value() );
		fd = open( tmp_filename.Value(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
	} while (fd == -1 && errno == EEXIST); 
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "open(%s) returned %d '%s' (errno %d) in "
				 "set_persistent_config()\n", tmp_filename.Value(),
				 fd, strerror(errno), errno );
		ABORT;
	}
	const char param[] = "RUNTIME_CONFIG_ADMIN = ";
	if (write(fd, param, strlen(param)) != (ssize_t)strlen(param)) {
		dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		ABORT;
	}
	PersistAdminList.rewind();
	bool first_time = true;
	while( (tmp = PersistAdminList.next()) ) {
		if (!first_time) {
			if (write(fd, ", ", 2) != 2) {
				dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
						 "set_persistent_config()\n", strerror(errno), errno );
				ABORT;
			}
		} else {
			first_time = false;
		}
		if (write(fd, tmp, strlen(tmp)) != (ssize_t)strlen(tmp)) {
			dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			ABORT;
		}
	}
	if (write(fd, "\n", 1) != 1) {
		dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		ABORT;
	}
	if (close(fd) < 0) {
		dprintf( D_ALWAYS, "close() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		ABORT;
	}
	
	rval = rotate_file( tmp_filename.Value(),
						toplevel_persistent_config.Value() );
	if (rval < 0) {
		dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' (errno %d) "
				 "in set_persistent_config()\n", tmp_filename.Value(),
				 filename.Value(), strerror(errno), errno );
		ABORT;
	}

	// if we removed a config, then we should clean up by removing the file(s)
	if (!config || !config[0]) {
		filename.sprintf( "%s.%s", toplevel_persistent_config.Value(), admin );
		unlink( filename.Value() );
		if (PersistAdminList.number() == 0) {
			unlink( toplevel_persistent_config.Value() );
		}
	}

	set_priv( priv );
	free( admin );
	if (config) { free( config ); }
	return 0;
}


int
set_runtime_config(char *admin, char *config)
{
	int i;

	if (!admin || !admin[0] || !enable_runtime) {
		if (admin)  { free(admin);  }
		if (config) { free(config); }
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


extern "C" {

static int
process_persistent_configs()
{
	char *tmp;
	int rval;
	bool processed = false;
	MyString filename;

	if( access( toplevel_persistent_config.Value(), R_OK ) == 0 &&
		PersistAdminList.number() == 0 )
	{
		processed = true;

		rval = Read_config( toplevel_persistent_config.Value(), ConfigTab,
							TABLESIZE, EXPAND_LAZY, true, extra_info );
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d while reading "
					 "top-level persistent config file: %s\n",
					 ConfigLineNo, toplevel_persistent_config.Value() );
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
		filename.sprintf( "%s.%s", toplevel_persistent_config.Value(), tmp );
		rval = Read_config( filename.Value(), ConfigTab, TABLESIZE,
							 EXPAND_LAZY, true, extra_info );
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d "
					 "while reading persistent config file: %s\n",
					 ConfigLineNo, filename.Value() );
			exit(1);
		}
	}
	return (int)processed;
}


static int
process_runtime_configs()
{
	char filename[_POSIX_PATH_MAX];
	int i, rval, fd;
	bool processed = false;

	for (i=0; i <= rArray.getlast(); i++) {
		processed = true;

#if HAVE_MKSTEMP && !defined( WIN32 )
		sprintf( filename, "/tmp/cndrtmpXXXXXX" );
		fd = mkstemp( filename ); 
		if (fd < 0) {
			dprintf( D_ALWAYS, "mkstemp(%s) returned %d, '%s' (errno %d) in "
					 "process_dynamic_configs()\n", filename, fd,
					 strerror(errno), errno );
			exit(1);
		}
#else /* ! HAVE_MKSTEMP */
		// EVIL!  tmpnam() isn't safe!
		tmpnam( filename );
		fd = open( filename, O_WRONLY|O_CREAT|O_TRUNC, 0644 );
		if (fd < 0) {
			dprintf( D_ALWAYS, "open(%s) returns %d, '%s' (errno %d) in "
					 "process_dynamic_configs()\n", filename, fd,
					 strerror(errno), errno );
			exit(1);
		}
#endif /* ! HAVE_MKSTEMP */

		if (write(fd, rArray[i].config, strlen(rArray[i].config))
			!= (ssize_t)strlen(rArray[i].config)) {
			dprintf( D_ALWAYS, "write failed with errno %d in "
					 "process_dynamic_configs\n", errno );
			exit(1);
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close failed with errno %d in "
					 "process_dynamic_configs\n", errno );
			exit(1);
		}
		rval = Read_config( filename, ConfigTab, TABLESIZE,
							EXPAND_LAZY, false, extra_info );
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


/* 
** returns 1 if dynamic (runtime or persistent) configs were
** processed; 0 if no dynamic configs were defined, and -1 on error.
*/
static int
process_dynamic_configs()
{
	int per_rval, run_rval;

	init_dynamic_config();

	if( enable_persistent ) {
		per_rval = process_persistent_configs();
	}

	if( enable_runtime ) {
		run_rval = process_runtime_configs();
	}

	if( per_rval < 0 || run_rval < 0 ) {
		return -1;
	}
	if( per_rval || run_rval ) {
		return 1;
	}
	return 0;
}

} // end of extern "C" 

/* End code for runtime support for modifying a daemon's config file. */
