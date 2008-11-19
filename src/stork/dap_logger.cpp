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

/* ============================================================================
 * dap_logger.C - DaP Scheduler Logging Utility
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * 2001-2002
 * ==========================================================================*/

#include "condor_common.h"
#include "dap_constants.h"
#include "dap_logger.h"
#include "dap_error.h"
#include "dap_server_interface.h"
#include "write_user_log.h"
#include "condor_netdb.h"

extern char *clientagenthost;

void write_dap_log(const char *logfilename, const char *status, const char *param1, const char *value1, const char *param2, const char *value2, const char *param3, const char *value3, const char *param4, const char *value4, const char *param5, const char *value5, const char *param6, const char *value6)
{
  
	if (! param_boolean("STORK_HISTORY_LOGGING", true) ) return;

  // -------- ClassAd stuff ------------------------------
  
  //create ClassAd
  classad::ClassAd *classad = new classad::ClassAd;
  classad::ClassAdParser parser;
  classad::ExprTree *expr = NULL;

  //insert attributes to ClassAd
  classad->Insert("timestamp",classad::Literal::MakeAbsTime());

  if ( !parser.ParseExpression(value1, expr) )
    dprintf(D_ALWAYS,"Parse error\n");
  classad->Insert(param1, expr);
  
  if ((param2 != NULL) && (value2 != NULL)){
    if ( !parser.ParseExpression(value2, expr) )
      dprintf(D_ALWAYS,"Parse error\n");
    classad->Insert(param2, expr);
  }

  if ((param3 != NULL) && (value3 != NULL)){
    if ( !parser.ParseExpression(value3, expr) )
      dprintf(D_ALWAYS,"Parse error\n");
    classad->Insert(param3, expr);
  }

  if ((param4 != NULL) && (value4 != NULL)){
    if ( !parser.ParseExpression(value4, expr) )
      dprintf(D_ALWAYS,"Parse error\n");
    classad->Insert(param4, expr);
  }

  if ((param5 != NULL) && (value5 != NULL)){
    if ( !parser.ParseExpression(value5, expr) )
      dprintf(D_ALWAYS,"Parse error\n");
    classad->Insert(param5, expr);
  }

  if ((param6 != NULL) && (value6 != NULL)){
    if ( !parser.ParseExpression(value6, expr) )
      dprintf(D_ALWAYS,"Parse error\n");
    classad->Insert(param6, expr);
  }

  if ( !parser.ParseExpression(status, expr) )
    dprintf(D_ALWAYS,"Parse error\n");
  classad->Insert("status", expr);


  //parse the ClassAd
  classad::PrettyPrint unparser;
  std::string adbuffer = "";
  unparser.Unparse(adbuffer, classad);

  //destroy the ClassAd
  //  delete expr;
  delete classad;

  // -------- End of ClassAd stuff ------------------------------

  FILE *flog;

  //write the classad to classad file
  if ((flog = safe_fopen_wrapper(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open logfile :%s...\n",logfilename);
    exit(1);
  }
  fprintf (flog,"%s",adbuffer.c_str());
  fclose(flog);

}

//--------------------------------------------------------------------------
void write_classad_log(const char *logfilename, const char *status, classad::ClassAd *classad)
{

  classad::ClassAdParser parser;
  classad::ExprTree *expr = NULL;
  classad::PrettyPrint unparser;
  std::string adbuffer = "";
  FILE *flog;

  printf("Logging the classad..\n");

  //insert attributes to ClassAd
  classad->Insert("timestamp",classad::Literal::MakeAbsTime());

  printf("*1*\n");

  if ( !parser.ParseExpression(status, expr) )
    dprintf(D_ALWAYS,"Parse error\n");
  classad->Insert("status", expr);

  printf("*2*\n");
  /*
  if (!(expr = classad->Lookup("src_url")) ){
    printf("%s not found!\n", "src_url");
    return;
  }
  unparser.Unparse(adbuffer,expr);
  printf("VALUE => %s\n",adbuffer.c_str());
  */

  //parse the ClassAd
  unparser.Unparse(adbuffer, classad);
  
  printf("*3*\n");

  //write the classad to classad file
  if ((flog = safe_fopen_wrapper(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open logfile :%s...\n",logfilename);
    exit(1);
  }

  printf("*4*\n");

  fprintf (flog,"%s",adbuffer.c_str());

  printf("*5*\n");

  fclose(flog);
}

//--------------------------------------------------------------------------
void write_collection_log(classad::ClassAdCollection *dapcollection, const char *dap_id, 
			  const char *update)
{
  //log the new status of the request
  classad::ClassAdParser parser;
  bool status;
  std::string key;
  key = "key = ";
  key += dap_id;
  
  std::string partial_s = "[ ";
  partial_s += update;
  partial_s += " ]";
  
  classad::ClassAd* partialAd = parser.ParseClassAd(partial_s, true);
  if (partialAd == NULL) {
	dprintf(D_ALWAYS, "ParseClassAd partial %s failed! dap_id:%s\n",
		partial_s.c_str(), dap_id);
	return;
  }
  status = partialAd->Insert("timestamp",classad::Literal::MakeAbsTime());
  if (status == false) {
	  	delete partialAd;
		dprintf(D_ALWAYS,
			"ParseClassAd partial %s insert timestamp failed! dap_id:%s\n",
			partial_s.c_str(), dap_id);
	  return;
  }
    
  classad::PrettyPrint unparser;
  std::string adbuffer = "";
  unparser.Unparse(adbuffer, partialAd);

  std::string modify_s ="[ Updates  = ";
  modify_s += adbuffer;
  modify_s += " ]";
  
  classad::ClassAd* tmpad = parser.ParseClassAd(modify_s, true);
  if (tmpad == NULL) {
	  delete partialAd;
	  dprintf(D_ALWAYS, "ParseClassAd modify %s failed! dap_id:%s\n",
			partial_s.c_str(), dap_id);
	  return;
  }

  unparser.Unparse(adbuffer, tmpad);
		  

  if (!dapcollection->ModifyClassAd(key, tmpad)){ // deletes ModifyClassAd()
    dprintf(D_ALWAYS, "ModifyClassAd failed! dap_id:%s\n", dap_id);
  }

  delete partialAd;
  //delete tmpad;	// delete'ed by ModifyClassAd()
  return;
}
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
void write_xml_log(const char *logfilename, classad::ClassAd *classad, const char *status)
{

  classad::ClassAdParser parser;
  classad::ExprTree *expr = NULL;
  classad::ClassAdXMLUnParser  xmlunparser;
  std::string adbuffer = "";
  FILE *flog;

  //insert attributes to ClassAd
  classad->Insert("timestamp",classad::Literal::MakeAbsTime());

  //update status
  classad->Delete("status");
  if ( !parser.ParseExpression(status, expr) )
    dprintf(D_ALWAYS,"Parse error\n");
  classad->Insert("status", expr);

  if (!strcmp(status, "\"processing_request\"") || 
      !strcmp(status, "\"request_rescheduled\"")){
    classad->Delete("error_code");
  }
  
  //convert classad to XML
  xmlunparser.SetCompactSpacing(false);
  xmlunparser.Unparse(adbuffer, classad);
  
  //write the classad to classad file
  if ((flog = safe_fopen_wrapper(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open logfile :%s...\n",logfilename);
    exit(1);
  }
  fprintf (flog,"%s",adbuffer.c_str());
  fclose(flog);
}

//--------------------------------------------------------------------------
int send_log_to_client_agent(const char *log)
{
  struct sockaddr_in cli_addr; 
  struct hostent* client;
  int dap_conn;

  if ( (client = condor_gethostbyname(clientagenthost)) == NULL ){
    fprintf(stderr, "Cannot get address for host %s\n",clientagenthost);
    return DAP_ERROR;
    //exit(1);	
  }
  
  if ( (dap_conn = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
    fprintf(stderr, "Stork server: cannot open socket to client agent!\n");
    return DAP_ERROR;
    //exit(1);
  }

  bzero(&cli_addr, sizeof(cli_addr)); 
  cli_addr.sin_family      = AF_INET;		
  memcpy(&(cli_addr.sin_addr.s_addr), client->h_addr, client->h_length);  
  cli_addr.sin_port        = htons(CLI_AGENT_LOG_TCP_PORT);

  /*
  hostIP = (char *)malloc(16*sizeof(char));
  hostIP = (char *)inet_ntoa(cli_addr.sin_addr);
  fprintf(stdout, "Trying %s..\n",hostIP);
  free(hostIP);
  */

  if ( connect(dap_conn, (struct sockaddr*) &cli_addr, sizeof(cli_addr)) != 0){
    fprintf(stderr, "Stork server:cannot connect to client agent @%s!\n", clientagenthost);
    return DAP_ERROR;
    //exit(1);
  }
  
  //  fprintf(stdout, "Connected to %s..\n", clientagenthost);

  
  
  //  printf("sending log to client agent: %s\n%s\n", clientagenthost, log);

  if ( send (dap_conn, log, strlen(log), 0) == -1){
    fprintf(stderr, "Stork server: send error!\n");
    return DAP_ERROR;
    //    exit(1);
  }

  if ( close(dap_conn) != 0){
    fprintf(stderr, "Error in closing the connection!\n");
    return DAP_ERROR;
    //    exit(1);
  }

  //fprintf(stdout, "Connection closed..\n");
  return DAP_SUCCESS;

}

//--------------------------------------------------------------------------
void
write_xml_user_log(
		const char *logfilename,
		const char *param1, const char *value1,
		const char *param2, const char *value2,
		const char *param3, const char *value3,
		const char *param4, const char *value4,
		const char *param5, const char *value5,
		const char *param6, const char *value6,
		const char *param7, const char *value7,
		const char *param8, const char *value8,
		const char *param9, const char *value9,
		const char *param10, const char *value10
)
{
  if (! logfilename ) return;

	// this function has been replaced by user_log() below.  However, disable
	// this function, but keep it around for a release or two, as this is
	// significant API change.
	if ( ! param_boolean("STORK_ENABLE_DEPRECATED_USERLOG", false) ) {
		return;
	}

  classad::ClassAdParser parser;
  classad::ExprTree *expr = NULL;
  classad::ClassAdXMLUnParser  xmlunparser;
  std::string adbuffer = "";
  FILE *flog;


  //create ClassAd
  classad::ClassAd *classad = new classad::ClassAd;
  ASSERT(classad);

  //insert attributes to ClassAd
  classad->Insert("EventTime",classad::Literal::MakeAbsTime());

  if ( parser.ParseExpression(value1, expr) )
    classad->Insert(param1, expr);

   if ((param2 != NULL) && (value2 != NULL)){
    if ( parser.ParseExpression(value2, expr) )
      classad->Insert(param2, expr);
  }

  if ((param3 != NULL) && (value3 != NULL)){
    if ( parser.ParseExpression(value3, expr) )
      classad->Insert(param3, expr);
  }

  if ((param4 != NULL) && (value4 != NULL)){
    if ( parser.ParseExpression(value4, expr) )
      classad->Insert(param4, expr);
  }

  if ((param5 != NULL) && (value5 != NULL)){
    if ( parser.ParseExpression(value5, expr) )
      classad->Insert(param5, expr);
  }

  if ((param6 != NULL) && (value6 != NULL)){
    if ( parser.ParseExpression(value6, expr) )
      classad->Insert(param6, expr);
  }

  if ((param7 != NULL) && (value7 != NULL)){
    if ( parser.ParseExpression(value7, expr) )
      classad->Insert(param7, expr);
  }
  
  if ((param8 != NULL) && (value8 != NULL)){
    if ( parser.ParseExpression(value8, expr) )
      classad->Insert(param8, expr);
  }

  if ((param9 != NULL) && (value9 != NULL)){
    if ( parser.ParseExpression(value9, expr) )
      classad->Insert(param9, expr);
  }

  if ((param10 != NULL) && (value10 != NULL)){
    if ( parser.ParseExpression(value10, expr) )
      classad->Insert(param10, expr);
  }

  //convert classad to XML
  xmlunparser.SetCompactSpacing(false);
  xmlunparser.Unparse(adbuffer, classad);

  //destroy the ClassAd
  //  delete expr;
  delete classad;

  //write the classad buffer to classad file
  if ((flog = safe_fopen_wrapper(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open user logfile :%s...\n",logfilename);
	return;
  }
  fprintf (flog,"%s",adbuffer.c_str());

  if (clientagenthost != NULL){
    send_log_to_client_agent(adbuffer.c_str());
  }
      
  fclose(flog);
  return;
}

bool
user_log_submit(	const classad::ClassAd *ad,
					UserLog& user_log)
{
	SubmitEvent jobSubmit;
	std::string logNotes;

	std::string dap_id;
	ad->EvaluateAttrString("dap_id", dap_id);

	if	(	ad->EvaluateAttrString("LogNotes", logNotes) &&
			!logNotes.empty()
		)
	{
		// Ugh.  The SubmitEvent destructor will
		// delete[] submitEventLogNotes, so "new" one up here.
		jobSubmit.submitEventLogNotes = strnewp( logNotes.c_str() );
	}

	std::string submit_host;
	ad->EvaluateAttrString("submit_host", submit_host);
	strncpy(jobSubmit.submitHost, submit_host.c_str(),
			sizeof(jobSubmit.submitHost) - 1 );
	jobSubmit.submitHost[ sizeof(jobSubmit.submitHost) - 1 ] = '\0';

	if ( ! user_log.writeEvent(&jobSubmit) ) {
		dprintf(D_ALWAYS, "ERROR: Failed to log job %s submit event.\n",
				dap_id.c_str() );
		return false;
	}

	return true;
}

bool
user_log_execute(	const classad::ClassAd *ad,
					UserLog& user_log)
{
	ExecuteEvent event;

	std::string dap_id;
	ad->EvaluateAttrString("dap_id", dap_id);

	std::string execute_host;
	ad->EvaluateAttrString("execute_host", execute_host);
	strncpy(event.executeHost, execute_host.c_str(),
			sizeof(event.executeHost) - 1 );
	event.executeHost[ sizeof(event.executeHost) - 1 ] = '\0';

	if ( ! user_log.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "ERROR: Failed to log job %s execute event.\n",
				dap_id.c_str() );
		return false;
	}

	return true;
}

bool
user_log_generic(	const classad::ClassAd *ad,
					UserLog& user_log)
{
	GenericEvent event;

	std::string dap_id;
	ad->EvaluateAttrString("dap_id", dap_id);

	std::string generic_event;
	ad->EvaluateAttrString("generic_event", generic_event);
	event.setInfoText( generic_event.c_str() );

	if ( ! user_log.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "ERROR: Failed to log job %s generic event.\n",
				dap_id.c_str() );
		return false;
	}

	return true;
}

bool
user_log_terminated(	const classad::ClassAd *ad,
						UserLog& user_log)
{
	JobTerminatedEvent event;
	std::string termination_type;

	std::string dap_id;
	ad->EvaluateAttrString("dap_id", dap_id);

	if	(	! ad->EvaluateAttrString("termination_type", termination_type) ||
			termination_type.empty() )
	{
		dprintf(D_ALWAYS, "job %s has no termination type\n", dap_id.c_str() );
		return false;
	}

	if (termination_type == "job_retry_limit" ||
		termination_type == "server_error") {
		// Stork removed the job before it could exit.  exit_status has no
		// useful information.
		event.normal = false;
	} else if (termination_type == "exited") {
		// The job exited.  Parse the exit status.
		int exit_status;
		if (! ad->EvaluateAttrInt("exit_status", exit_status) ) {
			dprintf(D_ALWAYS, "job %s exit_status not found\n",
					dap_id.c_str() );
			return false;
		} else if ( WIFEXITED( exit_status) ) {
			// Stork requires all successful jobs to exit with status 0
			event.normal = (exit_status == 0) ? true : false;
			event.returnValue = WEXITSTATUS( exit_status );
		} else if ( WIFSIGNALED( exit_status) ) {
			event.normal = false;
			event.signalNumber = WTERMSIG( exit_status );
		} else {
			dprintf(D_ALWAYS, "job %s exit_status %d unknown\n",
					dap_id.c_str(), exit_status );
			return false;
		}
	} else {
		dprintf(D_ALWAYS, "job %s unknown termination type: %s\n",
				dap_id.c_str(), termination_type.c_str() );
		return false;
	}

	if ( ! user_log.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "ERROR: Failed to log job %s terminated event.\n",
				dap_id.c_str() );
		return false;
	}

	return true;
}

bool
user_log_aborted(	const classad::ClassAd *ad,
						UserLog& user_log)
{
	JobAbortedEvent event;

	std::string dap_id;
	ad->EvaluateAttrString("dap_id", dap_id);

	if ( ! user_log.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "ERROR: Failed to log job %s aborted event.\n",
				dap_id.c_str() );
		return false;
	}

	return true;
}

bool
user_log(			const classad::ClassAd *ad,
					const enum ULogEventNumber eventNum)
{
	std::string userLogFile;
	int cluster_id, proc_id;
	bool log_xml;
	UserLog usr_log;

	if	(	!ad->EvaluateAttrString("log", userLogFile) ||
			userLogFile.empty()
		)
	{
		// No valid user log file specified.
		return true;
	}

	// Get job identifiers
	ad->EvaluateAttrInt("cluster_id",	cluster_id);
	ad->EvaluateAttrInt("proc_id",		proc_id);

	if	(	!ad->EvaluateAttrBool("log_xml", log_xml) ) {
		log_xml = false;	// Default: do not log in XML format
	}
	usr_log.setUseXML(log_xml);

	std::string owner;
	if ( ! ad->EvaluateAttrString("owner", owner) || owner.empty() ) {
		dprintf(D_ALWAYS,
				"unable to extract owner to log job %d user log event %d\n",
				cluster_id, eventNum);
		return false;
	}
	if (! usr_log.initialize(
				owner.c_str(),
				NULL,					// domain TODO: fix for WIN32
				userLogFile.c_str(),
				cluster_id,
				proc_id,
				-1,
				NULL)
		)
	{
		dprintf(D_ALWAYS,
				"error initializing user log event %d for job %d\n",
				eventNum, cluster_id);
		return false;
	}

	switch(eventNum) {
		case ULOG_SUBMIT:
			return user_log_submit(ad, usr_log);
			break;

		case ULOG_EXECUTE:
			return user_log_execute(ad, usr_log);
			break;

		case ULOG_JOB_TERMINATED:
			return user_log_terminated(ad, usr_log);
			break;

		case ULOG_JOB_ABORTED:
			return user_log_aborted(ad, usr_log);
			break;

		case ULOG_GENERIC:
			return user_log_generic(ad, usr_log);
			break;

		default:
			dprintf(D_ALWAYS, "job %d write user log for unknown event %d\n",
					cluster_id, eventNum);
			return false;
	}

	return true;
}

