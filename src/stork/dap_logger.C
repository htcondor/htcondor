/* ============================================================================
 * dap_logger.C - DaP Scheduler Logging Utility
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * 2001-2002
 * ==========================================================================*/

#include "dap_constants.h"
#include "dap_logger.h"
#include "dap_error.h"
#include "dap_server_interface.h"

extern char *clientagenthost;

void write_dap_log(char *logfilename, char *status, char *param1, char *value1, char *param2, char *value2, char *param3, char *value3, char *param4, char *value4, char *param5, char *value5, char *param6, char *value6)
{
  
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
  if ((flog = fopen(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open logfile :%s...\n",logfilename);
    exit(1);
  }
  fprintf (flog,"%s",adbuffer.c_str());
  fclose(flog);

}

//--------------------------------------------------------------------------
void write_classad_log(char *logfilename, char *status, classad::ClassAd *classad)
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
  if ((flog = fopen(logfilename,"a+")) == NULL){
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
void write_collection_log(classad::ClassAdCollection *dapcollection, char *dap_id, 
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
void write_xml_log(char *logfilename, classad::ClassAd *classad, const char *status)
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
  if ((flog = fopen(logfilename,"a+")) == NULL){
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

  if ( (client = gethostbyname(clientagenthost)) == NULL ){
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
		char *logfilename,
		char *param1, char *value1,
		char *param2, char *value2,
		char *param3, char *value3,
		char *param4, char *value4,
		char *param5, char *value5,
		char *param6, char *value6,
		char *param7, char *value7,
		char *param8, char *value8,
		char *param9, char *value9,
		char *param10, char *value10
)
{

  classad::ClassAdParser parser;
  classad::ExprTree *expr = NULL;
  classad::ClassAdXMLUnParser  xmlunparser;
  std::string adbuffer = "";
  FILE *flog;

  //create ClassAd
  classad::ClassAd *classad = new classad::ClassAd;

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
  
  //write the classad to classad file
  if ((flog = fopen(logfilename,"a+")) == NULL){
    dprintf(D_ALWAYS,
	    "cannot open logfile :%s...\n",logfilename);
    exit(1);
  }
  fprintf (flog,"%s",adbuffer.c_str());

  if (clientagenthost != NULL){
    send_log_to_client_agent(adbuffer.c_str());
  }
      
  fclose(flog);
}












