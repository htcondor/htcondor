/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_md.h"
#include "directory_util.h"
#include "stat_wrapper.h"

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_contains contains_anycase
#else
#	define file_contains contains
#endif


using namespace std;

namespace {	// Anonymous namespace to limit scope of names to this file
  
const int HASHNAMELEN = 17;


string MakeHashName(const char *fileName) {
  unsigned char result[HASHNAMELEN * 3]; // Allocate extra space for safety.
  memcpy(result, Condor_MD_MAC::computeOnce((unsigned char *) fileName,
	  strlen(fileName)), HASHNAMELEN);
  char entryhashname[HASHNAMELEN * 2]; // 2 chars per hex byte
  entryhashname[0] = '\0';
  char letter[3];
  for (int ind = 0; ind < HASHNAMELEN - 1; ++ind) {
	  sprintf(letter, "%x", result[ind]);
	  strcat(entryhashname, letter);
  }
  return (entryhashname);
}


bool MakeLink(const char *const srcFile, const string &newLink) {
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
      } else dprintf(D_ALWAYS, "Could not stat file %s\n", targetLink);
    } else if (symlink(srcFile, targetLink) == 0) {
      retVal = true;
    } else dprintf(D_ALWAYS, "Could not link %s to %s, error = %s\n", srcFile,
       targetLink, strerror(errno));
    delete [] targetLink;
  }
  return (retVal);
}

} // end namespace


void ProcessCachedInpFiles(ClassAd *const Ad, StringList *const InputFiles,
  StringList &PubInpFiles) {
  char *buf = NULL;
  if (Ad->LookupString(ATTR_PUBLIC_INPUT_FILES, &buf) == 1) {
    PubInpFiles.initializeFromString(buf);
    free(buf);
    buf = NULL;
    int webSrvrPort = param_integer("WEB_SERVER_PORT", 80);
    char portNum[50];
    sprintf(portNum, "%d", webSrvrPort);
    string url = "http://localhost:";
    url += portNum;
    url += "/";
    PubInpFiles.rewind();
    const char *path;
    MyString remap;
    while ((path = PubInpFiles.next()) != NULL) {
      string hashName = MakeHashName(path);
      if (MakeLink(path, hashName)) {
	InputFiles->remove(path); // Remove plain file name from InputFiles
	remap +=hashName;
	remap += "=";
	remap += basename(path);
	remap += ";";
	hashName = url + hashName;
	const char *const namePtr = hashName.c_str();
	if ( !InputFiles->file_contains(namePtr) ) {
	  InputFiles->append(namePtr);
	  dprintf(D_FULLDEBUG, "Adding url to InputFiles: %s\n", namePtr);
	} else dprintf(D_FULLDEBUG, "url already in InputFiles: %s\n", namePtr);
      }
    }
    if (remap.Length() > 0) {
      MyString remapnew;
      if (Ad->LookupString(ATTR_TRANSFER_OUTPUT_REMAPS, &buf) == 1) {
	MyString remapnew = buf;
	free(buf);
	remapnew[remapnew.Length() - 1] = '\0'; // Trim last quote
	remapnew += remap;
      } else {
	remapnew = ATTR_TRANSFER_OUTPUT_REMAPS;
	remapnew += " = \"";
	remapnew += remap;
      }
      remapnew += "\"";
      // What about REPLACE job ad?????
      if (Ad->Insert(remap.Value()) == false) {
	dprintf(D_ALWAYS, "Could not insert into jobAd: %s\n", remap.c_str());
      }
    }
  } else  dprintf(D_FULLDEBUG, "No public input files.\n");
}

