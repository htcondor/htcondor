/*
 *  File :     proxy_hashcontainer.h
 *
 *
 *  Author :   Francesco Prelz ($Author: mezzadri $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  12-Dec-2006 Original release
 *
 *  Description:
 *    Prototypes of functions defined in proxy_hashcontainer.c
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

#ifndef __PROXY_HASHCONTAINER_H__
#define __PROXY_HASHCONTAINER_H__

typedef struct proxy_hashcontainer_entry_s
 {
   char *id;
   char *proxy_file_name;
   struct proxy_hashcontainer_entry_s *next;
 } proxy_hashcontainer_entry;

#define PROXY_HASHCONTAINER_SUCCESS    0
#define PROXY_HASHCONTAINER_NOT_FOUND -1 

void proxy_hashcontainer_init();
unsigned int proxy_hashcontainer_hashfunction(char *id);
proxy_hashcontainer_entry *proxy_hashcontainer_lookup(char *id);
proxy_hashcontainer_entry *proxy_hashcontainer_new(char *id, char *proxy_file_name);
void proxy_hashcontainer_free(proxy_hashcontainer_entry *entry);
void proxy_hashcontainer_cleanup(void);
proxy_hashcontainer_entry *proxy_hashcontainer_append(char *id, char *proxy_file_name);
int proxy_hashcontainer_unlink(char *id);
proxy_hashcontainer_entry *proxy_hashcontainer_add(char *id, char *proxy_file_name);

#endif /*defined __PROXY_HASHCONTAINER_H__*/
