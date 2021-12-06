/*
 *  File :     proxy_hashcontainer.c
 *
 *
 *  Author :   Francesco Prelz ($Author: mezzadri $)
 *  e-mail :   "francesco.prelz@mi.infn.it"
 *
 *  Revision history :
 *  12-Dec-2006 Original release
 *
 *  Description:
 *    Simple hashed container to store proxy filenames in response to
 *    the CACHE_PROXY_FROM_FILE, USE_CACHED_PROXY and UNCACHE_PROXY
 *    commands, as received from the Condor gridmanager.
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

#include <stdlib.h>
#include <string.h>

#include "proxy_hashcontainer.h"

#define PROXY_HASHCONTAINER_HASH_BUCKETS 200
static proxy_hashcontainer_entry *proxy_hash[PROXY_HASHCONTAINER_HASH_BUCKETS];

void 
proxy_hashcontainer_init()
 {
   int i;

   for (i=0; i<PROXY_HASHCONTAINER_HASH_BUCKETS; i++)
    {
     proxy_hash[i] = NULL;
    }
 }

unsigned int
proxy_hashcontainer_hashfunction(char *id)
 {
  unsigned int bucket=0;
  int i;

  if (id != NULL)
   {
    for (i=0; i<strlen(id); i++)
     {
      bucket += id[i];
     }
   } 

  bucket %= PROXY_HASHCONTAINER_HASH_BUCKETS;

  return(bucket);
 }

proxy_hashcontainer_entry *
proxy_hashcontainer_lookup(char *id)
 {
   proxy_hashcontainer_entry *retval;

   retval = proxy_hash[proxy_hashcontainer_hashfunction(id)];
   while ((retval != NULL) && (strcmp(retval->id, id) != 0))
    {
     retval = retval->next;
    }

   return(retval);
 }


proxy_hashcontainer_entry *
proxy_hashcontainer_new(char *id, char *proxy_file_name)
 {
   proxy_hashcontainer_entry *retval;

   if (id == NULL || proxy_file_name == NULL) return (NULL);

   retval = (proxy_hashcontainer_entry *)malloc(sizeof(proxy_hashcontainer_entry));
   if (retval != NULL)
    {
     retval->id = strdup(id);
     retval->proxy_file_name = strdup(proxy_file_name);
     retval->next = NULL;

     /* Waste this entire entry if we couldn't allocate */

     if (retval->id == NULL || retval->proxy_file_name == NULL)
      {
       if (retval->id != NULL) free(retval->id);
       if (retval->proxy_file_name != NULL) free(retval->proxy_file_name);
       free(retval);
       retval = NULL;
      }
    }

   return(retval);
 }

void
proxy_hashcontainer_free(proxy_hashcontainer_entry *entry)
 {
  if (entry != NULL)
   {
    if (entry->id != NULL) free(entry->id);
    if (entry->proxy_file_name != NULL) free(entry->proxy_file_name);
    free(entry);
   }
 }

void
proxy_hashcontainer_cleanup(void)
 {
  int i;
  proxy_hashcontainer_entry *cur;
  proxy_hashcontainer_entry *next;
  
  for (i=0; i<PROXY_HASHCONTAINER_HASH_BUCKETS; i++)
   {
    if (proxy_hash[i] != NULL)
     {
      for (cur = proxy_hash[i]; cur != NULL; cur = next) 
       {
        next = cur->next;
        proxy_hashcontainer_free(cur); 
       }
      proxy_hash[i] = NULL;
     }
   }
 }

proxy_hashcontainer_entry *
proxy_hashcontainer_append(char *id, char *proxy_file_name)
 {
  proxy_hashcontainer_entry *entry;
  unsigned int hash_id;

  entry = proxy_hashcontainer_new(id, proxy_file_name);
  if (entry == NULL) return (NULL);
  
  hash_id = proxy_hashcontainer_hashfunction(id);
  if (proxy_hash[hash_id] == NULL) proxy_hash[hash_id] = entry;
  else
   {
    entry->next = proxy_hash[hash_id];
    proxy_hash[hash_id] = entry;
   }
  return(entry);
 }

int
proxy_hashcontainer_unlink(char *id)
 {
   proxy_hashcontainer_entry *entry;
   proxy_hashcontainer_entry *prev=NULL;
   unsigned int hash_id;

   hash_id = proxy_hashcontainer_hashfunction(id);

   entry = proxy_hash[hash_id];
   while ((entry != NULL) && (strcmp(entry->id, id) != 0))
    {
     prev = entry;
     entry = entry->next;
    }

   if (entry != NULL)
    {
     if (prev != NULL) prev->next = entry->next;
     else proxy_hash[hash_id] = entry->next;
     proxy_hashcontainer_free(entry);
     return(PROXY_HASHCONTAINER_SUCCESS);
    }
   else return(PROXY_HASHCONTAINER_NOT_FOUND); /* Entry not found */
 }

