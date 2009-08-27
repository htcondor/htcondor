/**
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef LIBHDFS_JNI_HELPER_H
#define LIBHDFS_JNI_HELPER_H

#include <jni.h>
#include <stdio.h>

#include <stdlib.h>
#include <stdarg.h>
#include <search.h>
#ifdef WIN32
#include <Windows.h>
#include "hashwin32.h"
#else
#include <pthread.h>
#endif
#include <errno.h>

#ifdef WIN32
static CRITICAL_SECTION hdfsHashMutex;
static CRITICAL_SECTION jvmMutex;
static volatile int hashTableInited = 0;

#define LOCK_HASH_TABLE() EnterCriticalSection(&hdfsHashMutex)
#define UNLOCK_HASH_TABLE() LeaveCriticalSection(&hdfsHashMutex)
#define LOCK_JVM_MUTEX() EnterCriticalSection(&jvmMutex)
#define UNLOCK_JVM_MUTEX() LeaveCriticalSection(&jvmMutex)

#define snprintf _snprintf

#ifdef HDFS_DLL
#define HDFS_DLL_API __declspec(dllexport)
#else
#define HDFS_DLL_API __declspec(dllimport)
#endif
#else
#define HDFS_DLL_API

static pthread_mutex_t hdfsHashMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t jvmMutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int hashTableInited = 0;

#define LOCK_HASH_TABLE() pthread_mutex_lock(&hdfsHashMutex)
#define UNLOCK_HASH_TABLE() pthread_mutex_unlock(&hdfsHashMutex)
#define LOCK_JVM_MUTEX() pthread_mutex_lock(&jvmMutex)
#define UNLOCK_JVM_MUTEX() pthread_mutex_unlock(&jvmMutex)

#endif

#define PATH_SEPARATOR ':'

#define USER_CLASSPATH "/home/y/libexec/hadoop/conf:/home/y/libexec/hadoop/lib/hadoop-0.1.0.jar"

#ifdef __cplusplus
extern "C" {
#endif


	/** Denote the method we want to invoke as STATIC or INSTANCE */
	typedef enum {
		STATIC,
		INSTANCE
	} MethType;


	/** Used for returning an appropriate return value after invoking
	* a method
	*/
	typedef jvalue RetVal;

	/** Used for returning the exception after invoking a method */
	typedef jthrowable Exc;

#define vmBufLength 1

	/** invokeMethod: Invoke a Static or Instance method.
	* className: Name of the class where the method can be found
	* methName: Name of the method
	* methSignature: the signature of the method "(arg-types)ret-type"
	* methType: The type of the method (STATIC or INSTANCE)
	* instObj: Required if the methType is INSTANCE. The object to invoke
	the method on.
	* env: The JNIEnv pointer
	* retval: The pointer to a union type which will contain the result of the
	method invocation, e.g. if the method returns an Object, retval will be
	set to that, if the method returns boolean, retval will be set to the
	value (JNI_TRUE or JNI_FALSE), etc.
	* exc: If the methods throws any exception, this will contain the reference
	* Arguments (the method arguments) must be passed after methSignature
	* RETURNS: -1 on error and 0 on success. If -1 is returned, exc will have 
	a valid exception reference, and the result stored at retval is undefined.
	*/
	HDFS_DLL_API
		int invokeMethod(JNIEnv *env, RetVal *retval, Exc *exc, MethType methType,
		jobject instObj, const char *className, const char *methName, 
		const char *methSignature, ...);

	/** constructNewObjectOfClass: Invoke a constructor.
	* className: Name of the class
	* ctorSignature: the signature of the constructor "(arg-types)V"
	* env: The JNIEnv pointer
	* exc: If the ctor throws any exception, this will contain the reference
	* Arguments to the ctor must be passed after ctorSignature 
	*/
	HDFS_DLL_API
		jobject constructNewObjectOfClass(JNIEnv *env, Exc *exc, const char *className, 
		const char *ctorSignature, ...);

	HDFS_DLL_API
		jmethodID methodIdFromClass(const char *className, const char *methName, 
		const char *methSignature, MethType methType, 
		JNIEnv *env);

	HDFS_DLL_API
		jclass globalClassReference(const char *className, JNIEnv *env);

	/** classNameOfObject: Get an object's class name.
	* @param jobj: The object.
	* @param env: The JNIEnv pointer.
	* @return Returns a pointer to a string containing the class name. This string
	* must be freed by the caller.
	*/
	HDFS_DLL_API
		char *classNameOfObject(jobject jobj, JNIEnv *env);

	/** getJNIEnv: A helper function to get the JNIEnv* for the given thread.
	* If no JVM exists, then one will be created. JVM command line arguments
	* are obtained from the LIBHDFS_OPTS environment variable.
	* @param: None.
	* @return The JNIEnv* corresponding to the thread.
	* */
	HDFS_DLL_API
		JNIEnv* getJNIEnv(void);

	HDFS_DLL_API
		jarray constructNewArrayString(JNIEnv *env, Exc *exc, const char **elements, int size) ;

#ifdef __cplusplus
}
#endif



#endif /*LIBHDFS_JNI_HELPER_H*/

/**
* vim: ts=4: sw=4: et:
*/
