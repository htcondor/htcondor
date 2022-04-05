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

#include "condor_common.h"
#include "FilesOperations.h"
#include "Utils.h"
// for 'param' function
#include "condor_config.h"
// for rotate_file
#include "util_lib_proto.h"
// for 'StatWrapper'
#include "stat_wrapper.h"

// Manipulation functions
/* Function    : rotateFile
 * Arguments   : filePath                - file to rotate
 *               temporaryFilesExtension - temporary (relatively to 'filePath')
 *                                         file extension
 * Return value: bool - success/failure value
 * Description : rotates specified file with its temporary counterpart
 */
bool
FilesOperations::rotateFile( const char* filePath, 
						     const char* temporaryFilesExtension )
{
    dprintf( D_ALWAYS, "FilesOperations::rotateFile %s with extension %s "
					   "started\n",
             filePath, temporaryFilesExtension );
    std::string temporaryFilePath = filePath;

    temporaryFilePath += ".";
    temporaryFilePath += temporaryFilesExtension;

    if( rotate_file( temporaryFilePath.c_str( ), filePath ) < 0 ) {
        dprintf( D_ALWAYS, "FilesOperations::rotateFile "
                           "cannot rotate file %s\n", filePath );
        return false;
    }
    return true;
}
/* Function    : copyFile
 * Arguments   : filePath                - file to copy
 *               temporaryFilesExtension - temporary (relatively to 'filePath')
 *                                         file extension
 * Return value: bool - success/failure value
 * Description : copies specified file to its temporary counterpart
 */
bool
FilesOperations::copyFile( const char* filePath, 
						   const char* temporaryFilesExtension)
{
    dprintf( D_ALWAYS, "FilesOperations::copyFile %s with extension %s "
					   "started\n", filePath, temporaryFilesExtension );
    std::string temporaryFilePath = filePath;

    temporaryFilePath += ".";
    temporaryFilePath += temporaryFilesExtension;

    if( copy_file( filePath, temporaryFilePath.c_str( ) ) ) {
        dprintf( D_ALWAYS, "FilesOperations::copyFile "
                           "unable to copy %s to %s\n",
                    filePath, temporaryFilePath.c_str( ) );
        return false;
    }
    return true;
}
/* Function    : unlinkFile
 * Arguments   : filePath                - file to unlink
 *               temporaryFilesExtension - temporary (relatively to 'filePath')
 *                                         file extension
 * Return value: bool - success/failure value
 * Description : unlinks specified file's temporary counterpart
 * Note        : if the file does not exist, returns true
 */
bool
FilesOperations::unlinkFile( const char* filePath, 
						     const char* temporaryFilesExtension )
{
    dprintf( D_ALWAYS, "FilesOperations::unlinkFile %s with extension %s "
					   "started\n", filePath, temporaryFilesExtension );
    std::string temporaryFilePath = filePath;

    temporaryFilePath += ".";
    temporaryFilePath += temporaryFilesExtension;

	StatWrapper statWrapper( temporaryFilePath );
	
	if ( statWrapper.GetRc( ) && statWrapper.GetErrno( ) == ENOENT ) {
		dprintf( D_ALWAYS, "FilesOperations::unlinkFile the specified file %s "
						   "does not exist\n", temporaryFilePath.c_str( ) );
		return true;
    }
	
	if( unlink( temporaryFilePath.c_str( ) ) != 0 ) {
        dprintf( D_ALWAYS, "FilesOperations::unlinkFile unable "
                           "to unlink %s, reason: %s\n",
                   temporaryFilePath.c_str( ), strerror(errno));
        return false;
    }
    return true;
}
// End of manipulation functions

/* Function    : safeManipulateFile
 * Arguments   : filePath - path to file, on which the manipulations are to 
 * 							be performed
 *				 temporaryFilesExtension - extension of temporary file,
 *               						   on which the manipulations are to 
 * 										   be performed
 *               manipulation            - specific manipulation function
 * Return value: bool - success/failure value
 * Description : performs specific manipulations on the file and its temporary
 *				 copy; tries several ('IO_RETRY_TIMES') times to handle 
 *				 various temporary OS instabilities
 */
bool
FilesOperations::safeManipulateFile( const char* filePath,
						         const char* temporaryFilesExtension,
                                 ManipulationFunction manipulation )
{
    int retryTimes = 0;

    while( retryTimes < IO_RETRY_TIMES &&
         (*manipulation)( filePath,
                          temporaryFilesExtension ) == false ) {
        retryTimes ++;
    }
    if (retryTimes == IO_RETRY_TIMES) {
        dprintf( D_ALWAYS, "FilesOperations::safeManipulateFile failed with "
						   "%s.%s %d times\n", filePath,
                temporaryFilesExtension, IO_RETRY_TIMES );
        return false;
    }
//    retryTimes = 0;
    /* files rotation (state files and version file and their
     * temporary copies) appear in the code together - there must not be any
     * heavy operations between them
     */
//    while( retryTimes < IO_RETRY_TIMES &&
//         (*manipulation)(versionFilePath.Value(),
//                         temporaryFilesExtension) == false ) {
//        retryTimes ++;
//    }
//    if (retryTimes == IO_RETRY_TIMES) {
//        dprintf( D_ALWAYS, "FilesOperations::safeManipulateFile failed with "
//							 "%s.%s %d times\n", versionFilePath.Value( ),
//                temporaryFilesExtension, IO_RETRY_TIMES );
//        return false;
//    }
    return true;
}

bool
FilesOperations::safeRotateFile( const char* filePath, 
						         const char* temporaryFilesExtension )
{
    return safeManipulateFile( filePath, temporaryFilesExtension, &rotateFile);
}

bool
FilesOperations::safeCopyFile( const char* filePath, 
					           const char* temporaryFilesExtension )
{
   return safeManipulateFile( filePath, temporaryFilesExtension, &copyFile);
}

bool
FilesOperations::safeUnlinkFile( const char* filePath,
						      const char* temporaryFilesExtension )
{
   return safeManipulateFile( filePath, temporaryFilesExtension, &unlinkFile);
}
