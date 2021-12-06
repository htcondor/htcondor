/*
 *  File :     test_job_registry_scan.c
 *
 *
 *  Author :   Francesco Prelz ($Author: mezzadri $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  15-Nov-2007 Original release
 *
 *  Description:
 *   Collect contents statistics from a test job registry.
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
#include <errno.h>
#include "job_registry.h"

int
main(int argc, char *argv[])
{
  char *test_registry_file = JOB_REGISTRY_TEST_FILE;
  job_registry_entry *en;
  time_t old_date;
  time_t first_cdate=0;
  time_t last_cdate;
  int count[10];
  FILE *fd;
  int ret;
  int i;

  if (argc > 1) test_registry_file = argv[1];

  if (argc > 2) 
   {
    old_date = atol(argv[2]);
    if (job_registry_purge(test_registry_file, old_date, FALSE) < 0)
     {
      /* Registry file may be corrupt. Try forcing repair by rewriting */
      /* the file. */
      if ((ret=job_registry_purge(test_registry_file, old_date, TRUE)) < 0)
       {
        fprintf(stderr,"%s: job_registry_purge returns %d: ",argv[0],ret);
        return 1;
       }
     }
   }

  job_registry_handle *rha;

  rha=job_registry_init(test_registry_file, NO_INDEX);

  if (rha == NULL)
   {
    fprintf(stderr,"%s: error initialising job registry %s: ",argv[0],test_registry_file);
    perror("");
    return 1;
   }

  printf("%s: job registry %s contains %d entries.\n",argv[0],
         test_registry_file, rha->lastrec - rha->firstrec + 1);

  fd = job_registry_open(rha, "r");
  if (fd == NULL)
   {
    fprintf(stderr,"%s: Error opening registry %s: ",argv[0],test_registry_file);
    perror("");
    return 1;
   }
  if (job_registry_rdlock(rha, fd) < 0)
   {
    fprintf(stderr,"%s: Error read locking registry %s: ",argv[0],test_registry_file);
    perror("");
    return 1;
   }
 
  for (i=0;i<10;i++) count[i] = 0;

  while ((en = job_registry_get_next(rha, fd)) != NULL)
   {
    if (first_cdate == 0) first_cdate = en->cdate;
    last_cdate = en->cdate;
    count[en->status]++;    
    free(en); 
   }

  printf("First cdate == %d\n", first_cdate);
  printf("Last  cdate == %d\n", last_cdate);
  for (i=0;i<10;i++) printf("count[%2d] == %d\n",i,count[i]);

  fclose(fd);
  job_registry_destroy(rha);
  return 0;
}
