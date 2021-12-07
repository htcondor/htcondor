/*
 *  File :     blah_job_registry_add.c
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  16-Nov-2007 Original release
 *  27-Feb-2008 Added user_prefix.
 *   3-Mar-2008 Added non-privileged updates to fit CREAM's file and process
 *              ownership model.
 *  14-Jul-2011 Added remote/multicast registry update if found in config file.
 *
 *  Description:
 *   Executable to add (append or update) an entry to the job registry.
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* geteuid() */
#include <time.h>
#include <errno.h>
#include "job_registry.h"
#include "job_registry_updater.h"
#include "config.h"

int
main(int argc, char *argv[])
{
  char *registry_file = NULL, *registry_file_env = NULL;
  int need_to_free_registry_file = FALSE;
  const char *default_registry_file = "blah_job_registry.bjr";
  char *my_home;
  job_registry_entry en;
  job_status_t status=IDLE;
  int exitcode = -1; 
  char *exitreason = "";
  char *user_prefix = "";
  char *user_proxy = "";
  char *proxy_subject = "";
  int  renew_proxy = 0;
  char *wn_addr = "";
  time_t udate=0;
  char *blah_id, *batch_id;
  int ent, ret, prret, rhret;
  config_handle *cha;
  config_entry *rge, *remupd_conf;
  job_registry_handle *rha, *rhano;
  job_registry_index_mode rgin_mode = NO_INDEX;
  job_registry_updater_endpoint *remupd_head = NULL;

  if (argc > 1 && (strcmp(argv[1], "-u") == 0))
   {
    /* Obtain an index to the registry so that we can */
    /* check that the record is unique. */
    rgin_mode = BY_BLAH_ID;
    argv[1] = argv[0];
    argc--;
    argv++;
   }

  if (argc < 3)
   {
    fprintf(stderr,"Usage: %s [-u] <BLAH id> <batch id> [job status] [udate] [user prefix] [user proxy] [renew proxy] [proxy subject] [worker node] [exit code] [exit reason]\n",argv[0]);
    return 1;
   }

  blah_id  = argv[1]; 
  batch_id = argv[2]; 

  if (argc > 3) status = atoi(argv[3]);
  if (argc > 4) udate = atol(argv[4]);
  if (argc > 5) user_prefix = argv[5];
  if (argc > 6) user_proxy = argv[6];
  if (argc > 7) renew_proxy = atoi(argv[7]);
  if (argc > 8) proxy_subject = argv[8];
  if (argc > 9) wn_addr = argv[9];
  if (argc > 10) exitcode = atoi(argv[10]);
  if (argc > 11) exitreason = argv[11];
   
  cha = config_read(NULL); /* Read config from default locations. */
  if (cha != NULL)
   {
    rge = config_get("job_registry", cha);
    remupd_conf = config_get("job_registry_add_remote", cha);
    if (remupd_conf != NULL)
     {
      if (job_registry_updater_setup_sender(remupd_conf->values,
                                            remupd_conf->n_values, 2,
                                           &remupd_head) < 0)
       {
         fprintf(stderr,"%s: warning: cannot set network sender(s) up for remote update to:\n",argv[0]);
         for (ent = 0; ent < remupd_conf->n_values; ent++)
          {
           fprintf(stderr," - %s\n", remupd_conf->values[ent]);
          }
       }
     }
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
      if (cha != NULL) config_free(cha);
      if (remupd_head != NULL) job_registry_updater_free_endpoints(remupd_head);
      return 1;
     }
   }

  JOB_REGISTRY_ASSIGN_ENTRY(en.blah_id,blah_id); 
  JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,batch_id); 
  en.status = status;
  en.exitcode = exitcode;
  JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,wn_addr); 
  en.udate = udate;
  JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,exitreason); 
  en.submitter = geteuid();
  JOB_REGISTRY_ASSIGN_ENTRY(en.user_prefix,user_prefix); 
  en.proxy_link[0] = '\000'; /* Start with a valid string */
  en.updater_info[0] = '\000'; 
  en.renew_proxy = 0; /* Enable renewal only if a proxy is found */

  rha=job_registry_init(registry_file, rgin_mode);

  if (rha == NULL)
   {
    if (errno == EACCES)
     {
      /* Try nonpriv update. It may work. */
      rhano = job_registry_init(registry_file, NAMES_ONLY);
      if (cha != NULL) config_free(cha);
      if (need_to_free_registry_file) free(registry_file);
      if (rhano != NULL)
       {
        if (strlen(user_proxy) > 0)
         {
          prret = job_registry_set_proxy(rhano, &en, user_proxy);
          if (prret < 0)
           {
            fprintf(stderr,"%s: warning: setting proxy to %s: ",argv[0],user_proxy);
            perror("");
           }
          else en.renew_proxy = renew_proxy;
         }
        ret=job_registry_append_nonpriv(rhano, &en);
        job_registry_destroy(rhano);
        if (ret < 0)
         {
          fprintf(stderr,"%s: job_registry_append_nonpriv returns %d: ",argv[0],ret);
          perror("");
          if (remupd_head != NULL) job_registry_updater_free_endpoints(remupd_head);
          return 4;
         } 
        else goto happy_ending;
       }
     }
    else
     {
      fprintf(stderr,"%s: error initialising job registry: ",argv[0]);
      perror("");
     }
    if (remupd_head != NULL) job_registry_updater_free_endpoints(remupd_head);
    return 2;
   }

  /* Filename is stored in job registry handle. - Don't need these anymore */
  if (cha != NULL) config_free(cha);
  if (need_to_free_registry_file) free(registry_file);

  if (strlen(user_proxy) > 0)
   {
    prret = job_registry_set_proxy(rha, &en, user_proxy);
    if (prret < 0)
     {
      fprintf(stderr,"%s: warning: setting proxy to %s: ",argv[0],user_proxy);
      perror("");
     }
    else en.renew_proxy = renew_proxy;
   }

  en.subject_hash[0] = '\000';
  if (strlen(proxy_subject) > 0)
   {
    job_registry_compute_subject_hash(&en, proxy_subject);
    rhret = job_registry_record_subject_hash(rha, en.subject_hash, 
                                             proxy_subject, TRUE);  
    if (rhret < 0)
     {
      fprintf(stderr,"%s: warning: recording proxy subject %s (hash %s): ", proxy_subject, en.subject_hash);
      perror("");
     }
   }

  if ((ret=job_registry_append(rha, &en)) < 0)
   {
    if (errno == EACCES)
     {
      /* Try nonpriv update. It may work. */
      ret=job_registry_append_nonpriv(rha, &en);
      job_registry_destroy(rha);
      if (ret < 0)
       {
        fprintf(stderr,"%s: job_registry_append_nonpriv returns %d: ",argv[0],ret);
        perror("");
        if (remupd_head != NULL) job_registry_updater_free_endpoints(remupd_head);
        return 5;
       } 
      else goto happy_ending;
     }

    fprintf(stderr,"%s: job_registry_append returns %d: ",argv[0],ret);
    perror("");
    job_registry_destroy(rha);
    if (remupd_head != NULL) job_registry_updater_free_endpoints(remupd_head);
    return 3;
   } 

  job_registry_destroy(rha);

 happy_ending:
  if (remupd_head != NULL)
   {
    if (job_registry_send_update(remupd_head, &en,
              (strlen(proxy_subject) > 0 ? proxy_subject : NULL),
              (strlen(user_proxy) > 0 ? user_proxy : NULL)) < 0)
     {
      fprintf(stderr,"%s: warning: sending network update: ",argv[0]);
      perror("");
     }
    job_registry_updater_free_endpoints(remupd_head);
   }
  return 0;
}
