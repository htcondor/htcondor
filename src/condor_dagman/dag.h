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


#ifndef DAG_H
#define DAG_H

#include "condor_common.h"
#include "job.h"
#include "scriptQ.h"
#include "condor_constants.h"      /* from condor_includes/ directory */
#include "condor_daemon_core.h"
#include "read_multiple_logs.h"
#include "check_events.h"
#include "condor_id.h"
#include "prioritysimplelist.h"
#include "throttle_by_category.h"
#include "MyString.h"
#include "../condor_utils/dagman_utils.h"
#include "jobstate_log.h"
#include "dagman_classad.h"

#include <queue>

// Which layer of splices do we want to lift?
enum SpliceLayer {
	SELF,
	DESCENDENTS,
};

class Dagman;
class MyString;
class DagmanMetrics;
class CondorID;

// used for RelinquishNodeOwnership and AssumeOwnershipofNodes
// This class owns the containers with which it was constructed, but
// not the memory in those containers. Even though there is one thing
// in here, the design of the overall codebase say that eventually more
// things will probably end up in this class when lifting a splice into
// a container dag.
class OwnedMaterials
{
	public:
		// this structure owns the containers passed to it, but not the memory 
		// contained in the containers...
		OwnedMaterials(std::vector<Job*> *a, ThrottleByCategory *tr,
				bool reject, MyString firstRejectLoc ) :
				nodes (a), throttles (tr), _reject(reject),
				_firstRejectLoc(firstRejectLoc) {};
		~OwnedMaterials() 
		{
			delete nodes;
		};

	std::vector<Job*> *nodes;
	ThrottleByCategory *throttles;
	bool _reject;
	MyString _firstRejectLoc;
};

//------------------------------------------------------------------------
/** A Dag instance stores information about a job dependency graph,
    and the state of jobs as they are running. Jobs are run by
    submitting them to condor using the UserLog API<p>
    
    The public interface to a Dag allows you to add jobs, add
    dependencies, submit jobs, control the locations of log files,
    query the state of a job, and process any new events that have
    appeared in the condor log file.
*/
class Dag {
  friend class DagmanMetrics;

  public:
  
    /** Create a DAG
		@param dagFile the DAG file name
        @param maxJobsSubmitted the maximum number of jobs to submit to HTCondor
               at one time
        @param maxPreScripts the maximum number of PRE scripts to spawn at
		       one time
        @param maxPostScripts the maximum number of POST scripts to spawn at
		       one time
		@param allowLogError whether to allow the DAG to run even if we
			   have an error determining the job log files
		@param useDagDir run DAGs in directories from DAG file paths
		       if true
		@param maxIdleJobProcs the maximum number of idle job procs to
			   allow at one time (0 means unlimited)
		@param retrySubmitFirst whether, when a submit fails for a node's
		       job, to put the node at the head of the ready queue
		@param retryNodeFirst whether, when a node fails and has retries,
			   to put the node at the head of the ready queue
		@param condorRmExe executable to remove HTCondor jobs
		@param DAGManJobId HTCondor ID of this DAGMan process
		@param prohibitMultiJobs whether submit files queueing multiple
			   job procs are prohibited
		@param submitDepthFirst whether ready nodes should be submitted
			   in depth-first (as opposed to breadth-first) order
		@param defaultNodeLog The user log file to be used for node jobs.
		@param isSplice is a boolean which lets the dag object know whether
				of not it is a splicing dag, or the toplevel dag. We don't
				wan't to allocate some regulated resources we won't need
				if we are a splice. It is true for a top level dag, and false
				for a splice.
		@param spliceScope a string containing the names of all splices
				on the "path" to the top-level DAG (e.g., "A+B+"), or
				"root" for the top-level DAG.
    */

    Dag( /* const */ std::list<std::string> &dagFiles,
		 const int maxJobsSubmitted,
		 const int maxPreScripts, const int maxPostScripts,
		 const int maxHoldScripts,
		 bool useDagDir, int maxIdleJobProcs, bool retrySubmitFirst,
		 bool retryNodeFirst, const char *condorRmExe,
		 const CondorID *DAGManJobId,
		 bool prohibitMultiJobs, bool submitDepthFirst,
		 const char *defaultNodeLog, bool generateSubdagSubmits,
		 SubmitDagDeepOptions *submitDagDeepOpts,
		 bool isSplice = false, DCSchedd *schedd = NULL,
		 const MyString &spliceScope = "root" );

    ///
    ~Dag();

		/** Create the DagmanMetrics object for this DAGMan.
			@param primaryDagFile The primary (first) DAG file specified.
			@param rescueDagNum The number of the rescue DAG we're
					running (0 if not running a rescue DAG).
		*/
	void CreateMetrics( const char *primaryDagFile, int rescueDagNum );

		/** Report the metrics for this run (if metrics reporting is
			enabled).
			@param exitCode The exit code of this DAGMan.
		*/
	void ReportMetrics( int exitCode );

	/** Set the _abortOnScarySubmit value -- controls whether we abort
		the DAG on "scary" submit events.
		@param The abortOnScarySubmit value
	*/
	void SetAbortOnScarySubmit( bool abortOnScarySubmit ) {
		_abortOnScarySubmit = abortOnScarySubmit;
	}

    /** Prepare DAG for initial run.  Call this function ONLY ONCE.
        @param recovery specifies if this is a recovery
        @return true: successful, false: failure
    */
    bool Bootstrap (bool recovery);

    /// Add a job to the collection of jobs managed by this Dag.
    bool Add( Job& job );

#ifdef DEAD_CODE
    /** Specify a dependency between two jobs. The child job will only
        run after the parent job has finished.
        @param parent The parent job
        @param child The child job (depends on the parent)
        @return true: successful, false: failure
    */
    static bool AddDependency (Job * parent, Job * child);
#endif

	/** Run waiting/deferred scripts that are ready to run.  Note: scripts
	    are also limited by halt status and maxpre/maxpost.
	*/
	void RunWaitingScripts();
  
