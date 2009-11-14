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


#include "condor_common.h"
#include "self_monitor.h"
#include "condor_daemon_core.h"
#include "../condor_procapi/procapi.h"

static void self_monitor()
{
    daemonCore->monitor_data.CollectData();
}

SelfMonitorData::SelfMonitorData()
{
    _monitoring_is_on = false;
    _timer_id         = -1;

    last_sample_time = -1;
    cpu_usage        = -1.0;
    image_size       = 0;
    rs_size          = 0;
    age              = -1;
	registered_socket_count = 0;
    return;
}

SelfMonitorData::~SelfMonitorData()
{
    return;
}

void SelfMonitorData::EnableMonitoring(void)
{
    if (!_monitoring_is_on) {
        _monitoring_is_on = true;
        _timer_id = daemonCore->Register_Timer( 0, 240,
                                self_monitor, "self_monitor" );
    }
    return;
}

void SelfMonitorData::DisableMonitoring(void)
{
    if (_monitoring_is_on) {
        _monitoring_is_on = false;
        daemonCore->Cancel_Timer(_timer_id);
        _timer_id = -1;
    }
    return;
}

void SelfMonitorData::CollectData(void)
{
	int status; 

    // Record what time we are collecting this info
    last_sample_time = time(NULL);
    
    // Use ProcAPI to get what we can
    piPTR my_process_info = NULL;

    dprintf(D_FULLDEBUG, "Getting monitoring info for pid %d\n", getpid());

    ProcAPI::getProcInfo(getpid(), my_process_info, status);

    if (my_process_info != NULL) {
        cpu_usage  = my_process_info->cpuusage;
        image_size = my_process_info->imgsize;
        rs_size    = my_process_info->rssize;
        age        = my_process_info->age;

        delete my_process_info;
    }

    // Collect CEDAR's monitoring data

	registered_socket_count = daemonCore->RegisteredSocketCount();

    // Collecting more info is yet to be done
    return;
}

bool SelfMonitorData::ExportData(ClassAd *ad)
{
    bool      success;
    MyString  attribute;

    if (ad == NULL) {
        success = false;
    } else {
        ad->Assign("MonitorSelfTime",            (int)   last_sample_time);
        ad->Assign("MonitorSelfCPUUsage",        (float) cpu_usage);
        ad->Assign("MonitorSelfImageSize",       (float) image_size);
        ad->Assign("MonitorSelfResidentSetSize", (int)   rs_size);
        ad->Assign("MonitorSelfAge",             (int)   age);
		ad->Assign("MonitorSelfRegisteredSocketCount", (int) registered_socket_count);

        success = true;
    }

    return success;
}

