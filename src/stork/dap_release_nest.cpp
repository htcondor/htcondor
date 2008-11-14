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

    f = safe_fopen_wrapper((fname, "w");
    fprintf(f, "error in release: (NeST error: %d)", nest_status);
    fclose(f);  
  }
  //--

  return status;

}

















