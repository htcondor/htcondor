#ifndef _SCRIPT_H_
#define _SCRIPT_H_

class Job;

class Script {
  public:
    /// True is this script is a POST script, false if a PRE script
    bool _post;

    /// Return value of the script
    int  _retValScript;

    /// Return value of the job run.  Only valid of POST script
    int  _retValJob;

    /// Has this script been logged?
    bool   _logged;

    /** Runs the script and sets _retValScript.
        @return returns _retValScript
    */
    int Run ();

    ///
    inline char * GetCmd () const { return _cmd; }

    ///
    Script (bool post, char *cmd, Job * job);

    ///
    ~Script();

  protected:

    ///
    Job  * _job;

    ///
    char * _cmd;
};

#endif
