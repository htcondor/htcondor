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
#include <unistd.h>
#include "config-ibp.h"
#include "ibp_ClientLib.h"

/* ============================================================================
 * Allocate space on a IBP depot
 * ==========================================================================*/
int allocate_space_on_depot(char *dest_host, char *output_filename)
{
  struct ibp_depot ls_depot1;
  struct ibp_attributes ls_att1;
  IBP_set_of_caps ls_caps1;
  time_t ls_now;
  struct ibp_timer ls_timeout;
  FILE *fout;

  strcpy (ls_depot1.host, dest_host);
  ls_depot1.port =  6714;

  // set up the attributes
  time(&ls_now);
  ls_att1.duration=( ls_now + 24*3600);
  ls_att1.reliability=IBP_STABLE;
  ls_att1.type=IBP_BYTEARRAY;

  // allocation
  ls_caps1 = IBP_allocate(&ls_depot1, &ls_timeout, 1024*1000, &ls_att1);
  if ((ls_caps1 == NULL) ) {
    fprintf(stderr,"IBP Allocate failed! %d\n", IBP_errno);
    return DAP_ERROR;
  }
  
  fout = safe_fopen_wrapper(output_filename, "w");
  if (fout == NULL){
    fprintf(stderr,"Could not open %s for writing..\n", output_filename);
    return DAP_ERROR;
  }
  
  fprintf (fout, "[\n");
  fprintf (fout, "  writeCap = \"%s\";\n\n", ls_caps1->writeCap);
  fprintf (fout, "  readCap = \"%s\";\n", ls_caps1->readCap);
  fprintf (fout, "]\n");
  
  return DAP_SUCCESS;
}

int main(int argc, char *argv[])
{

  char dest_host[MAXSTR], output_filename[MAXSTR];
  int status;

  
  if (argc < 3) {
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <dest_host> <output_filaname>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  printf("CALLED: %s %s %s\n", argv[0], argv[1], argv[2]);

  strncpy(dest_host, argv[1], MAXSTR);
  strncpy(output_filename, argv[2], MAXSTR);
 
  /*
  parse_url(src_url, src_protocol, src_host, src_file);
  parse_url(dest_url, dest_protocol, dest_host, dest_file);
  
  fprintf(stdout, "Transfering file:%s from host %s to file:%s on host %s..\n",
	  src_file, src_host, dest_file, dest_host);

  if ( !strcmp(src_protocol, "file") && !strcmp(dest_protocol, "file")){
  */

  status = allocate_space_on_depot(dest_host, output_filename);

    /*
  }
    */
  return status;
  
}







