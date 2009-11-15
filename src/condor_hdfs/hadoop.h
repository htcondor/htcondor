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

#ifndef __HADOOP_H__
#define __HADOOP_H__

#include "condor_daemon_core.h"

enum {
    HADOOP_NAMENODE,
    HADOOP_DATANODE,
    HADOOP_SECONDARY
};

enum {
    STATE_NULL,
    STATE_RUNNING,
    STATE_REINIT,
    STATE_STOP_REQUESTED

};

enum {
    AD_NULL,
    AD_STRING,
    AD_INT,
    AD_DOUBLE,
    AD_BOOLEAN
};


class Hadoop : public Service {
    public:

        Hadoop();

        int reaperResponse(int exit_pid, int exit_status);

        void killTimer();

        //figures out all the parameters required ro launch a hadoop process.
        void initialize();

        //stops all hadoop process invoked by this class earliers
        //resets the reaper and process ids' values to default. 
        //It is indirectly called by master to re-initialize 
        //the daemon being re-initialized. 
        //The choice of signals(SIGTERM, SIGKILL), to send to hadoop process,
        //depends if param 'fast' is set or not. If 'fast' is true SIGKILL is
        //send, SIGTERM otherwise

        void stop(bool fast=false);

    private:
        int m_reaper;

        int m_pid;

        int m_timer;

        int m_state;

        int m_stdOut;

        int m_stdErr;

        int m_adPubInterval;

        ClassAd m_hdfsAd;

        //keeps tracks of std output and error of  hadoop process
        MyString m_line_stdout, m_line_stderr;

        MyString m_java;

        //absoluate path to top level directory containingg hadoop installation.
        MyString m_hadoopHome;

        //Hadoop's namenode uses this directory for storing metadata about its files.
        MyString m_nameNodeDir;

        MyString m_nameNodeClass;

        MyString m_dataNodeClass;

        MyString m_secondaryNodeClass;

        //Name of hdfs's site configuration files differs among hadoop version
        //Versions > 0.19 has hdfs-site.xml 
        //versions < 0.19 has hadoop-site.xml
        MyString m_siteFile;

        //contains path of all jar files required to run
        //hadoop services.
        StringList m_classpath;
     
        //Write the hadoop-site.xml file based  on parameters specified in
        //condor_config files.
        void writeConfigFile();

        //Identifies the role of this machines (data node, name node or both)
        //and calls appropriate methods.
        void startServices();

        void startService(int /*type*/);

        void writeXMLParam(const char *key, const char *value, StringList *buff);

        void recurrBuildClasspath(const char *file);

        void publishClassAd();

        void stdoutHandler(int /*pipe*/);

        void stderrHandler(int /*pipe*/);

        int getKeyValue(MyString line, MyString *key, MyString *value);
};

#endif

