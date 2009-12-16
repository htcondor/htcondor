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

#ifndef CONDOR_HDFS_IO
#define CONDOR_HDFS_IO

#include "CondorError.h"
#include "jni.h"

typedef void* hdfsFS;

typedef struct JNIEnv_ JNIEnv;

#define SUBSYSTEM_STRING strdup("hdfs")

template <class Key, class Value> class HashTable;
typedef HashTable <MyString, hdfsFS> ConnectionHash;

//int my_hash_func(const MyString &key);

class HdfsIO {

public:
        HdfsIO();

        ~HdfsIO();

        int put_file(const char */*hdfs_url*/, const char * /*dest_url*/, CondorError &error);

        int get_file(const char */*local_url*/, const char * /*hdfs_url*/, CondorError &error);
        
private:
        //represent connection to local-filesystem and also
        //hadoop file system. These are lazily initialized.
        static hdfsFS HDFS, LFS; 

        MyString m_classpath;

        //ip:port of name node server
        MyString m_nameServer;

        static int refCount;

        ConnectionHash *connection_hash;

        //initialize and 
        int validate(const char * /*url*/, const char * /* url2 */, CondorError &error);

        //returns the local copy of JNIEnv object if it is already
        //created or creates a new one and returns.
        JNIEnv * getLocalJNIEnv();

        //There is a similar method in hdfs.c, but we probably like to
        //read exception in our code. The returned char* will be owned
        //by CondorError object, so we can't use MyString here. 
        void getMsgFromExcep(CondorError *, char * /*def_msg*/);

        //TODO:I ended up using this method at two other places. I guess
        //we can put it in java_config utility class in Condor. 

        //TODO: I guess once we know the classpath jars we can just
        //place the resolved values inside config file?
        void recurrBuildClasspath(const char *path);

        //helper method for deleting jni references. 
        void deleteLocalJNIRef(jobject jobj);
};

#endif