proxy_hashcontainer_entry *
proxy_hashcontainer_add(char *id, char *proxy_file_name)
 {
  proxy_hashcontainer_entry *entry;

  entry = proxy_hashcontainer_lookup(id);

  if (entry != NULL)
   {
    /* Id was found. Try to update contents and go home. */
    if (entry->proxy_file_name != NULL) free(entry->proxy_file_name);
    entry->proxy_file_name = strdup(proxy_file_name);
    if (entry->proxy_file_name == NULL)
     {
      /* Bad alloc. Need to clean up this node */
      proxy_hashcontainer_unlink(id);
      return(NULL);
     }
   }
  else
   {
    /* New node */
    entry = proxy_hashcontainer_append(id, proxy_file_name);
   }
  return(entry);
 }

#ifdef PROXY_HASHCONTAINER_TEST_CODE

#include <stdio.h>
#include <time.h>

unsigned int
proxy_hashcontainer_traverse_and_count(void)
 {
  int i;
  unsigned int count=0;
  proxy_hashcontainer_entry *cur;
  
  for (i=0; i<PROXY_HASHCONTAINER_HASH_BUCKETS; i++)
   {
    if (proxy_hash[i] != NULL)
     {
      for (cur = proxy_hash[i]; cur != NULL; cur = cur->next) count++;
     }
   }

  return(count);
 }

#define MAX_TEST_INSERTIONS PROXY_HASHCONTAINER_HASH_BUCKETS*4

int
main(int argc, char *argv[])
 {
  char id[64];
  unsigned int ids[MAX_TEST_INSERTIONS];

  char *base_path = "/this/is/a/test/path/";
  char *full_path;
  
  int n_insertions;
  int n_deletions;

  unsigned int count;
  unsigned int hash_id;
  proxy_hashcontainer_entry *entry;
  int test_status = 0;
  int i;

  srand(time(0));

  n_insertions = rand()%MAX_TEST_INSERTIONS; 

  printf("**** Test Phase 1 - Inserting %d paths.\n",n_insertions);

  for(i=0; i<n_insertions; i++)
   {
    ids[i] = rand()%100000;
    sprintf(id,"%08d",ids[i]);

    full_path = (char *)malloc(strlen(base_path)+strlen(id)+1);

    if (full_path != NULL)
     {
      strcpy(full_path, base_path);
      strcat(full_path, id);
     }
    else 
     {
      fprintf(stderr,"%s: Warning! Out of Memory.\n", argv[0]);
      full_path = base_path; /*Do at least something*/
     }
    if (proxy_hashcontainer_append(id, full_path) == NULL)
     {
      fprintf(stderr,"%s: Error appending id=<%s> path=<%s>.\n", argv[0], 
              id, full_path);
     }

    if (full_path != NULL && full_path != base_path) free(full_path);
   }

  printf("**** Test Phase 2 - Checking count.\n");

  count = proxy_hashcontainer_traverse_and_count();
  if (count != n_insertions)
   {
    fprintf(stderr,"%s: Error. Inserted %d elements. Found %d.\n",
            n_insertions, count);
    test_status |= 1;
   }

  n_deletions = rand()%n_insertions; 

  printf("**** Test Phase 3 - Deleting %d paths.\n",n_deletions);

  for(i=0; i<n_deletions; i++)
   {
    sprintf(id,"%08d",ids[i]);
    if (proxy_hashcontainer_unlink(id) < 0)
     {
      fprintf(stderr,"%s: Error deleting ID <%s>.\n",id);
      test_status |= 2;
     }
   }

  printf("**** Test Phase 4 - Checking count.\n");

  count = proxy_hashcontainer_traverse_and_count();
  if (count != (n_insertions-n_deletions))
   {
    fprintf(stderr,"%s: Error. %d elements left. Found %d.\n",
            argv[0],n_insertions-n_deletions, count);
    test_status |= 4;
   }

  printf("**** Test Phase 5 - Looking up remaining items.\n");

  for(i=n_deletions; i<n_insertions; i++)
   {
    sprintf(id,"%08d",ids[i]);
    entry = proxy_hashcontainer_lookup(id);
    if (entry == NULL)
     {
      fprintf(stderr,"%s: Error. ID <%s> not found.\n",argv[0],id);
      test_status |= 8;
      continue;
     }
    if (strcmp(id,entry->id)!=0)
     {
      fprintf(stderr,"%s: Error in lookup. Requested ID <%s>. Found <%s>.\n",
              argv[0],id,entry->id);
      test_status |= 16;
     }
    if (strstr(entry->proxy_file_name,id)==NULL)
     {
      fprintf(stderr,"%s: Error. ID <%s> not found in path <%s>.\n",
              argv[0],id,entry->proxy_file_name);
      test_status |= 16;
     }
    hash_id = proxy_hashcontainer_hashfunction(id);
#ifdef DEBUG
    fprintf(stderr,"DEBUG: entry == [%08X] entry->id == <%s> hash == %d entry->proxy_file_name == <%s> entry->next = [%08X]\n",
            entry, entry->id, hash_id, entry->proxy_file_name, entry->next); 
#endif
   }

  proxy_hashcontainer_cleanup(); 

  exit(test_status);
 }
#endif /*defined PROXY_HASHCONTAINER_TEST_CODE*/
