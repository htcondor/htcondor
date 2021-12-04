/*
 *  File :     test_job_registry_update_from_network.c
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  19-Jul-2011 Original issue.
 *
 *  Description:
 *   Test executable to receive job registry entries via the network and
 *   add them to a local registry.
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
  const char *network_endpoint_attr = "job_registry_add_remote";
  char *my_home;
  job_registry_entry *en;
  int ent, ret, prret, rhret;
  config_handle *cha;
  config_entry  *rge, *remupd_conf;
  job_registry_handle *rha, *rhano;
  job_registry_index_mode rgin_mode = NO_INDEX;
  job_registry_updater_endpoint *remupd_head = NULL;
  struct pollfd *remupd_pollset = NULL;
  int remupd_nfds;
  int timeout_ms = -1;
  char *proxy_path, *proxy_subject;

  if (argc > 1 && (strcmp(argv[1], "-u") == 0))
   {
    /* Obtain an index to the registry so that we can */
    /* check that the record is unique. */
    rgin_mode = BY_BLAH_ID;
    argv[1] = argv[0];
    argc--;
    argv++;
   }

  if (argc > 1)
   {
    timeout_ms = atoi(argv[1]);
    argv[1] = argv[0];
    argc--;
    argv++;
   }

  cha = config_read(NULL); /* Read config from default locations. */
  if (cha != NULL)
   {
    rge = config_get("job_registry", cha);
    remupd_conf = config_get(network_endpoint_attr, cha);
    if (remupd_conf != NULL)
     {
      if (job_registry_updater_setup_receiver(remupd_conf->values,
                                              remupd_conf->n_values,
                                             &remupd_head) < 0)
       {
         fprintf(stderr,"%s: warning: cannot set network receiver(s) up for remote update to:\n",argv[0]);
         for (ent = 0; ent < remupd_conf->n_values; ent++)
          {
           fprintf(stderr," - %s\n", remupd_conf->values[ent]);
          }
       }
     }
    if (rge != NULL) registry_file = rge->value;
   }

  /* Add command line endpoints if present. */
  if (argc > 1)
   {
    if (job_registry_updater_setup_receiver(argv+1,
                                            argc-1,
                                           &remupd_head) < 0)
       {
         fprintf(stderr,"%s: warning: cannot set network receiver(s) up for remote update to:\n",argv[0]);
         for (ent = 1; ent < argc; ent++)
          {
           fprintf(stderr," - %s\n", argv[ent]);
          }
       }
   }

  if (remupd_head == NULL)
   {
    fprintf(stderr,"%s: Error: cannot find values for network endpoints in configuration file (attribute '%s').\n",
            argv[0], network_endpoint_attr);
    if (cha != NULL) config_free(cha);
    return 4;
   }

  if ((remupd_nfds = job_registry_updater_get_pollfd(remupd_head, 
                                                    &remupd_pollset)) < 0)
   {
    fprintf(stderr,"%s: Error: Cannot setup poll set for receiving data.\n",
            argv[0]);
    if (cha != NULL) config_free(cha);
    job_registry_updater_free_endpoints(remupd_head);
    return 4;
   }
  if (remupd_pollset == NULL || remupd_nfds == 0)
   {
    fprintf(stderr,"%s: Error: No poll set available for receiving data.\n",
            argv[0]);
    if (cha != NULL) config_free(cha);
    job_registry_updater_free_endpoints(remupd_head);
    return 5;
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
      free(remupd_pollset);
      job_registry_updater_free_endpoints(remupd_head);
      return 1;
     }
   }

  rha=job_registry_init(registry_file, rgin_mode);

  if (rha == NULL)
   {
    /* Note: Non-privileged updates from the network make no sense. */
    fprintf(stderr,"%s: error initialising job registry: ",argv[0]);
    perror("");
    if (cha != NULL) config_free(cha);
    free(remupd_pollset);
    job_registry_updater_free_endpoints(remupd_head);
    return 2;
   }

  /* Filename is stored in job registry handle. - Don't need these anymore */
  if (cha != NULL) config_free(cha);
  if (need_to_free_registry_file) free(registry_file);
  proxy_path = NULL;
  proxy_subject = NULL;

  while (en = job_registry_receive_update(remupd_pollset, remupd_nfds,
                                          timeout_ms, &proxy_subject, 
                                          &proxy_path))
   {
    if (proxy_path != NULL && strlen(proxy_path) > 0)
     {
      prret = job_registry_set_proxy(rha, en, proxy_path);
      if (prret < 0)
       {
        fprintf(stderr,"%s: warning: setting proxy to %s: ",argv[0],proxy_path);
        perror("");
        /* Make sure we don't renew non-existing proxies */
        en->renew_proxy = 0;
       }
     }
    if (proxy_path != NULL) free(proxy_path);
  
    en->subject_hash[0] = '\000';
    if (proxy_subject != NULL && strlen(proxy_subject) > 0)
     {
      job_registry_compute_subject_hash(en, proxy_subject);
      rhret = job_registry_record_subject_hash(rha, en->subject_hash, 
                                               proxy_subject, TRUE);  
      if (rhret < 0)
       {
        fprintf(stderr,"%s: warning: recording proxy subject %s (hash %s): ", proxy_subject, en->subject_hash);
        perror("");
       }
     }
    if (proxy_subject != NULL) free(proxy_subject);
  
    if ((ret=job_registry_append(rha, en)) < 0)
     {
      fprintf(stderr,"%s: Warning: job_registry_append returns %d: ",argv[0],ret);
      perror("");
     } 
    free(en);
   }

  job_registry_destroy(rha);
  job_registry_updater_free_endpoints(remupd_head);
  return 0;
}
