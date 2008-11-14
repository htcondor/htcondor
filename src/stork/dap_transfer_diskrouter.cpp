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

//default value for DISK_COMAND, will be overwritten by stork.config
char DISK_COMMAND[MAXSTR] = "/p/condor/workspaces/kosart/disk_command-starlight";

/* ==========================================================================
 * read the stork config file to get DISK_COMMAND
 * ==========================================================================*/
void read_config_file()
{
  char disk_command[MAXSTR];
  
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

  //get value for DISK_COMMAND
  if (configreader.getValue("disk_command", disk_command) == DAP_SUCCESS){
    strncpy(DISK_COMMAND, strip_str(disk_command), MAXSTR);
  }

  printf("disk_command = %s\n", DISK_COMMAND);  
}


int transfer_from_diskrouter_to_diskrouter(char *src_host, char *src_file, 
					   char *dest_host, char *dest_file)
{

  FILE *pipe = 0;
  char pipecommand[MAXSTR];
  int ret;
  struct stat filestat;

  if (lstat(DISK_COMMAND,&filestat)){  //if file does not exists
    fprintf(stderr, "Executable %s not found!\n", DISK_COMMAND);
    return DAP_ERROR;
    
  }
  
  snprintf(pipecommand, MAXSTR, 
	   "%s %s %s %s %s", DISK_COMMAND, src_host, src_file, dest_host, dest_file);
  
  
  pipe = popen (pipecommand, "r");
  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to disk_command!\n");
    return DAP_ERROR;
  }
  ret = pclose(pipe);
 
//  ret = system(pipecommand);
  //  fprintf(stdout, "Cammand terminated with err_code: %d\n", ret);
  
  if (ret == 0) 
    return DAP_SUCCESS;
  else 
    return DAP_ERROR;
}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR];
  char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
  int status;

  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  read_config_file();
  
  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "FROM: %s:%s:%s\n",
	  src_protocol, src_host, src_file );
  fprintf(stdout, "TO  : %s:%s:%s\n",
	  dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "Transfering from: %s to: %s\n", src_url, dest_url);

  status = transfer_from_diskrouter_to_diskrouter(strip_str(src_host), 
						  strip_str(src_file),
						  strip_str(dest_host), 
						  strip_str(dest_file)); 

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "Diskrouter error!");
    fclose(f);  
  }
  //--

  return status;
}







