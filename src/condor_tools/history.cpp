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
#include "dc_schedd.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "print_wrapped_text.h"
#include "MyString.h"
#include "metric_units.h"
#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname
#include "match_prefix.h"
#include "subsystem_info.h"
#include "historyFileFinder.h"
#include "condor_id.h"
#include "userlog_to_classads.h"

#include "history_utils.h"
#include "backward_file_reader.h"
#include <fcntl.h>  // for O_BINARY

#ifdef HAVE_EXT_POSTGRESQL
#include "sqlquery.h"
#include "historysnapshot.h"
#define NUM_PARAMETERS 3
#endif /* HAVE_EXT_POSTGRESQL */

static int getConsoleWindowSize(int * pHeight = NULL);
void Usage(char* name, int iExitCode=1);

void Usage(char* name, int iExitCode) 
{
	printf ("Usage: %s [source] [restriction-list] [options]\n"
		"\n   where [source] is one of\n"
		"\t-file <file>\t\tRead history data from specified file\n"
		"\t-userlog <file>\t\tRead job data specified userlog file\n"
		"\t-name <schedd-name>\tRemote schedd to read from\n"
		"\t-pool <collector-name>\tPool remote schedd lives in.\n"
		"   If neither -pool, -name, -userlog or -file is specified, then the local history file is used.\n"
#ifdef HAVE_EXT_POSTGRESQL
		"\t-dbname <schedd-name>\tRead history data from Quill database\n"
		"\t-completedsince <time>\tDisplay jobs completed on/after time\n"
#endif
		"\n   and [restriction-list] is one or more of\n"
		"\t<cluster>\t\tGet information about specific cluster\n"
		"\t<cluster>.<proc>\tGet information about specific job\n"
		"\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t-constraint <expr>\tInformation about jobs matching <expr>\n"
		"\n   and [options] are one or more of\n"
		"\t-help\t\t\tDisplay this screen\n"
		"\t-backwards\t\tList jobs in reverse chronological order\n"
		"\t-forwards\t\tList jobs in chronological order\n"
		"\t-limit <number>\t\tLimit the number of jobs displayed\n"
		"\t-match <number>\t\tOld name for -limit\n"
		"\t-long\t\t\tDisplay entire classads\n"
		"\t-wide[:<width>]\tcon\tDon't truncate fields to fit into 80 columns\n"
		"\t-format <fmt> <attr>\tDisplay attr using printf formatting\n"
		"\t-autoformat[:lhVr,tng] <attr> [<attr2 ...]   Display attr(s) with automatic formatting\n"
		"\t-af[:lhVr,tng] <attr> [<attr2 ...]\t    Same as -autoformat above\n"
		"\t    where the [lhVr,tng] options influence the automatic formatting:\n"
		"\t    l\tattribute labels\n"
		"\t    h\tattribute column headings\n"
		"\t    V\t%%V formatting (string values are quoted)\n"
		"\t    r\t%%r formatting (raw/unparsed values)\n"
		"\t    t\ttab before each value (default is space)\n"
		"\t    g\tnewline between ClassAds, no space before values\n"
		"\t    ,\tcomma after each value\n"
		"\t    n\tnewline after each value\n"
		"\t    use -af:h to get tabular values with headings\n"
		"\t-print-format <file>\tUse <file> to specify the attributes and formatting\n"
		"\t\t\t\t(experimental, see htcondor-wiki for more information)\n"
		, name);
  exit(iExitCode);
}

#ifdef HAVE_EXT_POSTGRESQL
static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName, char *&queryPassword);
static bool checkDBconfig();
static	QueryResult result;
#endif /* HAVE_EXT_POSTGRESQL */

