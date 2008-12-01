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
#include "dap_utility.h"
#include "dap_error.h"
#include "dap_logger.h"
#include <unistd.h>

/* ============================================================================
 * Transfer a file from local disk to local disk
 * ==========================================================================*/
int transfer_from_file_to_file(char *src_file, char *dest_file)
{
    int in_fd, out_fd;
    char *buf;
    long src_filesize, dest_filesize;
    int bytes_read, bytes_written;
    struct stat src_filestat, dest_filestat;

    //start transfer
    buf = (char*)malloc(sizeof(char) * BUF_SIZE);
    
    //get the src filesize info
    if (!lstat(src_file, &src_filestat))  //if file exists
      src_filesize = src_filestat.st_size;

    //open the input file for reading
    in_fd = safe_open_wrapper(src_file, O_RDONLY);
    if (in_fd < 0){
      fprintf(stdout,"Error in opening file : %s\n",src_file);
      free(buf);
      return DAP_ERROR;
    }
    
    //get dest directory name
    char tmpstr[MAXSTR], dir[MAXSTR], dest_fname[MAXSTR];
    char *t = NULL, *p = NULL;

    strcpy(tmpstr, dest_file);
    strcpy(dir, "");

    t = strtok(tmpstr,"/");
    while(1){
      if (t == NULL) break;
      if (p!= NULL) strcat(strcat(dir, "/"), p);
      if ((p = strtok(NULL,"/")) != NULL){
	strcat(strcat(dir, "/"), t);   //get file name
      }
      else {
	strcpy(dest_fname, t);
	break;
      }
      t = strtok(NULL,"/");
    }

    if (t == NULL){
      if (p != NULL) 
	strcpy(dest_fname, p);
      else
	strcpy(dest_fname, "");
    }
    
    if (!strcmp(dir, "")) 
      strcpy (dir,"/");
    
    //check if dest directory exists, if not create
    struct stat dest_dirstat;
    if (lstat(dir, &dest_dirstat)){ //if dest dir does not exist
      char command[MAXSTR];
      
      snprintf(command, MAXSTR, "mkdir -p %s", dir);
      if (system(command) == -1){
	fprintf(stdout,"Error in creating directory : %s\n",dir);
	return DAP_ERROR;
      }
    }

    //open the output file for writing
    out_fd = safe_open_wrapper(dest_file, O_CREAT | O_RDWR, 00777);
    if (out_fd < 0){
      fprintf(stdout,"Error in opening file : %s\n",dest_file);
      free(buf);
      close(in_fd);
      return DAP_ERROR;
    }

    //transfer file
    while ( (bytes_read = read(in_fd, buf, BUF_SIZE)) > 0){
      bytes_written = write(out_fd, buf, bytes_read);
      
      if (bytes_written != bytes_read){
	fprintf(stdout,"Error! read = %d, written = %d\n",bytes_read, bytes_written);
	free(buf);
	close(in_fd);
	close(out_fd);
	return DAP_ERROR;
      }
    }

    //get the dest filesize info
    if (!lstat(dest_file, &dest_filestat))  //if file exists
      dest_filesize = dest_filestat.st_size;

    if (src_filesize != dest_filesize){
      printf("src and dest file sizes do not match!!\n");
      free(buf);
      close(in_fd);
      close(out_fd);
      return DAP_ERROR;
    } 

    
    //otherwise file is transfered succesfully..
    printf("SUCCESS!\n");
    free(buf);
    close(in_fd);
    close(out_fd);
    return DAP_SUCCESS;

}

int main(int argc, char *argv[])
{

  char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
  char src_url[MAXSTR], dest_url[MAXSTR];
  int status = 0;

  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  strncpy(src_url, argv[1], MAXSTR-1);
  strncpy(dest_url, argv[2], MAXSTR-1);
 
  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "Transfering file:%s from host %s to file:%s on host %s..\n",
	  src_file, src_host, dest_file, dest_host);

  if ( !strcmp(src_protocol, "file") && !strcmp(dest_protocol, "file")){
    status = transfer_from_file_to_file(src_file, dest_file);
  }
  
  return status;
  
}





