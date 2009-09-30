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


#include "hadoop.h"
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

Hadoop::Hadoop() { 
        m_pid    = -1;
        m_stdOut         = -1;
        m_stdErr         = -1;
        m_reaper         = -1;
        m_state          = STATE_NULL;
        m_adPubInterval  = 5;
        m_hadoopHome     = NULL;
        m_timer          = -1;
}

void Hadoop::initialize() {
        if (m_state == STATE_RUNNING) {
                //hadoop is already running, stopping it!
                m_state = STATE_REINIT;
                stop();

                //actual re-initialization will take place after
                //process reaper has been called.
                return; 
        }

        //parent directory of hadoop installation
        char *hh = param("HDFS_HOME");
        if (hh != NULL) {
                m_hadoopHome = MyString(hh);
                free(hh);
        } else {
                char * rd = param("RELEASE_DIR");
                if (rd != NULL) {
                        MyString tmp;
                        tmp.sprintf("%s/libexec/hdfs", rd);
                        m_hadoopHome = tmp;                
                        free(rd);
                }                                 
        }

        if (m_hadoopHome == NULL)
                EXCEPT("Misconfigured HDFS! Please specify a location of hadoop installation directory\n");                 
        else {
                Directory dir(m_hadoopHome.Value());
                if (dir.Next() == NULL)
                        EXCEPT("Misconfigure HDFS! Please specify a valid path to hadoop installation directory");
        }
        
        m_classpath.clearAll();

        MyString buff;

        //We need to put 'conf' directory path at the begining of the classpath
        //to ensure that any stale configuration file just laying around is not
        //fed into hadoop. Hadoop's site configuration in 'conf' direcotry is 
        //managed by condor.
        char *logdir = param("LOG");
        if (logdir != NULL) {
                buff.sprintf("%s/", logdir);
                m_classpath.insert(buff.Value());
                free(logdir);
        }
        buff.sprintf("%s/conf", m_hadoopHome.Value());
        m_classpath.insert(buff.Value());

        buff.sprintf("%s/", m_hadoopHome.Value());
        m_classpath.insert(buff.Value());

        buff.sprintf("%s/lib", m_hadoopHome.Value());
        recurrBuildClasspath(buff.Value());

        m_nameNodeClass  = "org.apache.hadoop.hdfs.server.namenode.NameNode";
        char *nclass = param("HDFS_NAMENODE_CLASS");
        if (nclass != NULL) {
                m_nameNodeClass = nclass;
                free(nclass);
        }

        m_dataNodeClass  = "org.apache.hadoop.hdfs.server.datanode.DataNode";
        char *dclass = param("HDFS_DATANODE_CLASS");
        if (dclass != NULL) {
                m_dataNodeClass = dclass;
                free(dclass);
        }

        m_secondaryNodeClass = "org.apache.hadoop.hdfs.server.namenode.SecondaryNameNode";
        char *snclass = param("HDFS_SECONDARYNODE_CLASS");
        if (snclass != NULL) {
                m_secondaryNodeClass = snclass;
                free(snclass);
        }

        m_siteFile = "hdfs-site.xml";
        char *sitef = param("HDFS_SITE_FILE");
        if (sitef != NULL) {
                m_siteFile = sitef;
                free(sitef);
        }

        char *adPubInt = param("HDFS_AD_PUBLISH_INTERVAL");
        if (adPubInt != NULL) {
                int intv =  atoi(adPubInt);
                if (intv != 0)
                        m_adPubInterval = intv;

                free(adPubInt);
        }

        //notify us when our process is down.
        m_reaper = daemonCore->Register_Reaper(
                        "reaperHadoop",
                        (ReaperHandlercpp) &Hadoop::reaperResponse,
                        "Hadoop process reaper", (Service*) this);		

        ASSERT(m_reaper != FALSE);

        writeConfigFile();

        startServices();

        //ClassAd Initialization
        m_hdfsAd.SetMyTypeName(GENERIC_ADTYPE);
        m_hdfsAd.SetTargetTypeName("hdfs");
        m_hdfsAd.Assign(ATTR_NAME, "hdfs");
        daemonCore->publish(&m_hdfsAd); 

        //Register a timer for periodically pushing classads.
        //TODO: Make these rate and interval configurable
        daemonCore->Register_Timer(1, m_adPubInterval, (TimerHandlercpp) &Hadoop::publishClassAd, 
                    "publishClassAd", this);

}