    // Get the current status of the condor log file
	ReadUserLog::FileStatus	GetCondorLogStatus();

    /** Force the Dag to process all new events in the condor log file.
        This may cause the state of some jobs to change.

        @param recover Process Log in Recover Mode, from beginning to end
        @return true on success, false on failure
    */
    bool ProcessLogEvents (bool recovery = false);

	/** Process a single event.  Note that this is called every time
			we attempt to read the user log, so we may or may not have
			a valid event here.
		@param The outcome from the attempt to read the user log.
	    @param The event.
		@param Whether we're in recovery mode.
		@param Whether we're done trying to read events (set by this
			function).
		@return True if the DAG should continue, false if we should abort.
	*/
	bool ProcessOneEvent (ULogEventOutcome outcome, const ULogEvent *event,
			bool recovery, bool &done);

	/** Process an abort or executable error event.
	    @param The event.
		@param The job corresponding to this event.
		@param Whether we're in recovery mode.
	*/
	void ProcessAbortEvent(const ULogEvent *event, Job *job,
			bool recovery);

	/** Process a terminated event.
	    @param The event.
		@param The job corresponding to this event.
		@param Whether we're in recovery mode.
	*/
	void ProcessTerminatedEvent(const ULogEvent *event, Job *job,
			bool recovery);

	/** Remove the batch system job for the given DAG node.
		@param The node for which to remove the job.
	*/
	void RemoveBatchJob(Job *node);

	/** Processing common to all "end-of-job proc" events.
		@param The node the event is associated with.
		@param Whether we're in recovery mode.
		@param Whether the job proc failed
	*/
	void ProcessJobProcEnd(Job *node, bool recovery, bool failed);

	/** Process a post script terminated event.
	    @param The event.
		@param The job corresponding to this event.
		@param Whether we're in recovery mode.
	*/
	void ProcessPostTermEvent(const ULogEvent *event, Job *job,
			bool recovery);

	/** Process a submit event.
	    @param The event.
		@param The job corresponding to this event.
		@param Whether we're in recovery mode.
		@param Whether the submit event is considered "sane".
	*/
	void ProcessSubmitEvent(Job *job, bool recovery, bool &submitEventIsSane);

	/** Process an event indicating that a job is in an idle state.
	    Note that this method only does processing relating to keeping
		the count of idle jobs -- any other processing should be done in
		a different method.
		@param The job corresponding to this event.
	*/
	void ProcessIsIdleEvent( Job *job, int proc );

	/** Process an event that indicates that a job is NOT in an idle state.
	    Note that this method only does processing relating to keeping
		the count of idle jobs -- any other processing should be done in
		a different method.
		@param The job corresponding to this event.
	*/
	void ProcessNotIdleEvent( Job *job, int proc );

	/** Process a held event for a job.
		@param The job corresponding to this event.
	*/
	void ProcessHeldEvent(Job *job, const ULogEvent *event);

	/** Process a released event for a job.
		@param The job corresponding to this event.
	*/
	void ProcessReleasedEvent(Job *job, const ULogEvent *event);

	/** Process a cluster submit event.
	    @param The job corresponding to this event.
		@param Whether we're in recovery mode.
	*/
	void ProcessClusterSubmitEvent(Job *job);

		/** Process a cluster remove event.
	    @param The job corresponding to this event.
		@param Whether we're in recovery mode.
	*/
	void ProcessClusterRemoveEvent(Job *job, bool recovery);

    /** Get pointer to job with id jobID
        @param the handle of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * FindNodeByNodeID (const JobID_t jobID) const;

    /** Get pointer to job with name jobName
        @param jobName the name of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * FindNodeByName (const char * jobName) const;

    /** Get pointer to job with condor ID condorID
        @param condorID the HTCondorID of the job in the DAG
        @return address of Job object, or NULL if not found
    */
    Job * FindNodeByEventID ( const CondorID condorID ) const;

	/** Find all nodes by name -- either a specific node or all
	 	nodes in the DAG.  To find all nodes in the DAG, pass
		"ALL_NODES" as the node name on the first call; then NULL
		in subsequent calls
		@param nodeName the name of the node to find, or "ALL_NODES",
			or NULL
		@param finalSkipMsg the message to print when skipping final
			nodes, in a form like this: "In parse_script(): skipping node
			%s because final nodes must have SCRIPT set explicitly
			(%s: %d)\n"
		@param file the name of the DAG file that is being parsed
		@param line the line in the DAG file that is being parsed
	 	@return a pointer to a Job (node) object, or NULL if there are
	 		no more node to be found
	*/
	Job * FindAllNodesByName( const char* nodeName,
				const char *finalSkipMsg,
				const char *file, int line) const;

    /** Ask whether a node name exists in the DAG
        @param nodeName the name of the node in the DAG
        @return true if the node exists, false otherwise
    */
    bool NodeExists( const char *nodeName ) const;

	/** Prefix all of the nodenames with a specified label.
		@param prefix a MyString of the prefix for all nodes.
	*/
	void PrefixAllNodeNames(const MyString &prefix);

    /** Set the event checking level.
		@param allowEvents what "bad" events to treat as non-fatal (as
			   opposed to fatal) errors; see check_events.h for values.
    */
	void SetAllowEvents( int allowEvents );

    /// Print the list of jobs to stdout (for debugging).
    void PrintJobList() const;
    void PrintJobList( Job::status_t status ) const;

    /** @param whether to include final node, if any, in the count
		@return the total number of nodes in the DAG
     */
    int NumNodes( bool includeFinal ) const;

    /** @param whether to include final node, if any, in the count
    	@return the number of nodes completed
     */
    int NumNodesDone( bool includeFinal ) const;

    /** @return the number of nodes that failed in the DAG
     */
    inline int NumNodesFailed() const { return _numNodesFailed; }

    /** @return the number of jobs currently submitted to batch system(s)
     */
    inline int NumJobsSubmitted() const { return _numJobsSubmitted; }

