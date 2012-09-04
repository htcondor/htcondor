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
#include "get_daemon_name.h"
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

// Find all of the history files that the schedd created, and put them
// in order by time that they were created. The time comes from a
// timestamp in the file name.
char **findHistoryFiles(const char *paramName, int *numHistoryFiles)
{
    int  fileIndex;
    char **historyFiles = NULL;
    char *historyDir;

    BaseJobHistoryFileName = param(paramName);
	if ( BaseJobHistoryFileName == NULL ) {
		return NULL;
	}
    historyDir = condor_dirname(BaseJobHistoryFileName);

    *numHistoryFiles = 0;
    if (historyDir != NULL) {
        Directory dir(historyDir);
        const char *current_filename;

        // We walk through once and count the number of history file backups
         for (current_filename = dir.Next(); 
             current_filename != NULL; 
             current_filename = dir.Next()) {
            
            if (isHistoryBackup(current_filename, NULL)) {
                (*numHistoryFiles)++;
            }
        }

        // Add one for the current history file
        (*numHistoryFiles)++;

        // Make space for the filenames
        historyFiles = (char **) malloc(sizeof(char*) * (*numHistoryFiles));
        ASSERT( historyFiles );

        // Walk through again to fill in the names
        // Note that we won't get the current history file
        dir.Rewind();
        for (fileIndex = 0, current_filename = dir.Next(); 
             current_filename != NULL; 
             current_filename = dir.Next()) {
            
            if (isHistoryBackup(current_filename, NULL)) {
                historyFiles[fileIndex++] = strdup(dir.GetFullPath());
            }
        }
        historyFiles[fileIndex] = strdup(BaseJobHistoryFileName);

        if ((*numHistoryFiles) > 2) {
            // Sort the backup files so that they are in the proper 
            // order. The current history file is already in the right place.
            qsort(historyFiles, (*numHistoryFiles)-1, sizeof(char*), compareHistoryFilenames);
        }
        
        free(historyDir);
    }
    return historyFiles;
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

        iso8601_to_time(filename + history_base_length + 1, &file_time, &is_utc);
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

    isHistoryBackup((const char *) item1, &time1);
    isHistoryBackup((const char *) item2, &time2);
    return time1 - time2;
}

