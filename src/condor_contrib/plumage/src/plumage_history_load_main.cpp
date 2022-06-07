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

// first always :-(
#include "condor_common.h"

// mongodb-devel includes
#include <mongo/client/dbclient.h>
#include <time.h>

// condor includes
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
#include "history_utils.h"
#include "subsystem_info.h"

// local includes
#include "ODSDBNames.h"
#include "ODSUtils.h"

#define NUM_PARAMETERS 3

using namespace std;
using namespace mongo;
using namespace plumage::etl;
using namespace plumage::util;

static void print_usage(char* name) 
{
  printf("Usage: %s [-db <mongodb URL>] [-f <full path to condor history file>] [-h | -help]\n",name);
  printf("\t-db : the mongod connection URL (host[:port])\n");
  printf("\t-f : full path to a specific Condor history file to load\n");
  printf("\t-h | -help : this help message\n");
  printf("No arguments will load all the history and backup files from $HISTORY directory to a mongod server at $PLUMAGE_DB_HOST/_PORT.\n");
  printf("Duplicate jobs (same GlobalJobID) will be upserted (updated or added if missing).\n");
  exit(1);
}

static void readHistoryFromFiles(char *JobHistoryFileName);
static char **findHistoryFiles(int *numHistoryFiles);
static bool isHistoryBackup(const char *fullFilename, time_t *backup_time);
static int compareHistoryFilenames(const void *item1, const void *item2);
static void readHistoryFromFile(char *JobHistoryFileName);

//------------------------------------------------------------------------

static  bool backwards=false;
static  AttrListPrintMask mask;
static  char *BaseJobHistoryFileName = NULL;
static int cluster=-1, proc=-1;
static int specifiedMatch = 0, matchCount = 0;

ODSMongodbOps* etl_client = NULL;

int
main(int argc, char* argv[])
{
    int i;
    char* file_arg = NULL;
    char* db_arg = NULL;
    char* schedd_name = NULL;
    string db_host_port;
    
    // get our condor vars
    config();

	set_mySubSystem( "TOOL", false, SUBSYSTEM_TYPE_TOOL );
    
    for(i=1; i<argc; i++) {
        if (strcmp(argv[i],"-h")==0) {
            print_usage(argv[0]);
        }
        if (strcmp(argv[i],"-help")==0) {
            print_usage(argv[0]);
        }
        if (strcmp(argv[i],"-f")==0) {
            file_arg = argv[i+1];
        }
        if (strcmp(argv[i],"-db")==0) {
            db_arg = argv[i+1];
        }
    }
    if (i<argc) {
        print_usage(argv[0]);
    }
    
    char* tmp = param("SCHEDD_NAME");
    if (!tmp) {
        schedd_name = build_valid_daemon_name(NULL);
    }
    else {
        schedd_name = strdup(tmp);
        free(tmp);
    }
    printf("SCHEDD_NAME = %s\n",schedd_name);

    if (!db_arg) {
        HostAndPort hap = getDbHostPort("PLUMAGE_DB_HOST","PLUMAGE_DB_PORT");
        db_host_port = hap.toString();
    }
    else {
        db_host_port = db_arg;
    }

    printf("mongod server = %s\n",db_host_port.c_str());

    try {
        etl_client = new ODSMongodbOps(DB_JOBS_HISTORY);
        etl_client->init(db_host_port);
        readHistoryFromFiles(file_arg);
    }
    catch(UserException& e) {
        cout << "Error: UserException - " << e.toString() << endl;
        return 1;
    }

    free (schedd_name);
    if (etl_client) delete etl_client;

    return 0;
}

// Read the history from the specified history file, or from all the history files.
// There are multiple history files because we do rotation. 
static void readHistoryFromFiles(char *JobHistoryFileName)
{

    if (JobHistoryFileName) {
        // If the user specified the name of the file to read, we read that file only.
        readHistoryFromFile(JobHistoryFileName);
    } else {
        // The user didn't specify the name of the file to read, so we read
        // the history file, and any backups (rotated versions). 
        int numHistoryFiles;
        char **historyFiles;

        historyFiles = findHistoryFiles(&numHistoryFiles);
        if (historyFiles && numHistoryFiles > 0) {
            int fileIndex;
            if (backwards) { // Reverse reading of history files array
                for(fileIndex = numHistoryFiles - 1; fileIndex >= 0; fileIndex--) {
                    readHistoryFromFile(historyFiles[fileIndex]);
                    free(historyFiles[fileIndex]);
                }
            }
            else {
                for (fileIndex = 0; fileIndex < numHistoryFiles; fileIndex++) {
                    readHistoryFromFile(historyFiles[fileIndex]);
                    free(historyFiles[fileIndex]);
                }
            }
        }
		free(historyFiles);
    }
    return;
}

