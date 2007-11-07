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
#include "condor_config.h"
#include "condor_classad.h"
#include "java_detect.h"
#include "java_config.h"

ClassAd * java_detect()
{
	MyString path;
	ArgList args;
	MyString command;
	MyString args_string;
	MyString args_error;

#ifndef WIN32
	sigset_t mask;
#endif

	if(!java_config(path,&args,0)) return 0;
	int benchmark_time = param_integer("JAVA_BENCHMARK_TIME",0);

	// NOTE: this is not quite right.  If any of the jvm arguments
	// contain spaces, then V1 syntax cannot handle it, and we
	// need to pass something more sophisticated to popen that
	// works under both unix and windows.

	if(!args.GetArgsStringV1Raw(&args_string,&args_error)) {
		dprintf(D_ALWAYS,"java_detect: failed to produce jvm arguments: %s\n",
				args_error.Value());
		return 0;
	}

	command.sprintf("%s %s CondorJavaInfo old %d",path.Value(),args_string.Value(),benchmark_time);

	/*
	N.B. Certain version of Java do not set up their own signal
	masks correctly.  DaemonCore has already blocked off a bunch
	of signals.  We have to reset them, otherwise some JVMs go
	off into an infinite loop.  However, we leave SIGCHLD blocked,
	as DaemonCore has already set up a handler, but isn't prepared
	to actually handle it before the DC constructor completes.
	*/

#ifndef WIN32
	sigemptyset(&mask);
	sigaddset(&mask,SIGCHLD);
	sigprocmask(SIG_SETMASK,&mask,0);
#endif

	FILE *stream = popen(command.Value(),"r");
	if(!stream) return 0;

	int eof=0,error=0,empty=0;
	ClassAd *ad = new ClassAd(stream,"***",eof,error,empty);

	pclose(stream);

	if(error || empty) {
		delete ad;
		return 0;
	} else {
		return ad;
	}
}