static void readHistoryRemote(classad::ExprTree *constraintExpr);
static void readHistoryFromFiles(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileEx(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr, bool read_backwards);
static void printJobAds(ClassAdList & jobs);
static void printJob(ClassAd & ad);

static int set_print_mask_from_stream(AttrListPrintMask & print_mask, std::string & constraint, const char * streamid, bool is_filename);
static int getDisplayWidth();


//------------------------------------------------------------------------

static CollectorList * Collectors = NULL;
#ifdef HAVE_EXT_POSTGRESQL
static	CondorQuery	quillQuery(QUILL_AD);
static	ClassAdList	quillList;
#endif
static  bool longformat=false;
static  bool diagnostic = false;
static  bool use_xml=false;
static  bool wide_format=false;
static  int  wide_format_width = 0;
static  bool customFormat=false;
static  bool disable_user_print_files=false;
static  bool backwards=true;
static  AttrListPrintMask mask;
//tj: headings moved into the mask object.
//static  List<const char> headings; // The list of headings for the mask entries
static int cluster=-1, proc=-1;
static int specifiedMatch = 0, matchCount = 0;
static std::string g_name, g_pool;

int
main(int argc, char* argv[])
{
  Collectors = NULL;

#ifdef HAVE_EXT_POSTGRESQL
  HistorySnapshot *historySnapshot;
  SQLQuery queryhor;
  SQLQuery queryver;
  QuillErrCode st;
  bool remotequill=false;
  char *quillName=NULL;
  AttrList *ad=0;
  int flag = 1;
  void **parameters;
  char *dbconn=NULL;
  char *completedsince = NULL;
  char *dbIpAddr=NULL, *dbName=NULL,*queryPassword=NULL;
  bool remoteread = false;
#endif /* HAVE_EXT_POSTGRESQL */

  const char *owner=NULL;
  bool readfromfile = true;
  bool fileisuserlog = false;

  char* JobHistoryFileName=NULL;
  const char * pcolon=NULL;


  GenericQuery constraint; // used to build a complex constraint.
  ExprTree *constraintExpr=NULL;

  std::string tmp;

  int i;
  myDistro->Init( argc, argv );

  config();

#ifdef HAVE_EXT_POSTGRESQL
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  queryhor.setQuery(HISTORY_ALL_HOR, NULL);
  queryver.setQuery(HISTORY_ALL_VER, NULL);
#endif /* HAVE_EXT_POSTGRESQL */

  for(i=1; i<argc; i++) {
    if (is_dash_arg_prefix(argv[i],"long",1)) {
      longformat=TRUE;   
    }
    
    else if (is_dash_arg_prefix(argv[i],"xml",3)) {
		use_xml = true;	
		longformat = true;
	}
    
    else if (is_dash_arg_prefix(argv[i],"backwards",1)) {
        backwards=TRUE;
    }

	// must be at least -forw to avoid conflict with -f (for file) and -format
    else if (is_dash_arg_prefix(argv[i],"nobackwards",3) ||
			 is_dash_arg_prefix(argv[i],"forwards",4)) {
        backwards=FALSE;
    }

    else if (is_dash_arg_colon_prefix(argv[i],"wide", &pcolon, 1)) {
        wide_format=TRUE;
        if (pcolon) {
            wide_format_width = atoi(++pcolon);
            if ( ! mask.IsEmpty()) mask.SetOverallWidth(getDisplayWidth()-1);
            if (wide_format_width <= 80) wide_format = FALSE;
        }
    }

    else if (is_dash_arg_prefix(argv[i],"match",1) || is_dash_arg_prefix(argv[i],"limit",3)) {
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
    else if(is_dash_arg_prefix(argv[i], "dbname",1)) {
		i++;
		if (argc <= i) {
			fprintf( stderr,
					 "Error: Argument -dbname requires the name of a quilld as a parameter\n" );
			exit(1);
		}
		

/*
		if( !(quillName = get_daemon_name(argv[i])) ) {
			fprintf( stderr, "Error: unknown host %s\n",
					 get_host_part(argv[i]) );
			printf("\n");
			print_wrapped_text("Extra Info: The name given with the -dbname "
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
		quillQuery.addORConstraint (tmp.c_str());

		remotequill = false;
		readfromfile = false;
    }
#endif /* HAVE_EXT_POSTGRESQL */
    else if (is_dash_arg_prefix(argv[i],"file",2)) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
    }
	else if (is_dash_arg_prefix(argv[i],"userlog",1)) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
		fileisuserlog = true;
	}
    else if (is_dash_arg_prefix(argv[i],"help",1)) {
		Usage(argv[0],0);
    }
    else if (is_dash_arg_prefix(argv[i],"format",1)) {
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
	else if (*(argv[i]) == '-' && 
				(is_arg_colon_prefix(argv[i]+1,"af", &pcolon, 2) ||
				 is_arg_colon_prefix(argv[i]+1,"autoformat", &pcolon, 5))) {
		// make sure we have at least one argument to autoformat
		if (argc <= i+1 || *(argv[i+1]) == '-') {
			fprintf (stderr, "Error: Argument %s requires at last one attribute parameter\n", argv[i]);
			fprintf(stderr, "\t\te.g. condor_history %s ClusterId\n", argv[i]);
			exit(1);
		}
		if (pcolon) ++pcolon; // if there are options, skip over the colon to the options.
		int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, mask, diagnostic);
		if (ixNext > i)
			i = ixNext-1;
		customFormat = true;
	}
	else if (is_dash_arg_colon_prefix(argv[i], "print-format", &pcolon, 2)) {
		if ( (argc <= i+1)  || (*(argv[i+1]) == '-' && (argv[i+1])[1] != 0)) {
			fprintf( stderr, "Error: Argument -print-format requires a filename argument\n");
			exit( 1 );
		}
		// hack allow -pr ! to disable use of user-default print format files.
		if (MATCH == strcmp(argv[i+1], "!")) {
			++i;
			disable_user_print_files = true;
			continue;
		}
		if ( ! wide_format) mask.SetOverallWidth(getDisplayWidth()-1);
		customFormat = true;
		++i;
		std::string where_expr;
		if (set_print_mask_from_stream(mask, where_expr, argv[i], true) < 0) {
			fprintf(stderr, "Error: cannot execute print-format file %s\n", argv[i]);
			exit (1);
		}
		if ( ! where_expr.empty()) {
			constraint.addCustomAND(where_expr.c_str());
		}
	}
    else if (is_dash_arg_prefix(argv[i],"constraint",1)) {
		// make sure we have at least one more argument
		if (argc <= i+1) {
			fprintf( stderr, "Error: Argument %s requires another parameter\n", argv[i]);
			exit(1);
		}
		i++;
		constraint.addCustomAND(argv[i]);
    }
#ifdef HAVE_EXT_POSTGRESQL
    else if (is_dash_arg_prefix(argv[i],"completedsince",3)) {
		i++;
		if (argc <= i) {
			fprintf(stderr,
					"Error: Argument -completedsince requires a date and "
					"optional timestamp as a parameter.\n");
			fprintf(stderr,
					"\t\te.g. condor_history -completedsince \"2004-10-19 10:23:54\"\n");
			exit(1);
		}
		
		if (constraint!="") break;
		completedsince = strdup(argv[i]);
		parameters[0] = completedsince;
		queryhor.setQuery(HISTORY_COMPLETEDSINCE_HOR,parameters);
		queryver.setQuery(HISTORY_COMPLETEDSINCE_VER,parameters);
    }
#endif /* HAVE_EXT_POSTGRESQL */

    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
		std::string jobconst;
		formatstr (jobconst, "%s == %d && %s == %d", 
				 ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
		constraint.addCustomOR(jobconst.c_str());
		#ifdef HAVE_EXT_POSTGRESQL
		parameters[0] = &cluster;
		parameters[1] = &proc;
		queryhor.setQuery(HISTORY_CLUSTER_PROC_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_PROC_VER, parameters);
		#endif /* HAVE_EXT_POSTGRESQL */
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
		std::string jobconst;
		formatstr (jobconst, "%s == %d", ATTR_CLUSTER_ID, cluster);
		constraint.addCustomOR(jobconst.c_str());
		#ifdef HAVE_EXT_POSTGRESQL
		parameters[0] = &cluster;
		queryhor.setQuery(HISTORY_CLUSTER_HOR, parameters);
		queryver.setQuery(HISTORY_CLUSTER_VER, parameters);
		#endif /* HAVE_EXT_POSTGRESQL */
    }
    else if (is_dash_arg_prefix(argv[i],"debug",1)) {
          // dprintf to console
          dprintf_set_tool_debug("TOOL", 0);
    }
    else if (is_dash_arg_prefix(argv[i],"diagnostic",4)) {
          // dprintf to console
          diagnostic = true;
    }
    else if (is_dash_arg_prefix(argv[i], "name", 1)) {
        i++;
        if (argc <= i)
        {
            fprintf(stderr,
                "Error: Argument -name requires name of a remote schedd\n");
            fprintf(stderr,
                "\t\te.g. condor_history -name submit.example.com \n");
            exit(1);
        }
        g_name = argv[i];
        readfromfile = false;
       #ifdef HAVE_EXT_POSTGRESQL
        remoteread = true;
       #endif
    }
    else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
        i++;    
        if (argc <= i)
        {
            fprintf(stderr,
                "Error: Argument -name requires name of a remote schedd\n");
            fprintf(stderr,
                "\t\te.g. condor_history -name submit.example.com \n");
            exit(1);    
        }       
        g_pool = argv[i];
        readfromfile = false;
       #ifdef HAVE_EXT_POSTGRESQL
        remoteread = true;
       #endif
    }
    else {
		std::string ownerconst;
		owner = argv[i];
		formatstr(ownerconst, "%s == \"%s\"", ATTR_OWNER, owner);
		constraint.addCustomOR(ownerconst.c_str());
#ifdef HAVE_EXT_POSTGRESQL
		parameters[0] = owner;
		queryhor.setQuery(HISTORY_OWNER_HOR, parameters);
		queryver.setQuery(HISTORY_OWNER_VER, parameters);
#endif /* HAVE_EXT_POSTGRESQL */
    }
  }
  if (i<argc) Usage(argv[0]);
  
  
  MyString my_constraint;
  constraint.makeQuery(my_constraint);
  if (diagnostic) {
	  fprintf(stderr, "Using effective constraint: %s\n", my_constraint.c_str());
  }
  if ( ! my_constraint.empty() && ParseClassAdRvalExpr( my_constraint.c_str(), constraintExpr ) ) {
	  fprintf( stderr, "Error:  could not parse constraint %s\n", my_constraint.c_str() );
	  exit( 1 );
  }

#ifdef HAVE_EXT_POSTGRESQL
	/* This call must happen AFTER config() is called */
  if (checkDBconfig() == true && !readfromfile) {
  	readfromfile = false;
  } else {
  	readfromfile = true;
  }
#endif /* HAVE_EXT_POSTGRESQL */

#ifdef HAVE_EXT_POSTGRESQL
  if(!readfromfile && !remoteread) {
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
		false, customFormat, &mask, constraint.c_str());

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
  }
