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

#ifndef _CHECKPOINT_MANIFEST_H
#define _CHECKPOINT_MANIFEST_H

///
// #include <string>
// #include <filesystem>
// #include "manifest.h"
//

namespace manifest {

    // If `fileName` matches `/^MANIFEST.(d\+)$/`, return (\d+) as an int;
    // otherwise return -1.
    int getNumberFromFileName( const std::string & fileName );

    // Return true if the SHA256 checksum on the last line of this MANIFEST
    // file is SHA256 checksum of the previous lines in this MANIFEST file.
    bool validateManifestFile( const std::string & fileName );

    // Return true if each line in fileName lists the correct SHA256 checksum
    // for the file it lists.  Returns false it the file doesn't exist or
    // doesn't have at least two lines in it.
    //
    // MANIFEST files contain relative paths, so this function must be run
    // from the same relative path as was used to generate the MANIFEST file
    // (which should always be the containing directory).
    bool validateFilesListedIn( const std::string & fileName, std::string & error );

    // Returns the line after its first space (and first '*', if present),
    // or the empty string if the line contains no space.
    std::string FileFromLine( const std::string & manifestLine );

    // Return the line up to its first space, or the whole line if it
    // contains no space.
    std::string ChecksumFromLine( const std::string & manifestLine );

    // Creates a MANIFEST file at `manifestFileName` for the directory
    // tree rooted at `path`.
    bool createManifestFor(
        const std::string & path,
        const std::string & manifestFileName,
        std::string & error );

    // Assuming a valid MANIFEST file at `manifestFileName`, invokes
    // the script implied by `checkpointDestination` to delete the files
    // listed in the MANIFEST (aside from `manifestFileName`) from
    // `checkpointDestination`, passing jobAdPath (among other things)
    // on the script's command line.
    bool deleteFilesStoredAt(
      const std::string & checkpointDestination,
      const std::string & manifestFileName,
      const std::filesystem::path & jobAdPath,
      std::string & error,
      bool wasFailedCheckpoint = false );
}

#endif /* _CHECKPOINT_MANIFEST_H */
