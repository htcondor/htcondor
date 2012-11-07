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
#include "condor_debug.h"
#include "sqlquery.h"
#include "quill_enums.h"
#include "condor_config.h"

#ifdef HAVE_EXT_POSTGRESQL
/* Some Oracle code preserved for posterity */

/*
The macroc below contains expressions for extracting unix time from a 
timestamp value in Oracle.
Because the timestamp attributes are stored in timestamp type in 
database, instead of the unix time (the number of seconds since Epoch) that is 
required by Condor tools (condor_q, condor_history), the expression tries to 
convert a timestamp value back to the unix time. The way we do is to extract 
number of days, number of hours, number of minutes and number of seconds 
since Epoch and add up their weighted sum.

For postgres, the expression is simpler since it provides a function 
for extracting epoch directly from a timestamp value.
 */

#ifndef WIN32

#define quill_oracle_history_hor_select_list "scheddname, cluster_id, \
proc_id, \
(extract(day from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as qdate, \
owner, globaljobid, numckpts, numrestarts, numsystemholds, condorversion, \
condorplatform, rootdir, Iwd, jobuniverse, cmd, minhosts, maxhosts, jobprio, \
negotiation_user_name, env, userlog, coresize, killsig, stdin, transferin, \
stdout, transferout, stderr, transfererr, shouldtransferfiles, transferfiles, \
executablesize, diskusage, filesystemdomain, args, \
(extract(day from (SYS_EXTRACT_UTC(lastmatchtime) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(lastmatchtime) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(lastmatchtime) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(lastmatchtime) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as lastmatchtime, \
numjobmatches, \
(extract(day from (SYS_EXTRACT_UTC(jobstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(jobstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(jobstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(jobstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as jobstartdate, \
(extract(day from (SYS_EXTRACT_UTC(jobcurrentstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(jobcurrentstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(jobcurrentstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(jobcurrentstartdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as jobcurrentstartdate, \
jobruncount, filereadcount, filereadbytes, filewritecount, filewritebytes, \
fileseekcount, totalsuspensions, imagesize, exitstatus, localusercpu, \
localsyscpu, remoteusercpu, remotesyscpu, bytessent, bytesrecvd, \
rscbytessent, rscbytesrecvd, exitcode, jobstatus, \
(extract(day from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as enteredcurrentstatus, \
remotewallclocktime, lastremotehost, \
(extract(day from (SYS_EXTRACT_UTC(completiondate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(completiondate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(completiondate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(completiondate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as completiondate"

#else
#define quill_oracle_history_hor_select_list "fix_me()"
#endif

#define quill_pgsql_history_hor_select_list "scheddname, cluster_id, proc_id, \
extract(epoch from qdate) as qdate, owner, globaljobid, numckpts, \
numrestarts, numsystemholds, condorversion, condorplatform, rootdir, Iwd, \
jobuniverse, cmd, minhosts, maxhosts, jobprio, negotiation_user_name, env, \
userlog, coresize, killsig, stdin, transferin, stdout, transferout, stderr, \
transfererr, shouldtransferfiles, transferfiles, executablesize, diskusage, \
filesystemdomain, args, extract(epoch from lastmatchtime) as lastmatchtime, \
numjobmatches, extract(epoch from jobstartdate) as jobstartdate, \
extract(epoch from jobcurrentstartdate) as jobcurrentstartdate, jobruncount, \
filereadcount, filereadbytes, filewritecount, filewritebytes, fileseekcount, \
totalsuspensions, imagesize, exitstatus, localusercpu, localsyscpu, \
remoteusercpu, remotesyscpu, bytessent, bytesrecvd, rscbytessent, \
rscbytesrecvd, exitcode, jobstatus, \
extract(epoch from enteredcurrentstatus) as enteredcurrentstatus, \
remotewallclocktime, lastremotehost, \
extract(epoch from completiondate) as completiondate"

#define quill_history_ver_select_list "scheddname, cluster_id, proc_id, \
attr, val"

#define quill_avg_time_template_pgsql "SELECT avg(now() - QDate) \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc_id, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM procads_horizontal where cluster_id != 0) AS p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM clusterads_horizontal where cluster_id != 0) AS c \
            WHERE p.cluster_id = c.cluster_id) AS h \
         WHERE (jobStatus != 3) and (jobstatus != 4)"


/* oracle doesn't support avg over time interval, therefore, the following 
   expression converts to difference in seconds between two timestamps and 
   then compute avg over seconds passed.

   After the averge of number of seconds is computed, we convert the avg back 
   to the format of 'days hours:minutes:seconds' for display.
*/
#define quill_avg_time_template_oracle "SELECT floor(t.elapsed/86400) || ' ' \
|| floor(mod(t.elapsed, 86400)/3600) || ':' || floor(mod(t.elapsed, 3600)/60) \
|| ':' || floor(mod(t.elapsed, 60)) \
FROM (SELECT avg((extract(day from (current_timestamp - QDate)))*86400 + (extract(hour from (current_timestamp - QDate)))*3600 + (extract(minute from (current_timestamp - QDate)))*60 + (extract(second from (current_timestamp - QDate)))) as elapsed \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus IS NULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc_id, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM quillwriter.procads_horizontal where cluster_id != 0)  p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM quillwriter.clusterads_horizontal where cluster_id != 0) c \
            WHERE p.cluster_id = c.cluster_id) h \
         WHERE (jobStatus != 3) and (jobstatus != 4)) t"

SQLQuery::
SQLQuery ()
{
  query_str = 0;
  declare_cursor_str = 0;
  fetch_cursor_str = 0;
  close_cursor_str = 0;
  scheddname = 0;
  jobqueuebirthdate = 0;
  query_predicates = NULL;
}

SQLQuery::
SQLQuery (query_types qtype, void **parameters)
{
	setQuery(qtype,parameters);
  	jobqueuebirthdate = 0;
}


SQLQuery::
~SQLQuery ()
{
  if(query_str) 
	  free(query_str);
  if(declare_cursor_str) 
	  free(declare_cursor_str);
  if(fetch_cursor_str) 
	  free(fetch_cursor_str);
  if(close_cursor_str) 
	  free(close_cursor_str);
  if(scheddname) 
	  free(scheddname);
}

query_types SQLQuery::
getType() 
{
  return type;
}

char * SQLQuery::
getQuery() 
{
  return query_str;
}

char * SQLQuery::
getDeclareCursorStmt() 
{
  return declare_cursor_str;
}

char * SQLQuery::
getFetchCursorStmt() 
{
  return fetch_cursor_str;
}

char * SQLQuery::
getCloseCursorStmt() 
{
  return close_cursor_str;
}

void SQLQuery::
setQuery(query_types qtype, void **parameters) 
{
	type = qtype;
	query_predicates = parameters;
}

void SQLQuery::
prepareQuery() 
{
  createQueryString(type, query_predicates);
}

void SQLQuery::
setScheddname(char *name) 
{
	if(name) {
		scheddname = strdup(name);
	}
}

void SQLQuery::
setJobqueuebirthdate(time_t birthdate)
{
	jobqueuebirthdate = birthdate;
}
int SQLQuery::
createQueryString(query_types qtype, void **parameters) {
  char *tmp;
  dbtype dt = T_PGSQL;

  tmp = param("QUILL_DB_TYPE");
  if (tmp) {
	  if (strcasecmp(tmp, "PGSQL") == 0) {
		  dt = T_PGSQL;
	  }
  free(tmp);
  } else {
	  dt = T_PGSQL; // assume PGSQL by default
  }

  query_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  declare_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  fetch_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  close_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));

  MyString schedd_predicate_full = "";
  MyString schedd_predicate_part = "";
  MyString schedd_predicate_hh = "";
  if (scheddname) { 
	  schedd_predicate_full.formatstr_cat("WHERE scheddname = '%s'", scheddname);
	  schedd_predicate_part.formatstr_cat("AND scheddname = '%s'", scheddname);
	  schedd_predicate_hh.formatstr_cat("AND hh.scheddname = '%s'", scheddname);

	  if (jobqueuebirthdate) {
		  schedd_predicate_full.formatstr_cat(" AND scheddbirthdate = %d", jobqueuebirthdate);
		  schedd_predicate_part.formatstr_cat(" AND scheddbirthdate = %d", jobqueuebirthdate);
		  schedd_predicate_hh.formatstr_cat(" AND hh.scheddbirthdate = %d", jobqueuebirthdate);
	  }
  }

  switch(qtype) {
    
  case HISTORY_ALL_HOR:
	  if (dt == T_PGSQL) {
			sprintf(declare_cursor_str, 
					"DECLARE HISTORY_ALL_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History %s ORDER BY scheddname, cluster_id, proc_id;", 
					quill_pgsql_history_hor_select_list, schedd_predicate_full.Value());
			sprintf(fetch_cursor_str,
				"FETCH FORWARD 100 FROM HISTORY_ALL_HOR_CUR");
			sprintf(close_cursor_str,
				"CLOSE HISTORY_ALL_HOR_CUR");
	  }
    
    break;
  case HISTORY_ALL_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_ALL_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History %s ORDER BY scheddname, cluster_id, proc_id;", 
				quill_history_ver_select_list, schedd_predicate_full.Value());
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_ALL_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_ALL_VER_CUR");
	  }

    break;
  case HISTORY_CLUSTER_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE cluster_id=%d %s ORDER BY scheddname, cluster_id, proc_id;",
				quill_pgsql_history_hor_select_list, 
				*((int *)parameters[0]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_HOR_CUR");
	  }

    break;
  case HISTORY_CLUSTER_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History WHERE cluster_id=%d %s ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_ver_select_list,
				*((int *)parameters[0]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_VER_CUR");
	  }

    break;
  case HISTORY_CLUSTER_PROC_HOR:
	  if (dt == T_PGSQL) {
    	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE cluster_id=%d and proc_id=%d %s ORDER BY scheddname, cluster_id, proc_id;",
				quill_pgsql_history_hor_select_list,
				*((int *)parameters[0]), *((int *)parameters[1]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_PROC_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_HOR_CUR");
	  }

    break;  
  case HISTORY_CLUSTER_PROC_VER:
	  if (dt == T_PGSQL) {
    	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History WHERE cluster_id=%d and proc_id=%d %s "
			"ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_ver_select_list, 
				*((int *)parameters[0]), *((int *)parameters[1]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_PROC_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_VER_CUR");
	  }

    break;  
  case HISTORY_OWNER_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE owner='%s' %s "
			"ORDER BY scheddname, cluster_id, proc_id;",
				quill_pgsql_history_hor_select_list,
				((char *)parameters[0]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_OWNER_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_HOR_CUR");
	  }

	  break;
  case HISTORY_OWNER_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_VER_CUR CURSOR FOR SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val, hv.scheddname FROM "
			"Jobs_Horizontal_History hh, Jobs_Vertical_History hv "
			"WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND hh.owner='%s' %s "
			" ORDER BY hv.scheddname, cluster_id,proc_id;",
			((char *)parameters[0]), schedd_predicate_hh.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_OWNER_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_VER_CUR");
	  }

	  break;
  case HISTORY_COMPLETEDSINCE_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_COMPLETEDSINCE_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History "
			"WHERE completiondate > "
			"('%s'::timestamp with time zone) %s "
			"ORDER BY scheddname, cluster_id,proc_id;",
				quill_pgsql_history_hor_select_list,
				((char *)parameters[0]), schedd_predicate_part.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_COMPLETEDSINCE_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_HOR_CUR");
	  }
		  
	  break;
  case HISTORY_COMPLETEDSINCE_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"SET enable_mergejoin=false; "
			"DECLARE HISTORY_COMPLETEDSINCE_VER_CUR CURSOR FOR SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val FROM "
			"Jobs_Horizontal_History hh, Jobs_Vertical_History hv "
			"WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND "
			"hh.completiondate > "
			"('%s'::timestamp with time zone) %s "
			"ORDER BY hh.scheddname, hh.cluster_id,hh.proc_id;",
			((char *)parameters[0]), schedd_predicate_hh.Value() );
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_COMPLETEDSINCE_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_VER_CUR");
	}

	  break;
  case QUEUE_AVG_TIME:
	  if (dt == T_PGSQL) 
		  sprintf(query_str, quill_avg_time_template_pgsql);
	  break;
	  
  default:
	EXCEPT("Incorrect query type specified\n");
	return -1;
  }
  return 1;
}

void SQLQuery::
Print() 
{
  printf("Query = %s\n", query_str);
  printf("DeclareCursorStmt = %s\n", declare_cursor_str);
  printf("FetchCursorStmt = %s\n", fetch_cursor_str);
  printf("CloseCursorStmt = %s\n", close_cursor_str);
}

#endif
