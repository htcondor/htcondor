/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.	You may
 * obtain a copy of the License at
 * 
 *		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef WIN32
 
#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_constants.h"
#include "condor_md.h"
#include "directory_util.h"
#include "filename_tools.h"
#include "stat_wrapper.h"
#include <string> 

#ifdef WIN32
   #define stat _stat
#else
   #include <unistd.h>
#endif


// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
    #define file_contains contains_anycase
    #define lutimes _lutimes
    #define realpath(path, resolved_path) _fullpath((resolved_path), (path), _MAX_PATH)
    #define symlink(source, target) CreateSymbolicLink(source, target, 0)
#else
    #define file_contains contains
#endif

#ifdef WIN32
    static const mode_t S_IROTH = mode_t(_S_IREAD);     ///< read by *USER*
#endif

using namespace std;

namespace {	// Anonymous namespace to limit scope of names to this file
	
const int HASHNAMELEN = 17;


static string MakeHashName(const char* fileName, time_t fileModifiedTime) {

	unsigned char hashResult[HASHNAMELEN * 3]; // Allocate extra space for safety.

    // Convert the modified time to a string object
    std::string modifiedTimeStr = std::to_string((long long int) fileModifiedTime);

    // Create a new string which will be the source for our hash function.
    // This will append two strings:
    // 1. Full path to the file
    // 2. Modified time of the file
    unsigned char* hashSource = new unsigned char[strlen(fileName) 
                                    + strlen(modifiedTimeStr.c_str()) + 1];
    strcpy( (char*) hashSource, fileName );
    strcat( (char*) hashSource, modifiedTimeStr.c_str() );

    // Now calculate the hash
	memcpy(hashResult, Condor_MD_MAC::computeOnce(hashSource,
		strlen((const char*) hashSource)), HASHNAMELEN);
	char entryHashName[HASHNAMELEN * 2]; // 2 chars per hex byte
	entryHashName[0] = '\0';
	char letter[3];
	for (int ind = 0; ind < HASHNAMELEN - 1; ++ind) {
		sprintf(letter, "%x", hashResult[ind]);
		strcat(entryHashName, letter);
	}

	return entryHashName;
}


static bool MakeLink(const char* srcFile, const string &newLink) {
	const char *const webRootDir = param("WEB_ROOT_DIR");
	if (webRootDir == NULL) {
		dprintf(D_ALWAYS, "WEB_ROOT_DIR not set\n");
		return (false);
	}
	char goodPath[PATH_MAX];
	if (realpath(webRootDir, goodPath) == NULL) {
		dprintf(D_ALWAYS, "WEB_ROOT_DIR not a valid path: %s\n", webRootDir);
		return (false);
    }
	StatWrapper fileMode;
	bool fileOK = false;
	if (fileMode.Stat(srcFile) == 0) {
		const StatStructType *statrec = fileMode.GetBuf();
		if (statrec != NULL)
			fileOK = (statrec->st_mode & S_IROTH); // Verify readable by all
	}
	if (fileOK == false) {
		dprintf(D_ALWAYS,
			"Cannot transfer -- public input file not world readable: %s\n", srcFile);
		return (false);
	}
	const char *const targetLink = dircat(goodPath, newLink.c_str()); // needs to be freed
	bool retVal = false;
	if (targetLink != NULL) {
		if (fileMode.Stat(targetLink, StatWrapper::STATOP_LSTAT) == 0) { // Check if target already exists
			retVal = true; // Good enough if link exists, ok if update fails
			// It is assumed that existing link points to srcFile
			const StatStructType *statrec = fileMode.GetBuf();
			if (statrec != NULL) {
				time_t filemtime = statrec->st_mtime;
				// Must be careful to operate on link, not target file.
				if ((time(NULL) - filemtime) > 3600 && lutimes(targetLink, NULL) != 0) {
					// Update mod time, but only once an hour to avoid excess file access
		            dprintf(D_ALWAYS, "Could not update modification date on %s, error = %s\n",
			            targetLink, strerror(errno));
		 	    }
			} else 
                dprintf(D_ALWAYS, "Could not stat file %s\n", targetLink);
		} else if (symlink(srcFile, targetLink) == 0) {
			retVal = true;
		} else dprintf(D_ALWAYS, "Could not link %s to %s, error = %s\n", srcFile,
			 targetLink, strerror(errno));
		delete [] targetLink;
	}
	return (retVal);
}

} // end namespace


static string MakeAbsolutePath(const char* path, const char* initialWorkingDir) {
	if (is_relative_to_cwd(path)) {
		string fullPath = initialWorkingDir;
		fullPath += DIR_DELIM_CHAR;
		fullPath += path;
		return (fullPath);
	}
	return (path);
}


void ProcessCachedInpFiles(ClassAd *const Ad, StringList *const InputFiles,
	StringList &PubInpFiles) {

	char *initialWorkingDir = NULL;
    const char *path;
	MyString remap;
    struct stat fileStat;
    time_t fileModifiedTime;

	if (PubInpFiles.isEmpty() == false) {
		const char *webSrvrPort = param("WEB_SERVER_PORT");

        // If a web server address is not defined, exit quickly. The file
        // transfer will go on using the regular CEDAR porotocl.
        if(!webSrvrPort) {
            dprintf(D_FULLDEBUG, "mk_cache_links.cpp: WEB_SERVER_PORT parameter not defined!\n");
            return;
        }

        // Build out the base URL for public files
		string url = "http://";
        url += webSrvrPort; 
        url += "/";

		PubInpFiles.rewind();
		
		if (Ad->LookupString(ATTR_JOB_IWD, &initialWorkingDir) != 1) {
			dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Job ad did not have an initialWorkingDir!\n");
			return;
		}
		while ((path = PubInpFiles.next()) != NULL) {
            // Determine the full path of the file to be transferred
			string fullPath = MakeAbsolutePath(path, initialWorkingDir);

            // Determine the time last modified of the file to be transferred
            if( stat( fullPath.c_str(), &fileStat ) == 0 ) {
                struct timespec fileTime = fileStat.st_mtim;
                fileModifiedTime = fileTime.tv_sec;
            }

			string hashName = MakeHashName( fullPath.c_str(), fileModifiedTime );
			if (MakeLink(fullPath.c_str(), hashName)) {
                InputFiles->remove(path); // Remove plain file name from InputFiles
                remap += hashName;
                remap += "=";
                remap += basename(path);
                remap += ";";
                hashName = url + hashName;
                const char *const namePtr = hashName.c_str();
                if ( !InputFiles->file_contains(namePtr) ) {
		            InputFiles->append(namePtr);
		            dprintf(D_FULLDEBUG, "Adding url to InputFiles: %s\n", namePtr);
	            } 
                else dprintf(D_FULLDEBUG, "url already in InputFiles: %s\n", namePtr);
			}
		}
		free( initialWorkingDir );
		if ( remap.Length() > 0 ) {
			MyString remapnew;
			char *buf = NULL;
			if (Ad->LookupString(ATTR_TRANSFER_INPUT_REMAPS, &buf) == 1) {
	            remapnew = buf;
	            free(buf);
	            buf = NULL;
	            remapnew += ";";
			} 
			remapnew += remap;
			if (Ad->Assign(ATTR_TRANSFER_INPUT_REMAPS, remap.Value()) == false) {
	            dprintf(D_ALWAYS, "Could not add to jobAd: %s\n", remap.c_str());
			}
		}
	} 
    else	
        dprintf(D_FULLDEBUG, "No public input files.\n");
}

#endif

