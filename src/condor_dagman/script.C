#include "condor_common.h"
#include "condor_string.h"

#include "script.h"
#include "util.h"
#include "job.h"
#include "types.h"

#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

//-----------------------------------------------------------------------------
Script::Script( bool post, char* cmd, Job* job ) :
    _post         (post),
    _retValScript (-1),
    _retValJob    (-1),
    _logged       (false),
	_pid		  (0),
    _job          (job)
{
    _cmd = strnewp (cmd);
}

//-----------------------------------------------------------------------------
Script::~Script () {
    delete [] _cmd;
    // Don't delete the _job pointer!
}

//-----------------------------------------------------------------------------
int
Script::BackgroundRun( int reaperId )
{
	// construct command line
    const char *delimiters = " \t";
    char * token;
    string send;
    char * cmd = strnewp(_cmd);
    for (token = strtok (cmd,  delimiters) ; token != NULL ;
         token = strtok (NULL, delimiters)) {
        if      (!strcasecmp(token, "$LOG"   )) send += _logged ? '1' : '0';
        else if (!strcasecmp(token, "$JOB"   )) send += _job->GetJobName();
        else if (!strcasecmp(token, "$RETURN")) send += _retValJob;
        else                                    send += token;

        send += ' ';
    }

	_pid = daemonCore->Create_Process( cmd, (char*) send.str(),
									   PRIV_UNKNOWN, reaperId, TRUE,
									   NULL, NULL, FALSE, NULL, NULL, 0 );
    delete [] cmd;
	return _pid;
}
