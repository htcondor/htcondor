/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "basename.h"
#include "util_lib_proto.h" // for rotate_file
#include "log_rotate.h"
#include <sys/types.h>
#ifndef WIN32
#include <dirent.h>
#endif

/** is it a *.old file? */
static int isOldString(const char *str);

/** is the argument an ISO timestamp? */
static int isTimestampString(const char *str);

/** is the argument a valid log filename? */
static int isLogFilename(const char *filename);

/** find the oldest file in the directory matching the 
  *current base name according to iso time ending 
 */
static char *findOldest(char *dirName, int *count);

#ifdef WIN32
char * searchLogName = NULL;
#endif
 
char * logBaseName = NULL;
char * baseDirName = NULL;
int isInitialized = 0;

int numLogs = 0;

// max size of an iso timestamp extenstion 4+2+2+1+2+2+2
#define MAX_ISO_TIMESTAMP 16

/** create an ISO timestamp string */
static void createTimestampString(std::string & timestamp, time_t tt)
{
	struct tm *tm;
	tm = localtime(&tt);

	char buf[80];
	strftime(buf, sizeof(buf), "%Y%m%dT%H%M%S", tm);

	timestamp = buf;
}

long long quantizeTimestamp(time_t tt, long long secs)
{
	if ( ! secs) return tt;

	static int leap_sec = -1;
	if (leap_sec < 0) {
		struct tm * ptm = localtime(&tt);
		ptm->tm_hour  = 0;
		ptm->tm_min = 0;
		ptm->tm_sec = 0;
		time_t today = mktime(ptm);
		leap_sec = today % 3600;
	}

	return tt - (tt % secs);
}


#ifndef WIN32
int scandirectory(const char *dir, struct dirent ***namelist,
            int (*select)(const struct dirent *),
        int (*compar)(const void*, const void*) ) {
	DIR *d;
	struct dirent *entry;
	register int i = 0;
	size_t entrysize;

	if ((d=opendir(dir)) == NULL)
		return(-1);

	*namelist=NULL;
	while ((entry=readdir(d)) != NULL) {
		if (select == NULL || (select != NULL && (*select)(entry))) {
			*namelist=(struct dirent **)realloc((void *)(*namelist), (size_t)((i+1)*sizeof(struct dirent *)));
			if (*namelist == NULL) {
				closedir(d);
				return -1;
			}
			entrysize=sizeof(struct dirent)-sizeof(entry->d_name)+strlen(entry->d_name)+1;
			(*namelist)[i]=(struct dirent *)malloc(entrysize);
			if ((*namelist)[i] == NULL) {
				closedir(d);
				return(-1);
			}
			memcpy((*namelist)[i], entry, entrysize);
    	    i++;
		}
	}
	if (closedir(d)) 
  		return -1;
	if (i == 0) 
		return -1;
	if (compar != NULL)
    	qsort((void *)(*namelist), (size_t)i, sizeof(struct dirent *), compar);

	return i;
}

int doalphasort(const void *a, const void *b) {
        const struct dirent **d1 = (const struct dirent**)const_cast<void*>(a);
        const struct dirent **d2 = (const struct dirent**)const_cast<void*>(b);
        return(strcmp(const_cast<char*>((*d1)->d_name),
		const_cast<char*>((*d2)->d_name)));
}

#endif

void setBaseName(const char *baseName) {
	// Since one log file can have different ones per debug level, 
	// we need to check whether we want to deal with a different base name
	if  ( (isInitialized == 1) && (strcmp(baseName, logBaseName) != 0) )  {
		isInitialized = 0;
	}
	if (isInitialized == 0) {
		char *tmpDir;

		if (logBaseName)
			free(logBaseName);
		logBaseName = strdup(baseName);
		tmpDir = condor_dirname(logBaseName);
		if (baseDirName)
			free(baseDirName);
		baseDirName = strdup(tmpDir);
		free(tmpDir);
#ifdef WIN32
		if (searchLogName)
			free(searchLogName);
		searchLogName = (char *)malloc(strlen(logBaseName)+3);
		sprintf(searchLogName, "%s.*", (const char*)logBaseName); 		
#endif
		isInitialized = 1;
	}
}

int
rotateSingle()
{
	return rotateTimestamp("old", 1, 0);
} 


const char *
createRotateFilename(const char *ending, int maxNum, time_t tt)
{
	static std::string timeStamp;
	if (maxNum <= 1)
		timeStamp = "old";
	else 	
	if (ending == NULL) {
		createTimestampString(timeStamp, tt);
	} else {
		timeStamp = ending;
	}
	return timeStamp.c_str();
}

