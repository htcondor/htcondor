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
  char *p;
  
  //initialize
  strcpy(protocol, "");
  strcpy(host, "");
  strcpy(filename, "");
  
  strcpy(temp_url, url);
  strcpy(temp_url, strip_str(temp_url));

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









