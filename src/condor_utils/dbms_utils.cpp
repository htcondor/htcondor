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
#include <string.h>
#include "condor_config.h"
#include "dbms_utils.h"
#include "misc_utils.h"
#include "condor_attributes.h"
#include "MyString.h"

#ifdef HAVE_EXT_POSTGRESQL

#include "pgsqldatabase.h"
#include "condor_tt/condor_ttdb.h"
#include "condor_tt/jobqueuecollection.h"

MyString condor_ttdb_fillEscapeCharacters(const char * str, dbtype dt) {
	int i;
	
	int len = strlen(str);

	MyString newstr;
        
	for (i = 0; i < len; i++) {
		switch(str[i]) {
        case '\\':
			if (dt == T_PGSQL) {
					/* postgres need to escape backslash */
				newstr += '\\';
				newstr += '\\';
			}
            break;
        case '\'':
				/* postgres can escape a single quote with 
				   another single quote */
            newstr += '\'';
            newstr += '\'';
            break;
        default:
            newstr += str[i];
            break;
		}
	}
    return newstr;
}

MyString condor_ttdb_buildts(time_t *tv, dbtype dt)
{
	char tsv[100];
	struct tm *tm;	
	MyString rv;

	tm = localtime(tv);		

	snprintf(tsv, 100, "%d/%d/%d %02d:%02d:%02d %s", 
			 tm->tm_mon+1,
			 tm->tm_mday,
			 tm->tm_year+1900,
			 tm->tm_hour,
			 tm->tm_min,
			 tm->tm_sec,
			 my_timezone(tm->tm_isdst));	

	switch(dt) {
	case T_PGSQL:
		rv.formatstr("'%s'", tsv);
		break;
	default:
		break;
	}

	return rv;
}

MyString condor_ttdb_buildtsval(time_t *tv, dbtype dt)
{
	char tsv[100];
	struct tm *tm;	
	MyString rv;

	tm = localtime(tv);		

	snprintf(tsv, 100, "%d/%d/%d %02d:%02d:%02d %s", 
			 tm->tm_mon+1,
			 tm->tm_mday,
			 tm->tm_year+1900,
			 tm->tm_hour,
			 tm->tm_min,
			 tm->tm_sec,
			 my_timezone(tm->tm_isdst));	

	switch(dt) {
	default:
		break;
	}

	return rv;
}
extern "C" {

//! Gets the writer password required by the quill++
//  daemon to access the database
static char * getWritePassword(const char *write_passwd_fname, 
							   const char *host, const char *port, 
							   const char *db,
							   const char *dbuser) {
	FILE *fp = NULL;
	char *passwd = (char *) malloc(64 * sizeof(char));
	int len;
	char *prefix;
	MyString *msbuf = 0;
	const char *buf;
	bool found = FALSE;

		// prefix is for the prefix of the entry in the .pgpass
		// it is in the format of the following:
		// host:port:db:user:password
	len = 10+strlen(host) + strlen(port) + strlen(db) + strlen(dbuser);

	prefix = (char  *) malloc (len * sizeof(char));

	snprintf(prefix, len, "%s:%s:%s:%s:", host, port, db, dbuser);

	len = strlen(prefix);

	fp = safe_fopen_wrapper_follow(write_passwd_fname, "r");

	if(fp == NULL) {
		EXCEPT("Unable to open password file %s\n", write_passwd_fname);
	}
	
		//dprintf(D_ALWAYS, "prefix: %s\n", prefix);

	msbuf = new MyString();
	while(msbuf->readLine(fp, true)) {
		buf = msbuf->Value();

			//dprintf(D_ALWAYS, "line: %s\n", buf);

			// check if the entry matches the prefix
		if (strncmp(buf, prefix, len) == 0) {
				// extract the password
			strncpy(passwd, &buf[len], 64);
			delete msbuf;
			found = TRUE;
			
				// remove the new line in the end
			if (passwd[strlen(passwd)-1] == '\n') {
				passwd[strlen(passwd)-1] = '\0';
			}

			break;
		}

		delete msbuf;
		msbuf = new MyString();
	}

	fclose(fp);
	if (!found) {
		EXCEPT("Unable to find password from file %s with prefix %s\n", write_passwd_fname, prefix);
	}
	free(prefix);
    
	return passwd;
}

dbtype getConfigDBType() 
{
	char *tmp;
	dbtype dt = T_PGSQL;

	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
		free (tmp);
	} 
	
	return dt;
}

