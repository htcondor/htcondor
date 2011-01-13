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
#include <unistd.h>
#include "config-ibp.h"
#include "ibp_ClientLib.h"
#include "dap_c_utility.h"

/* ============================================================================
 * Transfer a file from one depot to another
 * ==========================================================================*/
int transfer_from_capability_to_capability(char *src_cap, char *dest_cap,
					   unsigned long offset,
					   unsigned long size)
{
  int  li_ret;
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_port[MAXSTR], dest_file[MAXSTR];
  struct ibp_timer ls_timeout;
  struct ibp_attributes ls_att;
  struct ibp_depot ls_depot;
  IBP_set_of_caps ls_caps;
  time_t ls_now;


  //set the timeout, not used anyway
  ls_timeout.ServerSync = 10;
  ls_timeout.ClientTimeout = 10;

  parse_url_with_port(dest_cap, dest_protocol, dest_host, dest_port, dest_file);

  if (!strcmp(dest_file, "")) { // no pre-allocated capability

    strcpy (ls_depot.host, dest_host);
    ls_depot.port =  atoi(dest_port);
    
    //initialize
    time(&ls_now);
    ls_att.duration=( ls_now + 24*3600);
    ls_att.reliability=IBP_STABLE;
    ls_att.type=IBP_BYTEARRAY;

    if (size == 0) size = 1024*1024;
        
    //allocate space
    ls_caps = IBP_allocate(&ls_depot, &ls_timeout, size, &ls_att);
    if ((ls_caps == NULL) ) {
      fprintf(stderr,"allocation failed! %d\n", IBP_errno);
      return DAP_ERROR;
    }
    
    //write the read & write capabilities to stdout
    fprintf (stdout, "readCap->%s", ls_caps->readCap);
    fprintf (stdout, "writeCap->%s", ls_caps->writeCap);

    li_ret = IBP_copy(src_cap, ls_caps->writeCap, &ls_timeout, &ls_timeout, size, offset);
    if (li_ret <= 0) {
      fprintf(stderr, "Copy failed! %d\n",IBP_errno);
      return DAP_ERROR;
    }
  }
  else{ //if space already allocated before
    li_ret = IBP_copy(src_cap, dest_cap, &ls_timeout, &ls_timeout, size, offset);
    if (li_ret <= 0) {
      fprintf(stderr, "Copy failed! %d\n",IBP_errno);
      return DAP_ERROR;
    }
  }
  
  return DAP_SUCCESS;
}

/* ============================================================================
 * Transfer a local file to a depot
 * ==========================================================================*/
int transfer_from_file_to_capability(char *src_file, char *dest_url,
					   unsigned long offset,
					   unsigned long size)
{
  char *buf;
  int in_fd, bytes_read, li_ret = 0;
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_port[MAXSTR], dest_file[MAXSTR];
  char tempstr[MAXSTR];
  FILE *fcap;
  
  struct ibp_timer ls_timeout;
  struct ibp_attributes ls_att;
  struct ibp_depot ls_depot;
  IBP_set_of_caps ls_caps;
  time_t ls_now;

  struct stat filestat;
  long src_filesize;
  
  //open the input file for reading
  in_fd = safe_open_wrapper(src_file, O_RDONLY);
  if (in_fd < 0){
    fprintf(stdout,"Error in opening file : %s\n",src_file);
    return DAP_ERROR;
  }
  
  buf = (char*)malloc(sizeof(char) * BUF_SIZE);

  parse_url_with_port(dest_url, dest_protocol, dest_host, dest_port, dest_file);
  //printf("dest_host:%s, dest_port:%s\n", dest_host, dest_port);
  
  ls_timeout.ServerSync = 10;
  ls_timeout.ClientTimeout = 10;

  strcpy (ls_depot.host, dest_host);
  ls_depot.port =  atoi(dest_port);

  //get source file size info
  if (!lstat(src_file, &filestat))  //if file exists
    src_filesize = filestat.st_size;

  if (!strcmp(dest_file, "")) { // no pre-allocated capability

    //initialize
    time(&ls_now);
    ls_att.duration=( ls_now + 24*3600);
    ls_att.reliability=IBP_STABLE;
    ls_att.type=IBP_BYTEARRAY;
    
    //allocate space
    ls_caps = IBP_allocate(&ls_depot, &ls_timeout, src_filesize, &ls_att);
    if ((ls_caps == NULL) ) {
      fprintf(stderr,"allocation failed! %d\n", IBP_errno);
      return DAP_ERROR;
    }
    
    //write the read capability to a file for future reference
    sprintf(tempstr, "%s.readCap",src_file);
    fcap = safe_fopen_wrapper(tempstr, "w");
    fprintf (fcap, "%s", ls_caps->readCap);
    fclose(fcap);

    while ( (bytes_read = read(in_fd, buf, BUF_SIZE)) > 0){
      li_ret = IBP_store(ls_caps->writeCap, &ls_timeout, buf, bytes_read);
      if (li_ret <= 0) {
	fprintf(stderr, "store failed! %d\n",IBP_errno);
	free(buf);
	close(in_fd);
	return DAP_ERROR;
      }
    }
    
  }
  else{ //if space already allocated before
    while ( (bytes_read = read(in_fd, buf, BUF_SIZE)) > 0){
      li_ret = IBP_store(dest_url, &ls_timeout, buf, bytes_read);
      if (li_ret <= 0) {
	fprintf(stderr, "store failed! %d\n",IBP_errno);
	free(buf);
	close(in_fd);
	return DAP_ERROR;
      }
    }
  }
  
  
  free(buf);
  close(in_fd);

  if (li_ret == 0) 
    return DAP_ERROR;
  else
    return DAP_SUCCESS;
}

