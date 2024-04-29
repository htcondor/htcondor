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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "basename.h"
#include "directory.h"      // for StatInfo
#include "util_lib_proto.h" // for rotate_file
#include "iso_dates.h"
#include "condor_email.h"

#include "classadHistory.h"

static FILE *HistoryFile_fp = NULL;
static int HistoryFile_RefCount = 0;

char* JobHistoryFileName = NULL;
char* JobHistoryParamName = NULL;
bool        DoHistoryRotation = true;
char*       PerJobHistoryDir = NULL;
static HistoryFileRotationInfo hri;

static void RemoveExtraHistoryFiles(int max_backups, const char* filename);
static int MaybeDeleteOneHistoryBackup(int max_backups, const char* original_filename);
static bool IsHistoryFilename(const char* original_filename, const char *filename, time_t *backup_time);
static void RotateHistory(bool isHistory, const char* filename, const char* new_path);
static int findHistoryOffset(FILE *LogFile);
static FILE* OpenHistoryFile();
static void CloseJobHistoryFile();
static void RelinquishHistoryFile(FILE *fp);

// --------------------------------------------------------------------------
// --------- PUBLIC FUNCTIONS (called by schedd, startd, etc) ---------------
// --------------------------------------------------------------------------

void
InitJobHistoryFile(const char *history_param, const char *per_job_history_param) {

	CloseJobHistoryFile();
	if( history_param ) {
		free(JobHistoryParamName);
		JobHistoryParamName = strdup(history_param);
	}
	if( JobHistoryFileName ) free( JobHistoryFileName );
	if( ! (JobHistoryFileName = param(history_param)) ) {
		  dprintf(D_FULLDEBUG, "No %s file specified in config file\n", history_param );
	}

    // If history rotation is off, then the maximum file size and
    // number of backup files is ignored. 
    DoHistoryRotation = param_boolean("ENABLE_HISTORY_ROTATION", true);
    hri.DoDailyHistoryRotation = param_boolean("ROTATE_HISTORY_DAILY", false);
    hri.DoMonthlyHistoryRotation = param_boolean("ROTATE_HISTORY_MONTHLY", false);
    hri.IsStandardHistory = true;

	long long default_history = 20 * 1024 * 1024;
	long long history_filesize = 0;
    param_longlong("MAX_HISTORY_LOG", history_filesize, true, default_history);
    hri.MaxHistoryFileSize = history_filesize;
    hri.NumberBackupHistoryFiles = param_integer("MAX_HISTORY_ROTATIONS",
                                          2,  // default
                                          1); // minimum

    if (DoHistoryRotation) {
        dprintf(D_ALWAYS, "History file rotation is enabled.\n");
        dprintf(D_ALWAYS, "  Maximum history file size is: %zd bytes\n",
                (ssize_t)hri.MaxHistoryFileSize);
        dprintf(D_ALWAYS, "  Number of rotated history files is: %d\n", 
                hri.NumberBackupHistoryFiles);
    } else {
        dprintf(D_ALWAYS, "WARNING: History file rotation is disabled and it "
                "may grow very large.\n");
    }

    if (PerJobHistoryDir != NULL) free(PerJobHistoryDir);
    if ((PerJobHistoryDir = param(per_job_history_param)) != NULL) {
        StatInfo si(PerJobHistoryDir);
        if (!si.IsDirectory()) {
            dprintf(D_ERROR,
                    "invalid %s (%s): must point to a "
                    "valid directory; disabling per-job history output\n",
                    per_job_history_param, PerJobHistoryDir);
            free(PerJobHistoryDir);
            PerJobHistoryDir = NULL;
        }
        else {
            dprintf(D_ALWAYS,
                    "Logging per-job history files to: %s\n",
                    PerJobHistoryDir);
        }
    }

}

// --------------------------------------------------------------------------
// Write job ads to history file when they're destroyed
// --------------------------------------------------------------------------

