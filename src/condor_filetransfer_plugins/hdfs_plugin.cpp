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

#include "condor_common.h"
#include "hdfs_plugin.h"
#include "condor_config.h"
#include "directory.h"
#include "HashTable.h"
#include "setenv.h"

#include "hdfs.h"
#include "hdfsJniHelper.h"

//We only need to create JVM and hdfs connection once.
//This is kind of optimization but also because libhdfs uses
//global JNI references so our JVM and FS will always be
//same in all instances of this class.

hdfsFS HdfsIO::HDFS = NULL;
hdfsFS HdfsIO::LFS = NULL;

int HdfsIO::refCount = 0;

unsigned int my_hash_func(const MyString &key) {
        return key.Hash();
}

HdfsIO::HdfsIO() {
        m_nameServer = NULL;
        refCount++;
        connection_hash = new ConnectionHash(10, my_hash_func); 
}

HdfsIO::~HdfsIO() {
        refCount--;

        if (refCount == 0) {
            hdfsDisconnect(HDFS);
            hdfsDisconnect(LFS);
            HDFS = NULL;
            LFS = NULL;
        }

        delete connection_hash;

        //We can make destroy JVM call here in case we are sure that
        //all JVM operations are made through the JNIEnv object created
        //by this class
}

int HdfsIO::get_file(const char *hdfs_url, const char *dest_url, CondorError &error) {        
        if (validate(hdfs_url, dest_url, error)) {
                return -1;
        }

        int retCode = hdfsCopy(HDFS, hdfs_url, LFS, dest_url);
        if (retCode == -1) {

                if (getLocalJNIEnv() == NULL) {
                        error.push(SUBSYSTEM_STRING, 1, 
                                        strdup("Failed to get JNI handle"));
                        return retCode;
                }

                getMsgFromExcep(&error,
                    strdup("Unknown error occured while copying!"));
        }

        return retCode;
}

int HdfsIO::put_file(const char *src_url, const char *hdfs_url, CondorError &error) {
        if (validate(src_url, hdfs_url, error)) {
                return -1;
        }

        int retCode = hdfsCopy(LFS, src_url, HDFS, hdfs_url);
        if (retCode == -1) {

                getMsgFromExcep(&error, 
                    strdup("Unknown error occured while copying"));
        }

        return retCode;
}

int HdfsIO::validate(const char *url1, const char *url2, CondorError &error) {
        if (!url1 || !url2) {
                error.push(SUBSYSTEM_STRING, 1, 
                     strdup("Requested src and/or dest URL was NULL"));
                return 1;
        }

        StringList host_in_url(url1, "/");
        host_in_url.rewind();
        char *protocol = host_in_url.next();
        if (protocol == NULL && strcmp(protocol, "hdfs:") != 0) {
                error.push(SUBSYSTEM_STRING, 1, 
                                strdup("Invalid Protocol in source url"));
                return 1;
        }
        char *nameS = host_in_url.next();
        if (nameS == NULL) {
                error.push(SUBSYSTEM_STRING, 1,
                                strdup("Invalid source URL"));
                return 1;
        }

        //if there exist already a connection for given server
        MyString key(nameS);
        connection_hash->lookup(key, HDFS);
        if (!HDFS) {
                //first parse the url for getting a host:port string
               StringList sl(nameS, ":");

                //CLASSPATH variable is required.
                char *home = param("HDFS_HOME");
                if (home == NULL) {
                        error.push(SUBSYSTEM_STRING, 1, 
                            strdup("Missing HDFS_HOME in config file"));
                        return 1;
                }
                MyString buff;
                buff.sprintf("%s/conf", home);
                m_classpath = buff;
                buff.sprintf("%s/lib", home);
                recurrBuildClasspath(buff.Value());
                free(home);

                SetEnv("CLASSPATH", m_classpath.Value());

                sl.rewind();
                char *host = sl.next();
                char *port = sl.next();

                HDFS = hdfsConnect(host, atoi(port));
                if (!HDFS) {                        
                        getMsgFromExcep(&error, 
                            strdup("Failed to create remote file system object"));
                        return 1;
                }            

                connection_hash->insert(key, HDFS);
        }

        if (!LFS) {
                LFS = hdfsConnect(NULL, 0);
                if (!LFS) {
                        getMsgFromExcep(&error, 
                             strdup("Failed to create a local file system object"));
                        return 1;
                }
        }

        return 0;
}