char *getDBConnStr(
const char* jobQueueDBIpAddress,
const char* jobQueueDBName,
const char* jobQueueDBUser,
const char* spool
)
{
	char *host = NULL, *port = NULL;
	char *writePassword;
	MyString writePasswordFile;
	int len, tmp1, tmp2, tmp3;
	char *jobQueueDBConn;

	ASSERT(jobQueueDBIpAddress);
	ASSERT(jobQueueDBName);

	if (jobQueueDBUser == NULL) {
		dprintf(D_ALWAYS, "Please set the QUILL_DB_USER to a valid username\n");
		ASSERT(jobQueueDBUser);
	}

	ASSERT(spool);

		//parse the ip address and get host and port
	len = strlen(jobQueueDBIpAddress);
	host = (char *) malloc(len * sizeof(char));
	port = (char *) malloc(len * sizeof(char));
	
		//split the <ipaddress:port> into its two parts accordingly
	char const *ptr_colon = strchr(jobQueueDBIpAddress, ':');
	strncpy(host, jobQueueDBIpAddress, 
			ptr_colon - jobQueueDBIpAddress);
		// terminate the string properly
	host[ptr_colon - jobQueueDBIpAddress] = '\0';
	strncpy(port, ptr_colon+1, len);
		// terminate the string properyly
	port[strlen(ptr_colon+1)] = '\0';

		// get the password from the .pgpass file
	writePasswordFile.formatstr( "%s/.pgpass", spool);

	writePassword = getWritePassword(writePasswordFile.Value(), 
										   host, port,
										   jobQueueDBName?jobQueueDBName:"", 
										   jobQueueDBUser);

	tmp1 = jobQueueDBName?strlen(jobQueueDBName):0;
	tmp2 = strlen(writePassword);
	
		//tmp3 is the size of dbconn - its size is estimated to be
		//(2 * len) for the host/port part, tmp1 + tmp2 for the
		//password and dbname part and 1024 as a cautiously
		//overestimated sized buffer
	tmp3 = (2 * len) + tmp1 + tmp2 + 1024;

	jobQueueDBConn = (char *) malloc(tmp3 * sizeof(char));

	snprintf(jobQueueDBConn, tmp3, 
			 "host=%s port=%s user=%s password=%s dbname=%s", 
			 host?host:"", port?port:"", 
			 jobQueueDBUser?jobQueueDBUser:"", 
			 writePassword, 
			 jobQueueDBName?jobQueueDBName:"");

	free(writePassword);
	
	if(host) {
		free(host);
		host = NULL;
	}

	if(port) {
		free(port);
		port = NULL;
	}
	
	return jobQueueDBConn;
}

bool stripdoublequotes(char *attVal) {
	int attValLen;

	if (!attVal) {
		return FALSE;
	}

	attValLen = strlen(attVal);

		/* strip enclosing double quotes
		*/
	if (attVal[attValLen-1] == '"' && attVal[0] == '"') {
		//strncpy(attVal, &attVal[1], attValLen-2);
		memmove(attVal, &attVal[1], attValLen-2);
		attVal[attValLen-2] = '\0';
		return TRUE;	 
	} else {
		return FALSE;
	}	
}

bool stripdoublequotes_MyString(MyString &value) {
	int attValLen;

	if (value.IsEmpty()) {
		return FALSE;
	}
	
	attValLen = value.Length();

		/* strip enclosing double quotes
		*/
	if (value[attValLen-1] == '"' && value[0] == '"') {
		value = value.Substr(1, attValLen-2);
		return TRUE;
	} else {
		return FALSE;
	}
}

