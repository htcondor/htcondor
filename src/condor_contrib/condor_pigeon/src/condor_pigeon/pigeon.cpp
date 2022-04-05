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


#include "pigeon.h"
#include "getPort.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "get_daemon_name.h"
#include "directory.h"
#include "string_list.h"
#include "my_popen.h"
#include "condor_mkstemp.h"
#include "java_config.h"
#include "condor_classad.h"

#define STDOUT_READBUF_SIZE 1024

#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
//#include <iostream>
using namespace std;


void clean(void)
{
  dprintf(D_ALWAYS, "clean called ... \n");
  char *path = getPortPath(); 
  int res = unlink(path);
  dprintf(D_ALWAYS, "Unlink %s ended with status %i and errno %i \n", path, res, errno);
  free(path);
  
}


//qpid class

Pigeon::Pigeon() { 
  m_pid    = -1;
  m_stdOut         = -1;
  m_stdErr         = -1;
  m_reaper         = -1;
  m_state          = STATE_NULL;
  m_adPubInterval  = 30;
  m_qpidHome     = NULL;
}

void Pigeon::main_config() {}

void Pigeon::initialize() {
  /*        if (m_state == STATE_RUNNING) {
  */
  MyString* qpidPort;
  char *path = NULL;
  //notify us when our process is down.
  m_reaper = daemonCore->Register_Reaper(
      "reaperQpid",
      (ReaperHandlercpp) &Pigeon::reaperResponse,
      "Qpid process reaper", (Service*) this);		

  ASSERT(m_reaper != FALSE);

  //ClassAd Initialization
  //cleanup metafile
  ArgList argClean;
  clean();

  char* proc= param("QPID_EXEC");
  if (!proc) {
  	dprintf(D_ALWAYS, "You need to specify the QPID executable as QPID_EXEC in your condor config \n");
  	EXCEPT("No qpid executable (QPID_EXEC) specified!");
  }
  std::string hostname = get_local_fqdn();
  
  ArgList arglist; 
  arglist.AppendArg("qpidd");
  char *qpidConf = param("QPID_CONF");
  if (qpidConf) {
  	arglist.AppendArg("--config");
  	arglist.AppendArg(qpidConf);
  	free(qpidConf);
  } else {
  	
  	arglist.AppendArg("-p0");
  	arglist.AppendArg("--auth");
  	arglist.AppendArg("no");
  }
  
  
  MyString argString;
  arglist.GetArgsStringForDisplay(&argString);
  dprintf(D_ALWAYS, "\n Invoking: %s\n", argString.Value());
  path = getPortPath();
  int fd_stdout = safe_open_wrapper(path, O_RDWR|O_CREAT, 0666);
  free(path);
  int fds[3] = {-1, fd_stdout, -1};
  int mm_pid = daemonCore->Create_Process(proc,arglist,PRIV_CONDOR_FINAL, 0,FALSE,FALSE,NULL,NULL,NULL,NULL,fds);
  if (mm_pid <= 0) 
    EXCEPT("Failed to launch qpid process using Create_Process.");

  dprintf(D_ALWAYS, "Launched qpid process pid=%d \n", mm_pid);
  sleep(10);
  close(fd_stdout);
 
  char *portChr = getPort(false);
  string portStr = string(portChr);
  free(portChr);
  free(proc);
  if(strcmp(portStr.c_str(),"") != 0){
    m_qpidAd.Assign("PORT", portStr.c_str());
    dprintf(D_ALWAYS,"qpid process started on port number %s \n", portStr.c_str());
  }  
  SetMyTypeName(m_qpidAd, "pigeon");
  SetTargetTypeName(m_qpidAd, "");
  std::string hostAddr = "pigeon@";
  hostAddr += hostname.c_str();
  m_qpidAd.Assign(ATTR_NAME, "pigeon"); //hostAddr.c_str());
  m_qpidAd.Assign("Key", "qpidKey");
  m_qpidAd.Assign("IP","128" );
  daemonCore->publish(&m_qpidAd); 

  //Register a timer for periodically pushing classads.
  //TODO: Make these rate and interval configurable
  dprintf(D_ALWAYS, "Calling the classAd publish()\n");
  daemonCore->Register_Timer(1, m_adPubInterval, (TimerHandlercpp) &Pigeon::publishClassAd, 
      "publishClassAd", this);

  dprintf(D_ALWAYS, "Launched qpid process pid=%d at port=|%s|\n", mm_pid,portStr.c_str());
  
  
  char *execDir = param("SBIN");
  if (execDir) {
  	dprintf(D_ALWAYS, "Declaring queues...  \n");
  	ArgList qArglist;
  	proc = (char*)malloc(strlen(execDir) + 15);
  	sprintf(proc, "%s%c%s",execDir, DIR_DELIM_CHAR, "declareQueues");
  	qArglist.AppendArg(proc);
  	qArglist.AppendArg(hostname);
  	qArglist.AppendArg(portStr.c_str());
  	mm_pid = daemonCore->Create_Process(proc,qArglist,PRIV_CONDOR_FINAL, 0,FALSE,FALSE,NULL,NULL,NULL,NULL);
  	if (mm_pid <= 0) 
		EXCEPT("Failed to launch declareQueues process using Create_Process.");
    free(proc);
    free(execDir);
	dprintf(D_ALWAYS, "QPID queues declared. \n");
   }
}



  void Pigeon::stop(bool fast) {
    if (m_state == STATE_NULL)
      return;

    dprintf(D_FULLDEBUG, "Qpid::stop() current_state = %d\n", m_state);

    if (m_pid != -1) {

      //Stopping qpid process is not going to spit out any useful return code.
      //So, lets explicity mark that we initated a stop to differentiate between
      //a termination code generated by a fault in qpid or one generated
      //by just sending a kill signal. 

      if (m_state != STATE_REINIT)
        m_state = STATE_STOP_REQUESTED;

      daemonCore->Send_Signal(m_pid, (fast ? SIGKILL : SIGTERM) );
      if (!fast) {
        dprintf(D_ALWAYS, "Created timer on daemon kill signal\n");
        m_timer = daemonCore->Register_Timer(5, 
     	 //  &Qpid::killTimer, 
            (TimerHandlercpp) &Pigeon::killTimer, 
            "qpid kill timer", this);
      }
    }
  }

