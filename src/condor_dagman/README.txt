-----------------------------------------------------------------------------
           Condor DAGMan (Directed Acyclic Graph Manager)
-----------------------------------------------------------------------------
        Current Version:  1
                   Date:  October 28, 1998
  Current Documentation:  http://www.cs.wisc.edu/~colbster/ (click on Condor)
      Current Developer:  Colby O'Donnell <colbster@cs.wisc.edu>
         Past Developer:  George Varghese <joev@cs.wisc.edu>
-----------------------------------------------------------------------------

Building:
---------
The Imakefile is used to generate the Makefile. Add or remove the flags,
and execute the command "../condor_imake" in the working directory.  After the
Makefile is generated, type 'make' or 'make condor_dagman.pure' if you wish a
purified executable.

The working directory should look something like:
.../condor/src/condor_dagman, where the 'src' tree is a standard CVS build of
the Condor Suite.  Talk to other team members (Todd, Derek, Rajesh) to find
out how to set up a Condor build environment correctly.

Dagman version 1:   (+ is feature, - is limitation)
-----------------
  1)  + Dagman allows user to specify job dependencies
  2)  + Dagman output level specified dynamically
  3)  + Dagman will safely recover an unfinished Dag
  4)  + User can completely delete a running Dag.
  5)  - All jobs in a Dag must go to exactly one unique condor log file
  6)  - All jobs must exit normally, else Dag will be aborted

Dagman version 2:
-----------------
  5)  + All jobs in a Dag must go to one log file, but log file can be
        shared with other Dags.

  6)  + A job can be "undone", or there is some notion of a job instance.
        Hence, a job that exits abnormally or is cancelled by the user can
        be rerun such that the new run's log entry is unique from the old
        run's log entry (in terms of recovery)

  7)  + A general purpose command socket will be used to direct Dagman
        while it's running.  Commands like CANCEL_JOB X or DELETE_ALL
        would be supported, as well as notification messages like
        JOB_SUBMIT or JOB_TERMINATE, etc.  Eventually, a Java Gui would
        graphically represent the Dag's state, and offer buttons and dials
        for graphic Dag manipulation.

Status of Features:
-------------------

Feature 1) is done, and works.
Feature 2) The debug option (-debug <integer>) allows the user to
           specify the level of output.  The debug levels are as follows:

     Option    Meaning  Explanation
     ---------------------------------------------------------------------
     -debug 0  SILENT   absolutely no output during normal operation
                        good for running Dagman from a script
     -debug 1  QUIET    only print critical message to the screen
     -debug 2  NORMAL   Normal output
     -debug 3  VERBOSE  Verbose output, all relevant to the end user
     -debug 4  DEBUG_1  Simple debug output, only helpful for developers
     -debug 5  DEBUG_2  Detailed outer loop debug output
     -debug 6  DEBUG_3  Flood your screen with inner loop output.  Highly
                        recommand that all output be redirected to a file,
                        since an xterm would be heavily strained.
     -debug 6  DEBUG_4  Rarely used.  Produces maximum output.


Feature 3) Works, bug is severly flawed.  Currently, DagMan assumes that
           after it performs a popen("condor_submit"), the very next entry
           in the log will be the SUBMIT entry.  This assumption is easily
           false on a heavily active Dag, where some of random job event
           could occur in the small amount of the time between condor_submit
           and the event being read from the log.

           What is needed is a TERMINATE queue, where the implications
           of a terminated job are not applied to the Dag, until on the
           SUBMITs from all previous TERMINATEs are completely processed.

           I also need to daemon corify Dagman such that it can be run as a
           Condor Meta Scheduler.

Feature 4) is not currently implemented.  In general, I would add a
           command socket, such that the user could plug in some text or
           GUI command interpreter.  Since version one only requires one
           such command (DELETE_ALL), Todd, Refesh, and I decided the
           easiest way to add this would be through condor_rm of the
           Dagman job itself.  condor_rm would tell the schedd to remove
           the dagman job, and schedd, noting that dagman is a
           meta-scheduler, would signal Dagman to remove all of its
           running jobs before exiting.
Limit   5) Having all jobs in a Dag going to the same logfile will be
           a permanent limitation.  However, if two or more Dag's point
           to the same log, a notion of Dag instance must be added to
           each node's log entry, so that a Dag recovery can differentiate
           its job's log entries from another Dag's job log entries.
           In version 2, Dagman will find out its own cluster number
           (from the environment, or classad method) and add that to
           node names.

Limit   6) To overcome this limit, and job instance must be notioned.
           Since Dagman will only run once instance at a time, we do not
           have to worry about instance concurrency.  A job instance is
           either currently running, or was cancelled and possibly
           replaced by a new instance.  condor_submit must support naming
           a job, either via command argument or ClassAd attribute.

Feature 7) If I build it (good command socket), they (GUIs) will come.

Recent Work
-------------------
  - Heavy testing of jobs over 3000 nodes
  - Memory usage is stable (around 2-3 MB for 3000 node jobs where the
    name of each job is about 8 characters.
  - I've only been able to test for sunx86_56 and sun4x_56, because the
    trunk wouldn't compile on other platforms (chestnut, raven).  Derek
    says his next check-in will cure these compile problems.
  - Dagman is now "purify perfect"
  - Dagman is now condor_submit failure tolerant (before it would hang on
    a failed condor_submit)
  - Still tracking some recovery flaws with super-large Dags.  Dagman
    overly sensitive to the order of job submits and log entries.  Being
    able to name jobs in condor_submit would solve this, which is on the
    discussion table...Miron has pointed out that a TERMINATE queue is
    necessary, rather than a SUBMIT queue.
  - Had some quality discussion with Rajesh and Todd about
    Dagman design, and planned features for version 1 and later version 2.
    Conclusions were reached with Miron.
  - Cleaned up screen output for end users
  - Fixed memory leaks
  - Removed unused functions and variables
  - Improved Doc++ documentation of classes
  - Added a SimpleListIterator class to simplelist.h (will check in after
       further tests)
  - Made dagman file format a bit more flexible (easier to generate)
