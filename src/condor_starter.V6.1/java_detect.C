
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

	if(!java_config(path,args,0)) return 0;
	int benchmark_time = param_integer("JAVA_BENCHMARK_TIME",0);

	sprintf(command,"%s %s CondorJavaInfo old %d",path,args,benchmark_time);

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

