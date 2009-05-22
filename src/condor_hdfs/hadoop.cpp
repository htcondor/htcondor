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
        m_reaper         = -1;
        m_state          = STATE_NULL;
        m_adPubInterval  = 5;
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
        } else
                m_hadoopHome = MyString("./hadoop");                

        
        m_classpath.clearAll();

        MyString buff;

        //We need to put 'conf' directory path at the begining of the classpath
        //to ensure that any stale configuration file just laying around is not
        //fed into hadoop. Hadoop's site configuration in 'conf' direcotry is 
        //managed by condor.
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
        confFile.sprintf("%s/conf/%s", m_hadoopHome.Value(), m_siteFile.Value());
        dprintf(D_ALWAYS, "Config file location %s\n", confFile.Value());

        //confFile.sprintf("%s/conf/hdfs-site.xml", m_hadoopHome.Value());

        int fd = safe_create_replace_if_exists(confFile.Value(), O_CREAT|O_WRONLY);
        if (fd == -1) {
                dprintf(D_ALWAYS, "Failed to create hadoop configuration file\n");
                exit(1);
        }

        StringList xml("", "\n");;
        xml.append("<?xml version=\"1.0\"?>");
        xml.append("<?xml-stylesheet type=\"text/xsl\" href=\"configuration.xsl\"?>");
        xml.append("<!-- DON'T MODIFY this file manually, as it will be overwritten by CONDOR.-->");
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

        writeXMLParam("dfs.namenode.plugins", "edu.wisc.cs.condor.NameNodeAds", &xml);
        writeXMLParam("dfs.datanode.plugins", "edu.wisc.cs.condor.DataNodeAds", &xml);

        xml.append("</configuration>");

        char *str = xml.print_to_delimed_string();
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
        dprintf(D_ALWAYS, "Hadoop daemon returned %d\n", exit_status);

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
                else
                        startService(HADOOP_DATANODE);
        }
}

void Hadoop::startService(int type) {
        dprintf(D_ALWAYS, "Starting hadoop node service type = %d\n", type);
        ArgList arglist;
        int stdout_fd, stderr_fd;

        java_config(m_java, &arglist, &m_classpath);

        MyString stdout_file("/tmp/hdfs.stdout");
        MyString stderr_file("/tmp/hdfs.stderr");

        char *ldir = param("LOG");
        if (ldir != NULL) {

                //Tell hadoop's logger to place rolling log file inside condor's
                //local directory.
                arglist.AppendArg("-Dhadoop.root.logger=INFO,DRFA");

                MyString log_dir;
                log_dir.sprintf("-Dhadoop.log.dir=%s", ldir);
                arglist.AppendArg(log_dir.Value());
                arglist.AppendArg("-Dhadoop.log.file=hdfs.log");

                stdout_file.sprintf("%s/hdfs.stdout", ldir);
                stderr_file.sprintf("%s/hdfs.stderr", ldir);

                free(ldir);
        }

        if (type == HADOOP_NAMENODE) {
                arglist.AppendArg(m_nameNodeClass);
        }
        else {
                arglist.AppendArg(m_dataNodeClass);
       }

        /** Mapping for stdout/stderr **/
        char *file = param("HDFS_STDOUT");
        if (file) {
                stdout_file = file;
                free(file);
        }

        file = param("HDFS_STDERR");
        if (file) {
                stderr_file = file;
                free(file);
        } 

        /* Standard input and output for our this hadoop daemon */
        stdout_fd = safe_open_wrapper(stdout_file.Value(), O_CREAT|O_WRONLY);
        stderr_fd = safe_open_wrapper(stderr_file.Value(), O_CREAT|O_WRONLY);

        if (stdout_fd == -1) 
                dprintf(D_ALWAYS, "Failed to create stdout pipe for service type=%d\n", type);
        

        if (stderr_fd == -1) 
                dprintf(D_ALWAYS, "Failed to crate stderr pipe for service type=%d\n", type);
       
        int arrIO[1];

        if (! daemonCore->Create_Pipe(arrIO, true, false, true) ) {
                dprintf(D_ALWAYS, "Couldn't create a stdout pipe\n");
        } else {
                if (! daemonCore->Register_Pipe(arrIO[0], "hadoop stdout",
                        (PipeHandlercpp) &Hadoop::stdoutHandler,
                        "stdout", this) ) {

                        dprintf(D_ALWAYS, "Couldn't register stdout pipe\n");                        
                } else {
                        m_stdOut = arrIO[0];
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
                        PRIV_USER_FINAL, 
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

void Hadoop::writeXMLParam(char *key, char *value, StringList *buff) {
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
                char *match = strstr(ctmp, ".jar");
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

        while ( (bytes = daemonCore->Read_Pipe(m_stdOut, buff, 
                  STDOUT_READBUF_SIZE)) > 0) {
             buff[bytes] = '\0';
             m_line += buff;
             int pos = m_line.FindChar('\n', 0);
             if (pos > 0) {                
                    //Here we get a newline terminated string to process.
                    MyString line = m_line.Substr(0, pos-1);
                    m_line = m_line.Substr(pos+1, m_line.Length());

                    if (line.find("START_AD") >= 0) {
                            MyString adKey, adValue;

                            if (getKeyValue(line, &adKey, &adValue)) {
                                    m_hdfsAd.Assign(adKey.Value(), adValue);
                            }

                    } 
             }
        }
}

bool Hadoop::getKeyValue(MyString line, MyString *key, MyString *value) {
        bool isValid = true;
        int tokenno = -1;
        char *s1 = 0;

        line.Tokenize();
        while( (s1 = const_cast<char*>(line.GetNextToken(" ", true))) != NULL) {
                tokenno++;

                switch (tokenno) {
                        case 0:
                          if (strcmp(s1, "START_AD") != 0) 
                                isValid = false;                                                    

                          break;
                        case 1:
                          *key = s1;
                          break;

                        case 2:
                          *value = s1;
                          break;

                        case 3:
                          if (strcmp(s1, "END_AD") != 0)
                                  isValid = false;
                          
                          break;
                }
        }

        return isValid;        
}