    /** @return the number of nodes ready to submit job to batch system
     */
    inline int NumNodesReady() const { return _readyQ->Number(); }

    /** @param whether to include final node, if any, in the count
	    @return the number of nodes not ready to submit to batch system
	 */
    inline int NumNodesUnready( bool includeFinal ) const {
				return ( NumNodes( includeFinal )  -
				( NumNodesDone( includeFinal ) + PreRunNodeCount() +
				NumJobsSubmitted() + PostRunNodeCount() +
				NumNodesReady() + NumNodesFailed() ) ); }

    /** @return the number of PRE scripts currently running
     */
    inline int NumPreScriptsRunning() const
	{
		// Do not call this function when the dag is being used to parse
		// a splice.
		ASSERT( _isSplice == false ); 
		return _preScriptQ->NumScriptsRunning();
	}

    /** @return the number of POST scripts currently running
     */
    inline int NumPostScriptsRunning() const
	{
		// Do not call this function when the dag is being used to parse
		// a splice.
		ASSERT( _isSplice == false ); 
		return _postScriptQ->NumScriptsRunning(); 
	}

    /** @return the total number of PRE/POST scripts currently running
     */
    inline int NumScriptsRunning() const
	{
		// Do not call this function when the dag is being used to parse
		// a splice.
		ASSERT( _isSplice == false ); 
		return NumPreScriptsRunning() + NumPostScriptsRunning();
	}

	/** @return the number of nodes currently in the status
	 *          Job::STATUS_PRERUN (whether or not their PRE
	 *			script is actually running).
	 */
	inline int PreRunNodeCount() const
		{ return _preRunNodeCount; }

	/** @return the number of nodes currently in the status
	 *          Job::STATUS_POSTRUN (whether or not their POST
	 *			script is actually running).
	 */
	inline int PostRunNodeCount() const
		{ return _postRunNodeCount; }

	/** @return the number of nodes currently running a HOLD script.
	 *          These nodes do not have any special status.
	 */
	inline int HoldRunNodeCount() const
		{ return _holdRunNodeCount; }

	/** @return the number of nodes currently in the status
	 *          Job::STATUS_PRERUN or Job::STATUS_POSTRUN (whether or not
	 *			the script is actually running).
	 */
	inline int ScriptRunNodeCount() const
		{ return _preRunNodeCount + _postRunNodeCount; }

		/** Determine whether the DAG has finished running (whether
			successfully or unsuccessfully).
	    	(If no jobs are submitted and no scripts are running, but the
		    dag is not complete, then at least one job failed, or a cycle
			exists.)
    		@param whether to consider the final node, if any
			@return true iff the DAG is finished
		*/
	bool FinishedRunning( bool includeFinalNode ) const;

		/** Determine whether the DAG is successfully completed.
    		@param whether to consider the final node, if any
			@return true iff the DAG is successfully completed
		*/
	bool DoneSuccess( bool includeFinalNode ) const;

		/** Determine whether the DAG is finished, but failed (because
			of a node job failure, etc.).  Note that this returns false
			if there's a cycle in the DAG but no nodes failed.
    		@param whether to consider the final node, if any
			@return true iff the DAG is finished but failed
		*/
	bool DoneFailed( bool includeFinalNode ) const;

		/** Determine whether the DAG is finished because of a cycle in
			the DAG.
    		@param whether to consider the final node, if any
			@return true iff the DAG is finished but there is a cycle
		*/
	inline bool DoneCycle( bool includeFinalNode) const;

		/** Submit all ready jobs, provided they are not waiting on a
			parent job or being throttled.
			@param the appropriate Dagman object
			@return number of jobs successfully submitted
		*/
    int SubmitReadyJobs(const Dagman &dm);

		/** Start the DAG's final node if there is one.  Note that this
			method will not re-start the final node if it has already
			been started.
			@return true iff the final node was actually started.
		*/
	bool StartFinalNode();

		/** Start the DAG's provisioner node if there is one.
			@return true iff the provisioner node was actually started.
		*/
	bool StartProvisionerNode();

		/** Get the status of the provisioner by querying the schedd and
		    checking its job ad.
			@return status of the provisioner
		*/
	int GetProvisionerJobAdState();

    /** Remove all jobs (using condor_rm) that are currently running.
        All jobs currently marked Job::STATUS_SUBMITTED will be fed
        as arguments to condor_rm via popen.  This function is called
        when the Dagman HTCondor job itself is removed by the user via
        condor_rm.  This function <b>is not</b> called when the schedd
        kills Dagman.
		@param dmJobId: the HTCondor ID of this DAGMan job.
		@param removeCondorJobs: iff true we, remove our HTCondor node jobs.
			This is set to false when DAGMan itself is condor_rm'ed,
			because in that case the schedd removes the node jobs.
		@param bForce: iff true, we run the command to remove HTCondor
			node jobs even if we don't think we have any -- I think this
			is in case we have a failure in recovery mode before we've
			read the logs.  Setting bForce to true automatically
			implies removeCondorJobs.
    */
    void RemoveRunningJobs ( const CondorID &dmJobId, bool removeCondorJobs,
				bool bForce );

    /** Remove all pre- and post-scripts that are currently running.
	All currently running scripts will be killed via daemoncore.
	This function is called when the Dagman HTCondor job itself is
	removed by the user via condor_rm.  This function <b>is not</b>
	called when the schedd kills Dagman.
    */
    void RemoveRunningScripts ( ) const;

    /** Creates a DAG file based on the DAG in memory, except all
        completed jobs are premarked as DONE.  Rescue DAG file name
		is derived automatically from original DAG name.
        @param datafile The original DAG file
		@param multiDags Whether we have multiple DAGs
		@param maxRescueDagNum the maximum legal rescue DAG number
		@param overwrite Whether to overwrite the highest-numbered
			rescue DAG (because with a final node you can write the
			rescue DAG twice)
		@param parseFailed whether parsing the DAG(s) failed
		@param isPartial whether the rescue DAG is only a partial
			DAG file (needs to be parsed in combination with the original
			DAG file)
    */
    void Rescue (const char * dagFile, bool multiDags,
				int maxRescueDagNum, bool overwrite,
				bool parseFailed = false, bool isPartial = false) /* const */;

