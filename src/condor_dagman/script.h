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

#ifndef _SCRIPT_H_
#define _SCRIPT_H_

class Job;

class Script {
  public:
    // True is this script is a POST script, false if a PRE script
    bool _post;

    // Return value of the script
    int  _retValScript;

    // Return value of the job run.  Only valid of POST script
    int  _retValJob;

	// pid of running script, or 0 if not currently running
	int _pid;

	// has this script been run yet?
	bool _done;

	int BackgroundRun( int reaperId );
	const char* GetNodeName();

    inline const char* GetCmd() const { return _cmd; }
    Script( bool post, const char* cmd, const Job* node );
    ~Script();

    char * _cmd;

  protected:
    // the DAG node with which this script is associated.
    const Job *_node;
};

#endif
