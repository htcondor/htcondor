/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

#include "history_utils.h"

#if WANT_QUILL
#include "sqlquery.h"
#include "historysnapshot.h"
#endif /* WANT_QUILL */

#define NUM_PARAMETERS 3


static void Usage(char* name) 
{
#if WANT_QUILL
  printf("Usage: %s [-l] [-f history-filename] [-name quill-name] [-format spec attribute] [-constraint expr | cluster_id | cluster_id.proc_id | owner | -completedsince date/time]\n",name);
#else 
  printf("Usage: %s [-l] [-f history-filename] [-format spec attribute] [-constraint expr | cluster_id | cluster_id.proc_id | owner]\n",name);
#endif /* WANT_QUILL */

  exit(1);
}

#if WANT_QUILL
static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName, char *&queryPassword);
static bool checkDBconfig();
#endif /* WANT_QUILL */

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
static  AttrListPrintMask mask;
static  char *BaseJobHistoryFileName = NULL;

int
main(int argc, char* argv[])
{
  Collectors = NULL;

#if WANT_QUILL
  HistorySnapshot *historySnapshot;
  SQLQuery queryhor;
  SQLQuery queryver;
  QuillErrCode st;
#endif /* WANT_QUILL */

  void **parameters;
  char *dbconn=NULL;
  int cluster=-1, proc=-1;
  char *completedsince = NULL;
  char *owner=NULL;
  bool readfromfile = false,remotequill=false;

  char* JobHistoryFileName=NULL;
  char *dbIpAddr=NULL, *dbName=NULL,*queryPassword=NULL,*quillName=NULL;


  char* constraint=NULL;
  ExprTree *constraintExpr=NULL;

  AttrList *ad=0;

  int flag = 1;

  char tmp[512];

  int i;
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  myDistro->Init( argc, argv );

#if WANT_QUILL
  queryhor.setQuery(HISTORY_ALL_HOR, NULL);
  queryver.setQuery(HISTORY_ALL_VER, NULL);
#endif /* WANT_QUILL */

  for(i=1; i<argc; i++) {
    if (strcmp(argv[i],"-l")==0) {
      longformat=TRUE;   
    }

#if WANT_QUILL
    else if(strcmp(argv[i], "-name")==0) {
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
		sprintf (tmp, "%s == \"%s\"", ATTR_NAME, quillName);      		
		quillQuery.addORConstraint (tmp);

                sprintf (tmp, "%s == \"%s\"", ATTR_SCHEDD_NAME, quillName);
                quillQuery.addORConstraint (tmp);

		remotequill = true;
		readfromfile = false;
    }
#endif /* WANT_QUILL */
    else if (strcmp(argv[i],"-f")==0) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
    }
    else if (strcmp(argv[i],"-help")==0) {
		Usage(argv[0]);
    }
    else if (strcmp(argv[i],"-format")==0) {
		if (argc <= i + 2) {
			fprintf(stderr,
					"Error: Argument -format requires a spec and "
					"classad attribute name as parameters.\n");
			fprintf(stderr,
					"\t\te.g. condor_history -format '%%d' ClusterId\n");
			exit(1);
		}
		mask.registerFormat(argv[i + 1], argv[i + 2]);
		customFormat = true;
		i += 2;
    }
    else if (strcmp(argv[i],"-constraint")==0) {
		if (i+1==argc || constraint) break;
		sprintf(tmp,"(%s)",argv[i+1]);
		constraint=tmp;
		i++;
		readfromfile = true;
    }
#if WANT_QUILL
    else if (strcmp(argv[i],"-completedsince")==0) {
		i++;
		if (argc <= i) {
			fprintf(stderr,
					"Error: Argument -completedsince requires a date and "
					"optional timestamp as a parameter.\n");
			fprintf(stderr,
					"\t\te.g. condor_history -completedsince \"2004-10-19 10:23:54\"\n");
			exit(1);
		}
		
		if (constraint) break;
		constraint = completedsince;
		completedsince = strdup(argv[i]);
		parameters[0] = completedsince;
		queryhor.setQuery(HISTORY_COMPLETEDSINCE_HOR,parameters);
		queryver.setQuery(HISTORY_COMPLETEDSINCE_VER,parameters);
    }
#endif /* WANT_QUILL */

    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
		if (constraint) break;
		sprintf (tmp, "((%s == %d) && (%s == %d))", 
				 ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
		constraint=tmp;
		parameters[0] = &cluster;
		parameters[1] = &proc;
#if WANT_QUILL
		queryhor.setQuery(HISTORY_CLUSTER_PROC_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_PROC_VER, parameters);
#endif /* WANT_QUILL */
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
		if (constraint) break;
		sprintf (tmp, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
		constraint=tmp;
		parameters[0] = &cluster;
#if WANT_QUILL
		queryhor.setQuery(HISTORY_CLUSTER_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_VER, parameters);
#endif /* WANT_QUILL */
    }
    else {
		if (constraint) break;
		owner = (char *) malloc(512 * sizeof(char));
		sscanf(argv[i], "%s", owner);	
		sprintf(tmp, "(%s == \"%s\")", ATTR_OWNER, owner);
		constraint=tmp;
		parameters[0] = owner;
#if WANT_QUILL
		queryhor.setQuery(HISTORY_OWNER_HOR, parameters);
		queryver.setQuery(HISTORY_OWNER_VER, parameters);
#endif /* WANT_QUILL */
    }
  }
  if (i<argc) Usage(argv[0]);
  
  config();
  
  if( constraint && Parse( constraint, constraintExpr ) ) {
	  fprintf( stderr, "Error:  could not parse constraint %s\n", constraint );
	  exit( 1 );
  }

#if WANT_QUILL
	/* This call must happen AFTER config() is called */
  if (checkDBconfig() == true && !readfromfile) {
  	readfromfile = false;
  } else {
  	readfromfile = true;
  }
#else 
  readfromfile = true;
#endif /* WANT_QUILL */

  if(readfromfile == false) {
#if WANT_QUILL
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
	  printf ("\n\n-- Quill: %s : %s : %s\n", quillName, 
			  dbIpAddr, dbName);
	  
	  st = historySnapshot->sendQuery(&queryhor, &queryver, longformat);
		  //if there's a failure here and if we're not posing a query on a 
		  //remote quill daemon, we should instead query the local file
	  if(st == FAILURE) {
	        printf( "-- Database at %s not reachable\n", dbIpAddr);
		if(!remotequill) {
		  char *tmp = param("HISTORY");
		  printf( "--Failing over to the history file at %s instead --\n",tmp);
		  if(!tmp) {
			free(tmp);
		  }
		  readfromfile = true;
	  	}
	  }
		  // query history table
	  if (st == HISTORY_EMPTY) {
		  printf("No historical jobs in the database match your query\n");
	  }
	  historySnapshot->release();
	  delete(historySnapshot);
#endif /* WANT_QUILL */
  }
  
  if(readfromfile == true) {
      readHistoryFromFiles(JobHistoryFileName, constraint, constraintExpr);
  }
  
  
  if(owner) free(owner);
  if(completedsince) free(completedsince);
  if(parameters) free(parameters);
  if(dbIpAddr) free(dbIpAddr);
  if(dbName) free(dbName);
  if(queryPassword) free(queryPassword);
  if(quillName) free(quillName);
  if(dbconn) free(dbconn);
  return 0;
}