    /** Creates a DAG file based on the DAG in memory, except all
        completed jobs are premarked as DONE.
        @param rescue_file The name of the rescue file to generate
        @param datafile The original DAG file
		@param parseFailed whether parsing the DAG(s) failed
		@param isPartial whether the rescue DAG is only a partial
			DAG file (needs to be parsed in combination with the original
			DAG file)
    */
    void WriteRescue (const char * rescue_file,
				const char * dagFile, bool parseFailed = false,
				bool isPartial = false) /* const */;

	int PreScriptReaper( Job *job, int status );
	int PostScriptReaper( Job *job, int status );
	int HoldScriptReaper( Job *job );

	void PrintReadyQ( debug_level_t level ) const;

	bool _removeJobsAfterLimitChange = false;

	std::map<std::string, SubmitHash*> SubmitDescriptions;

#if 0
	bool RemoveNode( const char *name, MyString &whynot );
#endif

#ifdef DEAD_CODE
	bool RemoveDependency( Job *parent, Job *child );
	bool RemoveDependency( Job *parent, Job *child, MyString &whynot );
#endif
	
    /* Detects cycle within dag submitted by user
	   @return true if there is cycle
	*/
	bool isCycle();

	// max number of PRE & POST scripts to run at once (0 means no limit)
    int _maxPreScripts;
    int _maxPostScripts;
	int _maxHoldScripts;

	char* GetDotFileName(void) { return _dot_file_name; }
	void SetDotFileName(const char *dot_file_name);
	void SetDotIncludeFileName(const char *include_file_name);
	void SetDotFileUpdate(bool update_dot_file)       { _update_dot_file    = update_dot_file; }
	void SetDotFileOverwrite(bool overwrite_dot_file) { _overwrite_dot_file = overwrite_dot_file; }
	bool GetDotFileUpdate(void) const                       { return _update_dot_file; }
	void DumpDotFile(void);

	void SetNodeStatusFileName( const char *statusFileName,
				int minUpdateTime, bool alwaysUpdate = false );
	void DumpNodeStatus( bool held, bool removed );

		/** Set the reject flag to true for this DAG; if it hasn't been
			previously set, update the location info for the reject
			directive.
			@param location: the file and line # of the REJECT keyword
		*/
	void SetReject( const MyString &location );

		/** Get the reject setting of this DAG (true means we shouldn't
			actually run the DAG).
			@param firstLocation: the location of the first REJECT
				directive we encountered in parsing the DAG (including
				splices)
		*/
	bool GetReject( MyString &firstLocation );

	void SetJobstateLogFileName( const char *logFileName );

	void CheckAllJobs();

#ifdef DEAD_CODE
		/** Returns a delimited string listing the node names of all
			of the given node's parents if the number of parents is less than or equal to max_parents
			@return delimited string of parent node names
		*/
	const MyString ParentListString( Job *node, size_t max_parents=256, const char delim = ',' ) const;
#endif

	int NumIdleJobProcs() const { return _numIdleJobProcs; }

	int NumHeldJobProcs();

		/** Print the number of deferrals during the run (caused
		    by MaxJobs, MaxIdle, MaxPre, or MaxPost).
			@param level: debug level for output.
			@param force: whether to force output even if there have
				been no deferrals.
		*/
	void PrintDeferrals( debug_level_t level, bool force ) const;

    	/** Print information about the nodes that are currently
		    pending for.
		*/
    void PrintPendingNodes() const;

		// non-exe failure codes for return value integers -- we
		// represent DAGMan, batch-system, or other external errors
		// with the integer return-code space *below* -64.  So, 0-255
		// are normal exe return codes, -1 to -64 represent catching
		// exe signals 1 to 64, and -1000 and below represent DAGMan,
		// batch-system, or other external errors
	static const int DAG_ERROR_CONDOR_SUBMIT_FAILED;
	static const int DAG_ERROR_CONDOR_JOB_ABORTED;
	static const int DAG_ERROR_LOG_MONITOR_ERROR;
	static const int DAG_ERROR_JOB_SKIPPED;

		// The maximum signal we can deal with in the error-reporting
		// code.
	const int MAX_SIGNAL;

	bool ProhibitMultiJobs() const { return _prohibitMultiJobs; }

		/** Set the config file used for this DAG.
			@param configFile The configuration file for this DAG.
		*/
	void SetConfigFile( const char *configFile ) { _configFile = configFile; }

		/** Set the interval for "pending node" reports.
			@param interval The number of seconds without an event we
			wait before reporting the pending nodes.
		*/
	void SetPendingNodeReportInterval( int interval );
	
		// Node category throttle information for the DAG.
	ThrottleByCategory		_catThrottles;

		/**	Go through all of the node throttling categories, and print
			a warning if any categories have no nodes assigned to them,
			or don't have their throttle set.
		*/
	void CheckThrottleCats();

	int MaxJobsSubmitted(void) const { return _maxJobsSubmitted; }

	bool UseDagDir(void) const { return _useDagDir; }

	int MaxIdleJobProcs(void) const { return _maxIdleJobProcs; }
	int MaxPreScripts(void) const { return _maxPreScripts; }
	int MaxPostScripts(void) const { return _maxPostScripts; }
	int MaxHoldScripts(void) const { return _maxHoldScripts; }

	void SetMaxIdleJobProcs(int maxIdle) { _maxIdleJobProcs = maxIdle; };
	void SetMaxJobsSubmitted(int newMax);
	void SetMaxPreScripts(int maxPreScripts) { _maxPreScripts = maxPreScripts; };
	void SetMaxPostScripts(int maxPostScripts) { _maxPostScripts = maxPostScripts; };
	void SetMaxHoldScripts(int maxHoldScripts) { _maxHoldScripts = maxHoldScripts; };

	bool RetrySubmitFirst(void) const { return m_retrySubmitFirst; }

