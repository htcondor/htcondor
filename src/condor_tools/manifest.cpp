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

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

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
    return 1;
}

bool
generateFileForDir( const std::string & dirName, std::string & error ) {
    std::string manifestText;

    DIR * dir = opendir( dirName.c_str() );
    if( dir == NULL ) {
        formatstr( error, "opendir(%s) failed: %s (%d)", dirName.c_str(), strerror(errno), errno );
        return false;
    }

    struct dirent * dentry;
    while( (dentry = readdir(dir)) != NULL ) {
        switch( dentry->d_type ) {
            case DT_BLK:
            case DT_CHR:
            case DT_DIR:
            case DT_FIFO:
            case DT_UNKNOWN:
                continue;
            default:
                break;
        }

        std::string fileName = dentry->d_name;
        // if( fileName == "." || fileName == ".." ) { continue; }

        std::string fileHash;
        if(! compute_file_checksum( fileName, fileHash)) {
            formatstr( error, "Failed to compute file (%s) checksum", fileName.c_str() );
            return false;
        }
        // The '*' marks the hash as having been computed in binary mode.
        formatstr_cat(
            manifestText, "%s *%s\n", fileHash.c_str(), fileName.c_str()
        );
    }
    closedir(dir);

    std::string manifestFileName = "MANIFEST.0000";
    if(! htcondor::writeShortFile( manifestFileName, manifestText )) {
        formatstr( error, "Failed to write manifest file" );
        return false;
    }

    std::string manifestHash;
    if(! compute_file_checksum( manifestFileName, manifestHash )) {
        formatstr( error, "Failed to compute MANIFEST checksum" );
        unlink( manifestFileName.c_str() );
        return false;
    }

    std::string append;
    formatstr( append,
        "%s *%s\n", manifestHash.c_str(), manifestFileName.c_str()
    );
    if(! htcondor::appendShortFile( manifestFileName, append )) {
        formatstr( error, "Failed to append MANIFEST checksum to MANIFEST file" );
        return false;
    }

    return true;
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
    } else if( function == "validateFile" ) {
        bool valid = manifest::validateFile( argument );
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
    } else if( function == "generateFileForDir" ) {
        std::string error;
        bool ok = generateFileForDir( argument, error );
        if(! ok) {
            fprintf( stdout, "%s\n", error.c_str() );
        }
        return ok ? 0 : 1;
    } else {
        return usage( argv[0] );
    }
}
