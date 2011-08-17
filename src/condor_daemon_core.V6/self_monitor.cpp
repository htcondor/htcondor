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
    daemonCore->dc_stats.Tick();
    daemonCore->dc_stats.DebugOuts += dprintf_getCount();
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
	cached_security_sessions = 0;
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

	cached_security_sessions = daemonCore->getSecMan()->session_cache->count();

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
		ad->Assign("MonitorSelfSecuritySessions", (int) cached_security_sessions);

        success = true;
    }

    return success;
}

//------------------------------------------------------------------------------------------
//                          DaemonCore Statistics

// this structure controls what members of dc_stats are published, and what names
// they are published as. 
//
//
#define DC_STATS_ENTRY(name, as)        GENERIC_STATS_ENTRY(DaemonCore::Stats, "DC", name, as)
#define DC_STATS_ENTRY_RECENT(name, as) GENERIC_STATS_ENTRY_RECENT(DaemonCore::Stats, "DC", name, as)
#define DC_STATS_ENTRY_PEAK(name, as)   GENERIC_STATS_ENTRY_PEAK(DaemonCore::Stats, "DC", name, as)

static const GenericStatsEntry DCStatsTable[] = {
   DC_STATS_ENTRY(StatsLifetime,         AS_RELTIME),
   DC_STATS_ENTRY(StatsLastUpdateTime,   AS_ABSTIME),
   DC_STATS_ENTRY(RecentStatsLifetime,   AS_RELTIME),
   DC_STATS_ENTRY(RecentWindowMax,       AS_COUNT),

   DC_STATS_ENTRY(SelectWaittime,  AS_RELTIME),
   DC_STATS_ENTRY(SignalRuntime,   AS_RELTIME),
   DC_STATS_ENTRY(TimerRuntime,    AS_RELTIME),
   DC_STATS_ENTRY(SocketRuntime,   AS_RELTIME),
   DC_STATS_ENTRY(PipeRuntime,     AS_RELTIME),

   DC_STATS_ENTRY(Signals,       AS_COUNT),
   DC_STATS_ENTRY(TimersFired,   AS_COUNT),
   DC_STATS_ENTRY(SockMessages,  AS_COUNT),
   DC_STATS_ENTRY(PipeMessages,  AS_COUNT),
   //DC_STATS_ENTRY(SockBytes,     AS_COUNT),
   //DC_STATS_ENTRY(PipeBytes,     AS_COUNT),
   DC_STATS_ENTRY(DebugOuts,     AS_COUNT),

   DC_STATS_ENTRY_RECENT(SelectWaittime,  AS_RELTIME),
   DC_STATS_ENTRY_RECENT(SignalRuntime,   AS_RELTIME),
   DC_STATS_ENTRY_RECENT(TimerRuntime,    AS_RELTIME),
   DC_STATS_ENTRY_RECENT(SocketRuntime,   AS_RELTIME),
   DC_STATS_ENTRY_RECENT(PipeRuntime,     AS_RELTIME),

   DC_STATS_ENTRY_RECENT(Signals,       AS_COUNT),
   DC_STATS_ENTRY_RECENT(TimersFired,   AS_COUNT),
   DC_STATS_ENTRY_RECENT(SockMessages,  AS_COUNT),
   DC_STATS_ENTRY_RECENT(PipeMessages,  AS_COUNT),
   //DC_STATS_ENTRY_RECENT(SockBytes,     AS_COUNT),
   //DC_STATS_ENTRY_RECENT(PipeBytes,     AS_COUNT),
   DC_STATS_ENTRY_RECENT(DebugOuts,     AS_COUNT),
};

// the windowed schedd statistics are quantized to the nearest N seconds
// WINDOWED_STAT_WIDTH/schedd_stats_window_quantum is the number of slots
// in the window ring_buffer.
const int dc_stats_window_quantum = 5*60; // 5min quantum


void DaemonCore::Stats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   int cMax = window / dc_stats_window_quantum;
   generic_stats_SetRBMax(DCStatsTable, COUNTOF(DCStatsTable), (char*)this, cMax);
}

// this is for first time initialization before calling SetWindowSize,
// use the Clear() method to reset stats after the window size has been set.
//
void DaemonCore::Stats::Init() 
{ 
   // clear all data members between InitTime and RecentWindowMax.  
   // note that this will leak Recent data if you do this after calling SetWindowSize.
   int cb = (int)((char*)&this->RecentWindowMax - (char*)&this->InitTime);
   memset((char*)&this->InitTime, 0, cb); 
   this->InitTime = time(NULL); 
   this->RecentWindowMax = dc_stats_window_quantum; 
}

void DaemonCore::Stats::Clear()
{
   generic_stats_ClearAll(DCStatsTable, COUNTOF(DCStatsTable), (char*)this);
   this->InitTime = time(NULL);
   this->StatsPrevUpdateTime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsLifetime = 0;
}

void DaemonCore::Stats::Publish(ClassAd & ad) const
{
   generic_stats_PublishToClassAd(ad, DCStatsTable, COUNTOF(DCStatsTable), (const char *)this);  
}

void DaemonCore::Stats::Unpublish(ClassAd & ad) const
{
   generic_stats_DeleteInClassAd(ad, DCStatsTable, COUNTOF(DCStatsTable), (const char *)this);  
}

void DaemonCore::Stats::Tick()
{
   time_t now = time(NULL);

   // when working from freshly initialized stats, the first Tick should not Advance.
   //
   if (this->StatsLastUpdateTime == 0)
      {
      this->StatsLastUpdateTime = now;
      this->StatsPrevUpdateTime = now;
      this->RecentStatsLifetime = 0;
      return;
      }

   // whenever 'now' changes, we want to check to see how much time has passed
   // since the last Advance, and if that time exceeds the quantum, we advance.
   //
   if (this->StatsLastUpdateTime != now) 
      {
      time_t delta = now - this->StatsPrevUpdateTime;

      // if the time since last update exceeds the window size, just throw away the recent data
      if (delta >= this->RecentWindowMax)
         {
         generic_stats_ClearRecent(DCStatsTable, COUNTOF(DCStatsTable), (char*)this);
         this->StatsPrevUpdateTime = this->StatsLastUpdateTime;
         this->RecentStatsLifetime = this->RecentWindowMax;
         delta = 0;
         }
      else if (delta >= dc_stats_window_quantum)
         {
         for (int ii = 0; ii < delta / dc_stats_window_quantum; ++ii)
            {
            generic_stats_AdvanceRecent(DCStatsTable, COUNTOF(DCStatsTable), (char*)this, 1);
            }
         this->StatsPrevUpdateTime = now - (delta % dc_stats_window_quantum);
         }

      time_t recent_window = (int)(this->RecentStatsLifetime + now - this->StatsLastUpdateTime);
      this->RecentStatsLifetime = (recent_window < this->RecentWindowMax) ? recent_window : this->RecentWindowMax;
      this->StatsLastUpdateTime = now;
      }

   this->StatsLifetime = now - this->InitTime;
}