// Find all of the history files that the schedd created, and put them
// in order by time that they were created. The time comes from a
// timestamp in the file name.
static char **findHistoryFiles(int *numHistoryFiles)
{
    int  fileIndex;
    char **historyFiles = NULL;
    char *historyDir;

    BaseJobHistoryFileName = param("HISTORY");
	if ( BaseJobHistoryFileName == NULL ) {
		fprintf( stderr, "Error: No history file is defined\n");
		fprintf(stderr, "\n");
		print_wrapped_text("Extra Info: " 
						   "The variable HISTORY is not defined in "
						   "your config file. If you want Condor to "
						   "keep a history of past jobs, you must "
						   "define HISTORY in your config file", stderr );

		exit( 1 );    
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

// Given a history file, returns the position offset of the last delimiter
// The last delimiter will be found in the last line of file, 
// and will start with the "***" character string 
static long findLastDelimiter(FILE *fd, char *filename)
{
    int         i;
    bool        found;
    long        seekOffset, lastOffset;
    MyString    buf;
    struct stat st;
  
    // Get file size
    stat(filename, &st);
  
    found = false;
    i = 0;
    while (!found) {
        // 200 is arbitrary, but it works well in practice
        seekOffset = st.st_size - (++i * 200); 
	
        fseek(fd, seekOffset, SEEK_SET);
        
        while (1) {
            if (buf.readLine(fd) == false) 
                break;
	  
            // If line starts with *** and its last line of file
            if (strncmp(buf.Value(), "***", 3) == 0 && buf.readLine(fd) == false) {
                found = true;
                break;
            }
        } 
	
        if (seekOffset <= 0) {
            fprintf(stderr, "Error: Unable to find last delimiter in file: (%s)\n", filename);
            exit(1);
        }
    } 
  
    // lastOffset = beginning of delimiter
    lastOffset = ftell(fd) - buf.Length();
    
    return lastOffset;
}

// Given an offset count that points to a delimiter, this function returns the 
// previous delimiter offset position.
// If clusterId and procId is specified, it will not return the immediately
// previous delimiter, but the nearest previous delimiter that matches
static long findPrevDelimiter(FILE *fd, char* filename, long currOffset)
{
    MyString buf;
    char *owner;
    long prevOffset = -1, completionDate = -1;
    int clusterId = -1, procId = -1;
  
    fseek(fd, currOffset, SEEK_SET);
    buf.readLine(fd);
  
    owner = (char *) malloc(buf.Length() * sizeof(char)); 

    // Current format of the delimiter:
    // *** ProcId = a ClusterId = b Owner = "cde" CompletionDate = f
    // For the moment, owner and completionDate are just parsed in, reserved for future functionalities. 

    sscanf(buf.Value(), "%*s %*s %*s %ld %*s %*s %d %*s %*s %d %*s %*s %s %*s %*s %ld", 
           &prevOffset, &clusterId, &procId, owner, &completionDate);

    if (prevOffset == -1 && clusterId == -1 && procId == -1) {
        fprintf(stderr, 
                "Error: (%s) is an incompatible history file, please run condor_convert_history.\n",
                filename);
        free(owner);
        exit(1);
    }

    // If clusterId.procId is specified
    if (cluster != -1 || proc != -1) {

        // Ok if only clusterId specified
        while (clusterId != cluster || (proc != -1 && procId != proc)) {
	  
            if (prevOffset == 0) { // no match
                free(owner);
                return -1;
            }

            // Find previous delimiter + summary
            fseek(fd, prevOffset, SEEK_SET);
            buf.readLine(fd);
            
            owner = (char *) realloc (owner, buf.Length() * sizeof(char));
      
            sscanf(buf.Value(), "%*s %*s %*s %ld %*s %*s %d %*s %*s %d %*s %*s %s %*s %*s %ld", 
                   &prevOffset, &clusterId, &procId, owner, &completionDate);
        }
    }
 
    free(owner);

    return prevOffset;
}

void writeJobAd(ClassAd* ad) {
    string gjid;

    BSONObjBuilder key;
    if (ad->LookupString(ATTR_GLOBAL_JOB_ID,gjid)) {
        key.append(ATTR_GLOBAL_JOB_ID, gjid);
    }
    else {
        printf("WARNING: unable to parse %s from ClassAd\n",ATTR_GLOBAL_JOB_ID);
        return;
    }

    etl_client->updateAd(key,ad);

}

// Read the history from a single file and print it out. 
static void readHistoryFromFile(char *JobHistoryFileName)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    ClassAd *ad = NULL;
    clock_t start, end;
    double elapsed;
    int i = 0;

    long offset = 0;
    bool BOF = false; // Beginning Of File
    MyString buf;
    
    FILE* LogFile=safe_fopen_wrapper(JobHistoryFileName,"r");
    
	if (!LogFile) {
        fprintf(stderr,"History file (%s) not found or empty.\n", JobHistoryFileName);
        exit(1);
    }

	// In case of rotated history files, check if we have already reached the number of 
	// matches specified by the user before reading the next file
	if (specifiedMatch != 0) { 
        if (matchCount == specifiedMatch) { // Already found n number of matches, cleanup  
            fclose(LogFile);
            return;
        }
	}

	if (backwards) {
        offset = findLastDelimiter(LogFile, JobHistoryFileName);	
    }

    start = clock();
    while(!EndFlag) {

        if (backwards) { // Read history file backwards
            if (BOF) { // If reached beginning of file
                break;
            }
            
            offset = findPrevDelimiter(LogFile, JobHistoryFileName, offset);
            if (offset == -1) { // Unable to match constraint
                break;
            } else if (offset != 0) {
                fseek(LogFile, offset, SEEK_SET);
                buf.readLine(LogFile); // Read one line to skip delimiter and adjust to actual offset of ad
            } else { // Offset set to 0
                BOF = true;
                fseek(LogFile, offset, SEEK_SET);
            }
        }
      
        if( !( ad=new ClassAd(LogFile,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
            fprintf( stderr, "Error:  Out of memory\n" );
            exit( 1 );
        } 
        if( ErrorFlag ) {
            printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
            ErrorFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        } 
        if( EmptyFlag ) {
            EmptyFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        }

        // emit the ad here
        writeJobAd(ad);
        i++;
		
        if(ad) {
            delete ad;
            ad = NULL;
        }
    }
    fclose(LogFile);

    end = clock();
    elapsed = ((float) (end - start)) / CLOCKS_PER_SEC;
    printf("processed %d records from '%s' in %1.3f sec\n",i,JobHistoryFileName,elapsed);

    return;
}
