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
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "condor_tt/condor_ttdb.h"
#include "condor_tt/jobqueuecollection.h"
#include "dbms_utils.h"
#include "subsystem_info.h"

#define NUM_PARAMETERS 3

void Usage(char* name, int iExitCode=1);

void Usage(char* name, int iExitCode) 
{
  printf("Usage: %s -f history-filename [-name schedd-name jobqueue-birthdate]\n", name);
  exit(iExitCode);
}

static void doDBconfig();
static void readHistoryFromFile(char *JobHistoryFileName);
static MyString getWritePassword(const char *write_passwd_fname, const char *host, const char *port, 
							   const char *db, const char *dbuser);

//------------------------------------------------------------------------

/*
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
*/
static JobQueueDatabase* DBObj=NULL;
static dbtype dt;
static char *ScheddName=NULL;
static int ScheddBirthdate = 0;

int
main(int argc, char* argv[])
{

  void **parameters;

  char* JobHistoryFileName=NULL;

  int i;
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  myDistro->Init( argc, argv );

  config();
  dprintf_set_tool_debug("TOOL", 0);

  for(i=1; i<argc; i++) {

    if (strcmp(argv[i],"-f")==0) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
    }
    else if (strcmp(argv[i],"-help")==0) {
		Usage(argv[0],0);
    }
    else if (strcmp(argv[i],"-name")==0) {
		if (i+1==argc || ScheddName) break;
		i++;
		ScheddName=strdup(argv[i]);
        if ((i+1==argc) || ScheddBirthdate) break;
        i++;
        ScheddBirthdate = atoi(argv[i]);
    }
/*    else if (strcmp(argv[i],"-debug")==0) {
          // dprintf to console
          Termlog = 1;
    }
*/
    else {
		Usage(argv[0]);
    }
  }
  if (i<argc) Usage(argv[0]);

  if (JobHistoryFileName == NULL) 
	  Usage(argv[0]);

  if ((ScheddName == NULL) || (ScheddBirthdate == 0)) {

    if (ScheddName) 
        fprintf(stdout, "You specified Schedd name without a Job Queue"
                        "Birthdate. Ignoring value %s\n", ScheddName);
        
    Daemon schedd( DT_SCHEDD, 0, 0 );

    if ( schedd.locate() ) {
        char *scheddname; 
        if( (scheddname = schedd.name()) ) {
            ScheddName = strdup(scheddname);
        } else {
            fprintf(stderr, "You did not specify a Schedd name and Job Queue "
                           "Birthdate on the command line "
                           "and there was an error getting the Schedd "
                           "name from the local Schedd Daemon Ad. Please "
                           "check that the SCHEDD_DAEMON_AD_FILE config "
                           "parameter is set and the file contains a valid "
                           "Schedd Daemon ad.\n");
            exit(1);
        }
        ClassAd *daemonAd = schedd.daemonAd();
        if(daemonAd) {
            if (!(daemonAd->LookupInteger( ATTR_JOB_QUEUE_BIRTHDATE, 
                        ScheddBirthdate) )) {
                // Can't find the job queue birthdate
                fprintf(stderr, "You did not specify a Schedd name and "
                           "Job Queue Birthdate on the command line "
                           "and there was an error getting the Job Queue "
                           "Birthdate from the local Schedd Daemon Ad. Please "
                           "check that the SCHEDD_DAEMON_AD_FILE config "
                           "parameter is set and the file contains a valid "
                           "Schedd Daemon ad.\n");
                exit(1);
            }
        }
    } else {

		fprintf(stderr, "You did not specify a Schedd name and Job Queue "
				        "Birthdate on the command line and there was "
				        "an error getting the Schedd Daemon Ad. Please "
				        "check that Condor is running and the SCHEDD_DAEMON_AD_FILE "
				        "config parameter is set and the file contains a valid "
				        "Schedd Daemon ad.\n");
		exit(1);
	}
  }

  doDBconfig();
  readHistoryFromFile(JobHistoryFileName);

  if(parameters) free(parameters);
  if(ScheddName) free(ScheddName);
  return 0;
}


//------------------------------------------------------------------------

