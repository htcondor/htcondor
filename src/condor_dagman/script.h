/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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
