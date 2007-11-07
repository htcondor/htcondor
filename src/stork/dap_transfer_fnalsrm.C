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
#include "dap_classad_reader.h"
#include <unistd.h>

//default value for FNALSRM_BIN_DIR, will be overwritten by stork.config
char FNALSRM_BIN_DIR[MAXSTR] = "/scratch/kosart/srm";

/* ==========================================================================
 * read the stork config file to get FNALSRM_BIN_DIR
 * ==========================================================================*/
void read_config_file()
{
  int fconfig;
  char fnalsrm_bin_dir[MAXSTR];

  char * STORK_CONFIG_FILE = getenv ("STORK_CONFIG_FILE");
  if (STORK_CONFIG_FILE == NULL) {
    fprintf (stderr, "ERROR: STORK_CONFIG_FILE not set!\n");
    exit (1);
  }

  
  ClassAd_Reader configreader(STORK_CONFIG_FILE);
  
  if ( !configreader.readAd()) {
    printf("ERROR in parsing the Stork Config file: %s\n", STORK_CONFIG_FILE);    
    return;
  }

  //get value for GLOBUS_URL_COPY
  if (configreader.getValue("fnalsrm_bin_dir", fnalsrm_bin_dir) == DAP_SUCCESS){
    strncpy(FNALSRM_BIN_DIR, strip_str(fnalsrm_bin_dir), MAXSTR);
  }

  printf("fnalsrm_bin_dir = %s\n", FNALSRM_BIN_DIR);  
}

int transfer_from_to_fnalsrm(char *src_url, char *dest_url, 
			     char *arguments, char *error_str)
{
  FILE *pipe = 0, *outfile;
  char pipecommand[MAXSTR], executable[MAXSTR];
  char linebuf[MAXSTR];
  char status[MAXSTR];
  int ret;
  struct stat filestat;

  snprintf(executable, MAXSTR, "%s/srmcp", FNALSRM_BIN_DIR);

  if (lstat(executable, &filestat)){  //if file does not exists
    fprintf(stderr, "Executable %s not found!\n", executable);
    return DAP_ERROR;
  }

  // Stork server should set X509_USER_PROXY env variable when applicable
  MyString x509proxy;
  char * X509_USER_PROXY = getenv ("X509_USER_PROXY");
  if (X509_USER_PROXY != NULL) {
    x509proxy += " -x509_user_proxy=";
    x509proxy += X509_USER_PROXY;
    free (X509_USER_PROXY);
  }


  snprintf(pipecommand, MAXSTR, "%s %s %s %s %s 2>&1| cat", 
	  executable, arguments, src_url, dest_url, x509proxy.Value());


  pipe = popen (pipecommand, "r");

  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to hrm-get.linux !\n");
    return -1;
  }

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
  
  
  pclose(pipe);

  strncpy(error_str, linebuf, MAXSTR);
  return DAP_ERROR;
}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
  char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
  char error_str[MAXSTR];
  int status;
  
  if (argc < 3){
    fprintf(stdout, "==============================================================\n");
    fprintf(stdout, "USAGE: %s <src_url> <dest_url> [<arguments>]\n", argv[0]);
    fprintf(stdout, "==============================================================\n");
    exit(-1);
  }

  read_config_file();
  
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

  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
  if (!strcmp(src_protocol, "fnalsrm")){
    snprintf(src_url, MAXSTR, "srm://%s%s", src_host, src_file);
  }
  if (!strcmp(dest_protocol, "fnalsrm")){
    snprintf(dest_url, MAXSTR, "srm://%s%s", dest_host, dest_file);
  }
  
  fprintf(stdout, "Transfering from: %s to: %s with arguments: %s\n", 
	  src_url, dest_url, arguments);

  status = transfer_from_to_fnalsrm(src_url, dest_url, arguments, error_str); 

  //-- output to a temporary file....

  
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "FNAL SRM error: %s", error_str);
    fclose(f);  
  }
  //--

  return status;
}







