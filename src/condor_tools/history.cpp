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
#include "condor_xml_classads.h"
#include "subsystem_info.h"
#include "historyFileFinder.h"

#include "history_utils.h"

#ifdef HAVE_EXT_POSTGRESQL
#include "sqlquery.h"
#include "historysnapshot.h"
#endif /* HAVE_EXT_POSTGRESQL */

#define NUM_PARAMETERS 3


DECL_SUBSYSTEM( "TOOL", SUBSYSTEM_TYPE_TOOL );

static void Usage(char* name) 
{
	printf ("Usage: %s [options]\n\twhere [options] are\n"
		"\t\t-help\t\t\tThis screen\n"
		"\t\t-f <file>\t\tRead history data from specified file\n"
		"\t\t-backwards\t\tList jobs in reverse chronological order\n"
		"\t\t-match <number>\t\tLimit the number of jobs displayed\n"
		"\t\t-format <fmt> <attr>\tPrint attribute attr using format fmt\n"		
		"\t\t-l\t\t\tVerbose output (entire classads)\n"
		"\t\t-constraint <expr>\tAdd constraint on classads\n"
#ifdef HAVE_EXT_POSTGRESQL
		"\t\t-name <schedd-name>\tRead history data from Quill database\n"
		"\t\t-completedsince <time>\tDisplay jobs completed on/after time\n"
#endif
		"\t\trestriction list\n"
		"\twhere each restriction may be one of\n"
		"\t\t<cluster>\t\tGet information about specific cluster\n"
		"\t\t<cluster>.<proc>\tGet information about specific job\n"
		"\t\t<owner>\t\t\tInformation about jobs owned by <owner>\n",
			name);
  exit(1);
}

#ifdef HAVE_EXT_POSTGRESQL
static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName, char *&queryPassword);
static bool checkDBconfig();
#endif /* HAVE_EXT_POSTGRESQL */