	bool RetryNodeFirst(void) const { return m_retryNodeFirst; }

	// do not free this pointer
	const char* CondorRmExe(void) { return _condorRmExe; }

	const CondorID* DAGManJobId(void) { return _DAGManJobId; }

	bool SubmitDepthFirst(void) const { return _submitDepthFirst; }

	const char *DefaultNodeLog(void) { return _defaultNodeLog; }

	bool GenerateSubdagSubmits(void) const { return _generateSubdagSubmits; }

	std::list<std::string>& DagFiles(void) { return _dagFiles; }

	/** Determine whether a job is a NOOP job based on the HTCondor ID.
		@param the HTCondor ID of the job
		@return true iff the job is a NOOP
	*/
	static bool JobIsNoop( const CondorID &id ) {
		return (id._cluster == 0) && (id._proc == Job::NOOP_NODE_PROCID);
	}

	/** Get the part of the HTCondorID that we're indexing by (cluster ID
		for "normal" jobs, subproc ID for NOOP jobs).
		@param the HTCondor ID of the job
		@return the part of the ID to index by
	*/
	static int GetIndexID( const CondorID &id ) {
		return JobIsNoop( id ) ? id._subproc : id._cluster; 
	}

	// return same thing as std::map::insert()
	bool InsertSplice(MyString spliceName, Dag *splice_dag);

	// return same thing as std::map::find()
	Dag* LookupSplice(MyString name);

	// return an array of job pointers to all of the nodes with no
	// parents in this dag.
	// These pointers are aliased and should not be freed.
	// However the array itself is allocated and must be freed.
	std::vector<Job*>* InitialRecordedNodes(void);

	// return an array of job pointers to all of the nodes with no
	// children in this dag.
	// These pointers are aliased and should not be freed.
	// However the array itself is allocated and must be freed.
	std::vector<Job*>* FinalRecordedNodes(void);

	// called just after a parse of a dag, this will keep track of the
	// original intial and terminal nodes of a dag (after all parent and
	// child dependencies have been added or course).
	void RecordInitialAndTerminalNodes(void);

	// Recursively lift all splices into each other and then me
	OwnedMaterials* LiftSplices(SpliceLayer layer);

	// The dag will give back an array of all nodes and delete those nodes
	// from its internal lists. This is used when a splice is being
	// lifted into the containing dag.
	OwnedMaterials* RelinquishNodeOwnership(void);

	// Take an array from RelinquishNodeOwnership) and store it in my self.
	void AssumeOwnershipofNodes(const MyString &spliceName,
				OwnedMaterials *om);

#ifdef DEAD_CODE // we now do this at submit time.
	// This must be called after the toplevel dag has been parsed and
	// the splices lifted. It will resolve the use of $(JOB) in the value
	// of the VARS attribute.
	void ResolveVarsInterpolations(void);
#endif

	// When parsing a splice (which is itself a dag), there must always be a
	// DIR concept associated with it. If DIR is left off, then it is ".",
	// otherwise it is whatever specified.
	void SetDirectory(MyString &dir);
	void SetDirectory(char *dir);

	// After the nodes in the dag have been made, we take our DIR setting,
	// and if it isn't ".", we prefix it to the directory setting for each
	// node, unless it is an absolute path, in which case we ignore it.
	void PropagateDirectoryToAllNodes(void);

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Splice connections.

	/** Set a pin in or pin out connection for this DAG.
		@param isPinIn: true if this is for a pin in, false for pin out
		@param nodeName: the name of the node we're connecting
		@param pinNum: the number of the pin we're connecting
		@return: true on success, false otherwise
	*/
	bool SetPinInOut( bool isPinIn, const char *nodeName, int pinNum );

	/** Connect two splices via pin outs/pin ins
		@param parentSplice: the splice connected via its pin outs
		@param childSplice: the splice connected via its pin ins
		@return: true on success, false otherwise
	*/
	static bool ConnectSplices( Dag *parentSplice, Dag *childSplice );
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	/** Set the maximum number of job holds before a node is declared
		a failure.
		@param the maximum number of job holds before failure
	*/
	void SetMaxJobHolds(int maxJobHolds) { _maxJobHolds = maxJobHolds; }

	JobstateLog &GetJobstateLog() { return _jobstateLog; }
	bool GetPostRun() const { return _alwaysRunPost; }
	void SetPostRun(bool postRun) { _alwaysRunPost = postRun; }	

	int GetDryRun() const { return _dry_run; }
	void SetDryRun(int dry) { _dry_run = dry; }

		// Set the overall priority for this DAG (set on command
		// line (could be from higher-level DAG) or via config).
	void SetDagPriority(const int prio) { _dagPriority = prio; }

		// Get this DAG's overall priority.
	int GetDagPriority() const { return _dagPriority; }
		
		// Set priorities for the individual nodes within this DAG.
	void SetNodePriorities();

		// make a pass through the dag removing duplicate edges
		// and setting the waiting edges. not all edge strategies need this
	void AdjustEdges();

	/** Determine whether the DAG is currently halted (waiting for
		existing jobs to finish but not submitting any new ones).
		@return true iff the DAG is halted.
	*/
	bool IsHalted() const { return _dagIsHalted; }

	DagStatus _dagStatus;

	// WARNING!  DagStatus and _dag_status_names just be kept in sync!
	static const char *_dag_status_names[];

	const char *GetStatusName() const {
				return _dag_status_names[_dagStatus]; }

	static const char *ALL_NODES;

	/** Determine whether this DAG has a final node.
		@return true iff the DAG has a final node.
	*/
	inline bool HasFinalNode() const { return _final_job != NULL; }

	/** Determine whether the final node (if any) of this DAG is
		running (or has been run).
		@return true iff the final node is running or has been run
	*/
	inline bool FinalNodeRun() const { return _finalNodeRun; }

	/** Determine whether this DAG has a provisioner node.
		@return true iff the DAG has a provisioner node.
	*/
	inline bool HasProvisionerNode() const { return _provisioner_node != NULL; }

