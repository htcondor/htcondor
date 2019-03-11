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
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "dc_collector.h"
#include "internet.h"
#include "print_wrapped_text.h"
#include "MyString.h"
#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname
#include "match_prefix.h"
#include "subsystem_info.h"

#include "historyFileFinder.h"

static bool isHistoryBackup(const char *fullFilename, time_t *backup_time);
static int compareHistoryFilenames(const void *item1, const void *item2);

static  char *BaseJobHistoryFileName = NULL;

extern void freeHistoryFilesList(const char ** files)
{
	if (files) free(const_cast<char**>(files));
}

// Find all of the history files that the schedd created, and put them
// in order by time that they were created, so the current file is always last
// For instance, if there is a current file and 2 rotated older files we would have
//    [0] = "/scratch/condor/spool/history.20151019T161810"
//    [1] = "/scratch/condor/spool/history.20151020T161810"
//    [2] = "/scratch/condor/spool/history"
// the return value is a pointer to an array of const char* filenames.
// if the history configuration is invalid, then NULL is returned.
// if the history configuration is valid, but there are no history files
// then an allocated pointer is still returned, but *pnumHistoryFiles will be 0
const char **findHistoryFiles(const char *paramName, int *pnumHistoryFiles)
{
    char **historyFiles = NULL;
    bool foundCurrent = false; // true if 'current' history file (i.e. without extension) exists
    int  cchExtra = 0; // total number of characters needed to hold the file extensions
    int  numFiles = 0;
    StringList tmpList; // temporarily hold the filenames so we don't have to iterate the directory twice.

    if (BaseJobHistoryFileName) { free(BaseJobHistoryFileName); }
    BaseJobHistoryFileName = param(paramName);
	if ( BaseJobHistoryFileName == NULL ) {
		return NULL;
	}
    char *historyDir = condor_dirname(BaseJobHistoryFileName);
    const char * historyBase = condor_basename(BaseJobHistoryFileName);

    if (historyDir != NULL) {
        Directory dir(historyDir);
        int cchBaseName = strlen(historyBase);
        int cchBaseFileName = strlen(BaseJobHistoryFileName);

        // We walk through once and count the number of history file backups
        // and keep track of all of the file extensions for backup files.
         for (const char *current_filename = dir.Next(); 
             current_filename != NULL; 
             current_filename = dir.Next()) {

            const char * current_base = condor_basename(current_filename);
           #ifdef WIN32
            if (MATCH == strcasecmp(historyBase, current_base)) {
           #else
            if (MATCH == strcmp(historyBase, current_base)) {
           #endif
                numFiles++;
                foundCurrent = true;
            } else if (isHistoryBackup(current_filename, NULL)) {
                numFiles++;
                const char * pextra = current_filename + cchBaseName;
                tmpList.append(pextra);
                cchExtra += strlen(pextra);
            }
        }

        // allocate space for the array, and also for copies of the filenames
        // we will use this the first part of this allocation as the array,
        // and later parts to hold the filenames
        historyFiles = (char **) malloc(cchExtra + numFiles * (cchBaseFileName+1) + (numFiles+1) * (sizeof(char*)));
        ASSERT( historyFiles );

        // Walk through the extension list to again to fill in the names
        // then append the current history file (if any)
        int  fileIndex = 0;
        char * p = (char*)historyFiles + sizeof(const char*) * (numFiles+1); // start at first byte after the char* array
        for (const char * ext = tmpList.first(); ext != NULL; ext = tmpList.next()) {
            historyFiles[fileIndex++] = p;
            strcpy(p, BaseJobHistoryFileName);
            strcpy(p + cchBaseFileName, ext);
            p += cchBaseFileName + strlen(ext) + 1;
        }
        // if the base history file was found in the directory, add it to the list also
        if (foundCurrent) {
            historyFiles[fileIndex++] = p;
            strcpy(p, BaseJobHistoryFileName);
        }
        historyFiles[fileIndex] = NULL; // put a NULL at the end of the array.

        if (numFiles > 2) {
            // Sort the backup files so that they are in the proper 
            // order. The current history file is already in the right place.
            qsort(historyFiles, numFiles-1, sizeof(char*), compareHistoryFilenames);
        }
        
        free(historyDir);
    }
    *pnumHistoryFiles = numFiles;
    return const_cast<const char**>(historyFiles);
}

// Returns true if the filename is a history file, false otherwise.
// If backup_time is not NULL, returns the time from the timestamp in
// the file.
static bool isHistoryBackup(const char *fullFilename, time_t *backup_time)
{
    bool       is_history_filename;
    const char *filename;
    const char *history_base;
    int        history_base_length;

    if (backup_time != NULL) {
        *backup_time = -1;
    }
    
    is_history_filename = false;
    history_base        = condor_basename(BaseJobHistoryFileName);
    history_base_length = strlen(history_base);
    filename            = condor_basename(fullFilename);

    if (   !strncmp(filename, history_base, history_base_length)
        && filename[history_base_length] == '.') {
        // The filename begins correctly, now see if it ends in an 
        // ISO time
        struct tm file_time;
        bool is_utc;

        iso8601_to_time(filename + history_base_length + 1, &file_time, NULL, &is_utc);
        if (   file_time.tm_year != -1 && file_time.tm_mon != -1 
            && file_time.tm_mday != -1 && file_time.tm_hour != -1
            && file_time.tm_min != -1  && file_time.tm_sec != -1
            && !is_utc) {
            // This appears to be a proper history file backup.
            is_history_filename = true;
            if (backup_time != NULL) {
                *backup_time = mktime(&file_time);
            }
        }
    }

    return is_history_filename;
}

// Used by qsort in findHistoryFiles() to sort history files. 
static int compareHistoryFilenames(const void *item1, const void *item2)
{
    time_t time1, time2;

    isHistoryBackup(*(const char * const *) item1, &time1);
    isHistoryBackup(*(const char * const *) item2, &time2);
    return time1 - time2;
}

