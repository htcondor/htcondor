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
#include "condor_string.h"

#include "script.h"
#include "util.h"
#include "job.h"
#include "condor_id.h"

#include "HashTable.h"
#include "env.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;

//-----------------------------------------------------------------------------
Script::Script( bool post, const char* cmd, const Job* node ) :
    _post         (post),
    _retValScript (-1),
    _retValJob    (-1),
	_pid		  (0),
	_done         (FALSE),
	_node         (node)
{
	ASSERT( cmd != NULL );
    _cmd = strnewp (cmd);
    return;
}

//-----------------------------------------------------------------------------
Script::~Script () {
    delete [] _cmd;
    return;
}

const char *Script::GetNodeName()
{
    return _node->GetJobName();
}

//-----------------------------------------------------------------------------
int
Script::BackgroundRun( int reaperId )
{
	// Construct the command line, replacing some tokens with
    // information about the job.  All of these values would probably
    // be better inserted into the environment, rather than passed on
    // the command-line... some should be in the job's env as well...

    const char *delimiters = " \t";
    char * token;
    MyString send;
    char * cmd = strnewp(_cmd);
    for (token = strtok (cmd,  delimiters) ; token != NULL ;
         token = strtok (NULL, delimiters)) {
		if( !strcasecmp( token, "$JOB" ) ) {
			send += _node->GetJobName();
		} 
        else if ( !strcasecmp( token, "$RETRY" ) ) {
            send += _node->retries;
        }
        else if ( _post && !strcasecmp( token, "$JOBID" ) ) {
            send += _node->_CondorID._cluster;
            send += '.';
            send += _node->_CondorID._proc;
        }
        else if (!strcasecmp(token, "$RETURN")) send += _retValJob;
        else                                    send += token;

        send += ' ';
    }

	_pid = daemonCore->Create_Process( cmd, (char*) send.Value(),
									   PRIV_UNKNOWN, reaperId, FALSE,
									   NULL, NULL, FALSE, NULL, NULL, 0 );
    delete [] cmd;
	return _pid;
}
