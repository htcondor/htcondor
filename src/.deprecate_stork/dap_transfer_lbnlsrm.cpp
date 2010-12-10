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

//default value for LBNLSRM_BIN_DIR, will be overwritten by stork.config
char LBNLSRM_BIN_DIR[MAXSTR] = "/unsup/srm-1.1/FNAL/bin";

/* ==========================================================================
 * read the stork config file to get LBNLSRM_BIN_DIR
 * ==========================================================================*/
void read_config_file()
{
  char lbnlsrm_bin_dir[MAXSTR];


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
  if (configreader.getValue("lbnlsrm_bin_dir", lbnlsrm_bin_dir) == DAP_SUCCESS){
    strncpy(LBNLSRM_BIN_DIR, strip_str(lbnlsrm_bin_dir), MAXSTR);
  }

  printf("lbnlsrm_bin_dir = %s\n", LBNLSRM_BIN_DIR);  
}


int transfer_from_srm_to_srm(char *src_url, char *dest_url, char *arguments, 
			     char *error_str)
{

  std::string cc;
  
  FILE *pipe = 0;
  char pipecommand[MAXSTR], executable[MAXSTR];
  char linebuf[MAXSTR];
  int ret;
  struct stat filestat;

  snprintf(executable, MAXSTR, "%s/hrm-get.linux", LBNLSRM_BIN_DIR);

  if (lstat(executable, &filestat)){  //if file does not exists
    fprintf(stderr, "Executable %s not found!\n", executable);
    return DAP_ERROR;
  }

  snprintf(pipecommand, MAXSTR, 
	   "%s %s -s %s -t %s -b 0 -c %s/hrm.rc", 
	  executable, arguments, src_url, dest_url, LBNLSRM_BIN_DIR);
  

  pipe = popen (pipecommand, "r");
  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s!\n", executable);
    return -1;
  }

  /*
  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }

    if (sscanf(linebuf, "HRM_GRIDFTP: file transfer completed from %s to %s",
	       cacheurl, targeturl ) > 0){
      if (!strcmp(targeturl, dest_url)){
	return DAP_SUCCESS;
      }
    }

    //fprintf(stdout, ">%s", linebuf);
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
  
}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
  int status;
  char error_str[MAXSTR];

  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url> [<arguments>]\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
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

  fprintf(stdout, "Transfering from: %s to: %s\n", src_url, dest_url);

  status = transfer_from_srm_to_srm(src_url, dest_url , arguments, error_str); 

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "SRM error: %s", error_str);
    fclose(f);  
  }
  //--

  return status;
}







