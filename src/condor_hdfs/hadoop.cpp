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

Hadoop::Hadoop() { 
        m_pid    = -1;
        m_reaper         = -1;
        m_state          = STATE_NULL;
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
        buff.sprintf("%s/conf", m_hadoopHome.Value());
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

        //notify us when our process is down.
        m_reaper = daemonCore->Register_Reaper(
                        "reaperHadoop",
                        (ReaperHandlercpp) &Hadoop::reaperResponse,
                        "Hadoop process reaper", (Service*) this);		

        ASSERT(m_reaper != FALSE);

        writeConfigFile();

        startServices();
}

void Hadoop::writeConfigFile() {
        MyString confFile;
        confFile.sprintf("%s/conf/hadoop-site.xml", m_hadoopHome.Value());
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

        dprintf(D_FULLDEBUG, "Hadoop::stop()\n");

        if (m_pid != -1) {
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

        m_state = STATE_NULL;

        if (m_state == STATE_REINIT) {
                m_state = STATE_NULL;
                initialize();
        }
        else if ( exit_status != 0) {
            EXCEPT("hadoop daemon %d was killed unexpectedly\n", exit_pid);
        }

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

        MyString stdout("/tmp/hdfs.stdout");
        MyString stderr("/tmp/hdfs.stderr");

        char *ldir = param("LOG");
        if (ldir != NULL) {

                //Tell hadoop's logger to place rolling log file inside condor's
                //local directory.
                arglist.AppendArg("-Dhadoop.root.logger=INFO, DRFA");

                MyString log_dir;
                log_dir.sprintf("-Dhadoop.log.dir=%s", ldir);
                arglist.AppendArg(log_dir.Value());
                arglist.AppendArg("-Dhadoop.log.file=hdfs.log");

                stdout.sprintf("%s/hdfs.stdout", ldir);
                stderr.sprintf("%s/hdfs.stderr", ldir);

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
                stdout = file;
                free(file);
        }

        file = param("HDFS_STDERR");
        if (file) {
                stderr = file;
                free(file);
        } 

        /* Standard input and output for our this hadoop daemon */
        stdout_fd = safe_open_wrapper(stdout.Value(), O_CREAT|O_WRONLY);
        stderr_fd = safe_open_wrapper(stderr.Value(), O_CREAT|O_WRONLY);

        if (stdout_fd == -1) 
                dprintf(D_ALWAYS, "Failed to create stdout pipe for service type=%d\n", type);
        

        if (stderr_fd == -1) 
                dprintf(D_ALWAYS, "Failed to crate stderr pipe for service type=%d\n", type);
       

        int arrIO[3];
        arrIO[0] = 0;
        arrIO[1] = stdout_fd;
        arrIO[2] = stderr_fd;

        Directory dir(m_nameNodeDir.Value());

        if (type == HADOOP_NAMENODE && dir.Next() == NULL) {
                arglist.AppendArg("-format");

                MyString argString;
                arglist.GetArgsStringForDisplay(&argString);
                dprintf(D_ALWAYS, "%s\n", argString.Value());
              
                FILE *fp = my_popen(arglist, "w", 0);
                fwrite("Y", 1, 1, fp);
                fclose(fp);

                //Now we need the same command line but without '-format'
                arglist.RemoveArg(arglist.Count() - 1);
        }
        

        MyString argString;
        arglist.GetArgsStringForDisplay(&argString);
        dprintf(D_FULLDEBUG, "%s\n", argString.Value());

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
                EXCEPT("Failed to launch requested hadoop daemon.\n");

        dprintf(D_ALWAYS, "Launched hadoop service %d pid=%d\n", type, m_pid);
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