	/** Determine whether the DAG is in recovery mode.
		@return true iff the DAG is in recovery mode
	*/
	inline bool Recovery() const { return _recovery; }

  private:

	// If this DAG is a splice, then this is what the DIR was set to, it 
	// defaults to ".".
 	MyString m_directory;

	// move the nodes from the splice into the parent
	void LiftSplice(Dag *parent, Dag *splice);

    // Just after we parse a splice dag file, we record what the initial
	// and final nodes were for the dag. This is so when we are using this
	// dag as a parent or a child, we can always reference the correct nodes
	// even in the face of AddDependency().
	std::vector<Job*> _splice_initial_nodes;
	std::vector<Job*> _splice_terminal_nodes;

  	// A hash table with key of a splice name and value of the dag parse 
	// associated with the splice.
	std::map<MyString, Dag*> _splices;

	// A reference to something the dagman passes into the constructor
	std::list<std::string>& _dagFiles;

	// Internal instance of a DagmanUtils object
	DagmanUtils _dagmanUtils;

	/** Print a numbered list of the DAG files.
	    @param The list of DAG files being run.
	*/
	void PrintDagFiles( /* const */ std::list<std::string> &dagFiles );

    /* Prepares to submit job by running its PRE Script if one exists,
       otherwise adds job to _readyQ and calls SubmitReadyJobs()
	   @param The job to start
	   @param Whether this is a retry
	   @return true on success, false on failure
    */
    bool StartNode( Job *node, bool isRetry );

    /* A helper function to run the POST script, if one exists.
           @param The job owning the POST script
           @param Whether to use the status variable in determining
              if we should run the POST script
           @param The status; usually the result of the PRE script.
              The POST script will not run if ignore_status is false
              and status is nonzero.
			@param Whether to increment the run count when we run the
				script
			@return true if successful, false otherwise
    */
	bool RunPostScript( Job *job, bool ignore_status, int status,
				bool incrementRunCount = true );

	/* A helper function to run the HOLD script, if one exists.
           @param The job owning the POST script
           @param Whether to increment the run count when we run the
				script
			@return true if successful, false otherwise
    */
	bool RunHoldScript( Job *job, bool incrementRunCount = true );

	typedef enum {
		SUBMIT_RESULT_OK,
		SUBMIT_RESULT_FAILED,
		SUBMIT_RESULT_NO_SUBMIT,
	} submit_result_t;

	/** Submit the HTCondor job for a node, including doing
		some higher-level work such as sleeping before the actual submit
		if necessary.
		@param the appropriate Dagman object
		@param the node for which to submit a job
		@param reference to hold the HTCondor ID the job is assigned
		@return submit_result_t (see above)
	*/
	submit_result_t SubmitNodeJob( const Dagman &dm, Job *node,
				CondorID &condorID );

	/** Do the post-processing of a successful submit of a HTCondor job.
		@param the node for which the job was just submitted
		@param the HTCondor ID of the associated job
	*/	
	void ProcessSuccessfulSubmit( Job *node, const CondorID &condorID );

	/** Do the post-processing of a failed submit of a HTCondor job.
		@param the node for which the job was just submitted
		@param the maximum number of submit attempts allowed for a job.
	*/
	void ProcessFailedSubmit( Job *node, int max_submit_attempts );

	/** Decrement the proc count for this node (and also the overall
	    	job count if appropriate).
		@param The node for which to decrement the job counts.
	*/
	void DecrementProcCount( Job *node );

	// Note: there's no IncrementJobCounts method because the code isn't
	// exactly duplicated when incrementing.

	/** Update the overall submitted job count (and the appropriate
			category count, if the node belongs to a category).
		@param The amount by which to change the job counts.
	*/
	void UpdateJobCounts( Job *node, int change );

	/** Add job to termination queue and report termination to all
		child jobs by removing job ID from each child's waiting queue.
		Note that this method should be called only in the case of
		*successful* termination.
		@param The job (node) just terminated
		@param Whether we're in recovery mode (re-reading log events)
		@param Whether we're in bootstrap mode (dealing with jobs marked
			DONE in a rescue DAG)
	*/
    void TerminateJob( Job* job, bool recovery, bool bootstrap = false );
  
	void PrintEvent( debug_level_t level, const ULogEvent* event,
					 Job* node, bool recovery );

	// Retry a node that we ran, but which failed.
	void RestartNode( Job *node, bool recovery );

	/* DFS number the jobs in the DAG in order to detect cycle*/
	void DFSVisit (Job * job, int depth);

		/** Check whether we got an exit value that should abort the DAG.
			@param The job associated with either the PRE script, POST
				script, or "main" job that just finished.
			@param The "type" of script or job we're dealing with (PRE
				script, POST script, or node).
			@return True iff aborting the DAG (it really should not
			    return in that case)
		*/
	bool CheckForDagAbort(Job *job, const char *type);

		// takes a userlog event and returns the corresponding node
	Job* LogEventNodeLookup( const ULogEvent* event,
				bool &submitEventIsSane );

		// check whether a userlog event is sane, or "impossible"

	bool EventSanityCheck( const ULogEvent* event,
						const Job* node, bool* result );

		// compares a submit event's job ID with the one that appeared
		// earlier in the submit command's stdout (which we stashed in
		// the Job object)

	bool SanityCheckSubmitEvent( const CondorID condorID, const Job* node ) const;

		/** Get the appropriate hash table for event ID->node mapping.
			@param whether the node is a NOOP node
			@return a pointer to the appropriate hash table
		*/
	std::map<int, Job *> *		GetEventIDHash(bool isNoop);

		/** Get the appropriate hash table for event ID->node mapping.
			@param whether the node is a NOOP node
			@return a pointer to the appropriate hash table
		*/
	const std::map<int, Job *> *		GetEventIDHash(bool isNoop) const;

	// run DAGs in directories from DAG file paths if true
	bool _useDagDir;