void HdfsIO::getMsgFromExcep(CondorError *eStack, char* def_msg) {
        jthrowable throwObj = NULL;
        jthrowable excp1 = NULL, excp2 = NULL, excp3 = NULL;
        JNIEnv *env = getLocalJNIEnv();

        if (env == NULL) {
                eStack->push(SUBSYSTEM_STRING, 1, def_msg);
                return;
        }

        if ( (throwObj = env->ExceptionOccurred()) == NULL) { 
                eStack->push(SUBSYSTEM_STRING, 1, def_msg);
                return;
        }

        RetVal msgObj; 
        invokeMethod(env, &msgObj, &excp1, INSTANCE, throwObj, "java/lang/Exception", 
            "getMessage", "()Ljava/lang/String;");

        jboolean isCopy;
        const char *cmsg = env->GetStringUTFChars( (jstring)(msgObj.l), &isCopy);
       
        //copy exception stack trace
        RetVal obj;
        invokeMethod(env, &obj, &excp2, INSTANCE, throwObj, "java/lang/Exception", 
            "getStackTrace", "()[Ljava/lang/StackTraceElement;");

        if (obj.l != NULL) {
                jobjectArray traceElements = (jobjectArray) obj.l;
                jint size = env->GetArrayLength(traceElements);

                int i = 0;
                for (i=size-1 ; i>=0; i--) {
                    RetVal excElem;
                    jobject trace = env->GetObjectArrayElement(traceElements, i);
                    if (trace == NULL) {
                            break;
                    }

                    invokeMethod(env, &excElem, &excp3, INSTANCE, trace, 
                            "java/lang/StackTraceElement", "toString", "()Ljava/lang/String;");
                    if (excElem.l == NULL) {
                            break;
                    }

                    jboolean temp;
                    const char *msg2 = env->GetStringUTFChars( (jstring)(excElem.l), &temp);
                    eStack->push(SUBSYSTEM_STRING, 1, msg2);

                    deleteLocalJNIRef(excElem.l);
                    deleteLocalJNIRef(excp3);
                }
        }

        if (cmsg) {
                eStack->push(SUBSYSTEM_STRING, 1, cmsg);
                //any thing that go into error stack is free'd by 
                //CondorError. Here our default message isn't. 
                free(def_msg);
        } else {
                eStack->push(SUBSYSTEM_STRING, 1, def_msg);
        }
        
        env->ExceptionClear();
        deleteLocalJNIRef(msgObj.l);
        deleteLocalJNIRef(obj.l);
        deleteLocalJNIRef(throwObj);
        deleteLocalJNIRef(excp1);
        deleteLocalJNIRef(excp2);
}

JNIEnv * HdfsIO::getLocalJNIEnv() {
        return getJNIEnv();
}

void HdfsIO::deleteLocalJNIRef(jobject o) {
        if (o) {
                getLocalJNIEnv()->DeleteLocalRef(o);
        }
}

void HdfsIO::recurrBuildClasspath(const char *path) {
        Directory dir(path);
        dir.Rewind();

        char classpath_seperator;
#ifdef WIN32
        classpath_seperator = ';';
#else
        classpath_seperator = ':';
#endif

        const char *ctmp;
        while ( (ctmp = dir.Next())) {
                const char *match = strstr(ctmp, ".jar");
                if (match && strlen(match) == 4) {
                        m_classpath += classpath_seperator;
                        m_classpath += dir.GetFullPath();
                } else if (dir.IsDirectory()) {
                        //current file is a subdirectory.
                        recurrBuildClasspath(dir.GetFullPath());                    
                }
        }
}
