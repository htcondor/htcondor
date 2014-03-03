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
#ifdef HAVE_EXT_POSTGRESQL
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

#include "sqlquery.h"
#include "historysnapshot.h"

#define NUM_PARAMETERS 3

void Usage(char* name, int iExitCode=1);

void Usage(char* name, int iExitCode ) 
{
  printf("Usage: %s -name quill-name\n",name);
  exit(iExitCode);
}

static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName, char *&queryPassword);
static bool checkDBconfig();

static void readHistoryFromFiles(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr);
static char **findHistoryFiles(int *numHistoryFiles);
static bool isHistoryBackup(const char *fullFilename, time_t *backup_time);
static int compareHistoryFilenames(const void *item1, const void *item2);
static void readHistoryFromFile(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr);

//------------------------------------------------------------------------

static CollectorList * Collectors = NULL;
static	QueryResult result;
static	CondorQuery	quillQuery(QUILL_AD);
static	ClassAdList	quillList;
static  bool longformat=false;
static  bool customFormat=false;
static  bool backwards=false;
static  AttrListPrintMask mask;
static  char *BaseJobHistoryFileName = NULL;
static int cluster=-1, proc=-1;
static int specifiedMatch = 0, matchCount = 0;

int
main(int argc, char* argv[])
{
  Collectors = NULL;

  HistorySnapshot *historySnapshot;
  SQLQuery queryhor;
  SQLQuery queryver;
  QuillErrCode st;

  void **parameters;
  char *dbconn=NULL;
  bool readfromfile = false,remotequill=false;

  char *dbIpAddr=NULL, *dbName=NULL,*queryPassword=NULL,*quillName=NULL;

  AttrList *ad=0;

  int flag = 1;

  MyString tmp;

  int i;
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  myDistro->Init( argc, argv );

  queryhor.setQuery(HISTORY_ALL_HOR, NULL);
  queryver.setQuery(HISTORY_ALL_VER, NULL);

  longformat=TRUE;   
  for(i=1; i<argc; i++) {
    if(strcmp(argv[i], "-name")==0) {
		i++;
		if (argc <= i) {
			fprintf( stderr,
					 "Error: Argument -name requires the name of a quilld as a parameter\n" );
			exit(1);
		}
		
		if( !(quillName = get_daemon_name(argv[i])) ) {
			fprintf( stderr, "Error: unknown host %s\n",
					 get_host_part(argv[i]) );
			printf("\n");
			print_wrapped_text("Extra Info: The name given with the -name "
							   "should be the name of a condor_quilld process. "
							   "Normally it is either a hostname, or "
							   "\"name@hostname\". "
							   "In either case, the hostname should be the "
							   "Internet host name, but it appears that it "
							   "wasn't.",
							   stderr);
			exit(1);
		}
		tmp.formatstr ("%s == \"%s\"", ATTR_NAME, quillName);
		quillQuery.addORConstraint (tmp.Value());

                tmp.formatstr ("%s == \"%s\"", ATTR_SCHEDD_NAME, quillName);
                quillQuery.addORConstraint (tmp.Value());

		remotequill = true;
		readfromfile = false;
    }
    else if (strcmp(argv[i],"-help")==0) {
		Usage(argv[0],0);
    }
  }
  if (i<argc) Usage(argv[0]);
  
  config();
  
	/* This call must happen AFTER config() is called */
  if (checkDBconfig() == true && !readfromfile) {
  	readfromfile = false;
  } else {
		  /* couldn't get DB configuration, so bail out */
    printf("Error: Cannot use DB to get history information\n");
  	exit(1);
  }

  if(readfromfile == false) {
	  if(remotequill) {
		  if (Collectors == NULL) {
			  Collectors = CollectorList::create();
			  if(Collectors == NULL ) {
				  printf("Error: Unable to get list of known collectors\n");
				  exit(1);
			  }
		  }
		  result = Collectors->query ( quillQuery, quillList );
		  if(result != Q_OK) {
			  printf("Fatal Error querying collectors\n");
			  exit(1);
		  }

		  if(quillList.MyLength() == 0) {
			  printf("Error: Unknown quill server %s\n", quillName);
			  exit(1);
		  }
		  
		  quillList.Open();
		  while ((ad = quillList.Next())) {
				  // get the address of the database
			  dbIpAddr = dbName = queryPassword = NULL;
			  if (!ad->LookupString(ATTR_QUILL_DB_IP_ADDR, &dbIpAddr) ||
				  !ad->LookupString(ATTR_QUILL_DB_NAME, &dbName) ||
				  !ad->LookupString(ATTR_QUILL_DB_QUERY_PASSWORD, &queryPassword) || 
				  (ad->LookupBool(ATTR_QUILL_IS_REMOTELY_QUERYABLE,flag) && !flag)) {
				  printf("Error: The quill daemon \"%s\" is not set up "
						 "for database queries\n", 
						 quillName);
				  exit(1);
			  }
		  }
	  }
	  dbconn = getDBConnStr(quillName,dbIpAddr,dbName,queryPassword);
	  historySnapshot = new HistorySnapshot(dbconn);
		  //printf ("\n\n-- Quill: %s : %s : %s\n", quillName, dbIpAddr, dbName);
	  
	  st = historySnapshot->sendQuery(&queryhor, &queryver, longformat, true);
		  //if there's a failure here and if we're not posing a query on a 
		  //remote quill daemon, we should instead query the local file
	  if(st == QUILL_FAILURE) {
		  printf( "-- Database at %s not reachable\n", dbIpAddr);
	  }
		  // query history table
	  if (historySnapshot->isHistoryEmpty()) {
		  printf("No historical jobs in the database\n");
	  }
	  historySnapshot->release();
	  delete(historySnapshot);
  }
  
  
  if(parameters) free(parameters);
  if(dbIpAddr) free(dbIpAddr);
  if(dbName) free(dbName);
  if(queryPassword) free(queryPassword);
  if(quillName) free(quillName);
  if(dbconn) free(dbconn);
  return 0;
}