#endif /* HAVE_EXT_POSTGRESQL */
  
  if(readfromfile == true) {
      readHistoryFromFiles(fileisuserlog, JobHistoryFileName, my_constraint.c_str(), constraintExpr);
  }
  else {
      readHistoryRemote(constraintExpr);
  }
  
  
#ifdef HAVE_EXT_POSTGRESQL
  if(completedsince) free(completedsince);
  if(parameters) free(parameters);
  if(dbIpAddr) free(dbIpAddr);
  if(dbName) free(dbName);
  if(queryPassword) free(queryPassword);
  if(dbconn) free(dbconn);
#endif
  return 0;
}


#ifdef WIN32
static int getConsoleWindowSize(int * pHeight /*= NULL*/) {
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ws)) {
		if (pHeight)
			*pHeight = (int)(ws.srWindow.Bottom - ws.srWindow.Top)+1;
		return (int)ws.dwSize.X;
	}
	return 80;
}
#else
#include <sys/ioctl.h>
static int getConsoleWindowSize(int * pHeight /*= NULL*/) {
    struct winsize ws;
	if (0 == ioctl(0, TIOCGWINSZ, &ws)) {
		//printf ("lines %d\n", ws.ws_row);
		//printf ("columns %d\n", ws.ws_col);
		if (pHeight)
			*pHeight = (int)ws.ws_row;
		return (int) ws.ws_col;
	}
	return 80;
}
#endif

