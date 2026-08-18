/***************************************************************
 *
 * Startup limit support declarations
 *
 ***************************************************************/

#ifndef QMGMT_STARTUP_LIMITS_H
#define QMGMT_STARTUP_LIMITS_H

#include "condor_common.h"
#include "condor_classad.h"
#include <string>
#include <unordered_set>

// Startup limits support per-job cost expressions and burst capacity.

class JobQueueJob;
class ReliSock;
class Stream;

// Startup limit support
// Check whether a job can start under currently installed limits; appends any
// limit UUIDs that blocked the job into blocked_limits (if provided).
bool StartupLimitsAllowJob(JobQueueJob *job, ClassAd *match_ad, const char *user, const char *pool, std::unordered_set<std::string> *blocked_limits, bool record_stats = true);

// True when no startup limits are currently installed.
bool StartupLimitsEmpty();

// Record that a set of limits caused a match to be ignored, updating per-source
// ban/cooldown tracking for the given user/pool key.
void StartupLimitsRecordIgnoredMatches(const std::unordered_set<std::string> &blocked_limits, const char *user, const char *pool);

// Apply throttling side effects to a matchmaking request: tighten requirements
// or reduce match_max based on active limits that have recently blocked this
// user/pool source. Does not mutate AUTO_CLUSTER_ATTRS directly.
void StartupLimitsAdjustRequest(JobQueueJob *job, ClassAd &request_ad, int *match_max, const char *user, const char *pool);

// Gather all job attribute references used by job-only startup limits so the
// autocluster subsystem can include them when computing signatures.
void StartupLimitsCollectJobRefs(classad::References &refs);

// DaemonCore handlers for the startup limit RPC commands.
int HandleCreateStartupLimitCommand(int cmd, Stream *stream);
int HandleQueryStartupLimitsCommand(int cmd, Stream *stream);

#endif // QMGMT_STARTUP_LIMITS_H