    // Documentation on ReadUserLog is present in condor_utils
	ReadMultipleUserLogs _condorLogRdr;

		/** Get the total number of node job user log files we'll be
			accessing.
			@return The total number of log files.
		*/
	int TotalLogFileCount() { return CondorLogFileCount(); }
				
	int CondorLogFileCount() { return _condorLogRdr.totalLogFileCount(); }

		/** Write information for the given node to a rescue DAG.
			@param fp: file pointer to the rescue DAG file
			@param node: the node for which to write info
			@param reset_retries_upon_rescue: whether to reset any
				previous retries of a node
			@param isPartial: whether the rescue DAG is only a partial
				DAG file (needs to be parsed in combination with the
				original DAG file)
		*/
	void WriteNodeToRescue( FILE *fp, Job *node,
				bool reset_retries_upon_rescue, bool isPartial );

		/** Write a script specification to a rescue DAG.
			@param fp: file pointer to the rescue DAG file
		    @param script: the script to write
		*/
	static void WriteScriptToRescue( FILE *fp, Script *script );

		// True iff the final node is ready to be run, is running,
		// or has been run (including PRE and POST scripts, if any).
	bool _finalNodeRun;

	/** Escape a string according to new classad syntax.
	    Note:  This method uses a static buffer and is therefore not
		reentrant!
	    @param strIn:  the string to be escaped
		@return:  the properly-escaped string, including surrounding
			double quotes
	*/
	const char *EscapeClassadString( const char* strIn );

	/** Monitor the workflow log file for this DAG.
		@return:  true if successful, false otherwise
	*/
	bool MonitorLogFile();

	/** Unmonitor the workflow log file for this DAG.
		@return:  true if successful, false otherwise
	*/
	bool UnmonitorLogFile();

protected:
    // List of Job objects
    mutable std::vector<Job*>     _jobs;

private:
		// Note: the final node is in the _jobs list; this pointer is just
		// for convenience.
	Job* _final_job;

	Job* _provisioner_node = NULL;

	ProvisionerClassad* _provisionerClassad = NULL;

	bool _provisioner_ready = false;

	std::map<MyString, Job *>		_nodeNameHash;

	std::map<JobID_t, Job *>		_nodeIDHash;

	// Hash by HTCondorID (really just by the cluster ID because all
	// procs in the same cluster map to the same node).
	std::map<int, Job *>			_condorIDHash;

	// NOOP nodes are indexed by subprocID.
	std::map<int, Job *>			_noopIDHash;

    // Number of nodes that are done (completed execution)
    int _numNodesDone;
    
    // Number of nodes that failed (job or PRE or POST script failed)
    int _numNodesFailed;

    // Number of batch system jobs currently submitted
    int _numJobsSubmitted;

    /*  Maximum number of jobs to submit at once.  Non-negative.  Zero means
        unlimited
    */
    int _maxJobsSubmitted;

		// Number of DAG job procs currently idle.
	int _numIdleJobProcs;

    	// Maximum number of idle job procs to allow (stop submitting if the
		// number of idle job procs hits this limit).  Non-negative.  Zero
		// means unlimited.
    int _maxIdleJobProcs;

		// Policy for how we respond to DAG edits.
	std::string _editPolicy;

		// If this is true, nodes for which the job submit fails are retried
		// before any other ready nodes; otherwise a submit failure puts
		// a node at the back of the ready queue.  (Default is true.)
	bool		m_retrySubmitFirst;

		// If this is true, nodes for which the node fails (and the node
		// has retries) are retried before any other ready nodes;
		// otherwise a node failure puts a node at the back of the ready
		// queue.  (Default is false.)
	bool		m_retryNodeFirst;

		// Executable to remove HTCondor jobs.
	const char *	_condorRmExe;

		// HTCondor ID of this DAGMan process.
	const CondorID *	_DAGManJobId;

	// queue of jobs ready to be submitted to HTCondor
	PrioritySimpleList<Job*>* _readyQ;

	// queue of submitted jobs not yet matched with submit events in
	// the HTCondor job log
	std::queue<Job*>* _submitQ;

	ScriptQ* _preScriptQ;
	ScriptQ* _postScriptQ;
	ScriptQ* _holdScriptQ;

		// Number of nodes currently in status Job::STATUS_PRERUN.
	int		_preRunNodeCount;

		// Number of nodes currently in status Job::STATUS_POSTRUN.
	int		_postRunNodeCount;

		// Number of nodes currently running HOLD scripts.
		// We do not have a special status for these nodes.
	int		_holdRunNodeCount;
	
	int DFS_ORDER;
	int _graph_width;
	int _graph_height;
	std::vector<int> _graph_widths;

	// Information for producing dot files, which can be used to visualize
	// DAG files. Dot is part of the graphviz package, which is available from
	// http://www.research.att.com/sw/tools/graphviz/
	char *_dot_file_name;
	char *_dot_include_file_name;
	bool  _update_dot_file;
	bool  _overwrite_dot_file;
	int   _dot_file_name_suffix;

	void IncludeExtraDotCommands(FILE *dot_file);
	void DumpDotFileNodes(FILE *temp_dot_file);
	void DumpDotFileArcs(FILE *temp_dot_file);
	void ChooseDotFileName(MyString &dot_file_name);

		// Name of node status file.
	char *_statusFileName;
		
		// Whether things have changed since the last time the file
		// was written.
	bool _statusFileOutdated;
		
		// Minimum time between updates (so we can avoid trying to
		// write the file too often, e.g., for large DAGs).
	int _minStatusUpdateTime;

		// If this is true, we update the node status file even if
		// nothing has changed (defaults to false).
	bool _alwaysUpdateStatus;

		// Last time the status file was written.
	time_t _lastStatusUpdateTimestamp;

	CheckEvents	_checkCondorEvents;

		// Total count of jobs deferred because of MaxJobs limit (note
		// that a single job getting deferred multiple times is counted
		// multiple times).
	int		_maxJobsDeferredCount;

