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

#include <unistd.h>
#include <errno.h>

#define MSSCMD	"msscmd"

int transfer_from_unitree_to_file(char *src_file, char *dest_file)
{

  FILE *pipe = 0;
  char pipecommand[MAXSTR], t[MAXSTR];
  int ret = -1;
  struct stat filestat;
  unsigned long src_filesize = 1, dest_filesize = 0;
  char linebuf[MAXSTR] = "";

  //get the source file size
  snprintf(pipecommand, MAXSTR, "%s ls %s", MSSCMD, src_file);

  pipe = popen (pipecommand, "r");
  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s ls!\n", MSSCMD);
    return DAP_ERROR;
  }

  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }
    if (sscanf(linebuf, "-r%s      1 %s    ac       %s  common    %lu", t, t, t, &src_filesize) > 0){
      break;
    }
  }
  fprintf(stdout, ">source_filesize: %lu\n", src_filesize);

  //get the file
  snprintf(pipecommand, MAXSTR, "%s get %s %s", MSSCMD, src_file, dest_file);

  pipe = popen (pipecommand, "r");
  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s get!\n", MSSCMD);
    return DAP_ERROR;
  }
  /*
  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }
    if (sscanf(linebuf, "%d bytes received in %f seconds", &bytes_sent, &time ) == 2){
      if (bytes_sent == src_filesize){
        ret = 0;
        break;
      }
    }
    fprintf(stdout, ">bytes_sent:%d\n", bytes_sent);
  }
  */

  pclose(pipe);
  
  //check destination filesize
  if (!lstat(dest_file, &filestat)){//if file exist
    fprintf(stdout, ">dest_file:%s, size:%lu\n", dest_file,
			(unsigned long)filestat.st_size);
    dest_filesize = filestat.st_size;
  }

  fprintf(stdout, ">dest_filesize:%lu\n", dest_filesize);

  if (src_filesize == dest_filesize){
    ret = 0;
    printf("* SUCCESS!\n");
  }
  


  fprintf(stdout, "Command terminated with err_code: %d\n", ret);
  
  if (ret == 0) 
    return DAP_SUCCESS;
  else 
    return DAP_ERROR;
}

int transfer_from_file_to_unitree(char *src_file, char *dest_file)
{

  FILE *pipe = 0;
  char pipecommand[MAXSTR];
  int ret = -1;
  struct stat filestat;
  unsigned long src_filesize = 0, dest_filesize = 0;
  float time;
  char linebuf[MAXSTR] = "";
  
  //check source filesize
  if (!lstat(src_file, &filestat)){//if file exist
    src_filesize = filestat.st_size;
  }

  snprintf(pipecommand, MAXSTR, "%s put %s %s", 
  	   MSSCMD, src_file, dest_file);

  pipe = popen (pipecommand, "r");
  if ( pipe == 0 ) {
    fprintf(stderr,"Error:couldn't open pipe to %s put!\n", MSSCMD);
    return -1;
  }

  while ( 1 ) {
    if ( fgets( linebuf, MAXSTR, pipe ) == 0 ) {
      break;
    }

    if (sscanf(linebuf, "%lu bytes sent in %f seconds", &dest_filesize, &time ) == 2){
      if (src_filesize == dest_filesize){
	ret = 0;
	break;
      }
    }
    //    fprintf(stdout, ">%s", linebuf);
  }
  // fprintf(stdout, ">%s", linebuf);

  pclose(pipe);
  fprintf(stdout, "Command terminated with err_code: %d\n", ret);

   
  
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
  char arguments[MAXSTR];
  int status;

  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
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

  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "FROM: %s:%s:%s\n",
	  src_protocol, src_host, src_file );
  fprintf(stdout, "TO  : %s:%s:%s\n",
	  dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "Transfering from: %s to: %s\n", src_url, dest_url);


  if (!strcmp(strip_str(src_protocol), "unitree") 
      && !strcmp(strip_str(dest_protocol), "file")){
    
    status = transfer_from_unitree_to_file(strip_str(src_file), strip_str(dest_file)); 
  }
  else if (!strcmp(strip_str(src_protocol), "file") 
	   && !strcmp(strip_str(dest_protocol), "unitree")){
    
    status = transfer_from_file_to_unitree(strip_str(src_file), strip_str(dest_file)); 
  }
  else{
    fprintf(stderr, "Error: Transfer from %s to %s using this module not supported!\n", 
	    src_protocol, dest_protocol);
    exit(-1);
  }
  

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "SRB error!");
    fclose(f);  
  }
  //--

  return status;
}










