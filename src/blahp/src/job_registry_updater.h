/*
 *  File :     job_registry_updater.h
 *
 *
 *  Author :   Francesco Prelz ($Author: fprelz $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  13-Jul-2011 Original release
 *
 *  Description:
 *    Prototypes of functions defined in job_registry_updater.c,
 *    with relevant data structures.
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

#ifndef __JOB_REGISTRY_UPDATER_H__
#define __JOB_REGISTRY_UPDATER_H__

#include <netdb.h>
#include <poll.h>

#include "job_registry.h"

/* The default (IPv4) multicast address is taken out of the GLOP (RFC3180) /24
 * space for Autonomous System 64516.
 */

#define _job_registry_updater_h_DEFAULT_MCAST_ADDR "233.252.4.217"
#define _job_registry_updater_h_DEFAULT_PORT       "58464"
#define _job_registry_updater_h_DEFAULT_TTL        2

typedef struct job_registry_updater_endpoint_s
 {
   int fd;
   int is_multicast;
   int addr_family;
   unsigned char ttl;
   struct job_registry_updater_endpoint_s *next;
 } job_registry_updater_endpoint;

int job_registry_updater_parse_address(const char *addstr, struct addrinfo **ai_ans,
                                       unsigned int *ifindex);

int job_registry_updater_is_multicast(const struct addrinfo *cur_ans);

int job_registry_updater_setup_sender(char **destinations, int n_destinations,
                                      unsigned char ttl,
                                      job_registry_updater_endpoint **endpoints);
int job_registry_updater_setup_receiver(char **sources, int n_sources,
                                        job_registry_updater_endpoint **endpoints);

int job_registry_updater_set_ttl(job_registry_updater_endpoint *endp,
                                 unsigned char ttl);

void job_registry_updater_free_endpoints(job_registry_updater_endpoint *head);

int job_registry_updater_get_pollfd(job_registry_updater_endpoint *endpoints,
                                       struct pollfd **pollset);

int job_registry_send_update(const job_registry_updater_endpoint *endpoints,
                             const job_registry_entry *entry,
                             const char *proxy_subject, const char *proxy_path); 

job_registry_entry *
    job_registry_receive_update(struct pollfd *pollset, nfds_t nfds,
                                int timeout_ms,
                                char **proxy_subject, char **proxy_path);

#endif  /* defined __JOB_REGISTRY_UPDATER_H__ */