void
AppendHistory(ClassAd* ad)
{
  bool failed = false;
  static bool sent_mail_about_bad_history = false;

  if (!JobHistoryFileName) return;
  dprintf(D_FULLDEBUG, "Saving classad to history file\n");

  classad::References excludeAttrs;
  bool include_env = param_boolean("HISTORY_CONTAINS_JOB_ENVIRONMENT", true);

  // Remove Env before serializing, if asked to
  if (!include_env) {
	  excludeAttrs.emplace(ATTR_JOB_ENV_V1);
	  excludeAttrs.emplace(ATTR_JOB_ENVIRONMENT);
  }
  // First we serialize the ad. If history file rotation is on,
  // we'll need to know how big the ad is before we write it to the 
  // history file. 

  std::string ad_string;
  int ad_size;
  sPrintAd(ad_string, *ad, nullptr, include_env ? nullptr : &excludeAttrs);
  ad_size = ad_string.length();

  if (JobHistoryFileName && DoHistoryRotation) { MaybeRotateHistory(hri, ad_size, JobHistoryFileName); }

  FILE *LogFile = OpenHistoryFile();
  if (!LogFile) {
	  dprintf(D_ALWAYS,"ERROR saving to history file (%s): %s\n",
			  JobHistoryFileName, strerror(errno));
	  failed = true;
  } else {
	  int offset = findHistoryOffset(LogFile);
	  if (fputs(ad_string.c_str(), LogFile) == EOF) {
		  dprintf(D_ALWAYS, 
				  "ERROR: failed to write job class ad to history file %s\n",
				  JobHistoryFileName);
		  failed = true;
	  } else {
		  int cluster, proc, completion;
		  std::string owner;

		  if (!ad->LookupInteger("ClusterId", cluster)) {
			  cluster = -1;
		  }
		  if (!ad->LookupInteger("ProcId", proc)) {
			  proc = -1;
		  }
		  if (!ad->LookupInteger("CompletionDate", completion)) {
			  completion = -1;
		  }
		  if (!ad->LookupString("Owner", owner)) {
			  owner = "?";
		  }
		  fprintf(LogFile,
                      "*** Offset = %d ClusterId = %d ProcId = %d Owner = \"%s\" CompletionDate = %d\n",
				  offset, cluster, proc, owner.c_str(), completion);
		  fflush( LogFile );
      }
  }

  // If we successfully obtained a handle to the history file,
  // relinquish it now.  Note it is safe to call RelinquishHistoryFile
  // even if OpenHistoryFile returned NULL.
  RelinquishHistoryFile( LogFile );
  LogFile = NULL;

  if ( failed ) {
	  // We failed to write to the history file for some reason. May help
	  // to close it and attempt to re-open it next time.
	  // Note it is safe to call CloseJobHistoryFile() even if OpenHistoryFile
	  // returned NULL.
	  CloseJobHistoryFile();

	  // Send email to the admin.
	  if ( !sent_mail_about_bad_history ) {
		  std::string msg;
		  formatstr(msg, "Failed to write to %s file", JobHistoryParamName);
		  FILE* email_fp = email_admin_open(msg.c_str());
		  if ( email_fp ) {
			sent_mail_about_bad_history = true;
			fprintf(email_fp,
			 "Failed to write completed job class ad to %s file:\n"
			 "      %s\n"
			 "If you do not wish for Condor to save completed job ClassAds\n"
			 "for later viewing via the condor_history command, you can \n"
			 "remove the '%s' parameter line specified in the condor_config\n"
			 "file(s) and issue a condor_reconfig command.\n"
			 ,JobHistoryParamName,JobHistoryFileName,JobHistoryParamName);
			email_close(email_fp);
		  }
	  }
  } else {
	  // did not fail, reset our email flag.
	  sent_mail_about_bad_history = false;
  }
  
  return;
}

