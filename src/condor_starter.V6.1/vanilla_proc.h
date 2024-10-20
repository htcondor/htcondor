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


#if !defined(_CONDOR_VANILLA_PROC_H)
#define _CONDOR_VANILLA_PROC_H

#include "os_proc.h"
#include "generic_stats.h"

/* forward reference */
class SafeSock;

struct StarterStatistics {
    // these are used by generic tick
    time_t StatsLifetime;         // the total time covered by this set of statistics
    time_t StatsLastUpdateTime;   // last time that statistics were last updated. (a freshness time)
    time_t RecentStatsLifetime;   // actual time span of current RecentXXX data.
    time_t RecentStatsTickTime;   // last time Recent values Advanced

    time_t InitTime;            // last time we init'ed the structure
    int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
    int    RecentWindowQuantum;
    int    PublishFlags;

    StatisticsPool Pool;          // pool of statistics probes and Publish attrib names

    void Init();
    void Clear();
    time_t Tick(time_t now=0); // call this when time may have changed to update StatsUpdateTime, etc.
    void Reconfig();
    void Publish(ClassAd& ad) const { this->Publish(ad, this->PublishFlags); }
    void Publish(ClassAd& ad, int flags) const;

    // i/o statistics as collected from cgroups blkio controller
    stats_entry_recent<int64_t> BlockReads;        // read operations
    stats_entry_recent<int64_t> BlockWrites;       // write operations
    stats_entry_recent<int64_t> BlockReadBytes;    // bytes read
    stats_entry_recent<int64_t> BlockWriteBytes;   // bytes written
};


/** The Vanilla-type job process class.  Uses procfamily to do its
	dirty work.
 */
class VanillaProc : public OsProc
{
public:
	VanillaProc(ClassAd* jobAd);

	virtual ~VanillaProc();

		/** call OsProc::StartJob(), make a new ProcFamily with new
			process as head. */
	virtual int StartJob();

	virtual int outOfMemoryEvent();
		/** Make certain all decendants are	dead via the ProcFamily,
			save final usage statistics, and call OsProc::JobReaper().
		*/
	virtual bool JobReaper(int pid, int status);

		/** Call family->suspend() */
	virtual void Suspend();

		/** Cass family->resume() */
	virtual void Continue();

	virtual bool Ckpt();

		/** Take a family snapshot, call OsProc::ShutDownGraceful() */
	virtual bool ShutdownGraceful();

		/** Do a family->hardkill(); */
	virtual bool ShutdownFast();

		/** Publish all attributes we care about for updating the job
			controller into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

	virtual std::string CgroupSuffix() { return "";}

	virtual bool ShouldConvertCmdToAbsolutePath() { return m_pid_ns_status_filename.empty();}

	bool finishShutdownFast();

protected:

	virtual int outputOpenFlags();
	virtual int streamingOpenFlags( bool isOutput );

	virtual void restartCheckpointedJob();
private:
		// Final usage stats for this proc and all its children.
	ProcFamilyUsage m_final_usage;
		// Sums the total usage from each previous completed PID in this run.
	ProcFamilyUsage m_checkpoint_usage;

		// standardized statistics reporting logic
	StarterStatistics m_statistics;

#if !defined(WIN32)
	int m_escalation_tid;
#endif

	// Configure OOM killer for this job
	int64_t m_memory_limit; // Memory limit, in bytes.

		// old kernels have /proc/self/oom_adj, newer /proc/self/oom_score_adj
		// and the scales are different.
	int setupOOMScore(int oom_adj, int oom_score_adj);

	std::string m_pid_ns_status_filename;

	// Internal helper functions.
	int pidNameSpaceReaper( int status );
	void recordFinalUsage();
	void killFamilyIfWarranted();
	void notifySuccessfulEvictionCheckpoint();
	void notifySuccessfulPeriodicCheckpoint(int checkpointNumber);
	void notifyFailedPeriodicCheckpoint( int checkpointNumber );

	bool isCheckpointing;
	bool isSoftKilling;
};

#endif