static int getDisplayWidth() {
	if (wide_format_width <= 0) {
		wide_format_width = getConsoleWindowSize();
		if (wide_format_width <= 0)
			wide_format_width = 80;
	}
	return wide_format_width;
}

static const char *
format_hist_runtime (int /*unused_utime*/, AttrList * ad, Formatter & /*fmt*/)
{
	double utime;
	if(!ad->EvalFloat(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,utime)) {
		if(!ad->EvalFloat(ATTR_JOB_REMOTE_USER_CPU,NULL,utime)) {
			utime = 0;
		}
	}
	return format_time((time_t)utime);
}

static const char *
format_utime_double (double utime, AttrList * /*ad*/, Formatter & /*fmt*/)
{
	return format_time((time_t)utime);
}

static const char *
format_int_date(int date, AttrList * /*ad*/, Formatter & /*fmt*/)
{
	return format_date(date);
}

static const char *
format_readable_mb(const classad::Value &val, AttrList *, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi * 1024.0 * 1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0 * 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static const char *
format_readable_kb(const classad::Value &val, AttrList *, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi*1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static const char *
format_int_job_status(int status, AttrList * /*ad*/, Formatter & /*fmt*/)
{
	const char * ret = " ";
	switch( status ) 
	{
	case IDLE:      ret = "I"; break;
	case RUNNING:   ret = "R"; break;
	case COMPLETED: ret = "C"; break;
	case REMOVED:   ret = "X"; break;
	case TRANSFERRING_OUTPUT: ret = ">"; break;
	}
	return ret;
}

static const char *
format_job_status_raw(int job_status, AttrList* /*ad*/, Formatter &)
{
	switch(job_status) {
	case IDLE:      return "Idle   ";
	case HELD:      return "Held   ";
	case RUNNING:   return "Running";
	case COMPLETED: return "Complet";
	case REMOVED:   return "Removed";
	case SUSPENDED: return "Suspend";
	case TRANSFERRING_OUTPUT: return "XFerOut";
	default:        return "Unk    ";
	}
}

static const char *
format_job_universe(int job_universe, AttrList* /*ad*/, Formatter &)
{
	return CondorUniverseNameUcFirst(job_universe);
}

static const char *
format_job_id(int clusterId, AttrList * ad, Formatter & /*fmt*/)
{
	static MyString ret;
	ret = "";
	int procId;
	if( ! ad->EvalInteger(ATTR_PROC_ID,NULL,procId)) procId = 0;
	ret.formatstr("%4d.%-3d", clusterId, procId);
	return ret.Value();
}

static const char *
format_job_cmd_and_args(const char * cmd, AttrList * ad, Formatter & /*fmt*/)
{
	static MyString ret;
	ret = cmd;

	char * args;
	if (ad->EvalString (ATTR_JOB_ARGUMENTS1, NULL, &args) || 
		ad->EvalString (ATTR_JOB_ARGUMENTS2, NULL, &args)) {
		ret += " ";
		ret += args;
		free(args);
	}
	return ret.Value();
}

static void AddPrintColumn(const char * heading, int width, int opts, const char * expr)
{
	mask.set_heading(heading);

	int wid = width ? width : strlen(heading);
	mask.registerFormat("%v", wid, opts, expr);
}

static void AddPrintColumn(const char * heading, int width, int opts, const char * attr, const CustomFormatFn & fmt)
{
	mask.set_heading(heading);
	int wid = width ? width : strlen(heading);
	mask.registerFormat(NULL, wid, opts, fmt, attr);
}

// setup display mask for default output
static void init_default_custom_format()
{
	mask.SetAutoSep(NULL, " ", NULL, "\n");

	int opts = wide_format ? (FormatOptionNoTruncate | FormatOptionAutoWidth) : 0;
	AddPrintColumn(" ID",        -7, FormatOptionNoTruncate, ATTR_CLUSTER_ID, format_job_id);
	AddPrintColumn("OWNER",     -14, FormatOptionAutoWidth | opts, ATTR_OWNER);
	AddPrintColumn("SUBMITTED",  11,    0, ATTR_Q_DATE, format_int_date);
	AddPrintColumn("RUN_TIME",   12,    0, ATTR_CLUSTER_ID, format_hist_runtime);
	AddPrintColumn("ST",         -2,    0, ATTR_JOB_STATUS, format_int_job_status);
	AddPrintColumn("COMPLETED",  11,    0, ATTR_COMPLETION_DATE, format_int_date);
	AddPrintColumn("CMD",       -15, FormatOptionLeftAlign | FormatOptionNoTruncate, ATTR_JOB_CMD, format_job_cmd_and_args);

	customFormat = TRUE;
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

static void printHeader()
{
	// Print header
	if ( ! longformat) {
		if ( ! customFormat) {
			// hack to get backward-compatible formatting
			if ( ! wide_format && 80 == wide_format_width)
				short_header();
			else {
				init_default_custom_format();
				if ( ! wide_format || 0 != wide_format_width) {
					int console_width = wide_format_width;
					if (console_width <= 0) console_width = getConsoleWindowSize()-1; // -1 because we get double spacing if we use the full width.
					if (console_width < 0) console_width = 1024;
					mask.SetOverallWidth(console_width);
				}
			}
		}
		if (customFormat && mask.has_headings()) {
			mask.display_Headings(stdout);
		}
	}
}

// Read history from a remote schedd
static void readHistoryRemote(classad::ExprTree *constraintExpr)
{
	printHeader();
	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileHeader(out);
		printf("%s\n", out.c_str());
	}

	classad::ClassAd ad;
	classad::ExprList *projList(new classad::ExprList());
	classad::ExprTree *projTree = static_cast<classad::ExprTree*>(projList);
	ad.Insert(ATTR_PROJECTION, projTree);
	ad.Insert(ATTR_REQUIREMENTS, constraintExpr);
	ad.InsertAttr(ATTR_NUM_MATCHES, specifiedMatch <= 0 ? -1 : specifiedMatch);

	DCSchedd schedd(g_name.size() ? g_name.c_str() : NULL, g_pool.size() ? g_pool.c_str() : NULL);
	if (!schedd.locate()) {
		fprintf(stderr, "Unable to locate remote schedd (name=%s, pool=%s).\n", g_name.c_str(), g_pool.c_str());
		exit(1);
	}
	Sock* sock;
	if (!(sock = schedd.startCommand(QUERY_SCHEDD_HISTORY, Stream::reli_sock, 0))) {
		fprintf(stderr, "Unable to send history command to remote schedd;\n"
			"Typically, either the schedd is not responding, does not authorize you, or does not support remote history.\n");
		exit(1);
	}
	classad_shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, ad) || !sock->end_of_message()) {
		fprintf(stderr, "Unable to send request to remote schedd; likely a server or network error.\n");
		exit(1);
	}

	while (true) {
		compat_classad::ClassAd ad;
		if (!getClassAd(sock, ad)) {
			fprintf(stderr, "Failed to recieve remote ad.\n");
			exit(1);
		}
		long long intVal;
		if (ad.EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0)) { // Last ad.
			if (!sock->end_of_message()) {
				fprintf(stderr, "Unable to close remote socket.\n");
			}
			sock->close();
			std::string errorMsg;
			if (ad.EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad.EvaluateAttrString(ATTR_ERROR_STRING, errorMsg)) {
				fprintf(stderr, "Error %lld: %s\n", intVal, errorMsg.c_str());
				exit(intVal);
			}
			if (ad.EvaluateAttrInt("MalformedAds", intVal) && intVal) {
				fprintf(stderr, "Remote side had parse errors on history file");
				exit(1);
			}
			if (!ad.EvaluateAttrInt(ATTR_NUM_MATCHES, intVal) || (intVal != matchCount)) {
				fprintf(stderr, "Client and server do not agree on number of ads sent;\n"
					"Indicates lost network packets or an internal error\n");
				exit(1);
			}
			break;
		}
		matchCount++;
		printJob(ad);
	}

	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileFooter(out);
		printf("%s\n", out.c_str());
	}
}