//------------------------------------------------------------------------

#if WANT_QUILL

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

#endif /* WANT_QUILL */

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
            for (fileIndex = 0; fileIndex < numHistoryFiles; fileIndex++) {
                readHistoryFromFile(historyFiles[fileIndex], constraint, constraintExpr);
                free(historyFiles[fileIndex]);
            }
            free(historyFiles);
        }
    }
    return;
}

// Find all of the history files that the schedd created, and put them
// in order by time that they were created. The time comes from a
// timestamp in the file name.
static char **findHistoryFiles(int *numHistoryFiles)
{
    int  fileIndex;
    char **historyFiles;
    char *historyDir;

    BaseJobHistoryFileName = param("HISTORY");
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
            qsort(historyFiles, sizeof(char*), (*numHistoryFiles)-1, compareHistoryFilenames);
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

// Read the history from a single file and print it out. 
static void readHistoryFromFile(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    AttrList *ad = NULL;

    FILE* LogFile=fopen(JobHistoryFileName,"r");
    if (!LogFile) {
        fprintf(stderr,"History file (%s) not found or empty.\n", JobHistoryFileName);
        exit(1);
    }
    
    while(!EndFlag) {
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
                ad->fPrint(stdout); printf("\n"); 
            } else {
                if (customFormat) {
					mask.display(stdout, ad);
                } else {
				    displayJobShort(ad);
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
