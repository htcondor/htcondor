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
#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname
#include "match_prefix.h"
#include "subsystem_info.h"

#include "historyFileFinder.h"
#include <algorithm>

static const char* qsort_file_base = NULL;

// Returns true if the filename is a history file, false otherwise.
// If backup_time is not NULL, returns the time from the timestamp in
// the file.
static bool isHistoryBackup(const char *fullFilename, time_t *backup_time, const char *history_base)
{
    bool       is_history_filename;
    const char *filename;
    int        history_base_length;

    if (backup_time != NULL) {
        *backup_time = -1;
    }
    
    is_history_filename = false;
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

// Used by sort in findHistoryFiles() to sort history files. 
static bool compareHistoryFilenames(const std::string& lhs, const std::string& rhs)
{
    time_t time1, time2;

    isHistoryBackup(lhs.c_str(), &time1, qsort_file_base);
    isHistoryBackup(rhs.c_str(), &time2, qsort_file_base);
    return time1 < time2;
}

// Find all of the history files that the schedd created, and put them
// in order by time that they were created, so the current file is always last
// For instance, if there is a current file and 2 rotated older files we would have
//    [0] = "/scratch/condor/spool/history.20151019T161810"
//    [1] = "/scratch/condor/spool/history.20151020T161810"
//    [2] = "/scratch/condor/spool/history"
// the return value is a vector of strings
std::vector<std::string> findHistoryFiles(const char *passedFileName)
{
    std::vector<std::string> historyFiles;
    bool foundCurrent = false; // true if 'current' history file (i.e. without extension) exists

    if ( passedFileName == NULL ) {
        return historyFiles;
    }
	std::string historyDir(condor_dirname(passedFileName));
    const char * historyBase = condor_basename(passedFileName);

	Directory dir(historyDir.c_str());

	// We walk through directory and add matching files to vector
	for (const char *current_filename = dir.Next(); 
			current_filename != NULL; 
			current_filename = dir.Next()) {

		const char * current_base = condor_basename(current_filename);
#ifdef WIN32
		if (MATCH == strcasecmp(historyBase, current_base)) {
#else
			if (MATCH == strcmp(historyBase, current_base)) {
#endif
				foundCurrent = true;
			} else if (isHistoryBackup(current_filename, NULL, historyBase)) {
				std::string fullFilePath;
				dircat(historyDir.c_str(), current_filename, fullFilePath);
				historyFiles.push_back(fullFilePath);
			}
		}

		if (historyFiles.size() > 1) {
			// Sort the backup files so that they are in the proper order
			qsort_file_base = historyBase;
			std::sort(historyFiles.begin(), historyFiles.end(), compareHistoryFilenames);
		}

		// if the base history file was found in the directory, add it to
		// the end of the sorted vector
		if (foundCurrent) {
			historyFiles.push_back(passedFileName);
		}

		return historyFiles;
	}