// Read the history from the specified history file, or from all the history files.
// There are multiple history files because we do rotation. 
static void readHistoryFromFiles(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr)
{
	printHeader();

    if (JobHistoryFileName) {
        if (fileisuserlog) {
            ClassAdList jobs;
            if ( ! userlog_to_classads(JobHistoryFileName, jobs, NULL, 0, constraint)) {
                fprintf(stderr, "Error: Can't open userlog %s\n", JobHistoryFileName);
                exit(1);
            }
            printJobAds(jobs);
            jobs.Clear();
        } else {
            // If the user specified the name of the file to read, we read that file only.
            readHistoryFromFileEx(JobHistoryFileName, constraint, constraintExpr, backwards);
        }
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
                    readHistoryFromFileEx(historyFiles[fileIndex], constraint, constraintExpr, backwards);
                    free(historyFiles[fileIndex]);
                }
            }
            else {
                for (fileIndex = 0; fileIndex < numHistoryFiles; fileIndex++) {
                    readHistoryFromFileEx(historyFiles[fileIndex], constraint, constraintExpr, backwards);
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
static long findLastDelimiter(FILE *fd, const char *filename)
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
static long findPrevDelimiter(FILE *fd, const char* filename, long currOffset)
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

    int scan_result =
    sscanf(buf.Value(), "%*s %*s %*s %ld %*s %*s %d %*s %*s %d %*s %*s %s %*s %*s %ld", 
           &prevOffset, &clusterId, &procId, owner, &completionDate);

    if (scan_result < 1 || (prevOffset == -1 && clusterId == -1 && procId == -1)) {
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
	  
            if (prevOffset <= 0) { // no match
                free(owner);
                return -1;
            }

            // Find previous delimiter + summary
            fseek(fd, prevOffset, SEEK_SET);
            buf.readLine(fd);

            void * pvner = realloc (owner, buf.Length() * sizeof(char));
            ASSERT( pvner != NULL );
            owner = (char *) pvner;

			prevOffset = -1;
			clusterId = -1;
			procId = -1;

			scan_result =
            sscanf(buf.Value(), "%*s %*s %*s %ld %*s %*s %d %*s %*s %d %*s %*s %s %*s %*s %ld", 
                   &prevOffset, &clusterId, &procId, owner, &completionDate);

			if (scan_result < 1 || (prevOffset == -1 && clusterId == -1 && procId == -1)) {
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
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr)
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
	int LogFd = safe_open_wrapper_follow(JobHistoryFileName,flags,0);
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
		std::string out;
		AddClassAdXMLFileHeader(out);
		printf("%s\n", out.c_str());
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
        if (!constraint || constraint[0]=='\0' || EvalBool(ad, constraintExpr)) {
            if (longformat) { 
				if( use_xml ) {
					fPrintAdAsXML(stdout, *ad);
				}
				else {
					fPrintAd(stdout, *ad);
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
		std::string out;
		AddClassAdXMLFileFooter(out);
		printf("%s\n", out.c_str());
	}
    fclose(LogFile);
    return;
}

// return true if p1 starts with p2
// if ppEnd is not NULL, return a pointer to the first non-matching char of p1
static bool starts_with(const char * p1, const char * p2, const char ** ppEnd = NULL)
{
	if ( ! p2 || ! *p2)
		return false;

	const char * p1e = p1;
	const char * p2e = p2;
	while (*p2e) {
		if (*p1e != *p2e)
			return false;
		++p2e; ++p1e;
	}
	if (ppEnd)
		*ppEnd = p1e;
	return true;
}

// print a single job ad
//
static void printJob(ClassAd & ad)
{
	if (longformat) {
		if (use_xml) {
			fPrintAdAsXML(stdout, ad);
		} else {
			fPrintAd(stdout, ad);
		}
		printf("\n");
	} else {
		if (customFormat) {
			mask.display(stdout, &ad);
		} else {
			displayJobShort(&ad);
		}
	}
}

// convert list of expressions into a classad
//
static void printJobIfConstraint(std::vector<std::string> & exprs, const char* constraint, ExprTree *constraintExpr)
{
	if ( ! exprs.size())
		return;

	ClassAd ad;
	size_t ix;

	// convert lines vector into classad.
	while ((ix = exprs.size()) > 0) {
		const char * pexpr = exprs[ix-1].c_str();
		if ( ! ad.Insert(pexpr)) {
			dprintf(D_ALWAYS,"failed to create classad; bad expr = '%s'\n", pexpr);
			printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
			exprs.clear();
			return;
		}
		exprs.pop_back();
	}

	if (!constraint || constraint[0]=='\0' || EvalBool(&ad, constraintExpr)) {
		printJob(ad);
		matchCount++; // if control reached here, match has occured
	}
}

static void printJobAds(ClassAdList & jobs)
{
	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileHeader(out);
		printf("%s\n", out.c_str());
	}

	jobs.Open();
	ClassAd	*job;
	while (( job = jobs.Next())) {
		printJob(*job);
	}
	jobs.Close();

	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileFooter(out);
		printf("%s\n", out.c_str());
	}
}

static void readHistoryFromFileEx(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr, bool read_backwards)
{
	// In case of rotated history files, check if we have already reached the number of 
	// matches specified by the user before reading the next file
	if ((specifiedMatch != 0) && (matchCount == specifiedMatch)) {
		return;
	}

	// the old function doesn't work for backwards, but it does work for forwards so go ahead and call it.
	//
	if ( ! read_backwards) {
		readHistoryFromFileOld(JobHistoryFileName, constraint, constraintExpr);
		return;
	}

	// do backwards reading.
	BackwardFileReader reader(JobHistoryFileName, O_RDONLY);
	if (reader.LastError()) {
		// report error??
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(reader.LastError()));
		exit(1);
	}

	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileHeader(out);
		printf("%s\n", out.c_str());
	}

	std::string line;        // holds the current line from the log file.
	std::string banner_line; // the contents of the "*** " banner line for the job we are scanning

	// the last line in the file should be a "*** " banner line.
	// we want to scan backwards until we find it, what is above that in the file is the job
	// information for that banner line.
	while (reader.PrevLine(line)) {
		if (starts_with(line.c_str(), "*** ")) {
			banner_line = line;
			break;
		}
	}

	std::vector<std::string> exprs;
	while (reader.PrevLine(line)) {

		// the banner is at the end of the job information, so when we get to on, we 
		// know that we are done accumulating expressions into the vector.
		if (starts_with(line.c_str(), "*** ")) {

			// TODO: extract information from banner line and use it to skip parsing 
			// the job ad for the simple query for Cluster, Proc, or Owner.

			if (exprs.size() > 0) {
				printJobIfConstraint(exprs, constraint, constraintExpr);
				exprs.clear();
			}

			// the current line is the banner that starts (ends) the next job record
			// if we already hit our match count, we can stop now.
			banner_line = line;
			if ((specifiedMatch != 0) && (matchCount == specifiedMatch))
				break;

		} else {

			// we have to parse the lines in from the start of the file to the end
			// to handle chained ads correctly, so here we just push the lines into
			// a vector as they arrive.  note that this puts them in the vector backwards
			// comments can be discarded at this point.
			if ( ! line.empty()) {
				const char * psz = line.c_str();
				while (*psz == ' ' || *psz == '\t') ++psz;
				if (*psz != '#') {
					exprs.push_back(line);
				}
			}
		}
	}

	// when we hit the start of the file, we may still have 1 job record to print out.
	// TODO: verify that the Offset in the banner is 0 at this point. 
	if (exprs.size() > 0) {
		if ((specifiedMatch <= 0) || (matchCount < specifiedMatch))
			printJobIfConstraint(exprs, constraint, constraintExpr);
		exprs.clear();
	}

	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileFooter(out);
		printf("%s\n", out.c_str());
	}
	reader.Close();
}

