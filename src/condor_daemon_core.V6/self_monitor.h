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


#ifndef _SELF_MONITOR_H_
#define _SELF_MONITOR_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"

/*
 * An instantiation of this class is meant to be included in
 * DaemonCore It can be used by a daemon to do monitoring.  In
 * daemon_core_main.C, we currently automatically enable monitoring
 * for a few daemons and others can turn it on if they wish. 
 *
 */
class SelfMonitorData
{
public:
    SelfMonitorData();
    ~SelfMonitorData();

    // Turn on periodic monitoring
    void EnableMonitoring(void);
    // Turn off periodic monitoring.
    void DisableMonitoring(void);

    /* This is called to collect data. Normally it is called by a
       DaemonCore timer, which is set up by EnableMonitoring() */
    void CollectData(void);
    /* If you want to put the monitoring ClassAd, just call this
       function with your ClassAd, and we'll drop in the data for
       you. */
    bool ExportData(ClassAd *ad, bool verbose=false) const;

    /* Below is the data that we collect. Feel free to look at it
       directly if you don't want to put it into a ClassAd wtih
       ExportData() */

    // When we last called CollectData, as an epoch time
    time_t        last_sample_time;

    // How much CPU we are using, as a percentage. (from ProcAPI)
    double        cpu_usage;
    // How big the total image size of this program is (from ProcAPI)
    unsigned long image_size;
    // How much actual RAM we are using (resident set size) (from ProcAPI)
    unsigned long rs_size;
    // time (secs) in user mode (from ProcAPI)
    long user_time;
    // time (secs) in system mode (from ProcAPI)
    long sys_time;
    // How long this program has been alive, in seconds (from ProcAPI)
    long          age;
	// How many sockets are registered in daemonCore
	int           registered_socket_count;
	// How many security sessions exist in the cache
	int           cached_security_sessions;

private:
    int           _timer_id;
    bool          _monitoring_is_on;
};

#endif
