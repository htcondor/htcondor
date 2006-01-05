/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "java_detect.h"
#include "java_config.h"

ClassAd * java_detect()
{
	char path[ATTRLIST_MAX_EXPRESSION];
	ArgList args;
	char command[_POSIX_ARG_MAX];
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

	sprintf(command,"%s %s CondorJavaInfo old %d",path,args_string.Value(),benchmark_time);

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

	FILE *stream = popen(command,"r");
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

