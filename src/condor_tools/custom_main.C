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
#include "condor_fix_unistd.h"		// get geteuid()
#include <stdlib.h>		// for system() (yuck!)
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include "custom.h"

const int MATCH = 0;	// for strcmp()

int check_dir( const char *path );
int check_exec( const char *path );
char * xlate_tilde( const char *path );
char * lookup_tilde( char *orig,  const char *user );
int check_host( const char *name );
int set_all_permissions(const char *path);
int set_a_permission(const int op,const char *val, const char *path);
const int CHMOD_OP = 1;
const int CHOWN_OP = 2;

/*
  All the macros which are to be customized should go here.  Parameters
  are:
	Macro name
	Description
	Default value
*/
Macro release_dir(
	"RELEASEDIR",
	"Full pathname of the parent directory of Condor's \"bin\" and \"lib\"\n\
directories.  For example: specifying \"/usr/psup/condor\" means that\n\
users will find Condor libraries and executables in \"/usr/psup/condor/lib\"\n\
and \"/usr/psup/condor/bin\" respectively.",
	"/usr/local/condor"
);
Macro condor_host(
	"CONDOR_HOST",
	"Host where central manager should run, this should be a fully qualified\n\
internet domain name.",
	"condor-cm.my.domain.edu"
);
Macro condor_admin(
	"CONDOR_ADMIN",
"Address of the local condor administrator, for automatically generated\n\
mail regarding problems",
	"condor-admin"
);
Macro condor_developers(
	"CONDOR_DEVELOPERS",
"Address of condor development team at University of Wisconsin, for\n\
automatically generated mail regarding status of Condor at your site.\n\
You should _NOT_ have to change this; accept the default unless you know better.\n",
	"condor-admin@cs.wisc.edu"
);
Macro local_dir(
	"LOCAL_DIR",
"Parent directory for machine specific directories \"spool\", \"log\", and\n\
\"execute\".  Specifying $(TILDE) means condor's home directory",
	"$(TILDE)"
);
Macro has_afs(
	"HAS_AFS",
	"Does this machine run AFS?",
	"FALSE"
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
	"Pathname of user level program Condor software should use to send mail",
#if defined(SUNOS41) || defined(ULTRIX43)
	"/usr/ucb/mail"
#elif defined(OSF1)
	"/usr/ucb/mailx"
#elif defined(IRIX62)
	"/usr/sbin/mailx"
#elif defined(IRIX53)
	"/usr/sbin/Mail"
#elif defined(HPUX)
	"/bin/mailx"
#else
	"/bin/mail"
#endif
);
Macro uid_domain(
	"UID_DOMAIN",
	"Internet domain of machines sharing a common UID space.  If you\n\
specify a UID space, Condor will execute user jobs under the\n\
UID of the submitting user - otherwise Condor will execute user\n\
jobs with the UID \"nobody\".  Specify \"none\" unless all machines\n\
in your pool are guaranteed to have consistent UIDs.",
	"my.domain.edu"
);
Macro filesystem_domain(
	"FILESYSTEM_DOMAIN",
	"\nInternet domain of machines sharing a common NFS file space",
	"my.domain.edu"
);
Macro use_nfs(
	"USE_NFS",
	"Do you want to use NFS for condor file access?",
	"FALSE"
);
Macro use_ckpt_server(
	"USE_CKPT_SERVER",
	"Do you want to use the checkpoint server to store checkpoint files?",
	"FALSE"
);
Macro ckpt_server_host(
	"CKPT_SERVER_HOST",
	"Host where the checkpoint server should run, this should be a fully\nqualified internet domain name.",
	"condor-cs.my.domain.edu"	
);

