/*
 *  File :     test_job_registry_access.c
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  14-Nov-2007 Original release
 *  27-Feb-2008 Added test of job_registry_split_blah_id.
 *   8-Oct-2010 Added test for mmap index mode.
 *
 *  Description:
 *   Access test for job registries created by test_job_registry_create.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "job_registry.h"

int
main(int argc, char *argv[])
{
  char *test_registry_file = JOB_REGISTRY_TEST_FILE;
  job_registry_entry *en;
  job_registry_split_id *spid;
  int ret;
  int pick;
  int n_read_tests;
  struct timeval tm_start, tm_end;
  char *pname;
  struct stat pstat;
  float elapsed_secs;
  int i;
  job_registry_index_mode test_mode = BY_BLAH_ID;

  if (argc > 1 && (strncmp(argv[1],"-m",2) == 0))
   {
    test_mode = BY_BLAH_ID_MMAP;
    if (argc > 2) test_registry_file = argv[2];
   }
  else if (argc > 1) test_registry_file = argv[1];

  srand(time(0));

  job_registry_handle *rha;

  rha=job_registry_init(test_registry_file, test_mode);

  if (rha == NULL)
   {
    fprintf(stderr,"%s: error initialising job registry: ",argv[0]);
    perror("");
    return 1;
   }

  if (rha->n_entries <= 0)
   {
    fprintf(stderr,"%s: job registry %s has %d entries. Little to do.\n",
            argv[0], test_registry_file, rha->n_entries);
    job_registry_destroy(rha);
    return 1;
   }

  /* Check that index is ordered */
  for (i=1; i<rha->n_entries; i++)
   {
    if (strcmp(rha->entries[i].id,rha->entries[i-1].id) < 0)
     {
      fprintf(stderr,"%s: job registry entry #%d (%s) should not be before #%d (%s).\n",
              argv[0], i-1, rha->entries[i-1].id, i, rha->entries[i].id);   
      job_registry_destroy(rha);
      return 1;
     } 
   }
  n_read_tests = rha->n_entries*3;
  printf("%s: Successfully indexed %d entries. Now performing %d reads and checks.\n",
         argv[0],rha->n_entries, n_read_tests);
  gettimeofday(&tm_start, NULL);
  for (i=0; i<n_read_tests; i++)
   {
    pick = rand()%(rha->n_entries);
    en = job_registry_get(rha, rha->entries[pick].id);
    if (en == NULL)
     {
      fprintf(stderr,"%s: job registry entry with ID==%s not found.\n",
              argv[0], rha->entries[pick].id);
      job_registry_destroy(rha);
      return 1;
     }
    if (strlen(en->proxy_link) > 0)
     {
      pname = job_registry_get_proxy(rha, en);
      if (pname == NULL)
       {
        fprintf(stderr,"%s: Unable to resolve proxy link %s: ",
              argv[0], en->proxy_link);
        perror("");
       }
      else 
       {
        if (stat(pname, &pstat) < 0)
         {
          fprintf(stderr,"%s: stat of %s fails: ", argv[0], pname);
          perror("");
         }
        free(pname);
       }
     }
    if ((spid = job_registry_split_blah_id(en->blah_id)) == NULL)
     {
      fprintf(stderr,"%s: BLAH job ID %s invalid according to job_registry_split_blah_id.\n",
              argv[0], en->blah_id);
     }
    else 
     {
      if (strcmp(&(spid->proxy_id[strlen(spid->proxy_id)-6]),
                 &(en->batch_id[strlen(en->batch_id)-6])) != 0)
       {
        fprintf(stderr,"%s: Trailing number of IDs %s and %s differs.\n",
                argv[0], en->blah_id, en->batch_id);
        job_registry_free_split_id(spid);
        job_registry_destroy(rha);
        return 1;
       }
      job_registry_free_split_id(spid);
     }
    free(en);
   }
  gettimeofday(&tm_end, NULL);

  elapsed_secs = (tm_end.tv_sec - tm_start.tv_sec) +
                 (float)(tm_end.tv_usec - tm_start.tv_usec)/1000000;
  printf("%s: Successfully read/checked %d entries in %g seconds (%g entries/s).\n",
         argv[0],n_read_tests, elapsed_secs, n_read_tests/elapsed_secs);

  job_registry_destroy(rha);
  return 0;
}
