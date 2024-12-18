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
#include "condor_uid.h"
#include "condor_md.h"
#include "directory_util.h"
#include "file_lock.h"
#include "basename.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string> 

#ifndef WIN32
	#include <unistd.h>
#endif

#ifdef HAVE_HTTP_PUBLIC_FILES

namespace {	// Anonymous namespace to limit scope of names to this file
	
const int HASHNAMELEN = 17;


static std::string MakeHashName(const char* fileName, time_t fileModifiedTime) {

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
	unsigned char *theHash = Condor_MD_MAC::computeOnce(hashSource,
		strlen((const char *) hashSource));
	memcpy(hashResult, theHash, HASHNAMELEN);
	free(theHash);

	char entryHashName[HASHNAMELEN * 2]; // 2 chars per hex byte
	entryHashName[0] = '\0';
	char letter[3];
	for (int i = 0; i < HASHNAMELEN - 1; ++i) {
		sprintf(letter, "%x", hashResult[i]);
		strcat(entryHashName, letter);
	}
	delete [] hashSource;

	return entryHashName;
}


// WARNING!  This code changes priv state.  Be very careful if modifying it.
// Do not return in the block of code where the priv has been set to either
// condor or root.  -zmiller
static bool MakeLink(const char* srcFilePath, const std::string &newLink) {

	bool retVal = false;
	int srcFileInodeNum;
	int targetLinkInodeNum;
	struct stat srcFileStat;
	struct stat targetLinkStat;

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

	// Determine the full path of the access file.
	std::string accessFilePath;
	dircat(goodPath, newLink.c_str(), accessFilePath);
	accessFilePath += ".access";
	
	// STARTING HERE, DO NOT RETURN FROM THIS FUNCTION WITHOUT RESETTING
	// THE ORIGINAL PRIV STATE.
	
	// Check if an access file exists (which will be the case if someone has
	// already sent the source file).
	// If it does exist, lock it so that condor_preen cannot open it while
	// garbage collecting.
	// If it does not exist, carry on. We'll create it before exiting.
	
	priv_state original_priv = set_root_priv();
	FileLock *accessFileLock = NULL;
	
	if(access(accessFilePath.c_str(), F_OK) == 0) {
		accessFileLock = new FileLock(accessFilePath.c_str(), true, false);

		// Try to grab a lock on the access file. This should block until it 
		// obtains the lock. If this fails for any reason, bail out.
		bool obtainedLock = accessFileLock->obtain(WRITE_LOCK);
		if(!obtainedLock) {
			dprintf(D_ALWAYS, "MakeLink: Failed to obtain lock on access file with"
				" error code %d (%s).\n", errno, strerror(errno));
			set_priv(original_priv);
			return (false);
		}
	}
	
	// Impersonate the user and open the file using safe_open(). This will allow
	// us to verify the user has privileges to access the file, and later to
	// verify the hard link points back to the same inode.
	set_user_priv();
	
	bool fileOK = false;
	FILE *srcFile = safe_fopen_wrapper(srcFilePath, "r");
	if (srcFile) {
		if(stat(srcFilePath, &srcFileStat) == 0) {
			srcFileInodeNum = srcFileStat.st_ino;
			fileOK = (srcFileStat.st_mode & S_IRUSR); // Verify readable by owner
		}
	}
	if (fileOK == false) {
		dprintf(D_ALWAYS, "MakeLink: Cannot transfer -- public input file not "
			"readable by user: %s\n", srcFilePath);
		set_priv(original_priv);
		return (false);
	}
	fclose(srcFile);

	// Create the hard link using root privileges; it will automatically get
	// owned by the same owner of the file. If the link already exists, don't do 
	// anything at this point, we'll check later to make sure it points to the
	// correct inode.
	std::string linkpathbuf;
	const char *const targetLinkPath = dircat(goodPath, newLink.c_str(), linkpathbuf);

	// Switch to root privileges, so we can test if the link exists, and create
	// it if it does not
	set_root_priv();
	
	// Check if target link already exists
	FILE *targetLink = safe_fopen_wrapper(targetLinkPath, "r");
	if (targetLink) {
		// If link exists, great! Move on to the next step.
		retVal = true;
		fclose(targetLink);
	}	
	else {
		// Link does not exist, so create it as root.
		if (link(srcFilePath, targetLinkPath) == 0) {
			// so far, so good!
			retVal = true;
		}
		else {
			dprintf(D_ALWAYS, "MakeLink: Could not link %s to %s, error: %s\n", 
				targetLinkPath, srcFilePath, strerror(errno));
		}
	}

	// Open the hard link. Verify that the inode is the same as the file we 
	// opened earlier on
	if (retVal == true) {
		if (stat(targetLinkPath, &targetLinkStat) == 0) {
			targetLinkInodeNum = targetLinkStat.st_ino;
			if (srcFileInodeNum == targetLinkInodeNum) {
				retVal = true;
			}
			else {
				dprintf(D_ALWAYS, "Source file %s inode (%d) does not match "
					"hard link %s inode (%d), aborting.\n", srcFilePath, 
					srcFileInodeNum, targetLinkPath, targetLinkInodeNum);
			}
		}
		else {
			retVal = false;
			dprintf(D_ALWAYS, "Makelink: Cannot open hard link %s. Reverting to "
				"regular file transfer.\n", targetLinkPath);
		}
	}

	// Touch the access file. This will create it if it doesn't exist, or update
	// the timestamp if it does.
	if (retVal == true ) {
		FILE* accessFile = fopen(accessFilePath.c_str(), "w");
		if (accessFile) {
			fclose(accessFile);
		}
		else {
			dprintf(D_ALWAYS, "MakeLink: Failed to update access file %s (Error %d:"
				" %s)\n", accessFilePath.c_str(), errno, strerror(errno));
		}
	}
	
	// Release the lock on the access file
	if(accessFileLock && !accessFileLock->release()) {
		dprintf(D_ALWAYS, "MakeLink: Failed to release lock on access file with"
			" error code %d (%s).\n", errno, strerror(errno));
	}

	// Reset priv state
	set_priv(original_priv);

	return retVal;
}

static std::string MakeAbsolutePath(const char* path, const char* initialWorkingDir) {
	if (!fullpath(path)) {
		std::string fullPath = initialWorkingDir;
		fullPath += DIR_DELIM_CHAR;
		fullPath += path;
		return (fullPath);
	}
	return (path);
}

} // end namespace