void
WritePerJobHistoryFile(ClassAd* ad, bool useGjid)
{
	if (PerJobHistoryDir == nullptr) {
		return;
	}

	// construct the name (use cluster.proc)
	int cluster, proc;
	if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		dprintf(D_ERROR,
		        "not writing per-job history file: no cluster id in ad\n");
		return;
	}
	if (!ad->LookupInteger(ATTR_PROC_ID, proc)) {
		dprintf(D_ERROR,
		        "not writing per-job history file: no proc id in ad\n");
		return;
	}
	std::string file_name;
	std::string temp_file_name;
	if (useGjid) {
		std::string gjid;
		ad->LookupString(ATTR_GLOBAL_JOB_ID, gjid);
		formatstr(file_name, "%s/history.%s", PerJobHistoryDir, gjid.c_str());
		formatstr(temp_file_name, "%s/.history.%s.tmp", PerJobHistoryDir, gjid.c_str());
	} else {
		formatstr(file_name, "%s/history.%d.%d", PerJobHistoryDir, cluster, proc);
		formatstr(temp_file_name, "%s/.history.%d.%d.tmp", PerJobHistoryDir, cluster, proc);
	}

	// Now write out the file.  We write it first to temp_file_name, and then
	// atomically rename it to file_name, so that another process reading history
	// files will never see an empty file or incomplete data in the event the history file
	// is read at the same time we are still writing it.

	// first write out the file to the temp_file_name
	int fd = safe_open_wrapper_follow(temp_file_name.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd == -1) {
		EXCEPT("error %d (%s) opening per-job history file for job %d.%d",
		        errno, strerror(errno), cluster, proc);
		return;
	}
	FILE* fp = fdopen(fd, "w");
	if (fp == nullptr) {
		int err = errno;
		close(fd);
		unlink(temp_file_name.c_str());
		EXCEPT("error %d (%s) fdopening file stream for per-job history for job %d.%d",
		        err, strerror(err), cluster, proc);
		return;
	}
	bool include_env = param_boolean("HISTORY_CONTAINS_JOB_ENVIRONMENT", true);
	classad::References env;
	if (!include_env) {
		env.emplace(ATTR_JOB_ENV_V1);
		env.emplace(ATTR_JOB_ENVIRONMENT);
	}

	if (!fPrintAd(fp, *ad, true, nullptr, include_env ? nullptr : &env)) {
		int err = errno;
		fclose(fp);
		unlink(temp_file_name.c_str());
		EXCEPT("error %d writing per-job history file for job %d.%d",
		        err, cluster, proc);
		return;
	}
	fclose(fp);

	// now atomically rename from temp_file_name to file_name
    if (rotate_file(temp_file_name.c_str(), file_name.c_str())) {
		unlink(temp_file_name.c_str());
		EXCEPT("error writing per-job history file for job %d.%d (during rename)",
		        cluster, proc);
    }
}

// --------------------------------------------------------------------------
// ------ PRIVATE / STATIC FUNCTIONS (implementation specific to this module)
// --------------------------------------------------------------------------

// Obtain a handle to the HISTORY file.  Note that each call to OpenHistoryFile()
// that return non-NULL _MUST_ be paired with a call to RelinquishHistoryFile().
static FILE *
OpenHistoryFile() {
		// Note that we are passing O_LARGEFILE, which lets us deal
		// with files that are larger than 2GB. On systems where
		// O_LARGEFILE isn't defined, the Condor source defines it to
		// be 0 which has no effect. So we'll take advantage of large
		// files where we can, but not where we can't.
	if( !HistoryFile_fp ) {
		int fd = safe_open_wrapper_follow(JobHistoryFileName,
                O_RDWR|O_CREAT|O_APPEND|O_LARGEFILE|_O_NOINHERIT,
                0644);
		if( fd < 0 ) {
			dprintf(D_ALWAYS,"ERROR opening history file (%s): %s\n",
					JobHistoryFileName, strerror(errno));
			return NULL;
		}
		HistoryFile_fp = fdopen(fd, "r+");
		if ( !HistoryFile_fp ) {
			dprintf(D_ALWAYS,"ERROR opening history file fp (%s): %s\n",
					JobHistoryFileName, strerror(errno));
			// return now, because we CANNOT increment the refcount below on failure.
			close(fd);
			return NULL;
		}
	}
	HistoryFile_RefCount++;
	return HistoryFile_fp;
}

static void
RelinquishHistoryFile(FILE *fp) {
	if( fp ) {  // passing a NULL fp is allowed, but don't alter the refcount
		HistoryFile_RefCount--;
	}
		// keep the file open
}

static void
CloseJobHistoryFile() {
	ASSERT( HistoryFile_RefCount == 0 );
	if( HistoryFile_fp ) {
		fclose( HistoryFile_fp );
		HistoryFile_fp = NULL;
	}
}

