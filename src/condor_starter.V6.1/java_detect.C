
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "java_detect.h"
#include "java_config.h"

ClassAd * java_detect()
{
	char path[ATTRLIST_MAX_EXPRESSION];
	char args[ATTRLIST_MAX_EXPRESSION];
	char command[_POSIX_ARG_MAX];

#ifndef WIN32
	sigset_t mask;
#endif

	if(!java_config(path,args,0)) return 0;
	int benchmark_time = param_integer("JAVA_BENCHMARK_TIME",0);

	sprintf(command,"%s %s CondorJavaInfo old %d",path,args,benchmark_time);
	
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

