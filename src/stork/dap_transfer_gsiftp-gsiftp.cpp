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

#include "dap_logger.h"
#include "condor_common.h"
#include "string.h"
#include "globus_ftp_client.h"
#include "dap_constants.h"
#include "dap_utility.h"
#include "dap_error.h"
#include <unistd.h>

static globus_mutex_t lock1;
static globus_cond_t cond;
static globus_bool_t done;
static globus_bool_t error = GLOBUS_FALSE;
char globus_error_str[MAXSTR];


static
void
done_cb(
	void *					user_arg,
	globus_ftp_client_handle_t *		handle,
	globus_object_t *			err)
{
    char * tmpstr;

    if(err) { tmpstr = globus_object_printable_to_string(err);
	      fprintf(stderr, "Done with Error: %s\n", tmpstr);
	      strncpy(globus_error_str, tmpstr, MAXSTR-2);
	      globus_libc_free(tmpstr);
              error = GLOBUS_TRUE; }

    globus_mutex_lock(&lock1);
    done = GLOBUS_TRUE;
    globus_cond_signal(&cond);
    globus_mutex_unlock(&lock1);

}

int transfer_from_gridftp_to_gridftp(char *src_url, char *dest_url, 
				     char *error_str)
{
    globus_ftp_client_handle_t			handle;
    globus_ftp_client_operationattr_t		attr;
    globus_result_t				result;
    globus_ftp_client_handleattr_t		handle_attr;
    globus_ftp_client_restart_marker_t          restart;

    globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
    globus_ftp_client_handleattr_init(&handle_attr);
    globus_mutex_init(&lock1, GLOBUS_NULL);
    globus_cond_init(&cond, GLOBUS_NULL);

    globus_ftp_client_operationattr_init(&attr);
    globus_ftp_client_handle_init(&handle,  &handle_attr);

    globus_ftp_client_restart_marker_init(&restart);

    done = GLOBUS_FALSE;
    result = globus_ftp_client_third_party_transfer(&handle,
						    src_url,
						    &attr,
						    dest_url,
						    &attr,
						    //GLOBUS_NULL,
						    &restart,
						    done_cb,
						    0);
    if(result != GLOBUS_SUCCESS)
    {
	globus_object_t * err;
	char * tmpstr;
	err = globus_error_get(result);
	tmpstr = globus_object_printable_to_string(err);
	strncpy(globus_error_str, tmpstr, MAXSTR-2);
	fprintf(stderr, "Error: %s", tmpstr);
	globus_object_free(err);
	globus_libc_free(tmpstr);
	error = GLOBUS_TRUE;
	done = GLOBUS_TRUE;
    }
    globus_mutex_lock(&lock1);
    while(!done)
    {
	globus_cond_wait(&cond, &lock1);
    }
    globus_mutex_unlock(&lock1);

    globus_ftp_client_handle_destroy(&handle);
    globus_module_deactivate_all();

    if(error)
    {
      strncpy(error_str, globus_error_str, MAXSTR-2);
      return DAP_ERROR;
    }
    
    return DAP_SUCCESS;
}

int main(int argc, char *argv[])
{
  char src_url[MAXSTR], dest_url[MAXSTR];
  char error_str[MAXSTR];
  int status;

  if (argc < 3){
    printf("==============================================================\n");
    printf("USAGE: %s <src_url> <dest_url>\n", argv[0]);
    printf("==============================================================\n");
    exit(-1);
  }

  strcpy(src_url, argv[1]);
  strcpy(dest_url, argv[2]);

  fprintf(stdout, "Transfering from:%s to:%s\n", src_url, dest_url);

  status = transfer_from_gridftp_to_gridftp(src_url, dest_url, error_str); 
    
  //-- output to a temporary file....
  if (status != DAP_SUCCESS){
    unsigned int mypid;
    FILE *f;
    char fname[MAXSTR];

    mypid = getpid();
    sprintf(fname, "out.%d", mypid);

    f = safe_fopen_wrapper(fname, "w");
    fprintf(f, "Globus error: %s", error_str);

    fclose(f);  
  }
  //--

  return status;
}




