// --------------------------------------------------------------------------
// Decide if we should rotate the history file, and do the rotation if 
// necessary.
// --------------------------------------------------------------------------
void
MaybeRotateHistory(const HistoryFileRotationInfo& fri, int size_to_append, const char* filename, const char* new_filepath)
{
        StatInfo    history_stat_info(filename);
        filesize_t file_size = history_stat_info.GetFileSize();

        if (history_stat_info.Error() == SINoFile) {
            ; // Do nothing, the history file doesn't exist
        } else if (history_stat_info.Error() != SIGood) {
            dprintf(D_ALWAYS, "Couldn't stat history file, will not rotate.\n");
        } else {
			bool mustRotate = false;

            if (file_size + size_to_append > fri.MaxHistoryFileSize) {
				mustRotate = true;
			}


			if (fri.DoDailyHistoryRotation) {
				time_t mod_tt = history_stat_info.GetModifyTime();
				struct tm *mod_t = localtime(&mod_tt);
				int mod_yday = mod_t->tm_yday;
				int mod_year = mod_t->tm_year;

				time_t now_tt = time(0);
				struct tm *now_t = localtime(&now_tt);
				int now_yday = now_t->tm_yday;
				int now_year = now_t->tm_year;

				if ((now_yday > mod_yday) ||
					(now_year > mod_year)) {
					mustRotate = true;
				}
			}

			if (fri.DoMonthlyHistoryRotation) {
				time_t mod_tt = history_stat_info.GetModifyTime();
				struct tm *mod_t = localtime(&mod_tt);
				int mod_mon = mod_t->tm_mon;
				int mod_year = mod_t->tm_year;

				time_t now_tt = time(0);
				struct tm *now_t = localtime(&now_tt);
				int now_mon = now_t->tm_mon;
				int now_year = now_t->tm_year;

				if ((now_mon > mod_mon) ||
					(now_year > mod_year)) {
					mustRotate = true;
				}
			}

			if (mustRotate) {
                // Writing the new ClassAd will make the history file too 
                // big, so we will rotate the history file after removing
                // extra history files. 
                dprintf(D_ALWAYS, "Will rotate history file.\n");
                if (!new_filepath) { RemoveExtraHistoryFiles(fri.NumberBackupHistoryFiles, filename); }
                RotateHistory(fri.IsStandardHistory, filename, new_filepath);
            }
        }
    return;
    
}

// --------------------------------------------------------------------------
// We only keep a certain number of history files, so before we rotate the 
// history file, we need to make sure that we get rid of any old ones, so we
// don't go over the max once we do the rotation. 
//
// Our algorithm is simple (easy to debug) but might have pathological behavior.
// We walk through the directory that the history file is in, count how many
// history files there are, and if there are >= max, we delete the oldest. 
// If we need to delete more than one (perhaps someone changed the configured
// max to be smaller), then we walk through the whole directory more than once. 
// I am willing to bet that this is rare enough that it's not worth making this
// more complicated. 
// --------------------------------------------------------------------------
static void
RemoveExtraHistoryFiles(int max_backups, const char* file_name)
{
    int num_backups;

    do {
        num_backups = MaybeDeleteOneHistoryBackup(max_backups, file_name);
    } while (num_backups >= max_backups);
    return;
}

// --------------------------------------------------------------------------
// Count the number of history file backups. Delete the oldest one if we
// are at the maximum. See RemoveExtraHistoryFiles();
// --------------------------------------------------------------------------
static int
MaybeDeleteOneHistoryBackup(int max_backups, const char* original_filename)
{
    int num_backups = 0;
	std::string history_dir = condor_dirname(original_filename);

	Directory dir(history_dir.c_str());
	const char *current_filename;
	time_t current_time;
	char *oldest_history_filename = NULL;
	time_t oldest_time = 0;

	// Find number of backups and oldest backup
	for (current_filename = dir.Next(); 
			current_filename != NULL; 
			current_filename = dir.Next()) {

		if (IsHistoryFilename(original_filename, current_filename, &current_time)) {
			num_backups++;
			if (oldest_history_filename == NULL 
					|| current_time < oldest_time) {

				if (oldest_history_filename != NULL) {
					free(oldest_history_filename);
				}
				oldest_history_filename = strdup(current_filename);
				oldest_time = current_time;
			}
		}
	}

	// If we have too many backups, delete the oldest
	if (oldest_history_filename != NULL && num_backups >= max_backups) {
		dprintf(D_ALWAYS, "Before rotation, deleting old history file %s\n",
				oldest_history_filename);
		num_backups--;

		if (dir.Find_Named_Entry(oldest_history_filename)) {
			if (!dir.Remove_Current_File()) {
				dprintf(D_ALWAYS, "Failed to delete %s\n", oldest_history_filename);
				num_backups = 0; // prevent looping forever
			}
		} else {
			dprintf(D_ALWAYS, "Failed to find/delete %s\n", oldest_history_filename);
			num_backups = 0; // prevent looping forever
		}
	}
	free(oldest_history_filename);
	return num_backups;
}

