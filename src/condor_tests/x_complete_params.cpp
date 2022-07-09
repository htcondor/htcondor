

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <limits.h>
#include <float.h>

typedef enum param_info_t_type_e {
	PARAM_TYPE_STRING = 0,
	PARAM_TYPE_INT = 1,
	PARAM_TYPE_BOOL = 2,
	PARAM_TYPE_DOUBLE = 3,
	PARAM_TYPE_LONG = 4,
	PARAM_TYPE_KVP_TABLE = 14,
	PARAM_TYPE_KTP_TABLE = 15,
} param_info_t_type_t;

#define PARAM_DECLARE_TABLES
#include "param_info_tables.h"

//using namespace condor_params;

int jobpid = 0;
int myarg = 0;
const int cDefaults = sizeof(condor_params::defaults)/sizeof(condor_params::defaults[0]);


/*
 * This gets us to an argc of 21
*/
int main(int argc,char **argv)
{

	int idx = 0;
	FILE *fp;

	printf("Array members == %d\n", cDefaults);
	fp = fopen("param_perl_input","w+");
	if(fp == NULL) {
		printf("Failed to open param_perl_input\n");
	}

	for( idx = 0; idx < cDefaults; idx++ ) {
		const struct condor_params::key_value_pair *kvpair = &(condor_params::defaults[idx]);
		if ( ! kvpair->def) {
			printf("%s = NULL\n", kvpair->key);
			if (fp) fprintf(fp,"%s,NULL\n",kvpair->key);
		} else {
			printf("%s = \"%s\"\n", kvpair->key, kvpair->def->psz);
			if (fp) fprintf(fp,"%s,%s\n",kvpair->key,kvpair->def->psz);
		}
	}



	for(myarg = 2; myarg < argc; myarg++) {
		printf("%s\n",argv[myarg]);
		myarg++;
		//printf("%s\n",argv[myarg]);
		/*printf("%d\n",atoi(argv[myarg]));*/
	}

	return 0;
}
