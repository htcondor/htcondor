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
** Again, major re-write on 2/5/98 by Derek Wright to remove
**   config_master and replace it with condor_config.root and friends.
*/ 

/* 

  This file implements config(), the function all daemons call to
  configure themselves.  It takes up to two arguments: a pointer to a
  ClassAd to fill up with the expressions in the config_file, and a
  pointer to a string containing the name of the daemon calling config
  (mySubSystem).  

  In general, when looking for a given config file (either global,
  local, global-root or local-root), config() checks an environment
  variable to find the location of the global config files.  If that
  doesn't exist, it looks in /etc/condor.  If the files aren't there,
  it try's ~condor/.  If none of the above locations contain a config
  file, config() prints an error message and exits. 

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

#if defined(__cplusplus)
extern "C" {
#endif
	
// Function prototypes
void real_config(ClassAd *classAd, char *mySubsystem, char* host);
int Read_config(char*, ClassAd*, BUCKET**, int, int);
char* get_arch();
char* get_op_sys();
int SetSyscalls(int);
char* find_global();
char* find_local();
char* find_global_root();
char* find_local_root();
char* find_file();
void init_tilde();
void fill_attributes(ClassAd*);


// External variables
extern BUCKET	*ConfigTab[];
extern int	ConfigLineNo;


// Global variables
BUCKET	*ConfigTab[TABLESIZE];
static char* tilde = NULL;


// Function implementations

void
config_fill_ad(ClassAd* ad, char* mySubsys)
{
	char 		buffer[1024];
	char 		*tmp;
	char		*expr;
	StringList	reqdExprs;
	
	if (!mySubsys || !ad) return;

	sprintf (buffer, "%s_EXPRS", mySubsys);
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
}


void
config( ClassAd* classAd, char* mySubsystem )
{
	real_config( classAd, mySubsystem, NULL );
}


void
config_host( ClassAd* classAd, char* mySubsystem, char* host )
{
	real_config( classAd, mySubsystem, host );
}

void
real_config(ClassAd *classAd, char *mySubsystem, char* host)
{
	char	hostname[1024];
	char	*config_file;
	int		scm, rval;

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

		// Insert entries for "tilde" and "hostname". Note that
		// "hostname" ends up as just the machine name w/o the
		// . separators
	if( tilde ) {
		insert( "tilde", tilde, ConfigTab, TABLESIZE );
	} else {
			// What about tilde if there's no ~condor? 
	}
	if( host ) {
		insert( "hostname", host, ConfigTab, TABLESIZE );
	} else {
		insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );
	}

		// Try to find the global config file
	if( ! (config_file = find_global()) ) {
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

		// Actually read in the global file
	rval = Read_config( config_file, classAd, ConfigTab, TABLESIZE,
						EXPAND_LAZY );
	if( rval < 0 ) {
		fprintf( stderr,
				 "Configuration Error Line %d while reading global config file %s\n",
				 ConfigLineNo, config_file );
		exit( 1 );
	}
	free( config_file );

	
		// Try to find and read the local config file
	if( config_file = find_local() ) {
		if( access( config_file, R_OK ) != 0 ) {
			if( !host ) {
				fprintf( stderr, "ERROR: Can't read local config file %s\n", 
						 config_file );
				exit( 1 );
			} 
		} else {
			rval = Read_config( config_file, classAd, ConfigTab, 
								TABLESIZE, EXPAND_LAZY ); 
			if( rval < 0 ) {
				fprintf( stderr,
				  "Configuration Error Line %d while reading local config file %s\n",
						 ConfigLineNo, config_file );
				exit( 1 );
			}
		}
		free( config_file );
	}


		// Try to find and read the global root config file
	if( config_file = find_global_root() ) {
		rval = Read_config( config_file, classAd, ConfigTab, 
							TABLESIZE, EXPAND_LAZY ); 
		if( rval < 0 ) {
			fprintf( stderr,
					 "Configuration Error Line %d while reading global root config file %s\n",
					 ConfigLineNo, config_file );
			exit( 1 );
		}
		free( config_file );
	}

		// Try to find and read the local root config file
	if( config_file = find_local_root() ) {
		if( access( config_file, R_OK ) != 0 ) {
			if( !host ) {
				fprintf( stderr, "ERROR: Can't read local root config file %s\n", 
						 config_file );
				exit( 1 );
			} 
		} else {
			rval = Read_config( config_file, classAd, ConfigTab, 
								TABLESIZE, EXPAND_LAZY ); 
			if( rval < 0 ) {
				fprintf( stderr,
				 "Configuration Error Line %d while reading local root config file %s\n",
						 ConfigLineNo, config_file );
				exit( 1 );
			}
		}
		free( config_file );
	}

		// Now that we've read all the config files, there are some
		// attributes we want to define with defaults if they're not
		// defined in the config files: ARCH, OPSYS,
		// FILESYSTEM_DOMAIN and UID_DOMAIN.
	fill_attributes( classAd );

		// If mySubSystem_EXPRS is set, insert those expressions into
		// the given classad.
	config_fill_ad( classAd, mySubsystem );

	(void)SetSyscalls( scm );
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
find_global()
{
	return find_file( "CONDOR_CONFIG", "condor_config" );
}


char*
find_local()
{
	char* local_name = param( "LOCAL_CONFIG_FILE" );
	if( !local_name ) {
		local_name = find_file( NULL, "condor_config.local" );
	}
	return local_name;
}


char*
find_global_root()
{
	return find_file( "CONDOR_CONFIG_ROOT", "condor_config.root" );
}

char*
find_local_root()
{
	char* local_name = param( "LOCAL_ROOT_CONFIG_FILE" );
	if( !local_name ) {
		local_name = find_file( NULL, "condor_config.local.root" );
	}
	return local_name;
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
fill_attributes(ClassAd* classAd)
{

		/* Now that we've read in the whole config file, there are a
		   few attributes that we want to insert values for even if 
		   they're not defined in the file.  These are: ARCH, OPSYS,
		   UID_DOMAIN and FILESYSTEM_DOMAIN.  ARCH and OPSYS we get
		   from uname().  If either UID_DOMAIN or FILESYSTEM_DOMAIN
		   are not defined, we use the fully qualified hostname of
		   this host.  In all cases, the value in the config file
		   overrides these defaults.  Arch and OpSys by Jim B.  Uid
		   and FileSys by Derek Wright, 1/12/98 */

	char *arch, *op_sys, *uid_domain, *filesys_domain;
	char line[1024];

  	arch = param("ARCH");
	if( !arch ) {
		if( (arch = get_arch()) != NULL ) {
			insert( "ARCH", arch, ConfigTab, TABLESIZE );
			if(classAd)	{
				sprintf( line, "%s=\"%s\"", ATTR_ARCH, arch );
				classAd->Insert(line);
			}
		}
	} else {
		free( arch );
	}

	op_sys = param("OPSYS");
	if( !op_sys ) {
		if( (op_sys = get_op_sys()) != NULL ) {
			insert( "OPSYS", op_sys, ConfigTab, TABLESIZE );
			if(classAd) {
				sprintf( line, "%s=\"%s\"", ATTR_OPSYS, op_sys );
				classAd->Insert(line);
			}
		}
	} else {
		free( op_sys );
	}

	filesys_domain = param("FILESYSTEM_DOMAIN");
	if( !filesys_domain ) {
		filesys_domain = my_full_hostname();
		insert( "FILESYSTEM_DOMAIN", filesys_domain, ConfigTab, TABLESIZE );
		if(classAd) {
			sprintf( line, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, filesys_domain );
			classAd->Insert(line);
		}
	} else {
		free( filesys_domain );
	}

	uid_domain = param("UID_DOMAIN");
	if( !uid_domain ) {
		uid_domain = my_full_hostname();
		insert( "UID_DOMAIN", uid_domain, ConfigTab, TABLESIZE );
		if(classAd) {
			sprintf( line, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, uid_domain );
			classAd->Insert(line);
		}
	} else {
		free( uid_domain );
	}
}


void
init_config()
{
	memset( (char *)ConfigTab, 0, sizeof(ConfigTab) ); 
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


#if defined(WIN32)

char *
get_arch()
{
	static char answer[1024];	
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	switch(info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		sprintf(answer, "INTEL");
		break;
	case PROCESSOR_ARCHITECTURE_MIPS:
		sprintf(answer, "MIPS");
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
		sprintf(answer, "ALPHA");
		break;
	case PROCESSOR_ARCHITECTURE_PPC:
		sprintf(answer, "PPC");
		break;
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	default:
		sprintf(answer, "UNKNOWN");
		break;
	}

	return answer;
}

char *
get_op_sys()
{
	static char answer[1024];
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx(&info) > 0) {
		switch(info.dwPlatformId) {
		case VER_PLATFORM_WIN32s:
			sprintf(answer, "WIN32s%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			sprintf(answer, "WIN32%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_NT:
			sprintf(answer, "WINNT%d%d", info.dwMajorVersion, info.dwMinorVersion);
			break;
		default:
			sprintf(answer, "UNKNOWN");
			break;
		}
	} else {
		sprintf(answer, "ERROR");
	}

	return answer;
}

#else
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

#endif


#if defined(__cplusplus)
}
#endif
