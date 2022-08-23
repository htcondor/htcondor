/***************************************************************
 *
 * Copyright (C) 2022, Condor Team, Computer Sciences Department,
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

#include <stdio.h>
#include <string>

#include <errno.h>
#include <string.h>

#include "stl_string_utils.h"

#include "manifest.h"
#include "checksum.h"
#include "shortfile.h"

int
usage( const char * argv0 ) {
    fprintf( stderr, "%s: [function] [argument]\n", argv0 );
    fprintf( stderr, "where [function] is one of:\n" );
    fprintf( stderr, "  getNumberFromFileName\n" );
    fprintf( stderr, "  validateFile\n" );
    fprintf( stderr, "  validateFilesListedIn\n" );
    fprintf( stderr, "  FileFromLine\n" );
    fprintf( stderr, "  ChecksumFromLine\n" );
    fprintf( stderr, "  compute_file_sha256_checksum\n" );
    return 1;
}

int
main( int argc, char ** argv ) {
    if( argc != 3 ) {
        return usage( argv[0] );
    }

    std::string function = argv[1];
    std::string argument = argv[2];

    if( function == "getNumberFromFileName" ) {
        int no = manifest::getNumberFromFileName( argument );
        fprintf( stdout, "%d\n", no );
        return no;
    } else if( function == "validateManifestFile" ) {
        bool valid = manifest::validateManifestFile( argument );
        fprintf( stdout, "%s\n", valid ? "true" : "false" );
        return valid ? 0 : 1;
    } else if( function == "validateFilesListedIn" ) {
        std::string error;
        bool valid = manifest::validateFilesListedIn( argument, error );
        fprintf( stdout, "%s\n", valid ? "true" : "false" );
        if(! valid) {
            fprintf( stdout, "%s\n", error.c_str() );
        }
        return valid ? 0 : 1;
    } else if( function == "FileFromLine" ) {
        std::string file = manifest::FileFromLine( argument );
        fprintf( stdout, "%s\n", file.c_str() );
        return 0;
    } else if( function == "ChecksumFromLine" ) {
        std::string checksum = manifest::ChecksumFromLine( argument );
        fprintf( stdout, "%s\n", checksum.c_str() ) ;
        return 0;
    } else if( function == "compute_file_sha256_checksum" ) {
        std::string checksum;
        bool ok = compute_file_sha256_checksum( argument, checksum );
        if( ok ) {
            fprintf( stdout, "%s *%s\n", checksum.c_str(), argument.c_str() );
        }
        return ok ? 0 : 1;
    } else {
        return usage( argv[0] );
    }
}
