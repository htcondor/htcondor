#define _POSIX_SOURCE

#include "afs.h"
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "subproc.h"
#include <string.h>

static char *_FileName_ = __FILE__;     // Used by EXCEPT (see condor_debug.h)

AFS_Info::AFS_Info()
{
	char	*tmp;

	my_cell_name = 0;

		// find out if we're running AFS here
	if( (tmp=param("HAS_AFS")) == NULL ) {
		has_afs = FALSE;
		return;
	}
	if( tmp[0] != 't' || tmp[0] != 'T' ) {
		has_afs = TRUE;
	} else {
		has_afs = FALSE;
		return;
	}

		// get pathname for the "fs" program
	if( (tmp=param("FS_PATHNAME")) == NULL ) {
		EXCEPT( "FS_PATHNAME not defined" );
	}
	strcpy( fs_pathname, tmp );

		// get pathname for the "vos" program
	if( (tmp=param("VOS_PATHNAME")) == NULL ) {
		EXCEPT( "VOS_PATHNAME not defined" );
	}
	strcpy( vos_pathname, tmp );

	my_cell_name = 0;
}

AFS_Info::~AFS_Info()
{
	delete [] my_cell_name;
}

void
AFS_Info::display()
{
	if( !has_afs ) {
		dprintf( D_ALWAYS, "This machine doesn't run AFS\n" );
		return;
	}
	dprintf( D_ALWAYS, "fs_pathname: \"%s\"\n", fs_pathname );
	dprintf( D_ALWAYS, "vos_pathname: \"%s\"\n", vos_pathname );
	if( my_cell_name ) {
		dprintf( D_ALWAYS, "my_cell_name: \"%s\"\n", my_cell_name );
	} else {
		dprintf( D_ALWAYS, "my_cell_name: \"%s\"\n", "(NULL)" );
	}
}

char *
AFS_Info::my_cell()
{
	char	*ptr;

	if( !my_cell_name ) {

			// find out what AFS cell we're in
		if( (ptr=find_my_cell()) == NULL ) {
			return NULL;
		}
		my_cell_name = new char[ strlen(ptr) + 1 ];
		strcpy( my_cell_name, ptr );
	}
	return my_cell_name;
}

/*
  Use the "fs wscell" command to find the AFS cell of this host.
*/
char *
AFS_Info::find_my_cell()
{
	char 	*answer;
	char	buf[_POSIX_PATH_MAX ];
	FILE	*fp;
	char	*ptr;
	SubProc	p( fs_pathname, "wscell", "r" );

	if( !has_afs ) {
		return NULL;
	}
	return p.parse_output( "belongs to cell", "'", "'" );
}


/*
  Use the "fs whichcell" command to find the AFS cell of the given pathname.
*/
char *
AFS_Info::which_cell( const char *path )
{
	char 	*answer;
	char	args[ _POSIX_PATH_MAX ];
	FILE	*fp;
	SubProc	*fs;

	if( !has_afs ) {
		return NULL;
	}

	sprintf( args, "whichcell %s", path );
	fs = new SubProc( fs_pathname, args, "r" );
	answer = fs->parse_output("in cell", "'", "'" );
	delete fs;
	return answer;
}

/*
  Use the "fs examine" command to find the AFS volume of the given pathname.
*/
char *
AFS_Info::which_vol( const char *path )
{
	char 	*answer;
	char	args[ _POSIX_PATH_MAX ];
	FILE	*fp;
	char	*ptr;
	SubProc	*fs;

	if( !has_afs ) {
		return NULL;
	}

	sprintf( args, "examine %s", path );
	fs = new SubProc( fs_pathname, args, "r" );
	answer = fs->parse_output( "Volume status", "named ", "\n" );
	delete fs;
	return answer;
}

char *
AFS_Info::which_srvr( const char *path )
{
	SubProc	*vos;
	char	*answer;
	char	args[ _POSIX_PATH_MAX ];
	char	*vol;

	if( !has_afs ) {
		return NULL;
	}

	vol = which_vol( path );

	sprintf( args, "examine %s", vol );
	vos = new SubProc( vos_pathname, args, "r" );
	answer = vos->parse_output( "   server", "server ", " partition" );
	delete vos;
	return answer;
}


/*
  The following routines provide a simple C interface to this class.
*/

static AFS_Info *MyInfo;

extern "C" {

char *
get_host_cell()
{
	if( !MyInfo ) {
		MyInfo = new AFS_Info();
	}

	return MyInfo->my_cell();
}

char *get_file_cell( const char *pathname )
{
	if( !MyInfo ) {
		MyInfo = new AFS_Info();
	}
	return MyInfo->which_cell( pathname );
}

char *get_file_vol( const char *pathname )
{
	if( !MyInfo ) {
		MyInfo = new AFS_Info();
	}
	return MyInfo->which_vol( pathname );
}

char *get_file_srvr( const char *pathname )
{
	if( !MyInfo ) {
		MyInfo = new AFS_Info();
	}
	return MyInfo->which_srvr( pathname );
}

} /* end of extern "C" */
