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
#define _POSIX_SOURCE

#include "condor_common.h"
#include "afs.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "subproc.h"
#include <string.h>

static char *_FileName_ = __FILE__;     // Used by EXCEPT (see condor_debug.h)

AFS_Info::AFS_Info()
{
	char	*tmp;

	my_cell_name = NULL;

		// find out if we're running AFS here
	has_afs = FALSE;
	if( (tmp = param("HAS_AFS")) ) {
		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			has_afs = TRUE;
		} 
		free( tmp );
	}
	if( ! has_afs ) {
		return;
	}

		// get pathname for the "fs" program
	if( (tmp=param("FS_PATHNAME")) == NULL ) {
		EXCEPT( "FS_PATHNAME not defined" );
	}
	strcpy( fs_pathname, tmp );
	free( tmp );

		// get pathname for the "vos" program
	if( (tmp=param("VOS_PATHNAME")) == NULL ) {
		EXCEPT( "VOS_PATHNAME not defined" );
	}
	strcpy( vos_pathname, tmp );
	free( tmp );
}

AFS_Info::~AFS_Info()
{
	if( my_cell_name ) delete [] my_cell_name;
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

	if( !has_afs ) {
		return NULL;
	}
	if( !my_cell_name ) {
			// find out what AFS cell we're in
		if( (ptr=find_my_cell()) == NULL ) {
			EXCEPT( "Could not find AFS cell." );
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

	// must check has_afs _first_, because if it is not true,
	// then fs_pathname is unitialized.  -Todd 7/97
	if( !has_afs ) {
		return NULL;
	}


	SubProc	p( fs_pathname, "wscell", "r" );
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

	// file might not be created yet; if answer is NULL, try to find
	// directory on AFS
	if (answer == NULL) {
		int i;
		for (i = strlen(args); i >= 0 && args[i] != '/' && args[i] != '\\'; i--);
		if (i >= 0) {
			args[i] = '\0';
			fs = new SubProc( fs_pathname, args, "r" );
			answer = fs->parse_output("in cell", "'", "'" );
			delete fs;
		}
	}

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

	// file might not be created yet; if answer is NULL, try to find
	// directory on AFS
	if (answer == NULL) {
		int i;
		for (i = strlen(args); i >= 0 && args[i] != '/' && args[i] != '\\'; i--);
		if (i >= 0) {
			args[i] = '\0';
			fs = new SubProc( fs_pathname, args, "r" );
			answer = fs->parse_output("in cell", "'", "'" );
			delete fs;
		}
	}

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
