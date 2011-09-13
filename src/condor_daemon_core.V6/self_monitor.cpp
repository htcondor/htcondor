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
#include "classad_helpers.h" // for cleanStringForUseAsAttr
#include "../condor_procapi/procapi.h"
#undef min
#undef max
#include <limits>

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


// the windowed schedd statistics are quantized to the nearest N seconds
// WINDOWED_STAT_WIDTH/schedd_stats_window_quantum is the number of slots
// in the window ring_buffer.
const int dc_stats_window_quantum = 4*60; // 4min quantum

void DaemonCore::Stats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, dc_stats_window_quantum);
}

#define DC_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "DC", name, as) 
#define DC_STATS_PUB_DEBUG(pool,name,as)   STATS_POOL_PUB_DEBUG(pool, "DC", name, as) 

// this is for first time initialization before calling SetWindowSize,
// use the Clear() method to reset stats after the window size has been set.
//
void DaemonCore::Stats::Init() 
{ 
   Clear();
   this->RecentWindowMax = dc_stats_window_quantum; 

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   // and the Recent value
   //
   DC_STATS_ADD_RECENT(Pool, SelectWaittime,  0);
   DC_STATS_ADD_RECENT(Pool, SignalRuntime,   0);
   DC_STATS_ADD_RECENT(Pool, TimerRuntime,    0);
   DC_STATS_ADD_RECENT(Pool, SocketRuntime,   0);
   DC_STATS_ADD_RECENT(Pool, PipeRuntime,     0);
   DC_STATS_ADD_RECENT(Pool, Signals,       0);
   DC_STATS_ADD_RECENT(Pool, TimersFired,   0);
   DC_STATS_ADD_RECENT(Pool, SockMessages,  0);
   DC_STATS_ADD_RECENT(Pool, PipeMessages,  0);
   //DC_STATS_ADD_RECENT(Pool, SockBytes,     0);
   //DC_STATS_ADD_RECENT(Pool, PipeBytes,     0);
   DC_STATS_ADD_RECENT(Pool, DebugOuts,     0);

   // Insert additional publish entries for the XXXDebug values
   //
#if 0 //def DEBUG
   DC_STATS_PUB_DEBUG(Pool, SelectWaittime,  0);
   DC_STATS_PUB_DEBUG(Pool, SignalRuntime,   0);
   DC_STATS_PUB_DEBUG(Pool, TimerRuntime,    0);
   DC_STATS_PUB_DEBUG(Pool, SocketRuntime,   0);
   DC_STATS_PUB_DEBUG(Pool, PipeRuntime,     0);
   DC_STATS_PUB_DEBUG(Pool, Signals,       0);
   DC_STATS_PUB_DEBUG(Pool, TimersFired,   0);
   DC_STATS_PUB_DEBUG(Pool, SockMessages,  0);
   DC_STATS_PUB_DEBUG(Pool, PipeMessages,  0);
   //DC_STATS_PUB_DEBUG(Pool, SockBytes,     0);
   //DC_STATS_PUB_DEBUG(Pool, PipeBytes,     0);
   DC_STATS_PUB_DEBUG(Pool, DebugOuts,     0);
#endif
}

void DaemonCore::Stats::Clear()
{
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
   Pool.Clear();
}

void DaemonCore::Stats::Publish(ClassAd & ad) const
{
   ad.Assign("DCStatsLifetime", (int)StatsLifetime);
   ad.Assign("DCStatsLastUpdateTime", (int)StatsLastUpdateTime);
   ad.Assign("DCRecentStatsLifetime", (int)RecentStatsLifetime);
   ad.Assign("DCRecentStatsTickTime", (int)RecentStatsTickTime);
   ad.Assign("DCRecentWindowMax", (int)RecentWindowMax);
   Pool.Publish(ad);
}

void DaemonCore::Stats::Unpublish(ClassAd & ad) const
{
   ad.Delete("DCStatsLifetime");
   ad.Delete("DCStatsLastUpdateTime");
   ad.Delete("DCRecentStatsLifetime");
   ad.Delete("DCRecentStatsTickTime");
   ad.Delete("DCRecentWindowMax");
   Pool.Unpublish(ad);
}

void DaemonCore::Stats::Tick()
{
   int cAdvance = generic_stats_Tick(
      this->RecentWindowMax,   // RecentMaxTime
      dc_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);
}


void DaemonCore::Stats::AddToProbe(const char * name, int val)
{
   stats_entry_recent<int>* pstat = Pool.GetProbe<stats_entry_recent<int> >(name);
   if (pstat)
      pstat->Add(val);
}

void DaemonCore::Stats::AddToProbe(const char * name, int64_t val)
{
   stats_entry_recent<int64_t>* pstat = Pool.GetProbe<stats_entry_recent<int64_t> >(name);
   if (pstat)
      pstat->Add(val);
}

double DaemonCore::Stats::AddRuntime(const char * name, double before)
{
   double now = UtcTime::getTimeDouble();
   stats_recent_counter_timer * probe = Pool.GetProbe<stats_recent_counter_timer>(name);
   if (probe)
      probe->Add(now - before);
   return now;
}

void* DaemonCore::Stats::New(const char * category, const char * name, int as)
{
   MyString attr;
   attr.sprintf("DC%s_%s", category, name);
   cleanStringForUseAsAttr(attr);

   void * ret = NULL;
   switch (as & (AS_TYPE_MASK | IS_CLASS_MASK))
      {
      case AS_COUNT | IS_RECENT:
         {
         stats_entry_recent<int> * probe = 
         Pool.NewProbe< stats_entry_recent<int> >(name,  attr.Value(), 0);
         probe->SetRecentMax(this->RecentWindowMax / dc_stats_window_quantum);
         ret = probe;
         }
         break;

      case AS_ABSTIME | IS_RECENT:
      case AS_RELTIME | IS_RECENT:
         {
         stats_entry_recent<time_t> * probe =
         Pool.NewProbe< stats_entry_recent<time_t> >(name,  attr.Value(), 0);
         probe->SetRecentMax(this->RecentWindowMax / dc_stats_window_quantum);
         ret = probe;
         }
         break;

      case AS_COUNT | IS_RCT:
      case AS_RELTIME | IS_RCT:
         {
         stats_recent_counter_timer * probe = 
         Pool.NewProbe<stats_recent_counter_timer>(name, attr.Value());
        #if 0 // def DEBUG
         attr += "Debug";
         Pool.AddPublish(attr.Value(), probe, NULL, 0, 
                       (FN_STATS_ENTRY_PUBLISH)&stats_recent_counter_timer::PublishDebug);
        #endif
         probe->SetRecentMax(this->RecentWindowMax / dc_stats_window_quantum);
         ret = probe;
         }
         break;

      default:
         EXCEPT("unsupported probe type\n");
         break;
      }

   return ret;
}

