/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * Condor Software Copyright Notice
 * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE.TXT file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _SELF_MONITOR_H_
#define _SELF_MONITOR_H_

#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"
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
    bool ExportData(ClassAd *ad);

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
    // How long this program has been alive, in seconds (from ProcAPI)
    long          age;

private:
    int           _timer_id;
    bool          _monitoring_is_on;
};

#endif
