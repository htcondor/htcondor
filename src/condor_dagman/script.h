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

	// has this script been run yet?
	bool _done;

	int BackgroundRun( int reaperId );

    inline char * GetCmd () const { return _cmd; }
    Script( bool post, char* cmd, Job* job );
    ~Script();

    char * _cmd;
    Job  * _job;

  protected:
};

#endif
