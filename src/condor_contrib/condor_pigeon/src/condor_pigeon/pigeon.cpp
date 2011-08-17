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
  	EXCEPT("No qpid executable (QPID_EXEC) specified!\n");
  }
  char *hostname = my_full_hostname() ;
  
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
  dprintf(D_ALWAYS, "\n chk this chk this %s\n", argString.Value());
  path = getPortPath();
  int fd_stdout = safe_open_wrapper(path, O_RDWR|O_CREAT, 0666);
  free(path);
  int fds[3] = {-1, fd_stdout, 0};
  int mm_pid = daemonCore->Create_Process(proc,arglist,PRIV_CONDOR_FINAL, 0,FALSE,NULL,NULL,NULL,NULL,fds);
  if (mm_pid <= 0) 
    EXCEPT("Failed to launch qpid process using Create_Process.\n ");

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
  m_qpidAd.SetMyTypeName(GENERIC_ADTYPE);
  m_qpidAd.SetTargetTypeName("pigeon");
  std::string hostAddr = "qpid@";
  hostAddr += hostname;
  m_qpidAd.Assign(ATTR_NAME, hostAddr.c_str());
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
  	mm_pid = daemonCore->Create_Process(proc,qArglist,PRIV_CONDOR_FINAL, 0,FALSE,NULL,NULL,NULL,NULL);
  	if (mm_pid <= 0) 
		EXCEPT("Failed to launch declareQueues process using Create_Process.\n ");
    free(proc);
    free(execDir);
	dprintf(D_ALWAYS, "QPID queues declared. \n");
   }
}

