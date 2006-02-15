/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "dap_constants.h"
#include "dap_utility.h"

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

void parse_url(char *url, char *protocol, char *host, char *filename)
{

  char temp_url[MAXSTR];
  char unstripped[MAXSTR];
  char *p;
  
  //initialize
  strcpy(protocol, "");
  strcpy(host, "");
  strcpy(filename, "");
  
  strcpy(unstripped, url);
  strcpy(temp_url, strip_str(unstripped));

  //get protocola
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
    p = strtok(NULL,"/");
    if (p != NULL){
      strcpy(host, p);                 
    }
    else
      strcpy(host, "");   
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