// rotate the current file to file.timestamp
int
rotateTimestamp(const char *timeStamp, int maxNum, time_t tt)
{
	int save_errno;
	const char *ts = createRotateFilename(timeStamp, maxNum, tt);

	// First, select a name for the rotated history file
	char *rotated_log_name = (char*)malloc(strlen(logBaseName) + strlen(ts) + 2) ;
	ASSERT( rotated_log_name );
	(void)sprintf( rotated_log_name, "%s.%s", logBaseName, ts );
	save_errno = rotate_file_dprintf(logBaseName, rotated_log_name, 1);
	free(rotated_log_name);
	return save_errno; // will be 0 in case of success.
}


/*
tj : change to delete files rather than 'rotating' them when the number exceeds 1
this code currently will take the oldest .timestamp file and rename it to .old
which seems wrong.
*/

int cleanUpOldLogFiles(int maxNum) {
	int count;
	char *oldFile = NULL;
	char empty[BUFSIZ];
		/* even if current maxNum is set to 1, clean up in 
			case a config change took place. */
	if (maxNum > 0 ) {
		oldFile = findOldest(baseDirName, &count);
		while (count > maxNum) {
			(void)sprintf( empty, "%s.old", logBaseName );
			// catch the exception that the file name pattern is disturbed by external influence
			if (strcmp(oldFile, empty) == 0)
				break;
			if ( rotate_file(oldFile,empty) != 0) {
				dprintf(D_ALWAYS, "Rotation cleanup of old file %s failed.\n", oldFile);
			}
			free(oldFile);
			oldFile = findOldest(baseDirName, &count);
		}
	}
	if (oldFile != NULL) {
			free(oldFile);
			oldFile = NULL;
	}
	
	return 0;

}


static int isOldString(const char *str){
	if (strcmp(str, "old") == 0)
		return 1;
	return 0;
}

static int isTimestampString(const char *str) {
	int len = strlen(str);
	if (len != 15) {
		return 0;
	}
	int i = 0;
	while (i < 8) {
		if (str[i] < '0' || str[i] > '9')
			return 0;
		++i;
	}
	if (str[i++] != 'T')
		return 0;
	while (i < len) {
		if (str[i] < '0' || str[i] > '9')
			return 0;
		++i;
	}
	return 1;
} 

static int isLogFilename(const char *filename) {
	int dirLen = strlen(baseDirName);
#ifdef WIN32
	if (baseDirName[dirLen-1] != DIR_DELIM_CHAR && baseDirName[dirLen-1] != '/') ++dirLen;
#else
	if (baseDirName[dirLen-1] != DIR_DELIM_CHAR) ++dirLen;
#endif
    int fLen = strlen(logBaseName);
    if (strncmp(filename, (logBaseName+dirLen), fLen - dirLen) == 0 ) {
    	if (  (strlen(filename) > unsigned(fLen-dirLen)) && 
			  (filename[fLen-dirLen] == '.')  ) {
			if (  (isTimestampString(filename+(fLen-dirLen+1)) == 1) ||
				  (isOldString(filename+(fLen-dirLen+1)) == 1) )
    				return 1;
    	}
	}
        	
	return 0;
}

#ifndef WIN32

int file_select(const struct dirent *entry) {
	return isLogFilename(entry->d_name);
}


char *findOldest(char *dirName, int *count) {
	struct dirent **files;
	int  len;
	*count = scandirectory(dirName, &files, file_select, doalphasort);
	// no matching files in the directory
	if (*count <= 0)
		return NULL;
	char *oldFile = (char*)files[0]->d_name;
	len = strlen(oldFile);
	char *result = (char*)malloc(len+1 + strlen(dirName) + 1);
	(void)sprintf(result, "%s%c%s", dirName, DIR_DELIM_CHAR, oldFile);
	return result;
}
#else

// return a count of files matching the logfile name pattern,
// and the filename of the oldest of these files.
char *findOldest(char *dirName, int *count) {
	char *oldFile = NULL;
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	int result;
	int cchBase = strlen(logBaseName);
	
	*count = 0;
	
	hFind = FindFirstFile(searchLogName, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	
	// the filenames returned from FindNextFile aren't necessarily sorted
	// so scan the list, and return the one that sorts the lowest
	// this should work correctly for .old (because there is only one)
	// and for .{TIMESTAMP} because the timestamps will sort by date.

	while ((result = FindNextFile(hFind, &ffd)) != 0) {
		if ( ! isLogFilename(ffd.cFileName))
			continue;
		++(*count);
		if ( ! oldFile) {
			oldFile = (char*)malloc(strlen(logBaseName) + 2 + MAX_ISO_TIMESTAMP);
			ASSERT( oldFile );
			strcpy(oldFile, ffd.cFileName);
		} else {
			int cch = strlen(ffd.cFileName);
			if (cch > cchBase && strcmp(ffd.cFileName+cchBase, oldFile+cchBase) < 0) {
				strcpy(oldFile, ffd.cFileName);
			}
		}
	}

	FindClose(hFind);
	return oldFile;
}

#endif