int
main()
{
	ConfigFile	*file;
	char		dir[_POSIX_PATH_MAX];
	int			we_have_afs = 0;
	int			we_have_fs_domain = 0;
	int			no_condor_user = 0;

		// Check and see if we are running as root
	if ( geteuid() != 0 ) {
		printf("\ncondor_customize needs to run as super-user (root) in order to\nset default permission/ownerships on Condor files.  Please re-run me\nas root.  \n\n");
		if ( !confirm("Do you wish to continue anyhow, even though you are not root ? ") ) {
			printf("Goodbye!\n");
			exit(1);
		}
	}

		// Check and see if there is a user condor and a group condor
	if( getpwnam("condor") == NULL ) {
		no_condor_user = 1;
		printf( "Warning: Can't find user \"condor\" in system passwd file\n" );
	}
	if( getgrnam("condor") == NULL ) {
		no_condor_user = 1;
		printf( "Warning: Can't find group \"condor\" in system group file\n" );
	}

	if ( no_condor_user ) {
		printf( "This will cause problems later on when I try to set \n");
		printf( "ownerships and permissions on the Condor files. You really\n");
		printf( "should exit now and create a user \"condor\" and a group \"condor\"\n\n");
		if ( confirm("Do you wish to exit condor_customize now to fix these problems? ") ) {
			printf("Goodbye!\n");
			exit(1);
		}
	}

		// Customize the macros by dialoging with the user
	condor_host.init( check_host );
	condor_admin.init();
	condor_developers.init();
	release_dir.init( check_dir );
	local_dir.init( check_dir );
	if( confirm("Will all machines in your pool participate in a common file system via AFS? ") ) {
		we_have_afs = 1;
		has_afs.init( TRUE );
		if( confirm("\nDo you want to use AFS for condor file access? (This may not have\n a positive impact on performance.)") ) {
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
	if( confirm("Will all machines in your pool participate in a common file system via NFS? ") ) {
		we_have_fs_domain = 1;
		filesystem_domain.init();
		if( confirm("\nDo you want to use NFS for condor file access? (This may not have\n a positive impact on performance.)") ) {
			use_nfs.init( TRUE );
		} else {
			use_nfs.init( FALSE );
		}
	}

		// Make user confirm that everything is correct
	printf( "Please Confirm:\n" );
	printf( "\t%s\n", release_dir.gen() );
	printf( "\t%s\n", condor_host.gen() );
	printf( "\t%s\n", condor_admin.gen() );
	printf( "\t%s\n", condor_developers.gen() );
	printf( "\t%s\n", local_dir.gen() );
	printf( "\t%s\n", has_afs.gen() );
	if( we_have_afs ) {
		printf( "\t%s\n", use_afs.gen() );
		printf( "\t%s\n", fs_path.gen() );
		printf( "\t%s\n", vos_path.gen() );
	}
	printf( "\t%s\n", mailer_path.gen() );
	printf( "\t%s\n", uid_domain.gen() );
	if( we_have_fs_domain ) {
		printf( "\t%s\n", filesystem_domain.gen() );
		printf( "\t%s\n", use_nfs.gen() );
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

	if( !confirm("\nWould you like me to now set default ownership/permissions \non all Condor files (it's a good idea) ? ") ) {
		printf( "Permissions/Ownerships not set.  Goodbye!\n" );
		// it is not an error, just a user preference, so return ok
		return 0;
	}

	if ( set_all_permissions( release_dir.get_val() ) ) {
		printf("\nErrors encountered setting ownerships/permissions!\n");
		return 1;
	} else {
		printf("\nCompleted setting ownerships/permissions sucuessfully\n");
	}

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

/*
 Set all permissions/owners in the release_dir (bin/lib)
*/
int
set_all_permissions( const char *path )
{
	int result = 0;
	char buf[600];


	result = set_a_permission(CHOWN_OP,"condor",path);
	sprintf(buf,"%s/bin",path);
	result += set_a_permission(CHOWN_OP,"condor",buf);
	sprintf(buf,"%s/lib",path);
	result += set_a_permission(CHOWN_OP,"condor",buf);
	sprintf(buf,"%s/bin/*",path);
	result += set_a_permission(CHOWN_OP,"condor",buf);
	sprintf(buf,"%s/lib/*",path);
	result += set_a_permission(CHOWN_OP,"condor",buf);

	result += set_a_permission(CHMOD_OP,"755",path);
	sprintf(buf,"%s/bin",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
	sprintf(buf,"%s/lib",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
	sprintf(buf,"%s/bin/*",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
	sprintf(buf,"%s/lib/*",path);
	result += set_a_permission(CHMOD_OP,"644",buf);
	sprintf(buf,"%s/lib/ld",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
	sprintf(buf,"%s/lib/real-ld",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
#if defined(IRIX62)
	sprintf(buf,"%s/lib/old_ld",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
#endif
#if defined(IRIX53) || defined (IRIX62)
	sprintf(buf,"%s/lib/uld",path);
	result += set_a_permission(CHMOD_OP,"755",buf);
#endif

	// and finally set some commands to set-gid condor...
	sprintf(buf,"%s/bin/condor_globalq",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_history",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_jobqueue",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_preen",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_prio",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_q",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_rm",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_submit",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);
	sprintf(buf,"%s/bin/condor_summary",path);
	result += set_a_permission(CHMOD_OP,"g+s",buf);

	return(result);

}


int 
set_a_permission(const int op, const char *value, const char *path)
{
	int result = 0;
	char buf[600];

	if ( op == CHMOD_OP ) {
		printf("Setting permissions on %s to %s ... ",path,value);
		sprintf(buf,"/bin/chmod %s %s",value,path);
		result = system(buf);
		if ( result )
			printf("FAILED!\n");
		else
			printf("Done.\n");
	}

	if ( op == CHOWN_OP ) {
		printf("Setting ownerships on %s to %s ... ",path,value);
		sprintf(buf,"/bin/chown %s %s",value,path);
		result = system(buf);
		sprintf(buf,"/bin/chgrp %s %s",value,path);
		result += system(buf);
		if ( result )
			printf("FAILED!\n");
		else
			printf("Done.\n");
	}

	return(result);
}