void Pigeon::writeConfigFile() {
  MyString confFile;
  char *logdir = param("LOG");
  if (logdir == NULL) 
    EXCEPT("Misconfigured QPID!: log directory is not specified\n");

  confFile.sprintf("%s/%s", logdir, m_siteFile.Value());
  free(logdir);
  dprintf(D_ALWAYS, "Config file location %s\n", confFile.Value());

  int fd = safe_create_replace_if_exists(confFile.Value(), O_CREAT|O_WRONLY);
  if (fd == -1) {
    dprintf(D_ALWAYS, "Failed to create qpid configuration file\n");
    exit(1);
  }

  StringList xml("", "\n");;
  xml.append("<?xml version=\"1.0\"?>");
  xml.append("<?xml-stylesheet type=\"text/xsl\" href=\"configuration.xsl\"?>");
  xml.append("<!-- DON'T MODIFY this file manually, as it is overwritten by CONDOR.-->");
  xml.append("<configuration>");

  char *namenode = param("QPID_NAMENODE");
  if (namenode != NULL) {
    writeXMLParam("fs.default.name", namenode, &xml);
    free(namenode);
  } 

  m_nameNodeDir    = "/tmp/qpid_name";
  char *ndir = param("QPID_NAMENODE_DIR");
  if (ndir != NULL) {
    writeXMLParam("dfs.name.dir", ndir, &xml);
    m_nameNodeDir = ndir;
    free(ndir);
  }

  char *ddir = param("QPID_DATANODE_DIR");
  if (ddir != NULL) {
    writeXMLParam("dfs.data.dir", ddir, &xml);
    free(ddir);
  }

  char *dadd = param("QPID_DATANODE_ADDRESS");
  if (dadd != NULL) {
    writeXMLParam("dfs.datanode.address", dadd, &xml);
    free(dadd);
  }

  char *daddw = param("QPID_DATANODE_WEB");
  if (daddw != NULL) {
    writeXMLParam("dfs.datanode.http.address", daddw, &xml);
    free(daddw);
  }

  char *nnaddw = param("QPID_NAMENODE_WEB");
  if (nnaddw != NULL) {
    writeXMLParam("dfs.http.address", nnaddw, &xml);
    free(nnaddw);
  }

  char *rep = param("QPID_REPLICATION");
  if (rep != NULL) {
    writeXMLParam("dfs.replication", rep, &xml);
    free(rep);
  }

  //Host and IP based filter settings
  char *qpid_allow = param("QPID_ALLOW");
  if (qpid_allow != NULL) {
    writeXMLParam("dfs.net.allow", qpid_allow, &xml);
    free(qpid_allow);
  } else {
    StringList allow_list;
    qpid_allow = param("HOSTALLOW_READ");
    if (qpid_allow != NULL) {
      allow_list.append(qpid_allow);
      free(qpid_allow);
    }

    qpid_allow = param("HOSTALLOW_WRITE");
    if(qpid_allow != NULL) {
      allow_list.append(qpid_allow);
      free(qpid_allow);
    }
    char *tmp_str = allow_list.print_to_delimed_string(",");
    ASSERT(tmp_str !=  NULL);
    writeXMLParam("dfs.net.allow", tmp_str , &xml);
    free(tmp_str);
  }

  char *qpid_deny = param("QPID_DENY");
  if (qpid_deny != NULL) {
    writeXMLParam("dfs.net.deny", qpid_deny, &xml);
    free(qpid_deny);
  } else {
    StringList deny_list;
    qpid_deny = param("HOSTDENY_READ");
    if (qpid_deny != NULL) {
      deny_list.append(qpid_deny);
      free(qpid_deny);
    }
    qpid_deny = param("HOSTDENY_WRITE");
    if(qpid_deny != NULL) {
      deny_list.append(qpid_deny); 
      free(qpid_deny);
    }
    char *tmp_str = deny_list.print_to_delimed_string(",");
    ASSERT(tmp_str !=  NULL);
    writeXMLParam("dfs.net.deny", tmp_str , &xml);
    free(tmp_str);
  }

  //TODO these shouldn't be hard-coded
  writeXMLParam("dfs.namenode.plugins", "edu.wisc.cs.condor.NameNodeAds", &xml);
  writeXMLParam("dfs.datanode.plugins", "edu.wisc.cs.condor.DataNodeAds", &xml);

  xml.append("</configuration>");

  char *str = xml.print_to_delimed_string(NULL);
  ASSERT(str != NULL);
  int len = full_write(fd, str, strlen(str));
  ASSERT(len == strlen(str));
  close(fd);
  free(str);
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
    EXCEPT("qpid daemon %d was killed unexpectedly\n", exit_pid);
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

void Pigeon::startServices() {
  char *services = param ("QPID_SERVICES");

  if (services == NULL) {
    startService(QPID_DATANODE);
  } 
  else {
    MyString s(services);

    if (s == "QPID_NAMENODE")
      startService(QPID_NAMENODE);
    else if (s == "QPID_DATANODE")
      startService(QPID_DATANODE);
    else
      startService(QPID_SECONDARY);
  }
}

void Pigeon::startService(int type) {
  dprintf(D_ALWAYS, "Starting qpid node service type = %d\n", type);
  ArgList arglist;

  java_config(m_java, &arglist, &m_classpath);

  char *ldir = param("LOG");
  if (ldir != NULL) {

    //Tell qpid's logger to place rolling log file inside condor's
    //local directory.
    char *log4j = param("QPID_LOG4J");
    MyString log4jarg;
    log4jarg.sprintf("-Dqpid.root.logger=%s,DRFA", (log4j != NULL ? log4j : "INFO"));
    arglist.AppendArg(log4jarg.Value());

    MyString log_dir;
    log_dir.sprintf("-Dqpid.log.dir=%s/QPID_Logs", ldir);
    arglist.AppendArg(log_dir.Value());
    arglist.AppendArg("-Dqpid.log.file=qpid.log");
    free(ldir);
  }

  if (type == QPID_NAMENODE) {
    arglist.AppendArg(m_nameNodeClass);
  }
  else if (type == QPID_DATANODE) {
    arglist.AppendArg(m_dataNodeClass);
  }
  else if (type == QPID_SECONDARY) {
    arglist.AppendArg(m_secondaryNodeClass);
  }

  int arrIO[3];
  arrIO[0] = -1;
  arrIO[1] = -1;
  arrIO[2] = -1;

  if (! daemonCore->Create_Pipe(&arrIO[1], true, false, true) ) {
    dprintf(D_ALWAYS, "Couldn't create a stdout pipe\n");
  } else {
    if (! daemonCore->Register_Pipe(arrIO[1], "qpid stdout",
          (PipeHandlercpp) &Pigeon::stdoutHandler,
          "stdout", this) ) {

      dprintf(D_ALWAYS, "Couldn't register stdout pipe\n");                        
    } else {
      m_stdOut = arrIO[1];
    }
  }

  if (! daemonCore->Create_Pipe(&arrIO[2], true, false, true) ) {
    dprintf(D_ALWAYS, "Couldn't create a stderr pipe\n");
  } else {
    if (! daemonCore->Register_Pipe(arrIO[2], "qpid stderr",
          (PipeHandlercpp) &Pigeon::stderrHandler, 
          "stderr", this) ) {

      dprintf(D_ALWAYS, "Couldn't register stderr, pipe\n");
    } else {
      m_stdErr = arrIO[2];
    }
  }

  Directory dir(m_nameNodeDir.Value());

  if (type == QPID_NAMENODE) {
    arglist.RemoveArg(0);
    arglist.InsertArg(m_java.Value(), 0);

    if (dir.Next() == NULL) {
      arglist.AppendArg("-format");

      MyString argString;
      arglist.GetArgsStringForDisplay(&argString);
      dprintf(D_ALWAYS, "%s\n", argString.Value());

      FILE *fp = my_popen(arglist, "w", 0);
      fwrite("Y\n", 1, 2, fp);
      int status = my_pclose(fp);
      dprintf(D_ALWAYS, "Performed a format on QPID with status %d\n", status);
    } else {

      MyString argString;                       
      arglist.AppendArg("-finalize");

      arglist.GetArgsStringForDisplay(&argString);
      dprintf(D_FULLDEBUG, "%s\n", argString.Value());

      FILE *fp2 = my_popen(arglist, "w", 0);
      fwrite("Y\n", 1, 2, fp2);
      int status = my_pclose(fp2);
      dprintf(D_FULLDEBUG, "Finalized any pending upgrades (status = %d)\n", status);
    }

    //Now we need the same command line but without '-format'
    arglist.RemoveArg(arglist.Count() - 1);

    //For now always run name server with upgrade option, In case
    //Qpid Jar files are updated to a newer version.
    arglist.AppendArg("-upgrade");
  }

  MyString argString;
  arglist.GetArgsStringForDisplay(&argString);
  dprintf(D_ALWAYS, "%s\n", argString.Value());

  m_pid = daemonCore->Create_Process( m_java.Value(),  
      arglist,
      PRIV_CONDOR_FINAL, 
      m_reaper,
      FALSE,
      NULL,
      NULL,
      NULL,
      NULL,
      arrIO
      );

  if (m_pid == FALSE) 
    EXCEPT("Failed to launch qpid process using Create_Process.\n ");

  dprintf(D_ALWAYS, "Launched qpid process %d pid=%d\n", type, m_pid);
  m_state = STATE_RUNNING;
}

void Pigeon::writeXMLParam(const char *key, const char *value, StringList *buff) {
  MyString temp;

  buff->append("<property>");

  temp.sprintf("    <name>%s</name>", key);
  buff->append(temp.Value());

  temp.sprintf("    <value>%s</value>", value);
  buff->append(temp.Value());

  buff->append("</property>");
}

void Pigeon::recurrBuildClasspath(const char *path) {
  Directory dir(path);
  dir.Rewind();

  const char *ctmp;
  while ( (ctmp = dir.Next())) {
    const char *match = strstr(ctmp, ".jar");
    if (match && strlen(match) == 4) {
      m_classpath.insert(dir.GetFullPath());
    } else if (dir.IsDirectory()) {
      //current file is a subdirectory.
      recurrBuildClasspath(dir.GetFullPath());                    
    }
  }
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
      MyString line = m_line_stdout.Substr(0, pos-1);
      m_line_stdout = m_line_stdout.Substr(pos+1, m_line_stdout.Length());

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
      MyString line = m_line_stderr.Substr(0, pos-1);
      m_line_stderr = m_line_stderr.Substr(pos+1, m_line_stderr.Length());
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
