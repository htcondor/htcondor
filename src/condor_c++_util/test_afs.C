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
#include "condor_debug.h"
#include "condor_config.h"
#include "afs.h"



const char *TestFile1 = "/p/condor/home";
const char *TestFile2 = "/afs";

void print_cell( const char *file_name );
void print_vol( const char *file_name );
void print_srvr( const char *file_name );
void check_c( const char *path );
void check_cplusplus( const char *path );

AFS_Info *Info;

main( int argc, char *argv[] )
{

	config();
	Info = new AFS_Info();

	printf( "My cell is \"%s\"\n", Info->my_cell() );
	printf( "Using C, my cell is \"%s\"\n", get_host_cell() );
	printf( "\n" );

	check_cplusplus( TestFile1 );
	check_cplusplus( TestFile2 );
	check_cplusplus( "/" );

	check_c( TestFile1 );
	check_c( TestFile2 );
	check_c( "/" );


	printf( "==============================================\n" );
}

void
check_c( const char *path )
{
	printf( "File \"%s\" (using C interface)\n", path );
	printf( "  Cell: \"%s\"\n", get_file_cell(path) );
	printf( "  Volume: \"%s\"\n", get_file_vol(path) );
	printf( "  Server: \"%s\"\n", get_file_srvr(path) );
}

void
check_cplusplus( const char *path )
{
	printf( "File \"%s\" (using C++ interface)\n", path );
	printf( "  Cell: \"%s\"\n", Info->which_cell(path) );
	printf( "  Volume: \"%s\"\n", Info->which_vol(path) );
	printf( "  Server: \"%s\"\n", Info->which_srvr(path) );
}

void
print_vol( const char *file_name )
{
	char *answer;

	answer = Info->which_vol( file_name );
	if( answer ) {
		printf( "  Volume: \"%s\"\n", answer );
	} else {
		printf( "  Volume: (NULL)\n" );
	}
}

void
print_cell( const char *file_name )
{
	char *answer;

	answer = Info->which_cell( file_name );
	if( answer ) {
		printf( "AFS cell of \"%s\" is \"%s\"\n", file_name, answer );
	} else {
		printf( "AFS cell of \"%s\" is (NULL)\n", file_name );
	}
}

void
print_srvr( const char *file_name )
{
	char *answer;

	answer = Info->which_srvr( file_name );
	if( answer ) {
		printf( "AFS server of \"%s\" is \"%s\"\n", file_name, answer );
	} else {
		printf( "AFS server of \"%s\" is (NULL)\n", file_name );
	}
}


extern "C" int SetSyscalls( int mode ) { return mode; }
