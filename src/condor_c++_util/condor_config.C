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

  This file implements config(), the function all daemons call to
  configure themselves.  It takes up to two arguments: a pointer to a
  ClassAd to fill up with the expressions in the config_file, and a
  pointer to a string containing the name of the daemon calling config
  (mySubSystem).  config() simply calls real_config() with the right
  parameters for the default behavior.  real_config() is also called
  by config_master(), which is used to configure the condor_master
  process with different file names.

  In general, the config functions check an environment variable to
  find the location of the global config files.  If that doesn't
  exist, it looks in /etc/condor.  If the files aren't there, it try's
  ~condor/.  If none of the above locations contain a config file, the
  functions print error messages and exit
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
int real_config(const char*, const char*, const char*, ClassAd*);
int Read_config(char*, ClassAd*, BUCKET**, int, int);
char *get_arch();
char *get_op_sys();
int SetSyscalls(int);
void FillSubsysExprs( ClassAd*, char* );

// External variables
extern BUCKET	*ConfigTab[];
extern int	ConfigLineNo;


// Global variables
BUCKET	*ConfigTab[TABLESIZE];

// Function implementations
void 
config(ClassAd *classAd, char *mySubsystem)
{
	if( real_config("CONDOR_CONFIG", "condor_config", 
					"condor_config.local", (mySubsystem ? (ClassAd*)NULL : classAd)) ) {
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
	FillSubsysExprs( classAd, mySubsystem );
}

void
config_master(ClassAd *classAd)
{
	int rval = real_config("CONDOR_CONFIG_MASTER", "condor_config.master", 
						   "condor_config.master.local", NULL);
	if( rval ) {
			// Trying to find things in with the .master names failed,
			// try the regular config files.  
		rval = real_config("CONDOR_CONFIG", "condor_config", 
						   "condor_config.local", NULL);
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
	FillSubsysExprs( classAd, "MASTER" );
}


void
FillSubsysExprs(ClassAd* ad, char* mySubsys)
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


int
real_config(const char *env_name, const char *file_name,
			const char *local_name, ClassAd *classAd)
{
#if !defined(WIN32)
	struct passwd	*pw, *getpwnam();
#endif
	char			*ptr;
	int				rval, fd;
	char			hostname[1024];
	int				scm;
	char			*arch, *op_sys, *filesys_domain, *uid_domain;
	char			*env, *config_file = NULL, *tilde = NULL;
	char			line[256];
  
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

#if !defined(WIN32)
	if( (pw=getpwnam( "condor" )) ) {
		tilde = strdup( pw->pw_dir );
	} 
#endif

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
	insert( "hostname", my_hostname(), ConfigTab, TABLESIZE );

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

		/* Now that we've read in the whole config file, there are a
		   few attributes that we want to insert values for even if 
		   they're not defined in the file.  These are: ARCH, OPSYS,
		   UID_DOMAIN and FILESYSTEM_DOMAIN.  ARCH and OPSYS we get
		   from uname().  If either UID_DOMAIN or FILESYSTEM_DOMAIN
		   are not defined, we use the fully qualified hostname of
		   this host.  In all cases, the value in the config file
		   overrides these defaults.  Arch and OpSys by Jim B.  Uid
		   and FileSys by Derek Wright, 1/12/98 */

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
	
#if !defined(WIN32)
	(void)endpwent();
#endif
	(void)SetSyscalls( scm );
	return 0;
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
