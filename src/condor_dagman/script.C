/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
	_done         (FALSE),
    _job          (job)
{
	assert( cmd != NULL );
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
