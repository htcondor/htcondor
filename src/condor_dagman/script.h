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

#define SCRIPT_DEFER_STATUS_NONE (-1)

enum ScriptType {
    PRE,
    POST,
    HOLD
};

class Job;

class Script {
  public:
    // Type of script: PRE, POST or HOLD
    ScriptType _type;

    // Return value of the PRE script; it is only valid in the POST script.
    int  _retValScript;

    // Return value of the job run.  Only valid of POST script
    int  _retValJob;

    // pid of running script, or 0 if not currently running
    int _pid;

    // has this script been run yet?
    bool _done;

    // Return value which indicates this should be deferred until later.
    int _deferStatus;

    // How long to sleep when deferred.
    time_t _deferTime;

    // The next time at which we're eligible to run (0 means any time).
    time_t _nextRunTime;

    int BackgroundRun( int reaperId, int dagStatus, int failedCount );
    const char* GetNodeName();
    Job *GetNode() { return _node; }
    
    const char* GetScriptName() {
        switch( _type ) {
            case ScriptType::PRE: return "PRE";
            case ScriptType::POST: return "POST";
            case ScriptType::HOLD: return "HOLD";
        }
        return "";
    }

    inline const char* GetCmd() const { return _cmd; }
    Script( ScriptType type, const char* cmd, int deferStatus, time_t deferTime,
    			Job* node );
    ~Script();

    char * _cmd;

  protected:
    // the DAG node with which this script is associated.
    Job *_node;
};

#endif
