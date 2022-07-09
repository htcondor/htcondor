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


char *getHostname(char *givenHost) {
  if (givenHost == NULL) {
  		char *res = strdup(get_local_fqdn().Value());
  		return res;	
  } else {
  	return givenHost;
  }
}

//function:getQpidPort- reads port # from the classAd
char* getQpidPort(char *hName){
  
  config();
  char* port;
  MyString daemonHost = "pigeon@";

  daemonHost += hName;
  DaemonAllowLocateFull dObj(DT_GENERIC, daemonHost.Value(), NULL);
  dObj.setSubsystem("PIGEON");
  bool flag = dObj.locate(Daemon::LOCATE_FULL);
  if(!flag){
  	fprintf(stderr, "Problem locating daemon object: %s \n", dObj.error());
    return NULL;
  }
  
  ClassAd *qpidAd = dObj.daemonAd();

  if(qpidAd){
    MyString inBuf="";
    sPrintAd(inBuf, *qpidAd);
    char* start =strstr(inBuf.Value(),"PORT =");
    char* end =strstr(start,"\n");
    int len = end - start -9;
    port = (char*)malloc(sizeof(len+1));

    char *ports = strncpy(port,start+8,len);
    port[len]='\0';
    ports = NULL;
  } else {
  	fprintf(stderr, "Problem retrieving pigeon Ad \n");
  }

  return (port);
}

int main(int argc, char **argv){
  char *hostname = NULL;
  if (argc > 1) {
  	hostname = argv[1];
  }
  hostname = getHostname(hostname);
  char* res = getQpidPort(hostname);
  if (!res) {
  	printf("%s\n%s\nDONE \n", hostname, "-1");
  } else {
  	printf("%s\n%s\nDONE \n", hostname, res);  	
  	free(res);
  }
  if (argc <= 1)
  	free(hostname);
  
  return 0;
}