static void doDBconfig() {

	char *tmp, *host = NULL, *port = NULL, *DBIpAddress=NULL, *DBName=NULL, *DBUser=NULL;
	int len;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILL_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the .pgpass file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}
  
	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
		free(tmp);
	} else {
		dt = T_PGSQL; // assume PGSQL by default
	}

		/*
		  Here we try to read the <ipaddress:port> stored in condor_config
		  if one is not specified, by default we use the local address 
		  and the default postgres port of 5432.  
		*/
	DBIpAddress = param("QUILL_DB_IP_ADDR");
	if(DBIpAddress) {
		len = strlen(DBIpAddress);
		host = (char *) malloc(len * sizeof(char));
		port = (char *) malloc(len * sizeof(char));

			//split the <ipaddress:port> into its two parts accordingly
		char *ptr_colon = strchr(DBIpAddress, ':');
		strncpy(host, DBIpAddress, 
				ptr_colon - DBIpAddress);
			// terminate the string properly
		host[ptr_colon - DBIpAddress] = '\0';
		strncpy(port, ptr_colon+1, len);
			// terminate the string properyly
		port[strlen(ptr_colon+1)] = '\0';
	}

		/* Here we read the database name and if one is not specified
		   use the default name - quill
		   If there are more than one quill daemons are writing to the
		   same databases, its absolutely necessary that the database
		   names be unique or else there would be clashes.  Having 
		   unique database names is the responsibility of the administrator
		*/
	DBName = param("QUILL_DB_NAME");

	DBUser = param("QUILL_DB_USER");

		// get the password from the .pgpass file
	MyString writePasswordFile; 
	writePasswordFile.sprintf("%s/.pgpass", spool);

	MyString writePassword = getWritePassword(writePasswordFile.Value(), 
										   host?host:"", port?port:"", 
										   DBName?DBName:"", 
										   DBUser);
	MyString DBConn;

	DBConn.sprintf("host=%s port=%s user=%s password=%s dbname=%s", 
				   host?host:"", port?port:"", 
				   DBUser?DBUser:"", 
				   writePassword.Value(), 
				   DBName?DBName:"");
  	
	fprintf(stdout, "Using Database Type = Postgres\n");
	fprintf(stdout, "Using Database IpAddress = %s\n", 
			DBIpAddress?DBIpAddress:"");
	fprintf(stdout, "Using Database Name = %s\n", 
			DBName?DBName:"");
	fprintf(stdout, "Using Database User = %s\n", 
			DBUser?DBUser:"");

	if(spool) {
		free(spool);
		spool = NULL;
	}

	if(host) {
		free(host);
		host = NULL;
	}

	if(port) {
		free(port);
		port = NULL;
	}

	switch (dt) {				
		case T_PGSQL:
			DBObj = new PGSQLDatabase(DBConn.Value());
			break;
		default:
			break;;
	}

	QuillErrCode ret_st;

	ret_st = DBObj->connectDB();
	if (ret_st == QUILL_FAILURE) {
		fprintf(stderr, "doDBconfig: unable to connect to DB--- ERROR");
		exit(1);
	}

/*		
	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddname = build_valid_daemon_name( tmp );
		dprintf(D_FULLDEBUG, "scheddname %s built from param value %s\n", 
				scheddname, tmp);
		free(tmp);

	} else {
		scheddname = default_daemon_name();
		dprintf(D_FULLDEBUG, "scheddname built from default daemon name: %s\n", scheddname);
	}
*/	

	free(DBIpAddress);
	free(DBName);
	free(DBUser);
}

//! Gets the writer password required by the quill++
//  daemon to access the database
static MyString getWritePassword(const char *write_passwd_fname, 
							   const char *host, const char *port, 
							   const char *db,
							   const char *dbuser) {
	FILE *fp = NULL;
	MyString passwd;
	int len;
	MyString prefix;
	MyString msbuf;
	const char *buf;
	bool found = FALSE;

		// prefix is for the prefix of the entry in the .pgpass
		// it is in the format of the following:
		// host:port:db:user:password

	prefix.sprintf("%s:%s:%s:%s:", host, port, db, dbuser);

	len = prefix.Length();

	fp = safe_fopen_wrapper(write_passwd_fname, "r");

	if(fp == NULL) {
		EXCEPT("Unable to open password file %s\n", write_passwd_fname);
	}
	
		//dprintf(D_ALWAYS, "prefix: %s\n", prefix);

	while(msbuf.readLine(fp)) {
		msbuf.chomp();
		buf = msbuf.Value();

			//fprintf(stderr, "line: %s\n", buf);

			// check if the entry matches the prefix
		if (strncmp(buf, prefix.Value(), len) == 0) {
				// extract the password
			passwd = msbuf.Substr(len, msbuf.Length());
			found = TRUE;

			break;
		}

	}

    fclose(fp);
	if (!found) {
		EXCEPT("Unable to find password from file %s\n", write_passwd_fname);
	}

	return passwd;
}

QuillErrCode insertHistoryJob(AttrList *ad) {

	MyString errorSqlStmt;
	DBObj->beginTransaction();
	QuillErrCode ec = insertHistoryJobCommon(ad, DBObj, dt, errorSqlStmt, ScheddName, (time_t) ScheddBirthdate);
	DBObj->commitTransaction();
	return ec;
}

// Read the history from a single file and print it out. 
static void readHistoryFromFile(char *JobHistoryFileName)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    AttrList *ad = NULL;

    MyString buf;
    
    int fd = safe_open_wrapper_follow(JobHistoryFileName, O_RDONLY | O_LARGEFILE);
	if (fd < 0) {
        fprintf(stderr,"History file (%s) not found or empty.\n", JobHistoryFileName);
        exit(1);
    }
    
	FILE *LogFile = fdopen(fd, "r");
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

		insertHistoryJob(ad);

        if(ad) {
            delete ad;
            ad = NULL;
        }
    }
    fclose(LogFile);
    return;
}
#endif /* HAVE_EXT_POSTGRESQL */

