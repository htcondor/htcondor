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
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_utility.h"
#include "MyString.h"

#define SRMCP	"srmcp"

int transfer_from_to_dcache_srm(char *src_url, char *dest_url, 
			     char *arguments, char *error_str)
{
  FILE *pipe = 0;
  char pipecommand[MAXSTR];
  char linebuf[MAXSTR];
  int ret;

  // Stork server should set X509_USER_PROXY env variable when applicable
  MyString x509proxy;
  char * X509_USER_PROXY = getenv ("X509_USER_PROXY");
  if (X509_USER_PROXY != NULL) {
    x509proxy += " -x509_user_proxy=";
    x509proxy += X509_USER_PROXY;
    free (X509_USER_PROXY);
  }

  snprintf(pipecommand, MAXSTR, "%s %s %s %s %s", 
	   SRMCP,
	   arguments, 
	   src_url, 
	   dest_url,
	   x509proxy.Value());

  pipe = popen (pipecommand, "r");

  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s!\n", SRMCP);
    return -1;
  }

  /*
  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }
    if (sscanf(linebuf, "copying ... %s", status ) > 0){
      if (!strcmp(status, "done")){
	return DAP_SUCCESS;
      }
    }
    
    fprintf(stdout, ">%s", linebuf);
  }
  */

  ret = pclose(pipe);
  fprintf(stdout, "Cammand terminated with err_code: %d\n", ret);

  if (ret == 0)
    return DAP_SUCCESS;
  else {
    strncpy(error_str, linebuf, MAXSTR);
    return DAP_ERROR;
  }

  /*
  pclose(pipe);
  strncpy(error_str, linebuf, MAXSTR);
  return DAP_ERROR;
  */

}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
  char error_str[MAXSTR];
  int status;
  
  if (argc < 3){
    fprintf(stdout, "==============================================================\n");
    fprintf(stdout, "USAGE: %s [<arguments>] <src_url> <dest_url> \n", argv[0]);
    fprintf(stdout, "==============================================================\n");
    exit(-1);
  }

  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  strncpy(arguments, "", MAXSTR);
  for(int i=3;i<argc;i++) {
    size_t len = strlen(arguments);
    if(len) {
      strncpy(arguments+len, " ", MAXSTR-len);
      len++;
    }
    strncpy(arguments+len, argv[i], MAXSTR-len); 
  }

  fprintf(stdout, "Transfering from: %s to: %s with arguments: %s\n", 
	  src_url, dest_url, arguments);

  status = transfer_from_to_dcache_srm(src_url, dest_url, arguments, error_str); 


  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "DCACHE SRM error: %s", error_str);
    fclose(f);  
  }
  //--

  return status;
}