static void readHistoryFromFiles(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFile(char *JobHistoryFileName, char* constraint, ExprTree *constraintExpr);

//------------------------------------------------------------------------

static CollectorList * Collectors = NULL;
static	QueryResult result;
static	CondorQuery	quillQuery(QUILL_AD);
static	ClassAdList	quillList;
static  bool longformat=false;
static  bool use_xml=false;
static  bool customFormat=false;
static  bool backwards=false;
static  AttrListPrintMask mask;
static int cluster=-1, proc=-1;
static int specifiedMatch = 0, matchCount = 0;

int
main(int argc, char* argv[])
{
  Collectors = NULL;

#ifdef HAVE_EXT_POSTGRESQL
  HistorySnapshot *historySnapshot;
  SQLQuery queryhor;
  SQLQuery queryver;
  QuillErrCode st;
#endif /* HAVE_EXT_POSTGRESQL */

  void **parameters;
  char *dbconn=NULL;
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

  config();

#ifdef HAVE_EXT_POSTGRESQL
  queryhor.setQuery(HISTORY_ALL_HOR, NULL);
  queryver.setQuery(HISTORY_ALL_VER, NULL);
#endif /* HAVE_EXT_POSTGRESQL */

  for(i=1; i<argc; i++) {
    if (strcmp(argv[i],"-l")==0) {
      longformat=TRUE;   
    }

    else if (strcmp(argv[i],"-long")==0) {
		longformat = true;
	}
    
    else if (match_prefix(argv[i],"-xml")) {
		use_xml = true;	
		longformat = true;
	}
    
    else if (match_prefix(argv[i],"-backwards")) {
        backwards=TRUE;
    }

    else if (match_prefix(argv[i],"-match")) {
        i++;
        if (argc <= i) {
            fprintf(stderr,
                    "Error: Argument -match requires a number value "
                    " as a parameter.\n");
            exit(1);
        }
        specifiedMatch = atoi(argv[i]);
    }

#ifdef HAVE_EXT_POSTGRESQL
    else if(match_prefix(argv[i], "-name")) {
		i++;
		if (argc <= i) {
			fprintf( stderr,
					 "Error: Argument -name requires the name of a quilld as a parameter\n" );
			exit(1);
		}
		

/*
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

*/
		quillName = argv[i];

		sprintf (tmp, "%s == \"%s\"", ATTR_SCHEDD_NAME, quillName);
		quillQuery.addORConstraint (tmp);

		remotequill = false;
		readfromfile = false;
    }
#endif /* HAVE_EXT_POSTGRESQL */
    else if (strcmp(argv[i],"-f")==0) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
    }
    else if (match_prefix(argv[i],"-help")) {
		Usage(argv[0]);
    }
    else if (match_prefix(argv[i],"-format")) {
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
    else if (match_prefix(argv[i],"-constraint")) {
		if (i+1==argc || constraint) break;
		sprintf(tmp,"(%s)",argv[i+1]);
		constraint=tmp;
		i++;
		//readfromfile = true;
    }
#ifdef HAVE_EXT_POSTGRESQL
    else if (match_prefix(argv[i],"-completedsince")) {
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
#endif /* HAVE_EXT_POSTGRESQL */

    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
		if (constraint) break;
		sprintf (tmp, "((%s == %d) && (%s == %d))", 
				 ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
		constraint=tmp;
		parameters[0] = &cluster;
		parameters[1] = &proc;
#ifdef HAVE_EXT_POSTGRESQL
		queryhor.setQuery(HISTORY_CLUSTER_PROC_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_PROC_VER, parameters);
#endif /* HAVE_EXT_POSTGRESQL */
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
		if (constraint) break;
		sprintf (tmp, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
		constraint=tmp;
		parameters[0] = &cluster;
#ifdef HAVE_EXT_POSTGRESQL
		queryhor.setQuery(HISTORY_CLUSTER_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_VER, parameters);
#endif /* HAVE_EXT_POSTGRESQL */
    }
    else if (strcmp(argv[i],"-debug")==0) {
          // dprintf to console
          Termlog = 1;
          dprintf_config ("TOOL");
    }
    else {
		if (constraint) break;
		owner = (char *) malloc(512 * sizeof(char));
		sscanf(argv[i], "%s", owner);	
		sprintf(tmp, "(%s == \"%s\")", ATTR_OWNER, owner);
		constraint=tmp;
		parameters[0] = owner;
#ifdef HAVE_EXT_POSTGRESQL
		queryhor.setQuery(HISTORY_OWNER_HOR, parameters);
		queryver.setQuery(HISTORY_OWNER_VER, parameters);
#endif /* HAVE_EXT_POSTGRESQL */
    }
  }
  if (i<argc) Usage(argv[0]);
  
  
  if( constraint && ParseClassAdRvalExpr( constraint, constraintExpr ) ) {
	  fprintf( stderr, "Error:  could not parse constraint %s\n", constraint );
	  exit( 1 );
  }

#ifdef HAVE_EXT_POSTGRESQL
	/* This call must happen AFTER config() is called */
  if (checkDBconfig() == true && !readfromfile) {
  	readfromfile = false;
  } else {
  	readfromfile = true;
  }
#else 
  readfromfile = true;
#endif /* HAVE_EXT_POSTGRESQL */

  if(readfromfile == false) {
#ifdef HAVE_EXT_POSTGRESQL
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
	  } else {
			// they just typed 'condor_history' on the command line and want
			// to use quill, so get the schedd ad for the local machine if
			// we can, figure out the name of the schedd and the 
			// jobqueuebirthdate
		Daemon schedd( DT_SCHEDD, 0, 0 );

        if ( schedd.locate() ) {
			char *scheddname = quillName;	
			if (scheddname == NULL) {
				// none set explictly, look it up in the daemon ad
				scheddname = schedd.name();
				ClassAd *daemonAd = schedd.daemonAd();
				int scheddbirthdate;
				if(daemonAd) {
					if(daemonAd->LookupInteger( ATTR_JOB_QUEUE_BIRTHDATE, 	
								scheddbirthdate) ) {
						queryhor.setJobqueuebirthdate( (time_t)scheddbirthdate);	
						queryver.setJobqueuebirthdate( (time_t)scheddbirthdate);	
					}
				}
			} else {
				queryhor.setJobqueuebirthdate(0);	
				queryver.setJobqueuebirthdate(0);	
			}
			queryhor.setScheddname(scheddname);	
			queryver.setScheddname(scheddname);	
			
		}
	  }
	  dbconn = getDBConnStr(quillName,dbIpAddr,dbName,queryPassword);
	  historySnapshot = new HistorySnapshot(dbconn);
	  if (!customFormat) {
		  printf ("\n\n-- Quill: %s : %s : %s\n", quillName, 
			  dbIpAddr, dbName);
		}		

	  queryhor.prepareQuery();  // create the query strings before sending off to historySnapshot
	  queryver.prepareQuery();
	  
	  st = historySnapshot->sendQuery(&queryhor, &queryver, longformat,
	  	false, customFormat, &mask, constraint);

		  //if there's a failure here and if we're not posing a query on a 
		  //remote quill daemon, we should instead query the local file
	  if(st == QUILL_FAILURE) {
	        printf( "-- Database at %s not reachable\n", dbIpAddr);
		if(!remotequill) {
			char *tmp_hist = param("HISTORY");
			if (!customFormat) {
				printf( "--Failing over to the history file at %s instead --\n",
						tmp_hist ? tmp_hist : "(null)" );
			}
			if(!tmp_hist) {
				free(tmp_hist);
			}
			readfromfile = true;
	  	}
	  }
		  // query history table
	  if (historySnapshot->isHistoryEmpty()) {
		  printf("No historical jobs in the database match your query\n");
	  }
	  historySnapshot->release();
	  delete(historySnapshot);
#endif /* HAVE_EXT_POSTGRESQL */
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
  if(dbconn) free(dbconn);
  return 0;
}


//------------------------------------------------------------------------

#ifdef HAVE_EXT_POSTGRESQL

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
  strcpy(host, "host=");
  strncat(host,
          databaseIp+1,
          ptr_colon - databaseIp-1);
  strcpy(port, "port=");
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

#endif /* HAVE_EXT_POSTGRESQL */

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

        historyFiles = findHistoryFiles("HISTORY", &numHistoryFiles);
		if (!historyFiles) {
			fprintf( stderr, "Error: No history file is defined\n");
			fprintf(stderr, "\n");
			print_wrapped_text("Extra Info: " 
						   "The variable HISTORY is not defined in "
						   "your config file. If you want Condor to "
						   "keep a history of past jobs, you must "
						   "define HISTORY in your config file", stderr );
			exit(1);
		}
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
            free(historyFiles);
        }
    }
    return;
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
      
			prevOffset = -1;
			clusterId = -1;
			procId = -1;

            sscanf(buf.Value(), "%*s %*s %*s %ld %*s %*s %d %*s %*s %d %*s %*s %s %*s %*s %ld", 
                   &prevOffset, &clusterId, &procId, owner, &completionDate);

			if (prevOffset == -1 && clusterId == -1 && procId == -1) {
				fprintf(stderr, 
						"Error: (%s) is an incompatible history file, please run condor_convert_history.\n",
						filename);
				free(owner);
				exit(1);
			}
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
    ClassAd *ad = NULL;

    long offset = 0;
    bool BOF = false; // Beginning Of File
    MyString buf;

	int flags = 0;
	if( !backwards ) {
			// Currently, the file position manipulations used in -backwards
			// do not work with files > 2GB on platforms with 32-bit file
			// offsets.
		flags = O_LARGEFILE;
	}
	int LogFd = safe_open_wrapper(JobHistoryFileName,flags,0);
	if (LogFd < 0) {
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(errno));
#ifdef EFBIG
		if( (errno == EFBIG) && backwards ) {
			fprintf(stderr,"The -backwards option does not support files this large.\n");
		}
#endif
		exit(1);
	}

	FILE *LogFile = fdopen(LogFd,"r");
	if (!LogFile) {
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(errno));
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


	if(longformat && use_xml) {
		ClassAdXMLUnparser unparser;
		MyString out;
		unparser.AddXMLFileHeader(out);
		printf("%s\n", out.Value());
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
        if (!constraint || EvalBool(ad, constraintExpr)) {
            if (longformat) { 
				if( use_xml ) {
					ad->fPrintAsXML(stdout);
				}
				else {
					ad->fPrint(stdout);
				}
				printf("\n"); 
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
	if(longformat && use_xml) {
		ClassAdXMLUnparser unparser;
		MyString out;
		unparser.AddXMLFileFooter(out);
		printf("%s\n", out.Value());
	}
    fclose(LogFile);
    return;
}