#if 0
  int        cid, pid;
  MyString   sql_stmt, sql_stmt2;
  ExprTree *expr;
  ExprTree *L_expr;
  ExprTree *R_expr;
	  //char *value = NULL;
  MyString name, value;
	  //char *newname = NULL; /* *name = NULL,*/
  MyString newvalue;

  bool flag1=false, flag2=false,flag3=false, flag4=false;
  const char *scheddname = ScheddName;

  ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cid);
  ad->EvalInteger (ATTR_PROC_ID, NULL, pid);

  sql_stmt.sprintf("DELETE FROM History_Horizontal WHERE scheddname = '%s' AND cluster_id = %d AND proc = %d", scheddname, cid, pid);
  sql_stmt2.sprintf("INSERT INTO History_Horizontal(scheddname, cluster_id, proc, enteredhistorytable) VALUES('%s', %d, %d, current_timestamp)", scheddname, cid, pid);

  if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
	  fprintf(stderr, "Executing Statement --- Error\n");
	  fprintf(stderr, "sql = %s\n", sql_stmt.Value());
	  return FAILURE;	  
  }

  if (DBObj->execCommand(sql_stmt2.Value()) == QUILL_FAILURE) {
	  fprintf(stderr, "Executing Statement --- Error\n");
	  fprintf(stderr, "sql = %s\n", sql_stmt2.Value());
	  return QUILL_FAILURE;	  
  }
  

  ad->ResetExpr(); // for iteration initialization
  while((expr=ad->NextExpr()) != NULL) {
	  L_expr = expr->LArg();
	  L_expr->PrintToStr(name);

	  if (name == NULL) break;
	  
	  R_expr = expr->RArg();
	  R_expr->PrintToStr(value);
	  
	  if (value == NULL) {
			  //free(name);
		  break;	  	  
	  }
	  name.lower_case();

		  /* the following are to avoid overwriting the attr values. The hack 
			 is based on the fact that an attribute of a job ad comes before 
			 the attribute of a cluster ad. And this is because 
		     attribute list of cluster ad is chained to a job ad.
		   */
	  if(name == "jobstatus") {
		  if(flag4) continue;
		  flag4 = true;
	  }

	  if(name == "remotewallclocktime") {
		  if(flag1) continue;
		  flag1 = true;
	  }
	  else if(name == "completiondate") {
		  if(flag2) continue;
		  flag2 = true;
	  }
	  else if(name == "committedtime") {
		  if(flag3) continue;
		  flag3 = true;
	  }

	  if(isHorizontalHistoryAttribute(name.Value())) {
		  if(name == "in" || name == "user") {
			  name += "_j";
		  }
  
		  sql_stmt = ""; 
		  sql_stmt2 = "";

		  if((name == "lastmatchtime") || 
			 (name == "jobstartdate")  || 
			 (name == "jobcurrentstartdate") ||
			 (name == "enteredcurrentstatus") 
			 ) {
				  // avoid updating with epoch time
			  if (value == "0") {
				  continue;
			  } 
			
			  time_t clock;
			  MyString ts_expr;
			  clock = atoi(value.Value());
			  
			  ts_expr = condor_ttdb_buildts(&clock, dt);	
				  
			  sql_stmt.sprintf("UPDATE History_Horizontal SET %s = (%s) WHERE scheddname = '%s' and cluster_id = %d and proc = %d", name.Value(), ts_expr.Value(), scheddname, cid, pid);

		  }	else {
			  newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
			  sql_stmt.sprintf("UPDATE History_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster_id = %d and proc = %d", name.Value(), newvalue.Value(), 
							   scheddname, cid, pid);			  
		  }
	  } else {
		  newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
		  
		  sql_stmt = ""; 
		  sql_stmt2 = ""; 

		  sql_stmt.sprintf("DELETE FROM History_Vertical WHERE scheddname = '%s' AND cluster_id = %d AND proc = %d AND attr = '%s'", scheddname, cid, pid, name.Value());
			  
		  sql_stmt2.sprintf("INSERT INTO History_Vertical(scheddname, cluster_id, proc, attr, val) VALUES('%s', %d, %d, '%s', '%s')", scheddname, cid, pid, 
							name.Value(), newvalue.Value());

	  }	  

	  if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		  fprintf(stderr, "Executing Statement --- Error\n");
		  fprintf(stderr, "sql = %s\n", sql_stmt.Value());
		  
		  return QUILL_FAILURE;
	  }
		  
	  if (sql_stmt2 != "" && (DBObj->execCommand(sql_stmt2.Value()) == QUILL_FAILURE)) {
		  fprintf(stderr, "Executing Statement --- Error\n");
		  fprintf(stderr, "sql = %s\n", sql_stmt2.Value());
		  
		  return QUILL_FAILURE;			  
	  }
	  
  }  

  return QUILL_SUCCESS;

#endif