// --------------------------------------------------------------------------
// A history file should being with the base that the user specified, 
// and it should end with an ISO time. We check both, and return the time
// specified by the ISO time. 
// --------------------------------------------------------------------------
static bool
IsHistoryFilename(const char* original_filename, const char *filename, time_t *backup_time)
{
    bool       is_history_filename;
    const char *history_base;
    int        history_base_length;

    is_history_filename = false;
    history_base        = condor_basename(original_filename);
    history_base_length = strlen(history_base);

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
            *backup_time = mktime(&file_time);
        }
    }

    return is_history_filename;
}

// --------------------------------------------------------------------------
// Rotate the history file. This is called by MaybeRotateHistory()
// --------------------------------------------------------------------------
static void
RotateHistory(bool isHistory, const char* filename, const char* new_path)
{
    // The job history will be named with the current time. 
    // I hate timestamps of seconds, because they aren't readable by 
    // humans, so we'll use ISO 8601, which is easily machine and human
    // readable, and it sorts nicely. So first we create a representation
    // for the current time.
    time_t     current_time;
    struct tm  *local_time;
    char       iso_time[ISO8601_DateAndTimeBufferMax];

    current_time = time(NULL);
    local_time = localtime(&current_time);
    time_to_iso8601(iso_time, *local_time, ISO8601_BasicFormat, 
                               ISO8601_DateAndTime, false);

    // First, select a name for the rotated history file
    std::string rotated_history_name = "";
    if (new_path) {
        const char* file_base = condor_basename(filename);
        dircat(new_path, file_base, rotated_history_name);
    } else {
        rotated_history_name += filename;
    }
    rotated_history_name += '.';
    rotated_history_name += iso_time;

    if (isHistory) { CloseJobHistoryFile(); }

    // Now rotate the file
    if (rotate_file(filename, rotated_history_name.c_str())) {
        dprintf(D_ALWAYS, "Failed to rotate history file to %s\n",
                rotated_history_name.c_str());
        dprintf(D_ALWAYS, "Because rotation failed, the history file may get very large.\n");
    }

    return;
}

// --------------------------------------------------------------------------
// Figure out how far from the end the beginning of the last line in the
// history file is. We assume that the file is open. We reset the file pointer
// to the end of the file when we are done.
// --------------------------------------------------------------------------
static int
findHistoryOffset(FILE *LogFile)
{
    int offset=0;
    int file_size;
    const int JUMP = 200;

    fseek(LogFile, 0, SEEK_END);
    file_size = ftell(LogFile);
    if (file_size == 0 || file_size == -1) {
        // If there is nothing in the file, the offset of the previous
        // line is 0. 
        offset = 0;
    } else {
        bool found = false;
        char *buffer = (char *) malloc(JUMP + 1);
        ASSERT( buffer )
        int current_offset; 

        // We need to skip the last newline
        if (file_size > 1) {
            file_size--;
        }

        current_offset = file_size;
        while (!found) {
            current_offset -= JUMP;
            if (current_offset < 0) {
                current_offset = 0;
            }
            memset(buffer, 0, JUMP+1);
            if (fseek(LogFile, current_offset, SEEK_SET)) {
                // We failed for some reason
                offset = -1;
                break;
            }
            int n = fread(buffer, 1, JUMP, LogFile);
            if (n < JUMP) {
                // We failed for some reason: we know we should 
                // be able to read this much. 
                offset = -1;
                break;
            }
            // Look for the newline, backwards through this buffer
            for (int i = JUMP-1; i >= 0; i--) {
                if (buffer[i] == '\n') {
                    found = true;
                    offset = current_offset + i + 1;
                    break;
                }
            }
            if (current_offset == 0) {
                // We read all the way back to the beginning
                if (!found) {
                    offset = 0;
                    found = true;
                }
                break;
            }
        }
		free(buffer);
    }
	
    fseek(LogFile, 0, SEEK_END);
    return offset;
}