//------------------------------------------------------------------------

/* this function for checking whether database can be used for 
   querying in local machine */
static bool checkDBconfig() {
	char *tmp;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		return false;
	};

	tmp = param("QUILL_NAME");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_IP_ADDR");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_NAME");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_QUERY_PASSWORD");
	if (!tmp) {
		return false;
	}
	free(tmp);

	return true;
}


static char * getDBConnStr(char *&quillName, 
						   char *&databaseIp, 
						   char *&databaseName, 
						   char *&queryPassword) {
  char            *host, *port, *dbconn, *ptr_colon;
  char            *tmpquillname, *tmpdatabaseip, *tmpdatabasename, *tmpquerypassword;
  int             len, tmp1, tmp2, tmp3;

  if((!quillName && !(tmpquillname = param("QUILL_NAME"))) ||
     (!databaseIp && !(tmpdatabaseip = param("QUILL_DB_IP_ADDR"))) ||
     (!databaseName && !(tmpdatabasename = param("QUILL_DB_NAME"))) ||
     (!queryPassword && !(tmpquerypassword = param("QUILL_DB_QUERY_PASSWORD")))) {
    fprintf( stderr, "Error: Could not find database related parameter\n");
    fprintf(stderr, "\n");
    print_wrapped_text("Extra Info: " 
                       "The most likely cause for this error "
                       "is that you have not defined "
					   "QUILL_NAME/QUILL_DB_IP_ADDR/"
					   "QUILL_DB_NAME/QUILL_DB_QUERY_PASSWORD "
                       "in the condor_config file.  You must "
                       "define this variable in the config file", stderr);
 
    exit( 1 );    
  }
  
  if(!quillName) {
	  quillName = tmpquillname;
  }
  if(!databaseIp) {
	  if(tmpdatabaseip[0] != '<') {
			  //2 for the two brackets and 1 for the null terminator
		  databaseIp = (char *) malloc(strlen(tmpdatabaseip)+3);
		  sprintf(databaseIp, "<%s>", tmpdatabaseip);
		  free(tmpdatabaseip);
	  }
	  else {
		  databaseIp = tmpdatabaseip;
	  }
  }
  if(!databaseName) {
	  databaseName = tmpdatabasename;
  }
  if(!queryPassword) {
	  queryPassword = tmpquerypassword;
  }
  
  tmp1 = strlen(databaseName);
  tmp2 = strlen(queryPassword);
  len = strlen(databaseIp);

	  //the 6 is for the string "host= " or "port= "
	  //the rest is a subset of databaseIp so a size of
	  //databaseIp is more than enough 
  host = (char *) malloc((len+6) * sizeof(char));
  port = (char *) malloc((len+6) * sizeof(char));
  
	  //here we break up the ipaddress:port string and assign the  
	  //individual parts to separate string variables host and port
  ptr_colon = strchr(databaseIp, ':');
  strcpy(host, "host= ");
  strncat(host,
          databaseIp+1,
          ptr_colon - databaseIp-1);
  strcpy(port, "port= ");
  strcat(port, ptr_colon+1);
  port[strlen(port)-1] = '\0';
  
	  //tmp3 is the size of dbconn - its size is estimated to be
	  //(2 * len) for the host/port part, tmp1 + tmp2 for the 
	  //password and dbname part and 1024 as a cautiously 
	  //overestimated sized buffer
  tmp3 = (2 * len) + tmp1 + tmp2 + 1024;
  dbconn = (char *) malloc(tmp3 * sizeof(char));
  sprintf(dbconn, "%s %s user=quillreader password=%s dbname=%s", 
		  host, port, queryPassword, databaseName);

  free(host);
  free(port);
  return dbconn;
}


// Read the history from the specified history file, or from all the history files.
// There are multiple history files because we do rotation. 
static void readHistoryFromFiles(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr)
{
    // Print header
    if ((!longformat) && (!customFormat)) {
        short_header();
    }

    if (JobHistoryFileName) {
        // If the user specified the name of the file to read, we read that file only.
        readHistoryFromFile(JobHistoryFileName, constraint, constraintExpr);
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
                    readHistoryFromFile(historyFiles[fileIndex], constraint, constraintExpr);
                    free(historyFiles[fileIndex]);
                }
            }
            else {
                for (fileIndex = 0; fileIndex < numHistoryFiles; fileIndex++) {
                    readHistoryFromFile(historyFiles[fileIndex], constraint, constraintExpr);
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

// Read the history from a single file and print it out. 
static void readHistoryFromFile(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    AttrList *ad = NULL;

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
      
        if( !( ad=new AttrList(LogFile,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
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
        if (!constraint || EvalBool(ad, constraintExpr)) {
            if (longformat) { 
                fPrintAd(stdout, *ad); printf("\n"); 
            } else {
                if (customFormat) {
                    mask.display(stdout, ad);
                } else {
                    displayJobShort(ad);
                }
            }

            matchCount++; // if control reached here, match has occured

            if (specifiedMatch != 0) { // User specified a match number
                if (matchCount == specifiedMatch) { // Found n number of matches, cleanup  
                    if (ad) {
                        delete ad;
                        ad = NULL;
                    }
                    
                    fclose(LogFile);
                    return;
                }
            }
		}
		
        if(ad) {
            delete ad;
            ad = NULL;
        }
    }
    fclose(LogFile);
    return;
}

#endif /* HAVE_EXT_POSTGRESQL */

