#include "condor_common.h"
#include "condor_config.h"

extern "C" {

int
renice_self( char* param_name ) {
	int i = 0;
#ifndef WIN32
	char* ptmp = param( param_name );
	if ( ptmp ) {
		i = atoi(ptmp);
		if ( i > 0 && i < 20 ) {
			nice(i);
		} else if ( i >= 20 ) {
			i = 19;
			nice(i);
		} else {
			i = 0;
		}
		free(ptmp);
	}
#endif
	return i;
}

} /* end of extern "C" */