/* The following attributes are treated as horizontal daemon attributes.
   Notice that if you add more attributes to the following list, the horizontal
   schema for Daemons_Horizontal needs to revised accordingly.
*/
bool isHorizontalDaemonAttr(
char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, ATTR_NAME) &&
		  strcasecmp(attName, ATTR_UPDATESTATS_HISTORY))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "monitorselfcpuusage") &&
				 strcasecmp(attName, "monitorselfimagesize") && 
				 strcasecmp(attName, "monitorselfresidentsetsize") && 
				 strcasecmp(attName, "monitorselfage") &&
				 strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && 
				 strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) && 
				 strcasecmp(attName, ATTR_UPDATESTATS_LOST))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, "lastreportedtime") &&
				 strcasecmp(attName, "MonitorSelfTime"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;

	}
				 
	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

/* An attribute is treated as a static one if it is not one of the following
   Notice that if you add more attributes to the following list, the horizontal
   schema for Machines_Horizontal needs to revised accordingly.
*/
bool isHorizontalMachineAttr(
char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, ATTR_NAME) &&
		  strcasecmp(attName, ATTR_OPSYS) && 
		  strcasecmp(attName, ATTR_ARCH) &&		  
		  strcasecmp(attName, ATTR_STATE) && 
		  strcasecmp(attName, ATTR_ACTIVITY) &&		 
		  strcasecmp(attName, ATTR_GLOBAL_JOB_ID))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!strcasecmp(attName, ATTR_CPU_IS_BUSY)) {
		attr_type = CONDOR_TT_TYPE_BOOL;
		return TRUE;

	} else if (!(strcasecmp(attName, ATTR_KEYBOARD_IDLE) && 
				 strcasecmp(attName, ATTR_CONSOLE_IDLE) &&
				 strcasecmp(attName, ATTR_LOAD_AVG) && 
				 strcasecmp(attName, "CondorLoadAvg") &&
				 strcasecmp(attName, ATTR_TOTAL_LOAD_AVG) && 
				 strcasecmp(attName, ATTR_VIRTUAL_MEMORY) &&
				 strcasecmp(attName, ATTR_MEMORY ) &&
				 strcasecmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
				 strcasecmp(attName, ATTR_CPU_BUSY_TIME) &&
				 strcasecmp(attName, ATTR_CURRENT_RANK) &&
				 strcasecmp(attName, ATTR_CLOCK_MIN) && 
				 strcasecmp(attName, ATTR_CLOCK_DAY) &&
				 strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_LOST))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;
	} else if (!(strcasecmp(attName, "LastReportedTime") &&
				 strcasecmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) &&
				 strcasecmp(attName, ATTR_ENTERED_CURRENT_STATE))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalProcAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, "args"))) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!(strcasecmp(attName, "remotehost") && 
				 strcasecmp(attName, "globaljobid"))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotewallclocktime") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "numrestarts"))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, "shadowbday") && 
				 strcasecmp(attName, "enteredcurrentstatus"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalClusterAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, "cmd") && 
		  strcasecmp(attName, "args"))
		) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!strcasecmp(attName, "owner")) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotewallclocktime") &&
				 strcasecmp(attName, "jobuniverse"))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!strcasecmp(attName, "qdate")) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalHistoryAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {

	if (!(strcasecmp(attName, "cmd") && 
		  strcasecmp(attName, "env") &&
		  strcasecmp(attName, "args"))
		) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!(strcasecmp(attName, "transferin") && 
				 strcasecmp(attName, "transferout") &&
				 strcasecmp(attName, "transfererr"))
			   ) {
		
		attr_type = CONDOR_TT_TYPE_BOOL;
		return TRUE;

	} else if (!(strcasecmp(attName, "owner") &&
				 strcasecmp(attName, "globaljobid") &&				 
				 strcasecmp(attName, "condorversion") &&
				 strcasecmp(attName, "condorplatform") &&
				 strcasecmp(attName, "rootdir") &&
				 strcasecmp(attName, "Iwd") &&
				 strcasecmp(attName, "user") &&
				 strcasecmp(attName, "userlog")	&&
				 strcasecmp(attName, "killsig") &&
				 strcasecmp(attName, "in") &&
				 strcasecmp(attName, "out") &&
				 strcasecmp(attName, "err") &&
				 strcasecmp(attName, "shouldtransferfiles")  &&
				 strcasecmp(attName, "transferfiles") &&
				 strcasecmp(attName, "filesystemdomain") &&
				 strcasecmp(attName, "lastremotehost") 
				 )) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "numckpts") && 
				 strcasecmp(attName, "numrestarts") &&
				 strcasecmp(attName, "numsystemholds") &&
				 strcasecmp(attName, "jobuniverse") &&
				 strcasecmp(attName, "minhosts") &&
				 strcasecmp(attName, "maxhosts") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "coresize") &&
				 strcasecmp(attName, "executablesize") &&
				 strcasecmp(attName, "diskusage") &&
				 strcasecmp(attName, "numjobmatches") &&
				 strcasecmp(attName, "jobruncount") &&
				 strcasecmp(attName, "filereadcount") &&
				 strcasecmp(attName, "filereadbytes") &&
				 strcasecmp(attName, "filewritecount") &&
				 strcasecmp(attName, "filewritebytes") &&
				 strcasecmp(attName, "fileseekcount") &&
				 strcasecmp(attName, "totalsuspensions") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "exitstatus") && 
				 strcasecmp(attName, "localusercpu") &&
				 strcasecmp(attName, "localsyscpu") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotesyscpu") &&
				 strcasecmp(attName, "bytessent") &&
				 strcasecmp(attName, "bytesrecvd") && 
				 strcasecmp(attName, "rscbytessent") &&
				 strcasecmp(attName, "rscbytesrecvd") &&
				 strcasecmp(attName, "exitcode") && 
				 strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "remotewallclocktime")
				 )) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, "qdate") &&
		  strcasecmp(attName, "lastmatchtime") &&		  
		  strcasecmp(attName, "jobstartdate") &&	
		  strcasecmp(attName, "jobcurrentstartdate") &&	
		  strcasecmp(attName, "enteredcurrentstatus") && 
		  strcasecmp(attName, "completiondate"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

QuillErrCode insertHistoryJobCommon(AttrList *ad, JobQueueDatabase* DBObj, dbtype dt, MyString &errorSqlStmt, 
									const char*scheddname, const time_t scheddbirthdate) {
  int        cid, pid;
  MyString sql_stmt;
  MyString sql_stmt2;
  ExprTree *expr;
  const char *attr_name;
  MyString value = "";
  MyString name = "";
  MyString newvalue;
  const char* data_arr1[7];
  QuillAttrDataType data_typ1[7];

  const char* data_arr2[7];
  QuillAttrDataType data_typ2[7];

  QuillAttrDataType  attr_type;

  MyString scheddbirthdate_str;
  MyString ts_expr_val;

  bool flag1=false, flag2=false,flag3=false, flag4=false;
	  //const char *scheddname = jqDBManager.getScheddname();
	  //const time_t scheddbirthdate = jqDBManager.getScheddbirthdate();

  ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cid);
  ad->EvalInteger (ATTR_PROC_ID, NULL, pid);

  scheddbirthdate_str.formatstr("%lu", scheddbirthdate);

 {
	  sql_stmt.formatstr(
					   "DELETE FROM Jobs_Horizontal_History WHERE scheddname = '%s' AND scheddbirthdate = %lu AND cluster_id = %d AND proc_id = %d", scheddname, (unsigned long)scheddbirthdate, cid, pid);
	  sql_stmt2.formatstr(
						"INSERT INTO Jobs_Horizontal_History(scheddname, scheddbirthdate, cluster_id, proc_id, enteredhistorytable) VALUES('%s', %lu, %d, %d, current_timestamp)", scheddname, (unsigned long)scheddbirthdate, cid, pid);

	  if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		  errorSqlStmt = sql_stmt;
		  return QUILL_FAILURE;	  
	  }

	  if (DBObj->execCommand(sql_stmt2.Value()) == QUILL_FAILURE) {
		  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt2.Value());
		  errorSqlStmt = sql_stmt2;
		  return QUILL_FAILURE;	  
	  }
  }

  ad->ResetExpr(); // for iteration initialization
  while(ad->NextExpr(attr_name, expr)) {
	  name = attr_name;
	  if ( name.IsEmpty() ) break;

	  value = ExprTreeToString( expr );
	  if ( value.IsEmpty() ) break;

		  /* the following are to avoid overwriting the attr values. The hack 
			 is based on the fact that an attribute of a job ad comes before 
			 the attribute of a cluster ad. And this is because 
		     attribute list of cluster ad is chained to a job ad.
		   */
	  if(strcasecmp(name.Value(), "jobstatus") == 0) {
		  if(flag4) continue;
		  flag4 = true;
	  }

	  if(strcasecmp(name.Value(), "remotewallclocktime") == 0) {
		  if(flag1) continue;
		  flag1 = true;
	  }
	  else if(strcasecmp(name.Value(), "completiondate") == 0) {
		  if(flag2) continue;
		  flag2 = true;
	  }
	  else if(strcasecmp(name.Value(), "committedtime") == 0) {
		  if(flag3) continue;
		  flag3 = true;
	  }

	  if(isHorizontalHistoryAttribute(name.Value(), attr_type)) {
			  /* change the names for the following attributes
				 because they conflict with keywords of some
				 databases 
			  */
		  if (strcasecmp(name.Value(), "out") == 0) {
			  name = "stdout";
		  }

		  if (strcasecmp(name.Value(), "err") == 0) {
			  name = "stderr";
		  }

		  if (strcasecmp(name.Value(), "in") == 0) {
			  name = "stdin";
		  }		

		  if (strcasecmp(name.Value(), "user") == 0) {
			  name = "negotiation_user_name";
		  }		  
  
		  sql_stmt2 = "";
		  
		  if(attr_type == CONDOR_TT_TYPE_TIMESTAMP) {
			  time_t clock;
			  MyString ts_expr;

			  clock = atoi(value.Value());

			  {
				  ts_expr = condor_ttdb_buildts(&clock, dt);	
				  
				  sql_stmt.formatstr(
								   "UPDATE Jobs_Horizontal_History SET %s = (%s) WHERE scheddname = '%s' and scheddbirthdate = %lu and cluster_id = %d and proc_id = %d", name.Value(), ts_expr.Value(), scheddname, (unsigned long)scheddbirthdate, cid, pid);
			  }
		  }	else {
				  /* strip double quotes for string values */
			  if (attr_type == CONDOR_TT_TYPE_CLOB ||
				  attr_type == CONDOR_TT_TYPE_STRING) {
				  
				  if (!stripdoublequotes_MyString(value)) {
					  dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertHistoryJob\n", name.Value());
				  }
			  }
			  
			  newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);

			  {
				  sql_stmt.formatstr(
								   "UPDATE Jobs_Horizontal_History SET %s = '%s' WHERE scheddname = '%s' and scheddbirthdate = %lu and cluster_id = %d and proc_id = %d", name.Value(), newvalue.Value(), scheddname, (unsigned long)scheddbirthdate, cid, pid);
			  }
		  }
	  } else {
		  newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
		  
		  {
			  sql_stmt.formatstr(
							   "DELETE FROM Jobs_Vertical_History WHERE scheddname = '%s' AND scheddbirthdate = %lu AND cluster_id = %d AND proc_id = %d AND attr = '%s'", scheddname, (unsigned long)scheddbirthdate, cid, pid, name.Value());
		  }
			
		  {
			  sql_stmt2.formatstr(
								"INSERT INTO Jobs_Vertical_History(scheddname, scheddbirthdate, cluster_id, proc_id, attr, val) VALUES('%s', %lu, %d, %d, '%s', '%s')", scheddname, (unsigned long)scheddbirthdate, cid, pid, name.Value(), newvalue.Value());
		  }
	  }

	  {
		  if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		  
			  errorSqlStmt = sql_stmt;

			  return QUILL_FAILURE;
		  }
	  }

	  if (!sql_stmt2.IsEmpty()) {
		  
		{		  
			if ((DBObj->execCommand(sql_stmt2.Value()) == QUILL_FAILURE)) {
				dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				dprintf(D_ALWAYS, "sql = %s\n", sql_stmt2.Value());
		  
				errorSqlStmt = sql_stmt2;

				return QUILL_FAILURE;			  
			}
		}
	  }
	  
	  name = "";
	  value = "";
  }  

  return QUILL_SUCCESS;
} // insertHistoryJobCommon

} // extern "C"
#endif
