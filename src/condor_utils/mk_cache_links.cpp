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


	// see how we should create the link.  There are a few options.
	// 1) As the condor user.
	// 2) As root, and then chown to the user.
	// 3) As root, and then chown to some other user (like "httpd")
	std::string link_owner;
	param(link_owner, "HTTP_PUBLIC_FILES_USER");

	uid_t link_uid = -1;
	gid_t link_gid = -1;
	bool  create_as_root = false;

	priv_state priv = PRIV_UNKNOWN;

	if (strcasecmp(link_owner.c_str(), "<user>") == 0) {
		// we'll do everything in user priv except use root
		// to create and chown the link

		link_uid = get_user_uid();
		link_gid = get_user_gid();
		create_as_root = true;
		priv = set_user_priv();
	} else if (strcasecmp(link_owner.c_str(), "<condor>") == 0) {
		// in this case we do everything as the condor user since they
		// own the directory

		priv = set_condor_priv();
	} else {
		// in this case we need to determine what uid they requested
		// and then do everything as that user.  we set root priv and
		// then temporarily assume the uid and gid of the specified
		// user.

		// has to be a username and not just a uid, since otherwise it
		// isn't clear what gid we should use (if they aren't in the passwd
		// file, for instance) and we also save on lookups.
		bool isname = pcache()->get_user_ids(link_owner.c_str(), link_uid, link_gid);
		if (!isname) {
			dprintf(D_ALWAYS, "ERROR: unable to look up HTTP_PUBLIC_FILES_USER (%s)"
				" in /etc/passwd.\n", link_owner.c_str());

			// we ARE allowed to return here because we have not
			// yet switched priv state in this case.
			return false;
		}

		if (link_uid == 0 || link_gid == 0) {
			dprintf(D_ALWAYS, "ERROR: HTTP_PUBLIC_FILES_USER (%s)"
				" in /etc/passwd has UID 0.  Aborting.\n", link_owner.c_str());

			// we ARE allowed to return here because we have not
			// yet switched priv state in this case.
			return false;
		}


		// now become the specified user for this operation.
		priv = set_root_priv();
		if (setegid(link_gid)) {}
		if (seteuid(link_uid)) {}
	}


	// STARTING HERE, DO NOT RETURN FROM THIS FUNCTION WITHOUT RESETTING
	// THE ORIGINAL PRIV STATE.

	// we will now create or update the link
	const char *const targetLink = dircat(goodPath, newLink.c_str()); // needs to be freed
	bool retVal = false;
	if (targetLink != NULL) {
		// Check if target already exists
		if (fileMode.Stat(targetLink, StatWrapper::STATOP_LSTAT) == 0) { 
			// Good enough if link exists, ok if update fails
			retVal = true;

			// It is assumed that existing link points to srcFile.
			//
			// I don't like this assumption.  I think we need to
			// add username or uid to filename to prevent
			// accidents/mischeif when two users happen to us the
			// same file.  If user A then deletes or modifies the
			// file, user B is stuck with a link they probably
			// can't update.  To be fixed ASAP.  -zmiller
			const StatStructType *statrec = fileMode.GetBuf();
			if (statrec != NULL) {
				time_t filemtime = statrec->st_mtime;
				// Must be careful to operate on link, not target file.
				//
				// This should be done AS THE OWNER OF THE FILE.
				if ((time(NULL) - filemtime) > 3600 && lutimes(targetLink, NULL) != 0) {
					// Update mod time, but only once an hour to avoid excess file access
					dprintf(D_ALWAYS, "Could not update modification date on %s,"
						"error = %s\n", targetLink, strerror(errno));
				}
			} else {
				dprintf(D_ALWAYS, "Could not stat file %s\n", targetLink);
			}
		} else {
			// now create the link.  we may need to do this as root and chown
			// it, depending on the create_as_root flag.  if that's false, just
			// stay in the priv state we are in for creation and there's no need
			// to chown.

			priv_state link_priv = PRIV_UNKNOWN;
			if (create_as_root) {
				link_priv = set_root_priv();
			}

			if (symlink(srcFile, targetLink) == 0) {
				// so far, so good!
				retVal = true;
			} else {
				dprintf(D_ALWAYS, "Could not link %s to %s, error = %s\n", srcFile,
					targetLink, strerror(errno));
			}

			if (create_as_root) {
				// if we succesfully created the link, now chown to the user
				if (retVal && lchown(targetLink, link_uid, link_gid) != 0) {
					// chown didn't actually succeed, so this operation is now a failure.
					retVal = false;

					// destroy the evidence?
					unlink(targetLink);

					dprintf(D_ALWAYS, "Could not change ownership of %s to %i:%i, error = %s\n",
						targetLink, link_uid, link_gid, strerror(errno));
				}

				// either way, reset priv state
				set_priv(link_priv);
			}
		}

		delete [] targetLink;
	}

	// return to original privilege level, and exit
	set_priv(priv);

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
    time_t fileModifiedTime;

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