// !!! ENTRIES IN THIS TABLE MUST BE SORTED BY THE FIRST FIELD !!
static const CustomFormatFnTableItem LocalPrintFormats[] = {
	{ "DATE",            ATTR_Q_DATE, format_int_date, NULL },
	{ "JOB_COMMAND",     ATTR_JOB_CMD, format_job_cmd_and_args, ATTR_JOB_DESCRIPTION "\0MATCH_EXP_" ATTR_JOB_DESCRIPTION "\0" },
	{ "JOB_ID",          ATTR_CLUSTER_ID, format_job_id, ATTR_PROC_ID "\0" },
	{ "JOB_STATUS",      ATTR_JOB_STATUS, format_int_job_status, ATTR_LAST_SUSPENSION_TIME "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" },
	{ "JOB_STATUS_RAW",  ATTR_JOB_STATUS, format_job_status_raw, NULL },
	{ "JOB_UNIVERSE",    ATTR_JOB_UNIVERSE, format_job_universe, NULL },
	{ "READABLE_KB",     ATTR_REQUEST_DISK, format_readable_kb, NULL },
	{ "READABLE_MB",     ATTR_REQUEST_MEMORY, format_readable_mb, NULL },
	{ "RUNTIME",         ATTR_JOB_REMOTE_WALL_CLOCK, format_utime_double, NULL },
};
static const CustomFormatFnTable LocalPrintFormatsTable = SORTED_TOKENER_TABLE(LocalPrintFormats);

