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
#include "nest_speak.h"
#include <unistd.h>

#define return_error {free(buf);NestCloseConnection(nest_con_in);NestCloseConnection(nest_con_out);return DAP_ERROR;} 

/* ============================================================================
 * Transfer a file from local disk to NEST
 * ==========================================================================*/
int transfer_from_file_to_nest(char *src_file, char *dest_host, 
			       char *dest_file, NestReplyStatus & status)
{
    int in_fd;
    char *buf;
    long src_filesize;
	nestFileSize dest_filesize;
    int bytes_read;
    struct stat filestat;
    NestConnection nest_con_out;

    //start transfer
    buf = (char*)malloc(sizeof(char) * BUF_SIZE);
    
     //open connection to dest host
    status = NestOpenConnection(nest_con_out, dest_host);
    if (status != NEST_SUCCESS) {
      free(buf);
      NestCloseConnection(nest_con_out);
      return DAP_ERROR;
    }

    //get the local filesize info
    if (!lstat(src_file, &filestat))  //if file exists
      src_filesize = filestat.st_size;

    //open the input file for reading
    in_fd = safe_open_wrapper(src_file, O_RDONLY);
    if (in_fd < 0){
      fprintf(stdout,"Error in opening file : %s\n",src_file);
      free(buf);
      NestCloseConnection(nest_con_out);
      return DAP_ERROR;
    }

    //get dest directory name
    printf("det file  = %s\n", dest_file);

    char tmpstr[MAXSTR], dir[MAXSTR], dest_fname[MAXSTR];
    char *t = NULL, *p = NULL;

    strcpy(tmpstr, dest_file);
    strcpy(dir, "");

    t = strtok(tmpstr,"/");
    while(1){
      if (t == NULL) break;
      if (p!= NULL) {
	strcat(strcat(dir, "/"), p);
	NestMkDir(dir, nest_con_out);
      }
      if ((p = strtok(NULL,"/")) != NULL){
	strcat(strcat(dir, "/"), t);   //get file name
	NestMkDir(dir, nest_con_out);
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

    //    NestMkDir(dir, nest_con_out);

    /*
    struct stat dest_dirstat;
    if (lstat(dir, &dest_dirstat)){ //if dest dir does not exist
      char command[MAXSTR];
      
      snprintf(command, MAXSTR, "mkdir -p %s", dir);
      if (system(command) == -1){
	fprintf(stdout,"Error in creating directory : %s\n",dir);
	return DAP_ERROR;
      }
    }
    */

    //transfer file
    int i = 1;
    while ( (bytes_read = read(in_fd, buf, BUF_SIZE)) > 0){
      status = NestWriteBytes(nest_con_out, dest_file, (i-1)*BUF_SIZE ,
			      buf, bytes_read, 1 );
      if (status != NEST_SUCCESS){
	free(buf);
	close(in_fd);
	NestCloseConnection(nest_con_out);
	return DAP_ERROR;
      } 
      i++;
    }

    //get the filesize info for the dest file
    status = NestFilesize(dest_filesize, dest_file, nest_con_out);
    if (status != NEST_SUCCESS){
      free(buf);
      close(in_fd);
      NestCloseConnection(nest_con_out);
      return DAP_ERROR;
    }
    
    if (src_filesize != dest_filesize){
      printf("src and dest file sizes do not match!!\n");
      free(buf);
      close(in_fd);
      NestCloseConnection(nest_con_out);
      return DAP_ERROR;
    } 

    
    //otherwise file is transfered succesfully..
    printf("SUCCESS!\n");
    free(buf);
    close(in_fd);
    NestCloseConnection(nest_con_out);
    return DAP_SUCCESS;

}

/* ============================================================================
 * Transfer a file from NEST to local disk
 * ========================================================================== */

int transfer_from_nest_to_file(char *src_host, char *src_file, 
			       char *dest_file, NestReplyStatus & status)
{
    int out_fd;
    char *buf;
    long dest_filesize;
	nestFileSize src_filesize;
    int bytes_read, bytes_written;
    struct stat filestat;
    NestConnection nest_con;

    //start transfer
    buf = (char*)malloc(sizeof(char) * BUF_SIZE);
    
    //open connection to src host
    status = NestOpenConnection(nest_con, src_host);
    if (status != NEST_SUCCESS) {
      free(buf);
      NestCloseConnection(nest_con);
      return DAP_ERROR;
    }

    //get the filesize info for the src file
    status = NestFilesize(src_filesize, src_file, nest_con);
    if (status != NEST_SUCCESS){
      free(buf);
      NestCloseConnection(nest_con);
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
      NestCloseConnection(nest_con);
      return DAP_ERROR;
    }

    //transfer file from Nest to local disk
    status = NestReadBytes(nest_con, src_file, 0, buf, BUF_SIZE , bytes_read);
    if (status != NEST_SUCCESS){
      free(buf);
      close(out_fd);
      NestCloseConnection(nest_con);
      return DAP_ERROR;
    }

    int i = 1;
    while (bytes_read > 0){
      bytes_written = write(out_fd, buf, bytes_read);
      
      if (bytes_written != bytes_read){
	fprintf(stdout,"Error! read = %d, written = %d\n",bytes_read, bytes_written);
	free(buf);
	close(out_fd);
	NestCloseConnection(nest_con);
	return DAP_ERROR;
      }
      
      status = NestReadBytes(nest_con, src_file, i*BUF_SIZE, 
			     buf, BUF_SIZE, bytes_read);
      if (status != NEST_SUCCESS){
	free(buf);
	close(out_fd);
	NestCloseConnection(nest_con);
	return DAP_ERROR;
      }
      
      i++;
    }


    //get the local filesize info
    if (!lstat(dest_file, &filestat))  //if file exists
      dest_filesize = filestat.st_size;

    if (src_filesize != dest_filesize){
      printf("src and dest file sizes do not match!!\n");
      free(buf);
      close(out_fd);
      NestCloseConnection(nest_con);
      return DAP_ERROR;
    } 

    
    //otherwise file is transfered succesfully..
    printf("SUCCESS!\n");
    free(buf);
    close(out_fd);
    NestCloseConnection(nest_con);
    return DAP_SUCCESS;

}

/* ============================================================================
 * Transfer a file from a NEST server to another NEST server
 * ==========================================================================*/
int transfer_from_nest_to_nest(char *src_host, char *src_file, char *dest_host, char *dest_file, NestReplyStatus & status)
{
    char *buf;
	nestFileSize src_filesize, dest_filesize;
    int bytes_read;
    NestConnection nest_con_in, nest_con_out;

    //allocate memeory for buf
    buf = (char*)malloc(sizeof(char) * BUF_SIZE);
    if (buf == NULL) return DAP_ERROR;

    //open connection to src host
    status = NestOpenConnection(nest_con_in, src_host);
    if (status != NEST_SUCCESS) {
      free(buf);
      return DAP_ERROR;
    }

    //open connection to dest host
    status = NestOpenConnection(nest_con_out, dest_host);
    if (status != NEST_SUCCESS) {
      free(buf);
      NestCloseConnection(nest_con_in);
      return DAP_ERROR;
    }
    
    //get the filesize info for the src file
    status = NestFilesize(src_filesize, src_file, nest_con_in);
    if (status != NEST_SUCCESS) return_error;


    //transfer file from src to dest
    status = NestReadBytes(nest_con_in, src_file, 0, buf, BUF_SIZE , bytes_read);
    if (status != NEST_SUCCESS) return_error;

    int i = 1;
    while (bytes_read > 0){
      status = NestWriteBytes(nest_con_out, dest_file, (i-1)*BUF_SIZE ,
			      buf, bytes_read, 1 );
      if (status != NEST_SUCCESS) return_error;

      status = NestReadBytes(nest_con_in, src_file, i*BUF_SIZE, 
			     buf, BUF_SIZE, bytes_read);
      if (status != NEST_SUCCESS) return_error;

      i++;
    }

    //get the filesize info for the dest file
    status = NestFilesize(dest_filesize, dest_file, nest_con_out);
    if (status != NEST_SUCCESS) return_error;

    if (src_filesize != dest_filesize) return_error; 

    //otherwise file is transfered succesfully..
    free(buf);
    NestCloseConnection(nest_con_in);
    NestCloseConnection(nest_con_out);
    return DAP_SUCCESS;

}

int main(int argc, char *argv[])
{

  char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
  char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
  char src_url[MAXSTR], dest_url[MAXSTR];
  int status;

  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <src_url> <dest_url>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  strncpy(src_url, argv[1], MAXSTR);
  strncpy(dest_url, argv[2], MAXSTR);
 
  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  


  fprintf(stdout, "Transfering file:%s from host %s to file:%s on host %s..\n",
	  src_file, src_host, dest_file, dest_host);

  NestReplyStatus nest_status;
  if ( !strcmp(src_protocol, "nest") && !strcmp(dest_protocol, "nest")){

    status = transfer_from_nest_to_nest(src_host, src_file, dest_host, 
					dest_file, nest_status);
  }
  else if ( !strcmp(src_protocol, "file") && !strcmp(dest_protocol, "nest")){
    status = transfer_from_file_to_nest(src_file, dest_host, dest_file, nest_status);
  }
  else if ( !strcmp(src_protocol, "nest") && !strcmp(dest_protocol, "file")){
    status = transfer_from_nest_to_file(src_host, src_file, dest_file, nest_status);
  }
       
  
  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "out.%d", mypid);
    
    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "NeST error: %d", nest_status);
    printf("NeST error: %d", nest_status);
    fclose(f);  
  }
  //--
  
  return status;
  
}





