/*
 *  File :     blah_job_registry_dump.c
 *
 *  Author :   Massimo Mezzadri ($Author: mezzadri $)
 *  e-mail :   "massimo.mezzadri@mi.infn.it"
 *
 *  Revision history :
 *  05-Nov-2010 Original release
 *
 *  Description:
 *   Executable to dump the BLAH job registry.
 *
 *  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
 *
 *    See http://www.eu-egee.org/partners/ for details on the copyright
 *    holders.  
 *  
 *    Licensed under the Apache License, Version 2.0 (the "License"); 
 *    you may not use this file except in compliance with the License. 
 *    You may obtain a copy of the License at 
 *  
 *        http://www.apache.org/licenses/LICENSE-2.0 
 *  
 *    Unless required by applicable law or agreed to in writing, software 
 *    distributed under the License is distributed on an "AS IS" BASIS, 
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 *    See the License for the specific language governing permissions and 
 *    limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/utsname.h> /* For uname */
#include "job_registry.h"
#include "config.h"

int
main(int argc, char *argv[])
{
  int idc;
  char *registry_file=NULL, *registry_file_env=NULL;
  int need_to_free_registry_file = FALSE;
  const char *default_registry_file = "blah_job_registry.bjr";
  char *my_home;
  job_registry_index_mode mode=BY_BLAH_ID;
  job_registry_entry *ren;
  config_handle *cha;
  config_entry *rge;
  job_registry_handle *rha;
  int opt_get_unfinished = FALSE;
  int opt_get_finished = FALSE;
  FILE *fd;
  char *proxypath;
  
  mode = BY_BATCH_ID;
  
  /* Look up for command line switches */
  
  for (idc = 1; idc < argc; idc++)
   {
    if (argv[idc][0] != '-') break;
    switch (argv[idc][1])
     {
      case 'u':
        opt_get_unfinished = TRUE;
        break;
      case 'f':
        opt_get_finished = TRUE;
        break;
      case 'h':
        fprintf(stdout,"Usage: %s [-u|-f] [-h]\n",argv[0]);
        fprintf(stdout,"\tno options to list all the jobs in the registry \n");
        fprintf(stdout,"\t-u to list only unfinished\n");
        fprintf(stdout,"\t-u to list only finished\n");
        fprintf(stdout,"\t-h for help\n");
        return 0;
     }
   }

  cha = config_read(NULL); /* Read config from default locations. */
  if (cha != NULL)
   {
    rge = config_get("job_registry",cha);
    if (rge != NULL) registry_file = rge->value;
   }
  /* Env variable takes precedence */
  registry_file_env = getenv("BLAH_JOB_REGISTRY_FILE");
  if (registry_file_env != NULL) registry_file = registry_file_env;

  if (registry_file == NULL)
   {
    my_home = getenv("HOME");
    if (my_home == NULL) my_home = ".";
    registry_file = (char *)malloc(strlen(default_registry_file)+strlen(my_home)+2);
    if (registry_file != NULL) 
     {
      sprintf(registry_file,"%s/%s",my_home,default_registry_file);
      need_to_free_registry_file = TRUE;
     }
    else 
     {
      fprintf(stdout,"1ERROR %s: Out of memory.\n",argv[0]);
      if (cha != NULL) config_free(cha);
      return 1;
     }
   }

  rha=job_registry_init(registry_file, mode);

  if (rha == NULL)
   {
    fprintf(stdout,"1ERROR %s: error initialising job registry: %s\n",argv[0],strerror(errno));
    if (cha != NULL) config_free(cha);
    if (need_to_free_registry_file) free(registry_file);
    return 2;
   }

  /* Filename is stored in job registry handle. - Don't need these anymore */
  if (cha != NULL) config_free(cha);
  if (need_to_free_registry_file) free(registry_file);

fd = job_registry_open(rha, "r");
if (fd == NULL)
{
    fprintf(stdout,"1ERROR %s: error opening job registry: %s\n",argv[0],strerror(errno));
    if (cha != NULL) config_free(cha);
    if (need_to_free_registry_file) free(registry_file);
    return 2;
}
if (job_registry_rdlock(rha, fd) < 0)
{
    fprintf(stdout,"1ERROR %s: error locking job registry for reading: %s\n",argv[0],strerror(errno));
    if (cha != NULL) config_free(cha);
    if (need_to_free_registry_file) free(registry_file);
    return 2;
}
while ((ren = job_registry_get_next(rha, fd)) != NULL)
{
 if(opt_get_unfinished && (ren->status == 3 || ren->status == 4)) continue;
 if(opt_get_finished && (ren->status == 1 || ren->status == 2 || ren->status == 5)) continue;
 printf("[ BatchJobId=\"%s\"; JobStatus=%d; BlahJobId=\"%s\"; "
                   "CreateTime=%ld; ModifiedTime=%ld; UserTime=%ld; "
                   "SubmitterUid=%d;",ren->batch_id, ren->status, ren->blah_id,
                  (unsigned long)ren->cdate, (unsigned long)ren->mdate, (unsigned long)ren->udate, ren->submitter);
		  
  if ((ren->wn_addr != NULL) && (strlen(ren->wn_addr) > 0))
   { 
     printf("WorkerNode=\"%s\"; ",ren->wn_addr);
   }
  if ((ren->proxy_link != NULL) && (strlen(ren->proxy_link) > 0))
   {
    proxypath = job_registry_get_proxy(rha, ren);
    if (proxypath != NULL)
     {
      printf("X509UserProxy=\"%s\"; ",proxypath);
      free(proxypath);
     }
   }
  if (ren->status == COMPLETED)
   {
    printf("ExitCode=%d; ",ren->exitcode);
   }
  if ((ren->exitreason != NULL) && (strlen(ren->exitreason) > 0))
   { 
    printf("ExitReason=\"%s\"; ",ren->exitreason);
   }
  if ((ren->user_prefix != NULL) && (strlen(ren->user_prefix) > 0))
   { 
    printf("UserPrefix=\"%s\"; ",ren->user_prefix);
   }
 
 printf("]\n");
		  

/*
  cad = job_registry_entry_as_classad(rha, ren);
  if (cad != NULL) printf("0%s\n",cad);
  else 
   {
    fprintf(stdout,"1ERROR %s: Out of memory.\n",argv[0]);
    free(ren);
    job_registry_destroy(rha);
    return 1;
   }
  free(cad)
*/
  free(ren);
}
  job_registry_destroy(rha);
  return 0;
}
