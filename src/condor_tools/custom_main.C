/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

/*
  Build site specific versions of config and script files by dialoging
  with the user to get/confirm settings.
*/

#define _POSIX_SOURCE

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "custom.h"

const int MATCH = 0;	// for strcmp()

int check_dir( const char *path );
int check_exec( const char *path );
char * xlate_tilde( const char *path );
char * lookup_tilde( char *orig,  const char *user );
int check_host( const char *name );

/*
  All the macros which are to be customized should go here.  Parameters
  are:
	Macro name
	Description
	Default value
*/
Macro release_dir(
	"RELEASEDIR",
	"Directory where users should access bin and lib",
	"/usr/psup/condor"
);
Macro condor_host(
	"CONDOR_HOST",
	"Host where central manager should run",
	"frigg.cs.wisc.edu"
);
Macro condor_admin(
	"CONDOR_ADMIN",
	"Address where mail regarding problems should be sent",
	"condor-admin"
);
Macro condor_developers(
	"CONDOR_DEVELOPERS",
	"Address of condor development team",
	"condor-admin@cs.wisc.edu"
);
Macro local_dir(
	"LOCAL_DIR",
	"Directory for things local to an individual machine",
	"$(TILDE)"
);
Macro has_afs(
	"HAS_AFS",
	"Does this machine run AFS?",
	"TRUE"
);
Macro use_afs(
	"USE_AFS",
	"Do you want to use AFS for condor file access?",
	"FALSE"
);
Macro fs_path(
	"FS_PATHNAME",
	"Pathname of the AFS \"fs\" command",
	"/usr/afsws/bin/fs"
);
Macro vos_path(
	"VOS_PATHNAME",
	"Pathname of the AFS \"vos\" command",
	"/usr/afsws/etc/vos"
);
Macro mailer_path(
	"MAIL",
	"Pathname of program to send mail",
#if defined(SUNOS41)
	"/usr/ucb/mail"
#else
	"/bin/mail"
#endif
);
Macro uid_domain(
	"UID_DOMAIN",
	"Internet domain of machines sharing a common UID space",
	"cs.wisc.edu"
);
Macro filesystem_domain(
	"FILESYSTEM_DOMAIN",
	"Internet domain of machines sharing a common file space",
	"cs.wisc.edu"
);

int
main()
{
	ConfigFile	*file;
	char		dir[_POSIX_PATH_MAX];
	int			we_have_afs = 0;
	int			we_have_fs_domain = 0;

		// Customize the macros by dialoging with the user
	condor_host.init( check_host );
	condor_admin.init();
	condor_developers.init();
	release_dir.init( check_dir );
	local_dir.init( check_dir );
	if( confirm("Does this machine run AFS? ") ) {
		we_have_afs = 1;
		has_afs.init( TRUE );
		if( confirm("Do you want to use AFS for condor file access? ") ) {
			use_afs.init( TRUE );
		} else {
			use_afs.init( FALSE );
		}
		fs_path.init( check_exec );
		vos_path.init( check_exec );
	} else {
		has_afs.init( FALSE );
	}
	mailer_path.init( check_exec );
	uid_domain.init();
	if( confirm("Does this site have a common filesystem? ") ) {
		we_have_fs_domain = 1;
		filesystem_domain.init();
	}

		// Make user confirm that everything is correct
	printf( "Please Confirm:\n" );
	printf( "\t%s\n", release_dir.gen() );
	printf( "\t%s\n", condor_host.gen() );
	printf( "\t%s\n", condor_admin.gen() );
	printf( "\t%s\n", condor_developers.gen() );
	printf( "\t%s\n", local_dir.gen() );
	printf( "\t%s\n", has_afs.gen() );
	printf( "\t%s\n", use_afs.gen() );
	if( we_have_afs ) {
		printf( "\t%s\n", fs_path.gen() );
		printf( "\t%s\n", vos_path.gen() );
	}
	printf( "\t%s\n", mailer_path.gen() );
	printf( "\t%s\n", uid_domain.gen() );
	if( we_have_fs_domain ) {
		printf( "\t%s\n", filesystem_domain.gen() );
	}
	if( !confirm("Are all the above settings correct? ") ) {
		printf( "Configuration Aborted\n" );
		return 1;
	}

		// Prepare to customize the files in the "lib" directory.
	strcpy( dir, release_dir.get_val() );
	strcat( dir, "/lib" );
	file = new ConfigFile( dir );

		// Customize "condor_config"
	file->begin( "condor_config" );
	file->add_macro( release_dir.gen() );
	file->add_macro( condor_host.gen() );
	file->add_macro( condor_admin.gen() );
	file->add_macro( condor_developers.gen() );
	file->add_macro( local_dir.gen() );
	file->add_macro( has_afs.gen() );
	if( we_have_afs ) {
		file->add_macro( use_afs.gen() );
		file->add_macro( fs_path.gen() );
		file->add_macro( vos_path.gen() );
	}
	file->add_macro( mailer_path.gen() );
	file->add_macro( uid_domain.gen() );
	if( we_have_fs_domain ) {
		file->add_macro( filesystem_domain.gen() );
	}
	file->end();

		// Customize "config_off"
	file->begin( "config_off" );
	file->add_macro( release_dir.gen(TRUE) );
	file->end();

		// Customize "config_on"
	file->begin( "config_on" );
	file->add_macro( release_dir.gen(TRUE) );
	file->end();

		// Prepare to customize the files in the "bin" directory.
	delete file;
	strcpy( dir, release_dir.get_val() );
	strcat( dir, "/bin" );
	file = new ConfigFile( dir );

		// Customize "condor_init"
	file->begin( "condor_init" );
	file->add_macro( "#!/bin/csh -f" );
	file->add_macro( "" );
	file->add_macro( release_dir.gen(TRUE) );
	file->end();

		// Customize "condor_throttle"
	file->begin( "condor_throttle" );
	file->add_macro( "#!/bin/csh -f" );
	file->add_macro( "" );
	file->add_macro( release_dir.gen(TRUE) );
	file->end();

	delete file;
	printf( "\nCustomization Complete\n" );

	return 0;
}