/* ============================================================================
 * Transfer from a depot to a local file 
 * ==========================================================================*/
int transfer_from_capability_to_file(char *src_url, char *dest_file,
				     unsigned long offset,
				     unsigned long size)
{
  char *buf;
  int i, out_fd, bytes_written = 0, li_ret;
  struct ibp_timer ls_timeout;

  //open the output file for writing
  out_fd = safe_open_wrapper(dest_file, O_CREAT | O_RDWR, 00777);
  if (out_fd < 0){
    fprintf(stdout,"Error in opening file : %s\n",dest_file);
    return DAP_ERROR;
  }

  buf = (char*)malloc(sizeof(char) * BUF_SIZE);

  ls_timeout.ServerSync = 10;
  ls_timeout.ClientTimeout = 10;

  i = offset;
  while( (li_ret = IBP_load(src_url, &ls_timeout, buf, BUF_SIZE, i)) > 0){

    bytes_written = write(out_fd, buf, li_ret);
    i += li_ret;

    if (bytes_written <= 0) {
      fprintf(stderr, "writing to file %s failed!\n", dest_file);
      free(buf);
      close(out_fd);
      return DAP_ERROR;
    }
  }

  free(buf);
  close(out_fd);

  if (bytes_written == 0) 
    return DAP_ERROR;
  else 
    return DAP_SUCCESS;
}
//-------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  char src_protocol[MAXSTR], src_host[MAXSTR], src_port[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_port[MAXSTR], dest_file[MAXSTR];
  char src_url[MAXSTR], dest_url[MAXSTR];
  unsigned long size = 0, offset = 0;
  int status;
  int i;

  
  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url> [arguments]\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);

  fprintf(stdout, "Transfering from %s to %s\n", src_url, dest_url);
  
  
  for (i = 1; i < argc; i++) {
        if (!strcmp("-size", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                fprintf(stderr, "size not specified\n");
            }
            size = (unsigned long) atol (argv[i]);
        } else if (!strcmp("-offset", argv[i])) {
            i++;
            if( argc <= i || strcmp( argv[i], "" ) == 0 ) {
                fprintf(stderr, "offset not specified\n");
           }
            offset = (unsigned long) atol (argv[i]);
	}
	
  }
  

  parse_url_with_port(src_url, src_protocol, src_host, src_port, src_file);
  parse_url_with_port(dest_url, dest_protocol, dest_host, dest_port, dest_file);

  printf("src_protocol=%s, src_host=%s, src_port=%s, src_file=%s\n",
	 src_protocol, src_host, src_port, src_file);

  printf("dest_protocol=%s, dest_host=%s, dest_port=%s, dest_file=%s\n",
	 dest_protocol, dest_host, dest_port, dest_file);
  
  
  if ( !strcmp(src_protocol, "ibp") && !strcmp(dest_protocol, "ibp")){
    status = transfer_from_capability_to_capability(src_url, dest_url,
						    offset, size);
  }
  else if ( !strcmp(src_protocol, "file") && !strcmp(dest_protocol, "ibp")){
    status = transfer_from_file_to_capability(src_file, dest_url, offset, size);
  }
  else if ( !strcmp(src_protocol, "ibp") && !strcmp(dest_protocol, "file")){
    status = transfer_from_capability_to_file(src_url, dest_file, offset, size);
  }

  return status;
  
}







