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
         ret = Pool.NewProbe< stats_entry_recent<int> >(name,  attr.Value(), 0);
         break;

      case AS_ABSTIME | IS_RECENT:
      case AS_RELTIME | IS_RECENT:
         ret = Pool.NewProbe< stats_entry_recent<time_t> >(name,  attr.Value(), 0);
         break;

      case AS_COUNT | IS_RCT:
      case AS_RELTIME | IS_RCT:
         ret = Pool.NewProbe<stats_recent_counter_timer>(name, attr.Value());
         break;

      default:
         EXCEPT("unsupported probe type\n");
         break;
      }

   return ret;
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//////  test recent of probe
////////////////////////////////////////////////////////////////////

class Probe {
public:
   Probe(int=0) 
      : Count(0)
      , Max(std::numeric_limits<double>::min())
      , Min(std::numeric_limits<double>::max())
      , Sum(0.0)
      , SumSq(0.0) 
   {
   }
   Probe(double val) : Count(1), Max(val), Min(val), Sum(val), SumSq(val*val) {}

public:
   int    Count;      // count of samples 
   double Max;        // max sample so far
   double Min;        // min sample so far
   double Sum;        // Sum of samples
   double SumSq;      // Sum of samples squared

public:
   void Clear() {
      Count = 0; // value is use to store the count of samples.
      Max = std::numeric_limits<double>::min();
      Min = std::numeric_limits<double>::max();
      SumSq = Sum = 0.0;
   }

   double Add(double val) { 
      Count += 1; // value is use to store the count of samples.
      if (val > Max) Max = val;
      if (val < Min) Min = val;
      Sum += val;
      SumSq += val*val;
      return Sum;
   }

   Probe & Add(const Probe & val) { 
      if (val.Count >= 1) {
         Count += val.Count;
         if (val.Max > Max) Max = val.Max;
         if (val.Min < Min) Min = val.Min;
         Sum += val.Sum;
         SumSq += val.SumSq;
         }
      return *this;
   }

   double Avg() {
      if (Count > 0) {
         return this->Sum / this->Count;
      } else {
         return this->Sum;
      }
   }

   double Var() {
      if (Count <= 1) {
         return this->Min;
      } else {
         // Var == (SumSQ - count*Avg*Avg)/(count -1)
         return (this->SumSq - this->Sum * (this->Sum / this->Count))/(this->Count - 1);
      }
   }

   double Std() {
      if (Count <= 1) {
         return this->Min;
      } else {
         return sqrt(this->Var());
      }
   }

   // operator overloads
   Probe& operator+=(double val) { Add(val); return *this; }
   Probe& operator+=(const Probe & val) { Add(val); return *this; }
   //Probe& operator-=(const Probe & val) { Remove(val); return *this; }
   bool   operator!=(const int val) const { return this->Sum != val; }
};

template <>
void stats_entry_recent<Probe>::AdvanceBy(int cSlots) { 
   if (cSlots <= 0) 
      return;

   // remove the values associated with the slot being overwritten
   // from the aggregate value.
   while (cSlots > 0) {
      buf.Advance();
      --cSlots;
      }

   recent = buf.Sum();
}

template <>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, Probe& probe) {
   MyString attr(pattr);
   ad.Assign(pattr, probe.Count);
   attr += "Runtime";
   ad.Assign(attr.Value(), probe.Sum);
   if (probe.Count > 0)
      {
      int pos = attr.Length();
      attr += "Avg";
      ad.Assign(attr.Value(), probe.Avg());

      attr.replaceString("Avg","Min",pos);
      ad.Assign(attr.Value(), probe.Min);

      attr.replaceString("Min","Max",pos);
      ad.Assign(attr.Value(), probe.Max);

      attr.replaceString("Min","Max",pos);
      ad.Assign(attr.Value(), probe.Max);

      attr.replaceString("Max","Std",pos);
      ad.Assign(attr.Value(), probe.Std());
      }
}

void TestProbe()
{
   struct {
      stats_entry_recent<Probe> Runtime;
   } stats;

   stats.Runtime.SetRecentMax(5);

   double runtime = UtcTime::getTimeDouble();

   sleep(2);
   double now = UtcTime::getTimeDouble();
   stats.Runtime += (now - runtime);
   now = runtime;

   stats.Runtime.AdvanceBy(1);
}