/*
  Check a directory for validity.  We only check that this is a real
  directory on the system - no permissions or other reasonableness
  criteria are checked.  The directory name may contain the component
  $(TILDE) which is interpreted to mean "~condor".
*/
int
check_dir( const char *path )
{
	struct stat	buf;
	char		*tmp;

	if( path[0] != '/' && path[0] != '$' ) {
		printf( "Must supply a full pathname starting with \"/\"\n" );
		return FALSE;
	}

	tmp = xlate_tilde( path );

	if( stat(tmp,&buf) < 0 ) {
		printf( "Warning: \"%s\" is not a valid directory\n", path );
		return FALSE;
	}

	if( S_ISDIR(buf.st_mode) ) {
		return TRUE;
	} else {
		printf( "Warning: \"%s\" is not a valid directory\n", path );
		return FALSE;
	}
}

/*
  Translate a pathname which may contain $(TILDE) to a simple pathname.
*/
char *
xlate_tilde( const char *path )
{
	static char answer[_POSIX_PATH_MAX];
	char	*component;
	char	*tmp;

	tmp = new char[ strlen(path) + 1 ];
	strcpy( tmp, path );

	component = strtok( tmp, "/" );
	if( strcmp("$(TILDE)",component) == MATCH ) {
		component = lookup_tilde( component, "condor" );
	}
	strcpy( answer, "/" );
	strcat( answer, component );

	while( component = strtok(NULL,"/") ) {
		if( strcmp("$(TILDE)",component) == MATCH ) {
			component = lookup_tilde( component, "condor" );
		}
		strcat( answer, "/" );
		strcat( answer, component );
	}
	delete [] tmp;
	return answer;
}

/*
  Lookup the given user's home directory.
*/
#include <pwd.h>
char *
lookup_tilde( char *orig,  const char *user )
{
	struct passwd *entry;

	if( (entry = getpwnam(user)) == NULL ) {
		printf( "Warning: Can't find user \"%s\" in passwd file\n", user );
		return orig;
	}
	return entry->pw_dir + 1;
}

extern "C" {
#	include <netdb.h>
}
int check_host( const char *name )
{
	struct hostent	*hp;

	if( gethostbyname(name) ) {
		return TRUE;
	} else {
		printf( "Warning: Can't locate host \"%s\"\n", name );
		return FALSE;
	}
}

/*
  Check a pathname for an executable program for validity.  The
  pathname may contain the component $(TILDE) which is interpreted to
  mean "~condor".
*/
int
check_exec( const char *path )
{
	struct stat	buf;
	char		*tmp;

	if( path[0] != '/' && path[0] != '$' ) {
		printf( "Must supply a full pathname starting with \"/\"\n" );
		return FALSE;
	}

	tmp = xlate_tilde( path );

	if( stat(tmp,&buf) < 0 ) {
		printf( "Warning: \"%s\" is not a valid pathname\n", path );
		return FALSE;
	}

	if( S_IXUSR & buf.st_mode ) {
		return TRUE;
	} else {
		printf( "Warning: \"%s\" is not an executable file\n", path );
		return FALSE;
	}
}