void Hadoop::writeConfigFile() {
        MyString confFile;
        char *logdir = param("LOG");
        if (logdir == NULL) 
            EXCEPT("Misconfigured HDFS!: log directory is not specified\n");

        confFile.sprintf("%s/%s", logdir, m_siteFile.Value());
        free(logdir);
        dprintf(D_ALWAYS, "Config file location %s\n", confFile.Value());

        int fd = safe_create_replace_if_exists(confFile.Value(), O_CREAT|O_WRONLY);
        if (fd == -1) {
                dprintf(D_ALWAYS, "Failed to create hadoop configuration file\n");
                exit(1);
        }

        StringList xml("", "\n");;
        xml.append("<?xml version=\"1.0\"?>");
        xml.append("<?xml-stylesheet type=\"text/xsl\" href=\"configuration.xsl\"?>");
        xml.append("<!-- DON'T MODIFY this file manually, as it is overwritten by CONDOR.-->");
        xml.append("<configuration>");

        char *namenode = param("HDFS_NAMENODE");
        if (namenode != NULL) {
                writeXMLParam("fs.default.name", namenode, &xml);
                free(namenode);
        } 

        m_nameNodeDir    = "/tmp/hadoop_name";
        char *ndir = param("HDFS_NAMENODE_DIR");
        if (ndir != NULL) {
                writeXMLParam("dfs.name.dir", ndir, &xml);
                m_nameNodeDir = ndir;
                free(ndir);
        }

        char *ddir = param("HDFS_DATANODE_DIR");
        if (ddir != NULL) {
                writeXMLParam("dfs.data.dir", ddir, &xml);
                free(ddir);
        }

        char *dadd = param("HDFS_DATANODE_ADDRESS");
        if (dadd != NULL) {
                writeXMLParam("dfs.datanode.address", dadd, &xml);
                free(dadd);
        }

        char *daddw = param("HDFS_DATANODE_WEB");
        if (daddw != NULL) {
                writeXMLParam("dfs.datanode.http.address", daddw, &xml);
                free(daddw);
        }

        char *nnaddw = param("HDFS_NAMENODE_WEB");
        if (nnaddw != NULL) {
                writeXMLParam("dfs.http.address", nnaddw, &xml);
                free(nnaddw);
        }

        char *rep = param("HDFS_REPLICATION");
        if (rep != NULL) {
                writeXMLParam("dfs.replication", rep, &xml);
                free(rep);
        }

        //Host and IP based filter settings
        char *hdfs_allow = param("HDFS_ALLOW");
        if (hdfs_allow != NULL) {
                writeXMLParam("dfs.net.allow", hdfs_allow, &xml);
                free(hdfs_allow);
        } else {
                StringList allow_list;
                hdfs_allow = param("HOSTALLOW_READ");
                if (hdfs_allow != NULL) {
                        allow_list.append(hdfs_allow);
                        free(hdfs_allow);
                }

                hdfs_allow = param("HOSTALLOW_WRITE");
                if(hdfs_allow != NULL) {
                        allow_list.append(hdfs_allow);
                        free(hdfs_allow);
                }
                char *tmp_str = allow_list.print_to_delimed_string(",");
                writeXMLParam("dfs.net.allow", tmp_str, &xml);
                free(tmp_str);
        }

        char *hdfs_deny = param("HDFS_DENY");
        if (hdfs_deny != NULL) {
                writeXMLParam("dfs.net.deny", hdfs_deny, &xml);
                free(hdfs_deny);
        } else {
                StringList deny_list;
                hdfs_deny = param("HOSTDENY_READ");
                if (hdfs_deny != NULL) {
                        deny_list.append(hdfs_deny);
                        free(hdfs_deny);
                }
                hdfs_deny = param("HOSTDENY_WRITE");
                if(hdfs_deny != NULL) {
                        deny_list.append(hdfs_deny); 
                        free(hdfs_deny);
                }
                char *tmp_str = deny_list.print_to_delimed_string(",");
                writeXMLParam("dfs.net.deny", tmp_str, &xml);
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


void Hadoop::stop(bool fast) {
        if (m_state == STATE_NULL)
            return;

        dprintf(D_FULLDEBUG, "Hadoop::stop() current_state = %d\n", m_state);

        if (m_pid != -1) {

                //Stopping hdfs process is not going to spit out any useful return code.
                //So, lets explicity mark that we initated a stop to differentiate between
                //a termination code generated by a fault in hadoop or one generated
                //by just sending a kill signal. 

                if (m_state != STATE_REINIT)
                        m_state = STATE_STOP_REQUESTED;

                daemonCore->Send_Signal(m_pid, (fast ? SIGKILL : SIGTERM) );
                if (!fast) {
                        dprintf(D_ALWAYS, "Created timer on daemon kill signal\n");
                        m_timer = daemonCore->Register_Timer(5, 
                                     (Eventcpp) &Hadoop::killTimer, 
                                     "hadoop kill timer", this);
                }
        }
}

int Hadoop::reaperResponse(int exit_pid, int exit_status) {
        dprintf(D_ALWAYS, "HDFS daemon returned %d\n", exit_status);

        daemonCore->Cancel_Reaper(m_reaper);
        m_pid = -1;
        m_reaper = -1;

        if (m_state == STATE_REINIT) {
                m_state = STATE_NULL;
                initialize();
        }
        else if ( m_state != STATE_STOP_REQUESTED && exit_status != 0) {
            EXCEPT("hadoop daemon %d was killed unexpectedly\n", exit_pid);
        }

        m_state = STATE_NULL;

        DC_Exit(0);

        return 0;
}

int Hadoop::killTimer() {        
        dprintf(D_FULLDEBUG, "Hadoop::KillTimer()\n");
        daemonCore->Cancel_Timer(m_timer);                

        //send sigkill now!
        if (m_state != STATE_NULL)
            stop(true);

		return 0;
}

void Hadoop::startServices() {
        char *services = param ("HDFS_SERVICES");

        if (services == NULL) {
                startService(HADOOP_DATANODE);
        } 
        else {
                MyString s(services);
                
                if (s == "HDFS_NAMENODE")
                        startService(HADOOP_NAMENODE);
                else if (s == "HDFS_DATANODE")
                        startService(HADOOP_DATANODE);
                else
                        startService(HADOOP_SECONDARY);
        }
}

void Hadoop::startService(int type) {
        dprintf(D_ALWAYS, "Starting hadoop node service type = %d\n", type);
        ArgList arglist;

        java_config(m_java, &arglist, &m_classpath);

        char *ldir = param("LOG");
        if (ldir != NULL) {

                //Tell hadoop's logger to place rolling log file inside condor's
                //local directory.
                char *log4j = param("HDFS_LOG4J");
                MyString log4jarg;
                log4jarg.sprintf("-Dhadoop.root.logger=%s,DRFA", (log4j != NULL ? log4j : "INFO"));
                arglist.AppendArg(log4jarg.Value());

                MyString log_dir;
                log_dir.sprintf("-Dhadoop.log.dir=%s/HDFS_Logs", ldir);
                arglist.AppendArg(log_dir.Value());
                arglist.AppendArg("-Dhadoop.log.file=hdfs.log");
                free(ldir);
        }

        if (type == HADOOP_NAMENODE) {
                arglist.AppendArg(m_nameNodeClass);
        }
        else if (type == HADOOP_DATANODE) {
                arglist.AppendArg(m_dataNodeClass);
        }
        else if (type == HADOOP_SECONDARY) {
                arglist.AppendArg(m_secondaryNodeClass);
        }

        int arrIO[3];
        arrIO[0] = -1;
        arrIO[1] = -1;
        arrIO[2] = -1;

        if (! daemonCore->Create_Pipe(&arrIO[1], true, false, true) ) {
                dprintf(D_ALWAYS, "Couldn't create a stdout pipe\n");
        } else {
                if (! daemonCore->Register_Pipe(arrIO[1], "hadoop stdout",
                        (PipeHandlercpp) &Hadoop::stdoutHandler,
                        "stdout", this) ) {

                        dprintf(D_ALWAYS, "Couldn't register stdout pipe\n");                        
                } else {
                        m_stdOut = arrIO[1];
                }
        }

        if (! daemonCore->Create_Pipe(&arrIO[2], true, false, true) ) {
                dprintf(D_ALWAYS, "Couldn't create a stderr pipe\n");
        } else {
                if (! daemonCore->Register_Pipe(arrIO[2], "hadoop stderr",
                        (PipeHandlercpp) &Hadoop::stderrHandler, 
                        "stderr", this) ) {

                        dprintf(D_ALWAYS, "Couldn't register stderr, pipe\n");
                } else {
                        m_stdErr = arrIO[2];
                }
        }

        Directory dir(m_nameNodeDir.Value());

        if (type == HADOOP_NAMENODE) {
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
                        dprintf(D_ALWAYS, "Performed a format on HDFS with status %d\n", status);
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
                //Hadoop Jar files are updated to a newer version.
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
                EXCEPT("Failed to launch hadoop process using Create_Process.\n ");

        dprintf(D_ALWAYS, "Launched hadoop process %d pid=%d\n", type, m_pid);
        m_state = STATE_RUNNING;
}

void Hadoop::writeXMLParam(const char *key, const char *value, StringList *buff) {
        MyString temp;

        buff->append("<property>");

        temp.sprintf("    <name>%s</name>", key);
        buff->append(temp.Value());

        temp.sprintf("    <value>%s</value>", value);
        buff->append(temp.Value());

        buff->append("</property>");
}

void Hadoop::recurrBuildClasspath(const char *path) {
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

void Hadoop::publishClassAd() {
       daemonCore->UpdateLocalAd(&m_hdfsAd);
       int stat = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_hdfsAd, NULL, true);
       dprintf(D_FULLDEBUG, "Updated ClassAds (status = %d)\n", stat);
}

void Hadoop::stdoutHandler(int /*pipe*/) {
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
                                    m_hdfsAd.Assign(adKey.Value(), adValue);

                            else if (ad_type == AD_INT || ad_type == AD_BOOLEAN)
                                    m_hdfsAd.Assign(adKey.Value(), atoi(adValue.Value()));

                            else if (ad_type == AD_DOUBLE)
                                    m_hdfsAd.Assign(adKey.Value(), atof(adValue.Value()));
                    }
                    dprintf(D_ALWAYS, "STDOUT: %s\n", line.Value());
                    pos = m_line_stdout.FindChar('\n', 0);
             }
        }
}

void Hadoop::stderrHandler(int /*pipe*/) {
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

int Hadoop::getKeyValue(MyString line, MyString *key, MyString *value) {
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