static int set_print_mask_from_stream(
	AttrListPrintMask & print_mask,
	std::string & constraint,
	const char * streamid,
	bool is_filename)
{
	StringList attrs;  // used for projection, which we don't currently do. 
	std::string messages;
	printmask_aggregation_t aggregation;
	printmask_headerfooter_t headFoot = STD_HEADFOOT;
	std::vector<GroupByKeyInfo> group_by_keys;
	SimpleInputStream * pstream = NULL;

	FILE *file = NULL;
	if (MATCH == strcmp("-", streamid)) {
		pstream = new SimpleFileInputStream(stdin, false);
	} else if (is_filename) {
		file = safe_fopen_wrapper_follow(streamid, "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open select file: %s\n", streamid);
			return -1;
		}
		pstream = new SimpleFileInputStream(file, true);
	} else {
		pstream = new StringLiteralInputStream(streamid);
	}
	ASSERT(pstream);

	int err = SetAttrListPrintMaskFromStream(
					*pstream,
					LocalPrintFormatsTable,
					print_mask,
					headFoot,
					aggregation,
					group_by_keys,
					constraint,
					attrs,
					messages);
	delete pstream; pstream = NULL;
	if ( ! err) {
		customFormat = true;
		if ( ! constraint.empty()) {
			ExprTree *constraintExpr=NULL;
			if ( ! ParseClassAdRvalExpr(constraint.c_str(), constraintExpr)) {
				formatstr_cat(messages, "WHERE expression is not valid: %s\n", constraint.c_str());
				err = -1;
			} else {
				delete constraintExpr;
			}
		}
		if (aggregation) {
			formatstr_cat(messages, "AUTOCLUSTER or UNIQUE aggregation is not supported.\n");
			err = -1;
		}
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}



