
#include "condor_common.h"
#include "string_list.h"
#include "condor_config.h"
#include "../condor_sysapi/sysapi.h"

/*
Extract the java configuration from the local config files.
The name of the java executable gets put in 'cmd', and the necessary
arguments get put in 'args'.  If you have other dirs or jarfiles
that should be placed in the classpath, provide them in 'extra_classpath'.
*/

int java_config( char *cmd, char *args, StringList *extra_classpath )
{
	char *tmp;
	char separator;

	tmp = param("JAVA");
	if(!tmp) return 0;
	strcpy(cmd,tmp);
	free(tmp);

	tmp = param("JAVA_MAXHEAP_ARGUMENT");
	if(tmp) {
		sprintf(args,"%s%dm ",tmp,sysapi_phys_memory());
		free(tmp);
	} else {
		args[0] = 0;
	}
	
	tmp = param("JAVA_CLASSPATH_ARGUMENT");
	if(!tmp) tmp = strdup("-classpath");
	if(!tmp) return 0;
	strcat(args," ");
	strcat(args,tmp);
	strcat(args," ");
	free(tmp);

	tmp = param("JAVA_CLASSPATH_SEPARATOR");
	if(tmp) {
		separator = tmp[0];
		free(tmp);
	} else {
		separator = PATH_DELIM_CHAR;
	}

	tmp = param("JAVA_CLASSPATH_DEFAULT");
	if(!tmp) tmp = strdup(".");
	if(!tmp) return 0;
	StringList classpath_list(tmp);
	free(tmp);

	int first = 1;

	classpath_list.rewind();
	while((tmp=classpath_list.next())) {
		if(!first) {
			strncat(args,&separator,1);
		} else {
			first = 0;
		}
		strcat(args,tmp);
	}

	if(extra_classpath) {
		extra_classpath->rewind();
		while((tmp=extra_classpath->next())) {
			strncat(args,&separator,1);
			strcat(args,tmp);
		}
	}

	tmp = param("JAVA_EXTRA_ARGUMENTS");
	if(tmp) {
		strcat(args," ");
		strcat(args,tmp);
		free(tmp);
	}

	return 1;
}


