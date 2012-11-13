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
#include "condor_config.h"   // for param
#include "../condor_procapi/procapi.h"
#undef min
#undef max
#include <limits>


int configured_statistics_window_quantum() {
    int quantum = param_integer("STATISTICS_WINDOW_QUANTUM_DAEMONCORE", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM_DC", INT_MAX, 1, INT_MAX);
    if (quantum >= INT_MAX)
        quantum = param_integer("STATISTICS_WINDOW_QUANTUM", 4*60, 1, INT_MAX);

    return quantum;
}

static void self_monitor()
{
    daemonCore->monitor_data.CollectData();
    daemonCore->dc_stats.Tick(daemonCore->monitor_data.last_sample_time);
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
    int quantum = configured_statistics_window_quantum();
    if (!_monitoring_is_on) {
        _monitoring_is_on = true;
        _timer_id = daemonCore->Register_Timer( 0, quantum,
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


void DaemonCore::Stats::Reconfig()
{
    int window = param_integer("DCSTATISTICS_WINDOW_SECONDS", -1, -1, INT_MAX);
    if (window < 0)
       window = param_integer("STATISTICS_WINDOW_SECONDS", 1200, 1, INT_MAX);

    int quantum = configured_statistics_window_quantum();
    this->RecentWindowQuantum = quantum;
    this->RecentWindowMax = (window + quantum - 1) / quantum * quantum;

    this->PublishFlags    = 0 | IF_RECENTPUB;
    char * tmp = param("STATISTICS_TO_PUBLISH");
    if (tmp) {
       this->PublishFlags = generic_stats_ParseConfigString(tmp, "DC", "DAEMONCORE", this->PublishFlags);
       free(tmp);
    }
    SetWindowSize(this->RecentWindowMax);
}

void DaemonCore::Stats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, this->RecentWindowQuantum);
}

#define DC_STATS_ADD_RECENT(pool,name,as)  STATS_POOL_ADD_VAL_PUB_RECENT(pool, "DC", name, as) 
#define DC_STATS_PUB_DEBUG(pool,name,as)   STATS_POOL_PUB_DEBUG(pool, "DC", name, as) 

// this is for first time initialization before calling SetWindowSize,
// use the Clear() method to reset stats after the window size has been set.
//
void DaemonCore::Stats::Init() 
{ 
   Clear();
   this->RecentWindowQuantum = configured_statistics_window_quantum();
   this->RecentWindowMax = this->RecentWindowQuantum; 
   this->PublishFlags    = -1;

   // insert static items into the stats pool so we can use the pool 
   // to Advance and Clear.  these items also publish the overall value
   // and the Recent value
   //
   DC_STATS_ADD_RECENT(Pool, SelectWaittime,  IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, SignalRuntime,   IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, TimerRuntime,    IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, SocketRuntime,   IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, PipeRuntime,     IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, Signals,       IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, TimersFired,   IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, SockMessages,  IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, PipeMessages,  IF_BASICPUB);
   //DC_STATS_ADD_RECENT(Pool, SockBytes,     IF_BASICPUB);
   //DC_STATS_ADD_RECENT(Pool, PipeBytes,     IF_BASICPUB);
   DC_STATS_ADD_RECENT(Pool, DebugOuts,     IF_VERBOSEPUB);
   DC_STATS_ADD_RECENT(Pool, PumpCycle,     IF_VERBOSEPUB);

   // Insert additional publish entries for the XXXDebug values
   //
   DC_STATS_PUB_DEBUG(Pool, SelectWaittime,  IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, SignalRuntime,   IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, TimerRuntime,    IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, SocketRuntime,   IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, PipeRuntime,     IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, Signals,       IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, TimersFired,   IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, SockMessages,  IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, PipeMessages,  IF_BASICPUB);
   //DC_STATS_PUB_DEBUG(Pool, SockBytes,     IF_BASICPUB);
   //DC_STATS_PUB_DEBUG(Pool, PipeBytes,     IF_BASICPUB);
   DC_STATS_PUB_DEBUG(Pool, DebugOuts,     IF_VERBOSEPUB);
   DC_STATS_PUB_DEBUG(Pool, PumpCycle,     IF_VERBOSEPUB);
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
   this->Publish(ad, this->PublishFlags);
}

void DaemonCore::Stats::Publish(ClassAd & ad, const char * config) const
{
   int flags = this->PublishFlags;
   if (config && config[0]) {
      flags = generic_stats_ParseConfigString(config, "DC", "DAEMONCORE", 0 | IF_RECENTPUB);
   }
   this->Publish(ad, flags);
}

void DaemonCore::Stats::Publish(ClassAd & ad, int flags) const
{
   if ((flags & IF_PUBLEVEL) > 0) {
      ad.Assign("DCStatsLifetime", (int)StatsLifetime);
      if (flags & IF_VERBOSEPUB)
         ad.Assign("DCStatsLastUpdateTime", (int)StatsLastUpdateTime);

      if (flags & IF_RECENTPUB) {
         ad.Assign("DCRecentStatsLifetime", (int)RecentStatsLifetime);
         if (flags & IF_VERBOSEPUB) {
            ad.Assign("DCRecentStatsTickTime", (int)RecentStatsTickTime);
            ad.Assign("DCRecentWindowMax", (int)RecentWindowMax);
         }
      }
   }
   double dDutyCycle = 0.0;
   if (this->PumpCycle.value.Count) {
      if (this->PumpCycle.value.Sum > 1e-9)
         dDutyCycle = 1.0 - (this->SelectWaittime.value / this->PumpCycle.value.Sum);
   }
   ad.Assign("DaemonCoreDutyCycle", dDutyCycle);
   dDutyCycle = 0.0;
   if (this->PumpCycle.recent.Count) {
      dDutyCycle = 1.0 - (this->SelectWaittime.recent / this->PumpCycle.recent.Sum);
   }
   ad.Assign("RecentDaemonCoreDutyCycle", dDutyCycle);

   Pool.Publish(ad, flags);
}

void DaemonCore::Stats::Unpublish(ClassAd & ad) const
{
   ad.Delete("DCStatsLifetime");
   ad.Delete("DCStatsLastUpdateTime");
   ad.Delete("DCRecentStatsLifetime");
   ad.Delete("DCRecentStatsTickTime");
   ad.Delete("DCRecentWindowMax");
   ad.Delete("DaemonCoreDutyCycle");
   ad.Delete("RecentDaemonCoreDutyCycle");
   Pool.Unpublish(ad);
}

time_t DaemonCore::Stats::Tick(time_t now)
{
   if ( ! now) now = time(NULL);

   int cAdvance = generic_stats_Tick(
      now,
      this->RecentWindowMax,   // RecentMaxTime
      this->RecentWindowQuantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Pool.Advance(cAdvance);

   return now;
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

stats_entry_recent<Probe> * DaemonCore::Stats::AddSample(const char * name, int as, double val)
{
   stats_entry_recent<Probe> * probe = Pool.GetProbe< stats_entry_recent<Probe> >(name);
   if ( ! probe) {
       MyString attr;
       attr.formatstr("Recent%s",name);
       cleanStringForUseAsAttr(attr);
       int as_pub = as | stats_entry_recent<Probe>::PubValueAndRecent;
       probe = Pool.NewProbe< stats_entry_recent<Probe> >(name, attr.Value()+6, as_pub);
       if (probe) {
          //int as_recent = as | stats_entry_recent<Probe>::PubRecent | IF_RECENTPUB;
          //Pool.AddPublish(attr.Value(), probe, strdup(attr.Value()), as_recent);
          probe->SetRecentMax(this->RecentWindowMax / this->RecentWindowQuantum);
       }
   }

   if (probe) 
      probe->Add(val);
   return probe;
}

double DaemonCore::Stats::AddRuntimeSample(const char * name, int as, double before) // returns current time.
{
   double now = UtcTime::getTimeDouble();
   this->AddSample(name, as, now - before);
   return now;
}

void* DaemonCore::Stats::New(const char * category, const char * name, int as)
{
   MyString attr;
   attr.formatstr("DC%s_%s", category, name);
   cleanStringForUseAsAttr(attr);

   void * ret = NULL;
   switch (as & (AS_TYPE_MASK | IS_CLASS_MASK))
      {
      case AS_COUNT | IS_RECENT:
         {
         stats_entry_recent<int> * probe = 
         Pool.NewProbe< stats_entry_recent<int> >(name,  attr.Value(), as);
         probe->SetRecentMax(this->RecentWindowMax / this->RecentWindowQuantum);
         ret = probe;
         }
         break;

      case AS_ABSTIME | IS_RECENT:
      case AS_RELTIME | IS_RECENT:
         {
         stats_entry_recent<time_t> * probe =
         Pool.NewProbe< stats_entry_recent<time_t> >(name,  attr.Value(), as);
         probe->SetRecentMax(this->RecentWindowMax / this->RecentWindowQuantum);
         ret = probe;
         }
         break;

      case AS_COUNT | IS_RCT:
      case AS_RELTIME | IS_RCT:
         {
         stats_recent_counter_timer * probe = 
         Pool.NewProbe<stats_recent_counter_timer>(name, attr.Value(), as);
        #if 0 // def DEBUG
         attr += "Debug";
         Pool.AddPublish(attr.Value(), probe, NULL, 0, 
                       (FN_STATS_ENTRY_PUBLISH)&stats_recent_counter_timer::PublishDebug);
        #endif
         probe->SetRecentMax(this->RecentWindowMax / this->RecentWindowQuantum);
         ret = probe;
         }
         break;

      default:
         EXCEPT("unsupported probe type\n");
         break;
      }

   return ret;
}

dc_stats_auto_runtime_probe::dc_stats_auto_runtime_probe(const char * name, int as)
{
   StatisticsPool * pool = &daemonCore->dc_stats.Pool;
   this->probe = pool->GetProbe< stats_entry_recent<Probe> >(name);
   if ( ! this->probe) {
       MyString attr("DC_Func");
       attr += name;
       cleanStringForUseAsAttr(attr);
       int as_pub = as | stats_entry_recent<Probe>::PubValueAndRecent;
       this->probe = pool->NewProbe< stats_entry_recent<Probe> >(name, attr.Value(), as_pub);
       if (this->probe) {
          this->probe->SetRecentMax(daemonCore->dc_stats.RecentWindowMax / daemonCore->dc_stats.RecentWindowQuantum);
       }
   }
   if (this->probe)
       this->begin = UtcTime::getTimeDouble();
}

dc_stats_auto_runtime_probe::~dc_stats_auto_runtime_probe()
{
   if (this->probe) {
      double now = UtcTime::getTimeDouble();
      this->probe->Add(now - this->begin);
   }
}

