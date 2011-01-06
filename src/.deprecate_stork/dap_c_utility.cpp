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

#include "dap_constants.h"
#include "dap_c_utility.h"

/* ============================================================================
 * 
 * ==========================================================================*/
char *strip_str(char *str)
{

  while ( str[0] == '"' || str[0] ==' ') {
    str++;
  }
  
  while ( str[strlen(str)-1] == '"' || str[strlen(str)-1] == ' ') {
    str[strlen(str)-1] = '\0';
  }
  
  return str;
}
/* ============================================================================
 *
 * ==========================================================================*/
void parse_url_with_port(char *url, char *protocol, char *host, char *port, char *filename)
{

  char temp_url[MAXSTR];
  char *p;
  
  //initialize
  strcpy(protocol, "");
  strcpy(host, "");
  strcpy(port, "");
  strcpy(filename, "");
  
  strcpy(temp_url, url);
  strcpy(temp_url, strip_str(temp_url));

  //get protocol
  if (strcmp(temp_url,"")) {
    strcpy(protocol, strtok(temp_url, "://") );   
  }
  else {
    strcpy(protocol, "");       
    printf("Error in parsing URL %s\n", url);
    return;
  }

  //if protocol == file
  if (!strcmp(protocol, "file")){
    strcpy(host, "localhost");
  }
  else { //get the hostname
    p = strtok(NULL,":/");
    if (p != NULL){
      strcpy(host, p);                 
    }
    else
      strcpy(host, "");   
  }


  //if protocol == file
  if (!strcmp(protocol, "file")){
    strcpy(port, "");
  }
  else { //get port number
    p = strtok(NULL,"/");
    if (p != NULL){
      strcpy(port, p);                 
    }
    else
      strcpy(port, "");   
  }
  
  //get rest of the filename
  //add "/" to filename, if (protocol != nest)
  p = strtok(NULL,"");

  if (p != NULL){
    if (strcmp(protocol, "nest")  && strcmp(protocol, "file"))
      strcat(strcpy(filename, "/"), p);   //get file name
    else 
      strcpy(filename, p);
  }
  else {
    strcpy(filename, "");
  }

  // printf("protocol:%s\n",protocol);
  //printf("host:%s\n",host);
  //printf("filename:%s\n",filename);
  
}
