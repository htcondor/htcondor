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

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_constants.h"
#include "condor_uid.h"
#include "condor_md.h"
#include "directory_util.h"
#include "filename_tools.h"
#include "stat_wrapper.h"
#include <string> 

#ifndef WIN32
	#include <unistd.h>
#endif

#ifdef HAVE_HTTP_PUBLIC_FILES

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
	for (int i = 0; i < HASHNAMELEN - 1; ++i) {
		sprintf(letter, "%x", hashResult[i]);
		strcat(entryHashName, letter);
	}

	return entryHashName;
}


// WARNING!  This code changes priv state.  Be very careful if modifying it.
// Do not return in the block of code where the priv has been set to either
// condor or root.  -zmiller
static bool MakeLink(const char* srcFile, const string &newLink) {

	bool retVal = false;
	const StatStructType *srcFileStat;
	const StatStructType *targetLinKStat;
	StatWrapper srcFileMode;
	StatWrapper targetLinkMode;

	// Make sure the necessary parameters are set
	std::string webRootDir;
	param(webRootDir, "HTTP_PUBLIC_FILES_ROOT_DIR");
	if(webRootDir.empty()) {
		dprintf(D_ALWAYS, "mk_cache_links.cpp: HTTP_PUBLIC_FILES_ROOT_DIR "
						"not set! Falling back to regular file transfer\n");
		return (false);
	}
	char goodPath[PATH_MAX];
	if (realpath(webRootDir.c_str(), goodPath) == NULL) {
		dprintf(D_ALWAYS, "mk_cache_links.cpp: HTTP_PUBLIC_FILES_ROOT_DIR "
			"not a valid path: %s. Falling back to regular file transfer.\n",
			webRootDir.c_str());
		return (false);
	}
	
	// Impersonate the user and open the file using safe_open(). This will allow
	// us to verify the user has privileges to access the file, and later to
	// verify the hard link points back to the same inode.

	// STARTING HERE, DO NOT RETURN FROM THIS FUNCTION WITHOUT RESETTING
	// THE ORIGINAL PRIV STATE.

	priv_state original_priv = set_user_priv();
	
	bool fileOK = false;
	if (srcFileMode.Stat(srcFile) == 0) {
		srcFileStat = srcFileMode.GetBuf();
		if (srcFileStat != NULL)
			fileOK = (srcFileStat->st_mode & S_IRUSR); // Verify readable by owner
	}
	if (fileOK == false) {
		dprintf(D_ALWAYS,
			"Cannot transfer -- public input file not readable by user: %s\n", srcFile);
		set_priv(original_priv);
		return (false);
	}

	// Create the hard link using root privileges; it will automatically get
	// owned by the same owner of the file.. If the link already exists, don't do 
	// anything at this point, we'll check later to make sure it points to the
	// correct inode.
	const char *const targetLink = dircat(goodPath, newLink.c_str()); // needs to be freed
	if (targetLink != NULL) {
		// Check if target already exists
		if (targetLinkMode.Stat(targetLink, StatWrapper::STATOP_LSTAT) == 0) { 
			// Good enough if link exists, ok if update fails
			retVal = true;
		} 
		else {
			// Now create the link as root.
			set_root_priv();
			if (link(srcFile, targetLink) == 0) {
				// so far, so good!
				retVal = true;
			} 
			else {
				dprintf(D_ALWAYS, "Could not link %s to %s, error = %s\n", srcFile,
					targetLink, strerror(errno));
			}
		}
	}

	// Open the hard link using safe_open as user condor, and fstat() the two 
	// open file handles to make sure the inodes match.
	if (retVal == true) {
		set_condor_priv();

		if (srcFileMode.Stat(srcFile) == 0 && targetLinkMode.Stat(targetLink) == 0) {
			targetLinKStat = targetLinkMode.GetBuf();
			if (srcFileStat != NULL && targetLinKStat != NULL) {
				retVal = (srcFileStat->st_ino == targetLinKStat->st_ino);
			}
			else {
				retVal = false;
			}
		}
		else {
			retVal = false;
		}
	}
	

	// Free the hard link filename
	delete [] targetLink;

	// Reset priv state
	set_priv(original_priv);

	return retVal;
}

static string MakeAbsolutePath(const char* path, const char* initialWorkingDir) {
	if (is_relative_to_cwd(path)) {
		string fullPath = initialWorkingDir;
		fullPath += DIR_DELIM_CHAR;
		fullPath += path;
		return (fullPath);
	}
	return (path);
}

} // end namespace

void ProcessCachedInpFiles(ClassAd *const Ad, StringList *const InputFiles,
	StringList &PubInpFiles) {

	char *initialWorkingDir = NULL;
	const char *path;
	MyString remap;
	struct stat fileStat;
	time_t fileModifiedTime = time(NULL);

	if (PubInpFiles.isEmpty() == false) {
		const char *webServerAddress = param("HTTP_PUBLIC_FILES_ADDRESS");

		// If a web server address is not defined, exit quickly. The file
		// transfer will go on using the regular CEDAR protocol.
		if(!webServerAddress) {
			dprintf(D_FULLDEBUG, "mk_cache_links.cpp: HTTP_PUBLIC_FILES_ADDRESS "
							"not set! Falling back to regular file transfer\n");
			return;
		}

		// Build out the base URL for public files
		string url = "http://";
		url += webServerAddress; 
		url += "/";

		PubInpFiles.rewind();
		
		if (Ad->LookupString(ATTR_JOB_IWD, &initialWorkingDir) != 1) {
			dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Job ad did not have an "
				"initialWorkingDir! Falling back to regular file transfer\n");
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
			else {
				dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Unable to access file "
					"%s. Falling back to regular file transfer\n", fullPath.c_str());
				free( initialWorkingDir );
				return;
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
				if ( !InputFiles->contains(namePtr) ) {
					InputFiles->append(namePtr);
					dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Adding url to "
												"InputFiles: %s\n", namePtr);
				} 
				else dprintf(D_FULLDEBUG, "mk_cache_links.cpp: url already "
											"in InputFiles: %s\n", namePtr);
			}
			else {
				dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Failed to generate "
									" hash link for %s\n", fullPath.c_str());
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
				dprintf(D_ALWAYS, "mk_cache_links.cpp: Could not add to jobAd: "
													"%s\n", remap.c_str());
			}
		}
	} 
	else	
		dprintf(D_FULLDEBUG, "mk_cache_links.cpp: No public input files.\n");
}

#endif