int Pigeon::reaperResponse(int exit_pid, int exit_status) {
  dprintf(D_ALWAYS, "QPID daemon returned %d\n", exit_status);

  daemonCore->Cancel_Reaper(m_reaper);
  m_pid = -1;
  m_reaper = -1;

  if (m_state == STATE_REINIT) {
    m_state = STATE_NULL;
    initialize();
  }
  else if ( m_state != STATE_STOP_REQUESTED && exit_status != 0) {
    EXCEPT("qpid daemon %d was killed unexpectedly", exit_pid);
  }

  m_state = STATE_NULL;

  DC_Exit(0);

  return 0;
}

int Pigeon::killTimer() {        
  dprintf(D_FULLDEBUG, "Qpid::KillTimer()\n");
  daemonCore->Cancel_Timer(m_timer);                

  //send sigkill now!
  if (m_state != STATE_NULL)
    stop(true);

  return 0;
}


void Pigeon::publishClassAd() {
  //  dprintf(D_ALWAYS, "Calling the classAd sendUpdates()\n");
  daemonCore->UpdateLocalAd(&m_qpidAd);
  int stat = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_qpidAd, NULL, true);
  dprintf(D_ALWAYS, "************************ Updated ClassAds (status = %d)\n", stat);
}

void Pigeon::stdoutHandler(int /*pipe*/) {
  char buff[STDOUT_READBUF_SIZE];
  int bytes = 0;
  int ad_type = AD_NULL;

  while ( (bytes = daemonCore->Read_Pipe(m_stdOut, buff, 
          STDOUT_READBUF_SIZE)) > 0) {
    buff[bytes] = '\0';
    m_line_stdout += buff;
    int pos = m_line_stdout.FindChar('\n', 0);
    while (pos > 0) {                
      //Here we get a newline terminated string to process.
      MyString line = m_line_stdout.substr(0, pos);
      m_line_stdout = m_line_stdout.substr(pos+1, m_line_stdout.Length());

      if (line.find("START_AD") >= 0) {
        MyString adKey, adValue;

        ad_type = getKeyValue(line, &adKey, &adValue);
        dprintf(D_FULLDEBUG, "AD: %s type=%d\n", line.Value(), ad_type);

        if (ad_type == AD_NULL) {
          pos = m_line_stdout.FindChar('\n', 0);
          continue;
        }

        dprintf(D_FULLDEBUG, "AD: key %s, value %s\n", adKey.Value(), adValue.Value());                                        
        if (ad_type == AD_STRING)
          m_qpidAd.Assign(adKey.Value(), adValue);

        else if (ad_type == AD_INT || ad_type == AD_BOOLEAN)
          m_qpidAd.Assign(adKey.Value(), atoi(adValue.Value()));

        else if (ad_type == AD_DOUBLE)
          m_qpidAd.Assign(adKey.Value(), atof(adValue.Value()));
      }
      dprintf(D_ALWAYS, "STDOUT: %s\n", line.Value());
      pos = m_line_stdout.FindChar('\n', 0);
    }
  }
}

void Pigeon::stderrHandler(int /*pipe*/) {
  char buff[STDOUT_READBUF_SIZE];
  int bytes = 0;

  while ( (bytes = daemonCore->Read_Pipe(m_stdErr, buff, 
          STDOUT_READBUF_SIZE)) > 0) {
    buff[bytes] = '\0';
    m_line_stderr += buff;
    int pos = m_line_stderr.FindChar('\n', 0);
    while (pos > 0) {                
      //Here we get a newline terminated string to process.
      MyString line = m_line_stderr.ubstr(0, pos);
      m_line_stderr = m_line_stderr.substr(pos+1, m_line_stderr.Length());
      dprintf(D_ALWAYS, "STDERR: %s\n", line.Value());
      pos = m_line_stderr.FindChar('\n', 0);
    }
  }
}


//AD_NULL return value indicates an error in parsing the key-value from the input
//otherwise it indicates the type field in the parsed string.

int Pigeon::getKeyValue(MyString line, MyString *key, MyString *value) {
  int type = AD_NULL;
  int tokenno = -1;
  char *s1 = 0;

  line.Tokenize();

  while( (s1 = const_cast<char*>(line.GetNextToken(" ", true))) != NULL) {
    tokenno++;

    if (strlen(s1) < 1)
      goto end;

    //dprintf(D_FULLDEBUG, "Tokens %d: %s\n", tokenno, s1);
    switch (tokenno) {
      case 0:
        if (strcmp(s1, "START_AD") != 0) 
          goto end;

        break;

      case 1:                          

        if (s1[0] == 'S')
          type = AD_STRING;
        else if (s1[0] == 'I')
          type = AD_INT;
        else if (s1[0] == 'D')
          type = AD_DOUBLE;
        else if (s1[0] == 'B')
          type = AD_BOOLEAN;

        break;

      case 2:
        *key = s1;
        break;

      case 3:
        *value = s1;
        break;

      case 4:
        if (strcmp(s1, "END_AD") != 0)
          type = AD_NULL;

        break;
    }
  }

  //wrong number of tokens
  if (tokenno < 4)
    type = AD_NULL;

end:
  return type;
}
