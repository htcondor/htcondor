/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
