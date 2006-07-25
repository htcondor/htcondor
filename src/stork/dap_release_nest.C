/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "dap_constants.h"
#include "dap_logger.h"
#include "dap_error.h"
#include "nest_speak.h"
#include "dap_classad_reader.h"

int release_space_on_nest(char *dest_host, char *lot_id, NestReplyStatus & status)
{
    NestConnection nest_con;
    NestLotHandle *handle;
    //    NestReplyStatus status;

    fprintf(stdout, "Releasing lot# %ld on server %s..\n", atol(lot_id), dest_host);

    //open connection to nest server
    status = NestOpenConnection(nest_con, dest_host);
    if (status != NEST_SUCCESS) return DAP_ERROR;

    handle = (NestLotHandle *)malloc(sizeof(NestLotHandle));
    handle->rsv_id = atol(lot_id);

    status = NestTerminateLot(nest_con, handle);
    if (status != NEST_SUCCESS){
      fprintf(stderr, "release error: %d\n", status);
      NestCloseConnection(nest_con); 
      return DAP_ERROR;
    }

    NestCloseConnection(nest_con);
    return DAP_SUCCESS;

}

int main(int argc, char *argv[])
{

  char lot_id[MAXSTR], dest_host[MAXSTR], work_dir[MAXSTR];
  int status;
  NestReplyStatus nest_status;

  if (argc < 4){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <dest_host> <lot_id>\n",
	   argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  strncpy(dest_host, argv[1], MAXSTR);
  strncpy(lot_id, argv[2], MAXSTR);
  strncpy(work_dir, argv[3], MAXSTR);
  
  status = release_space_on_nest(dest_host, lot_id, nest_status);

  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];
    
    mypid = getpid();
    snprintf(fname, MAXSTR, "%s/out.%d", work_dir, mypid);

    f = fopen(fname, "w");
    fprintf(f, "error in release: (NeST error: %d)", nest_status);
    fclose(f);  
  }
  //--

  return status;

}

















