#include "condor_io.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

const int ACCESS_READ = 0;
const int ACCESS_WRITE = 1;

void attempt_access_handler(Service *, int i, Stream *s);
int  attempt_access(char *filename, int  mode, int uid, int gid, char *scheddAddress = NULL );
	
