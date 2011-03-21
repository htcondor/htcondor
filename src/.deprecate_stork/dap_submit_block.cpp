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

#include "dap_constants.h"
#include "condor_common.h"
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

int check_dap_format(classad::ClassAd *currentAd)
{
  char dap_type[MAXSTR];
  std::string adbuffer = "";
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;

  //should be context insensitive..
  //just check the format, not the content!
  //except dap_type ??
  
  if ( !(attrexpr = currentAd->Lookup("dap_type")) ) return 0;
  else{
    unparser.Unparse(adbuffer,attrexpr);

    char tmp_adbuffer[MAXSTR];
    strncpy(tmp_adbuffer, adbuffer.c_str(), MAXSTR);
    strncpy(dap_type,strip_str(tmp_adbuffer), MAXSTR);

    if ( !strcmp(dap_type, "transfer") ){
      if ( !currentAd->Lookup("src_url") )  return 0;
      if ( !currentAd->Lookup("dest_url") ) return 0;
    }
    
    else if ( !strcmp(dap_type, "reserve") ){
      if ( !currentAd->Lookup("dest_host") )    return 0;
      if ( !currentAd->Lookup("reserve_size") ) return 0;
      if ( !currentAd->Lookup("duration") )     return 0;
      if ( !currentAd->Lookup("reserve_id") )   return 0;
    }

    else if ( !strcmp(dap_type, "release") ){
      if ( !currentAd->Lookup("dest_host") ) return 0;
      if ( !currentAd->Lookup("reserve_id") ) return 0;
    }
    
    else return 0;
  }
  
  
  return 1;
}

/* ============================================================================
 * main body of dap_submit
 * ==========================================================================*/
int main(int argc, char **argv)
{
  char c;
  std::string adstr="";
  classad::ClassAdParser parser;
  classad::ClassAd *currentAd = NULL;
  int leftparan = 0;
  FILE *adfile;
  char fname[MAXSTR], hostname[MAXSTR];
  DapConnection dap_conn;
  long dap_id;

  //check number of arguments
  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <host_name> <submit_file>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit( 1 );
  }


  strncpy(hostname, argv[1], MAXSTR);
  strncpy(fname, argv[2], MAXSTR);
  

  //open the submit file
  adfile = safe_fopen_wrapper(fname,"r");
  if (adfile == NULL) {
    fprintf(stderr, "Cannot open file:%s\n",fname);
    exit(1);
  }

  //open the connection to the DaP server
  dap_open_connection(dap_conn, argv[1]);
  
  //read the request classAds

  while (1){
    c = fgetc(adfile);
    if (c == ']'){ 
      leftparan --; 
      if (leftparan == 0) break;
    }
    if (c == '[') leftparan ++; 
    
    if (c == EOF) {
      dap_close_connection(dap_conn); 
      return 0;
    }
    
    adstr += c;
  }
  adstr += c;
  
  //check the validity of the request
  currentAd = parser.ParseClassAd(adstr);
  if (currentAd == NULL) {
    fprintf(stderr, "Invalid input format! Not a valid classad!\n");
    dap_close_connection(dap_conn);    
    return 0;
  }
  
  if ( !check_dap_format(currentAd)){
    fprintf(stderr, "========================\n");
    fprintf(stderr, "Not a valid DaP request!\nPlease check your submit file and then resubmit...\n");
    fprintf(stderr, "========================\n");
    dap_close_connection(dap_conn);    
    return 0;
  }
  
  //if input is valid, then send the request:
  
  fprintf(stdout, "Sending request:\n %s\n", adstr.c_str());
  
  char tmp_adstr[MAXSTR];
  strncpy(tmp_adstr, adstr.c_str(), MAXSTR);
  dap_id = dap_send_request(dap_conn, tmp_adstr);
  fprintf(stdout, "Executing ......\n");

  //close the connection to the server
  dap_close_connection(dap_conn);    

  //check the status of the requests regularly..
  FILE *pipe = 0;
  char pipecommand[MAXSTR];
  char linebuf[MAXSTR];
  char status[MAXSTR], errorcode[MAXSTR];
  
  snprintf(pipecommand, MAXSTR, "dap_status %s %d", hostname, dap_id);
  
  bool can_exit = false;

  while(1){
    pipe = popen (pipecommand, "r");
    if ( pipe == 0 ) {
      fprintf(stderr,"Error:couldn't open pipe to dap_status!\n");
      return -1;
    }
    
    while ( 1 ) {
      if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
	break;
      }

      if (sscanf(linebuf, "status = %s;", status) > 0){
	if (!strcmp(status, "request_completed;")){
	  can_exit = true;
	  fprintf(stdout, "*** DaP request completed successfully!\n");
	  exit(0);
	}
	else if (!strcmp(status, "request_failed;")){
	  can_exit = true;
	  fprintf(stdout, "*** DaP request failed!\n");
	  exit(1);
	}
      }
    }//while (1)
    
    pclose(pipe);
    sleep(3);

    if (can_exit) break;
  }//while (1)
  
}











