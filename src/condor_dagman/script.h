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

    // Has this script been logged?
    bool   _logged;

	// pid of running script, or 0 if not currently running
	int _pid;

	int BackgroundRun( int reaperId );

    inline char * GetCmd () const { return _cmd; }
    Script( bool post, char* cmd, Job* job );
    ~Script();

    char * _cmd;
    Job  * _job;

  protected:
};

#endif