		// Total count of jobs deferred because of MaxIdle limit (note
		// that a single job getting deferred multiple times is counted
		// multiple times).
	int		_maxIdleDeferredCount;

		// Total count of jobs deferred because of node category throttles.
	int		_catThrottleDeferredCount;

		// whether or not to prohibit multiple job proc submitsn (e.g.,
		// node jobs that create more than one job proc)
	bool		_prohibitMultiJobs;

		// Whether to submit ready nodes in depth-first order (as opposed
		// to breadth-first order).
	bool		_submitDepthFirst;

	bool		_abortOnScarySubmit;

		// The next time we're allowed to try submitting a job -- 0 means
		// go ahead and submit right away.
	time_t		_nextSubmitTime;

		// The delay we use the next time a submit fails -- _nextSubmitTime
		// becomes the current time plus _nextSubmitDelay (this is in
		// seconds).
	int			_nextSubmitDelay;

		// Whether we're in recovery mode.  We only need this here for
		// the PR 554 fix in PostScriptReaper -- otherwise it gets passed
		// down thru the call stack.
	bool		_recovery;

		// The config file (if any) specified for this DAG.  As of 2007-06-08,
		// this is only used when writing a rescue DAG -- we already
		// parse the given config file (if any) before the Dag object
		// is created.
	const char *	_configFile;

		// The time interval (in seconds) at which to print pending
		// node reports.
	int			_pendingReportInterval;

		// The last time we got a log event for a node job.
	time_t		_lastEventTime;

		// The last time we printed a pending node report.
	time_t		_lastPendingNodePrintTime;

		// Default HTCondor ID to use in reseting a node's HTCondor ID on
		// retry.
	static const CondorID	_defaultCondorId;

		// The user log file to be used for nodes jobs.
	const char *_defaultNodeLog;

		// Whether to generate the .condor.sub files for sub-DAGs
		// at run time (just before the node is submitted).
	bool	_generateSubdagSubmits;

		// Options for running condor_submit_dag on nested DAGs.
	SubmitDagDeepOptions *_submitDagDeepOpts;

		// Dag objects are used to parse splice files, which are like include
		// files that ultimately result in a larger in memory dag. To toplevel
		// dag will have this be false, and any included splices will be true.
		// We use this knowledge to not allocate precious daemoncore resources
		// because we know we aren't going to have an "executing" dag object
		// which is also a splice.
	bool _isSplice;

		// The splice "scope" for this DAG (e.g., "A+B+", or "root").
		// This is currently just used for diagnostic messages
		// (wenger 2010-06-07)
	MyString _spliceScope;

		// The maximum fake subprocID we see in recovery mode (needed to
		// initialize the ID for subsequent fake events so IDs don't
		// collide).
	int _recoveryMaxfakeID;

		// The maximum number of times a node job can go on hold before
		// we declare it a failure and remove it; 0 means no limit.
	int _maxJobHolds;

		// If this flag is set, we shouldn't actually run this DAG file
		// (this is set when running a "rescue" produced when DAGMan is
		// run with -DumpRescue and parsing the DAG fails).
	bool _reject;

		// The file and line number where we first found a REJECT
		// specification, if any.
	MyString _firstRejectLoc;

		// The object for logging to the jobstate.log file (for Pegasus).
	JobstateLog _jobstateLog;

	// If true, run the POST script, regardless of the exit status of the PRE script
	// Defaults to true
	bool _alwaysRunPost;

	// If true, don't dry-run the dag. pretending that all jobs terminated successfully
	// upon submission
	int _dry_run;

		// The priority for this DAG. (defaults to 0)
	int _dagPriority;

		// Whether the DAG is currently halted.
	bool _dagIsHalted;

		// Whether the DAG has been aborted.
		// Note:  we need this in addition to _dagStatus, because if you
		// have a abort-dag-on return value of 0, _dagStatus will be
		// DAG_STATUS_OK even on the abort...
	bool _dagIsAborted;

		// The name of the halt file (we halt the DAG if that file exists).
	MyString _haltFile;
	
		// Object to deal with reporting DAGMan metrics (to Pegasus).
	DagmanMetrics *_metrics;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Splice connections.

		// This is the list of nodes connected to a given pin (in or out).
	typedef std::vector<Job *> PinNodes;

		// This is a list of pin ins or pin outs.
	typedef std::vector<PinNodes *> PinList;

		// The pin ins for this DAG.
	PinList _pinIns;

		// The pin outs for this DAG.
	PinList _pinOuts;

	/** Get the list of nodes connected to a pin in or pin out
		@param isPinIn: true if this is for a pin in, false for pin out
		@param pinNum: the number of the pin for which we're getting
			nodes
		@return: a list of nodes connected to this pin
	*/
	const PinNodes *GetPinInOut( bool isPinIn, int pinNum ) const;

	/** Get the number of pin ins or pin outs we have in this DAG
		@param isPinIn: true if this is for pin ins, false for pin outs
		@return: the number of pin ins or pin outs
	*/
	int GetPinCount( bool isPinIn );

	/** Set a pin in or pin out connection for this DAG.
		@param pinList: the pin list to update
		@param node: the node to connect
		@param pinNum: the number of the pin we're connecting
		@return: true on success, false otherwise
	*/
	bool SetPinInOut( PinList &pinList, Job *node, int pinNum );

	/** Get the list of nodes connected to a pin in or pin out
		@param pinList: the pin list to access
		@param inOutStr: "in" or "out" as appropriate
		@param pinNum: the number of the pin for which we're getting
			nodes
		@return: a list of nodes connected to this pin
	*/
	const static PinNodes *GetPinInOut( const PinList &pinList,
				const char *inOutStr, int pinNum );

	/** Delete memory allocated as part of this pin list
		@param pinList: the pin list to delete
	*/
	static void DeletePinList( PinList &pinList );
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Iterator for ALL_NODES implementation.
	mutable std::vector<Job*>::iterator _allNodesIt;

		// The schedd we need to talk to to update the classad.
	DCSchedd *_schedd;
};

#endif /* #ifndef DAG_H */
