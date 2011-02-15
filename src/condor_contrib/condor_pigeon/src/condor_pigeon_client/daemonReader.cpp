/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "subsystem_info.h"
#include "daemon.h"
#include "my_popen.h"
#include "condor_distribution.h"
#include "condor_config.h"


//function:getQpidPort- reads port # from the classAd
char* getQpidPort(char *hName){
  //int argc =0;
  //char **argv = NULL;
  
  config();
  char* port;
  MyString daemonHost = "qpid@";
  char *hostname = my_full_hostname() ;
  sprintf(hName, "%s", hostname);
  daemonHost += hostname;
  free(hostname);
  Daemon dObj(DT_GENERIC, daemonHost.Value(), NULL);
  dObj.setSubsystem("PIGEON");
  bool flag = dObj.locate();
  if(!flag){
    return "-1";
  }
  ClassAd *qpidAd = dObj.daemonAd();

  if(qpidAd){
    MyString inBuf="";
    char* eStr ="";

    qpidAd->sPrint(inBuf);
    char* mystr = (char*)inBuf.Value();
    //printf("%s \n", mystr);
    char* start =strstr(mystr,"PORT =");
    char* end =strstr(start,"\n");
    int len = end - start -9;
    port = (char*)malloc(sizeof(len+1));

    char *ports = strncpy(port,start+8,len);
    int size = sizeof(ports);
    port[len]='\0';

  } else {
  }

  return (port);
}

int main(int argc, char **argv){
  myDistro->Init(argc,argv);
  char hostname[256];
  char* res = getQpidPort(hostname);
  printf("%s\n%s\nDONE \n", hostname, res);
  return 0;
}