void ProcessCachedInpFiles(ClassAd *const Ad, std::vector<std::string> &InputFiles,
	const std::vector<std::string> &PubInpFiles) {

	char *initialWorkingDir = NULL;
	std::string remap;
	struct stat fileStat;
	time_t fileModifiedTime = time(NULL);

	if (PubInpFiles.empty() == false) {
		const char *webServerAddress = param("HTTP_PUBLIC_FILES_ADDRESS");

		// If a web server address is not defined, exit quickly. The file
		// transfer will go on using the regular CEDAR protocol.
		if(!webServerAddress) {
			dprintf(D_FULLDEBUG, "mk_cache_links.cpp: HTTP_PUBLIC_FILES_ADDRESS "
							"not set! Falling back to regular file transfer\n");
			return;
		}

		// Build out the base URL for public files
		std::string url = "http://";
		url += webServerAddress; 
		url += "/";

		if (Ad->LookupString(ATTR_JOB_IWD, &initialWorkingDir) != 1) {
			dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Job ad did not have an "
				"initialWorkingDir! Falling back to regular file transfer\n");
			return;
		}
		for (auto& path: PubInpFiles) {
			// Determine the full path of the file to be transferred
			std::string fullPath = MakeAbsolutePath(path.c_str(), initialWorkingDir);

			// Determine the time last modified of the file to be transferred
			if( stat( fullPath.c_str(), &fileStat ) == 0 ) {
#if defined(DARWIN)
				struct timespec fileTime = fileStat.st_mtimespec;
#else
				struct timespec fileTime = fileStat.st_mtim;
#endif
				fileModifiedTime = fileTime.tv_sec;
			}
			else {
				dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Unable to access file "
					"%s. Falling back to regular file transfer\n", fullPath.c_str());
				free( initialWorkingDir );
				return;
			}

			std::string hashName = MakeHashName( fullPath.c_str(), fileModifiedTime );
			if (MakeLink(fullPath.c_str(), hashName)) {
				std::erase_if(InputFiles, [&](auto& f) {return f == path;});

				remap += hashName;
				remap += "=";
				remap += condor_basename(path.c_str());
				remap += ";";
				hashName = url + hashName;
				const char *const namePtr = hashName.c_str();
				if ( !contains(InputFiles, namePtr) ) {
					InputFiles.emplace_back(namePtr);
					dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Adding url to "
												"InputFiles: %s\n", namePtr);
				} 
				else dprintf(D_FULLDEBUG, "mk_cache_links.cpp: url already "
											"in InputFiles: %s\n", namePtr);
			}
			else {
				dprintf(D_FULLDEBUG, "mk_cache_links.cpp: Failed to generate "
									 "hash link for %s\n", fullPath.c_str());
			}
		}
		free( initialWorkingDir );
		if ( remap.length() > 0 ) {
			std::string remapnew;
			if (Ad->LookupString(ATTR_TRANSFER_INPUT_REMAPS, remapnew) == 1) {
				remapnew += ";";
			} 
			remapnew += remap;
			if (Ad->Assign(ATTR_TRANSFER_INPUT_REMAPS, remap) == false) {
				dprintf(D_ALWAYS, "mk_cache_links.cpp: Could not add to jobAd: "
													"%s\n", remap.c_str());
			}
		}
	} 
	else	
		dprintf(D_FULLDEBUG, "mk_cache_links.cpp: No public input files.\n");
}

#endif

