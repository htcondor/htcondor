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

// this structure controls what members of dc_stats are published, and what names
// they are published as. 
//
//
void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<int> & me)
{
   MyString str;
   str.sprintf("%d %d {h:%d c:%d m:%d a:%d}", 
                 me.value, me.recent, me.buf.ixHead, me.buf.cItems, me.buf.cMax, me.buf.cAlloc);
   if (me.buf.pbuf) {
      for (int ix = 0; ix < me.buf.cAlloc; ++ix)
         str.sprintf_cat(!ix ? "[%d" : (ix == me.buf.cMax ? "|%d" : ",%d"), me.buf.pbuf[ix]);
      str.sprintf_cat("]");
      }
   ad.Assign(pattr, str);
}

void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<double> & me)
{
   MyString str;
   str.sprintf("%.3f %.3f {h:%d c:%d m:%d a:%d}", 
                 me.value, me.recent, me.buf.ixHead, me.buf.cItems, me.buf.cMax, me.buf.cAlloc);
   if (me.buf.pbuf) {
      for (int ix = 0; ix < me.buf.cAlloc; ++ix)
         str.sprintf_cat(!ix ? "[%.3f" : (ix == me.buf.cMax ? "|%.3f" : ",%.3f"), me.buf.pbuf[ix]);
      str.sprintf_cat("]");
      }
   ad.Assign(pattr, str);
}

/* long conflicts with int64_t on some platforms.
void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<long> & me)
{
   MyString str;
   str.sprintf("%d %d {h:%d c:%d m:%d a:%d}", 
                 (int)me.value, (int)me.recent, (int)me.buf.ixHead, (int)me.buf.cItems, (int)me.buf.cMax, (int)me.buf.cAlloc);
   if (me.buf.pbuf) {
      for (int ix = 0; ix < me.buf.cAlloc; ++ix)
         str.sprintf_cat(!ix ? "[%d" : (ix == me.buf.cMax ? "|%d" : ",%d"), (int)me.buf.pbuf[ix]);
      str.sprintf_cat("]");
      }
   ad.Assign(pattr, str);
}
*/
void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<int64_t> & me)
{
   MyString str;
   str.sprintf("%d %d {h:%d c:%d m:%d a:%d}", 
                 (int)me.value, (int)me.recent, (int)me.buf.ixHead, (int)me.buf.cItems, (int)me.buf.cMax, (int)me.buf.cAlloc);
   if (me.buf.pbuf) {
      for (int ix = 0; ix < me.buf.cAlloc; ++ix)
         str.sprintf_cat(!ix ? "[%d" : (ix == me.buf.cMax ? "|%d" : ",%d"), (int)me.buf.pbuf[ix]);
      str.sprintf_cat("]");
      }
   ad.Assign(pattr, str);
}

#define DC_STATS_PUB(name, as)        GENERIC_STATS_PUB(DaemonCore::Stats, "DC", name, as)
#define DC_STATS_PUB_RECENT(name, as) GENERIC_STATS_PUB_RECENT(DaemonCore::Stats, "DC", name, as)
#define DC_STATS_PUB_PEAK(name, as)   GENERIC_STATS_PUB_PEAK(DaemonCore::Stats, "DC", name, as)
#define DC_STATS_PUB_TYPE(name,T,as)  GENERIC_STATS_PUB_TYPE(DaemonCore::Stats, "DC", name, as, T)
#define DC_STATS_PUB_RECENT_DEBUG(name, as) GENERIC_STATS_PUB_RECENT_DEBUG(DaemonCore::Stats, "DC", name, as)

const GenericStatsPubItem DCStatsPub[] = {
   DC_STATS_PUB_TYPE(StatsLifetime,       time_t,  AS_RELTIME),
   DC_STATS_PUB_TYPE(StatsLastUpdateTime, time_t,  AS_ABSTIME),
   DC_STATS_PUB_TYPE(RecentStatsLifetime, time_t,  AS_RELTIME),
   DC_STATS_PUB_TYPE(RecentWindowMax,     int,     AS_COUNT),

   DC_STATS_PUB(SelectWaittime,  AS_RELTIME),
   DC_STATS_PUB(SignalRuntime,   AS_RELTIME),
   DC_STATS_PUB(TimerRuntime,    AS_RELTIME),
   DC_STATS_PUB(SocketRuntime,   AS_RELTIME),
   DC_STATS_PUB(PipeRuntime,     AS_RELTIME),

   DC_STATS_PUB(Signals,       AS_COUNT),
   DC_STATS_PUB(TimersFired,   AS_COUNT),
   DC_STATS_PUB(SockMessages,  AS_COUNT),
   DC_STATS_PUB(PipeMessages,  AS_COUNT),
   //DC_STATS_PUB(SockBytes,     AS_COUNT),
   //DC_STATS_PUB(PipeBytes,     AS_COUNT),
   DC_STATS_PUB(DebugOuts,     AS_COUNT),

   DC_STATS_PUB_RECENT(SelectWaittime,  AS_RELTIME),
   DC_STATS_PUB_RECENT(SignalRuntime,   AS_RELTIME),
   DC_STATS_PUB_RECENT(TimerRuntime,    AS_RELTIME),
   DC_STATS_PUB_RECENT(SocketRuntime,   AS_RELTIME),
   DC_STATS_PUB_RECENT(PipeRuntime,     AS_RELTIME),

   DC_STATS_PUB_RECENT(Signals,       AS_COUNT),
   DC_STATS_PUB_RECENT(TimersFired,   AS_COUNT),
   DC_STATS_PUB_RECENT(SockMessages,  AS_COUNT),
   DC_STATS_PUB_RECENT(PipeMessages,  AS_COUNT),
   //DC_STATS_PUB_RECENT(SockBytes,     AS_COUNT),
   //DC_STATS_PUB_RECENT(PipeBytes,     AS_COUNT),
   DC_STATS_PUB_RECENT(DebugOuts,     AS_COUNT),

   DC_STATS_PUB_TYPE(RecentStatsTickTime, time_t,  AS_ABSTIME),
   DC_STATS_PUB_RECENT_DEBUG(DebugOuts, AS_COUNT),
   DC_STATS_PUB_RECENT_DEBUG(Signals, AS_COUNT),
   //DC_STATS_PUB_RECENT_DEBUG(SignalRuntime, AS_RELTIME),

   #ifdef WIN32
    DC_STATS_PUB(AsyncPipe,     AS_COUNT),
    DC_STATS_PUB_RECENT(AsyncPipe,     AS_COUNT),
   #endif
   };

// the windowed schedd statistics are quantized to the nearest N seconds
// WINDOWED_STAT_WIDTH/schedd_stats_window_quantum is the number of slots
// in the window ring_buffer.
const int dc_stats_window_quantum = 4*60; // 4min quantum

void DaemonCore::Stats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   generic_stats_SetRecentMax(DCStatsPub, COUNTOF(DCStatsPub), (char*)this, 
                              window, dc_stats_window_quantum);
}

// this is for first time initialization before calling SetWindowSize,
// use the Clear() method to reset stats after the window size has been set.
//
void DaemonCore::Stats::Init() 
{ 
   // clear all data members between InitTime and RecentWindowMax.  
   // note that this will leak Recent data if you do this after calling SetWindowSize.
   Clear();
   this->RecentWindowMax = dc_stats_window_quantum; 
}

void DaemonCore::Stats::Clear()
{
   generic_stats_Clear(DCStatsPub, COUNTOF(DCStatsPub), (char*)this);
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
}

void DaemonCore::Stats::Publish(ClassAd & ad) const
{
   generic_stats_PublishToClassAd(ad, DCStatsPub, COUNTOF(DCStatsPub), (const char *)this);  
   Named.Publish(ad);
}

void DaemonCore::Stats::Unpublish(ClassAd & ad) const
{
   generic_stats_DeleteInClassAd(ad, DCStatsPub, COUNTOF(DCStatsPub), (const char *)this);  
   Named.Unpublish(ad);
}

void DaemonCore::Stats::Tick()
{
#if 1

const GenericStatsPubItem DCStatsTick[] = {
	   DC_STATS_PUB_RECENT(SelectWaittime, 0),
	   DC_STATS_PUB_RECENT(SignalRuntime, 0),
	   DC_STATS_PUB_RECENT(TimerRuntime, 0),
	   DC_STATS_PUB_RECENT(SocketRuntime, 0),
	   DC_STATS_PUB_RECENT(PipeRuntime, 0),

	   DC_STATS_PUB_RECENT(Signals, 0),
	   DC_STATS_PUB_RECENT(TimersFired, 0),
	   DC_STATS_PUB_RECENT(SockMessages, 0),
	   DC_STATS_PUB_RECENT(PipeMessages, 0),
	   //DC_STATS_PUB_RECENT(SockBytes, 0),
	   //DC_STATS_PUB_RECENT(PipeBytes, 0),
	   DC_STATS_PUB_RECENT(DebugOuts, 0),
   };

   int cAdvance = generic_stats_Tick(
      DCStatsTick, COUNTOF(DCStatsTick), 
      (char*)this,
      this->RecentWindowMax,   // RecentMaxTime
      dc_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

   if (cAdvance)
      Named.Advance(cAdvance);
#else
   time_t now = time(NULL);

   // when working from freshly initialized stats, the first Tick should not Advance.
   //
   if (this->StatsLastUpdateTime == 0)
      {
      this->StatsLastUpdateTime = now;
      this->RecentStatsTickTime = now;
      this->RecentStatsLifetime = 0;
      return;
      }

   // whenever 'now' changes, we want to check to see how much time has passed
   // since the last Advance, and if that time exceeds the quantum, we advance.
   //
   if (this->StatsLastUpdateTime != now) 
      {
      time_t delta = now - this->RecentStatsTickTime;

      // if the time since last update exceeds the window size, just throw away the recent data
      if (delta >= this->RecentWindowMax)
         {
         generic_stats_ClearRecent(DCStatsPub, COUNTOF(DCStatsPub), (char*)this);
         this->RecentStatsTickTime = now;
         this->RecentStatsLifetime = this->RecentWindowMax;
         delta = 0;
         }
      else if (delta >= dc_stats_window_quantum)
         {
         for (int ii = 0; ii < delta / dc_stats_window_quantum; ++ii)
            {
            generic_stats_AdvanceRecent(DCStatsPub, COUNTOF(DCStatsPub), (char*)this, 1);
            }
         this->RecentStatsTickTime = now - (delta % dc_stats_window_quantum);
         }

      time_t recent_window = (int)(this->RecentStatsLifetime + now - this->RecentStatsTickTime);
      this->RecentStatsLifetime = (recent_window < this->RecentWindowMax) ? recent_window : this->RecentWindowMax;
      this->StatsLastUpdateTime = now;
      }

   this->StatsLifetime = now - this->InitTime;
#endif
}

class stats_recent_counter_timer {
private:
   stats_entry_recent<int> count;
   stats_entry_recent<double> runtime;

public:
   stats_recent_counter_timer(int cRecentMax=0) 
      : count(cRecentMax)
      , runtime(cRecentMax) 
      {
      };

   double Add(double sec)     { count += 1; runtime += sec; return runtime.value; }
   time_t Add(time_t time)    { count += 1; runtime += double(time); }
   void Clear()              { count.Clear(); runtime.Clear();}
   void ClearRecent()        { count.ClearRecent(); runtime.ClearRecent(); }
   void AdvanceBy(int cSlots) { count.AdvanceBy(cSlots); runtime.AdvanceBy(cSlots); }
   void SetRecentMax(int cMax)    { count.SetRecentMax(cMax); runtime.SetRecentMax(cMax); }
   double operator+=(double val)    { return Add(val); }

   static const int unit = 0x4000 | IS_RINGBUF | stats_entry_type<int>::id;
   static void PublishValue(const char * me, ClassAd & ad, const char * pattr);
};

void stats_recent_counter_timer::PublishValue(const char * me, ClassAd & ad, const char * pattr)
{
   const stats_recent_counter_timer * pthis = (const stats_recent_counter_timer *)me;

   MyString str(pattr);
   MyString strR("Recent");
   strR += pattr;

   ClassAdAssign(ad, str.Value(), pthis->count.value);
   ClassAdAssign(ad, strR.Value(), pthis->count.recent);

   str += "Runtime";
   strR += "Runtime";
   ClassAdAssign(ad, str.Value(), pthis->runtime.value);
   ClassAdAssign(ad, strR.Value(), pthis->runtime.recent);

   //str.sprintf("Debug%s", pattr);
   //StatsPublishDebug(str.Value(), ad, pthis->count);
   //str += "Runtime";
   //StatsPublishDebug(str.Value(), ad, pthis->runtime);
}


unsigned int hashFuncVoidPtr( void* const & pv )
{
   unsigned int ui = 0;
   for (int ix = 0; ix < (int)(sizeof(void*) / sizeof(int)); ++ix)
      {
      ui += ((int*)&pv)[ix];
      }
   return ui;
}

int stats_pool::Remove (const char * name)
{
   pubitem item = { 0, NULL, NULL };
   if (pub.lookup(name, item) < 0)
      return 0;
   int ret =  pub.remove(name);
   //generic_stats_itemFree(item.units, item.pitem);
   return ret;
}

template <typename T> 
T stats_pool::Add (
   const char * name, 
   int as, 
   T probe, 
   int unit,
   const char * pattr/*=NULL*/,
   void (*fnpub)(const char * me, ClassAd & ad, const char * pattr)/*=NULL*/) 
{
   pubitem item = { 
      as | unit, 
      (void *)probe, 
      pattr ? strdup(pattr) : NULL, 
      fnpub ? fnpub : probe->PublishValue 
      };
   pub.insert(name, item);
   pool.insert((void*)probe, unit);
   return probe;
}

template <typename T> 
T stats_pool::Get(const char * name) 
{
   pubitem item = { 0, NULL, NULL, NULL };
   if (pub.lookup(name, item) >= 0) {
      return (T)item.pitem;
      }
   return 0;
}


int stats_pool::Advance(int cAdvance)
{
   if (cAdvance <= 0)
      return cAdvance;

   void* pitem;
   int   units;
   pool.startIterations();
   while (pool.iterate(pitem,units))
      {
      if (units == stats_recent_counter_timer::unit) 
         {
         stats_recent_counter_timer * probe = (stats_recent_counter_timer *)pitem;
         probe->AdvanceBy(cAdvance);
         } 
      else 
         generic_stats_itemAdvanceRecent(units,pitem,cAdvance);
      }
   return cAdvance;
}

void stats_pool::Publish(ClassAd & ad) const
{
   pubitem item;
   MyString name;

   // boo! HashTable doesn't support const, so I have to remove const from this
   // to make the compiler happy.
   stats_pool * pthis = const_cast<stats_pool*>(this);
   pthis->pub.startIterations();
   while (pthis->pub.iterate(name,item)) 
      {
      if (item.pub)
         item.pub((const char *)item.pitem, ad, item.pattr ? item.pattr : name.Value());
      }
}

void stats_pool::Unpublish(ClassAd & ad) const
{
   pubitem item;
   MyString name;

   // boo! HashTable doesn't support const, so I have to remove const from this
   // to make the compiler happy.
   stats_pool * pthis = const_cast<stats_pool*>(this);
   pthis->pub.startIterations();
   while (pthis->pub.iterate(name,item)) 
      {
      ad.Delete(item.pattr ? item.pattr : name.Value());
      }
}

void DaemonCore::Stats::AddToNamed(const char * name, int val)
{
   stats_entry_recent<int>* pstat = Named.Get<stats_entry_recent<int>*>(name);
   if (pstat)
      pstat->Add(val);
}

void DaemonCore::Stats::AddToNamed(const char * name, int64_t val)
{
   stats_entry_recent<int64_t>* pstat = Named.Get<stats_entry_recent<int64_t>*>(name);
   if (pstat)
      pstat->Add(val);
}

double DaemonCore::Stats::AddRuntime(const char * name, double before)
{
   double now = UtcTime::getTimeDouble();
   stats_recent_counter_timer * probe = Named.Get<stats_recent_counter_timer*>(name);
   if (probe)
      probe->Add(now - before);
   return now;
}

void* DaemonCore::Stats::New(const char * category, const char * name, int as)
{
   MyString clean; // use this to construct a attribute-safe name from the input 
   clean += name;
   for (int ii = 0; ii < clean.Length(); ++ii) {
      char ch = clean[ii];
      if (ch == '_' || ch >= '0' && ch <= '9' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
         continue;
      clean.setChar(ii,' ');
      }
   clean.replaceString(" ","");
   clean.trim();

   MyString attr;
   attr.sprintf("RecentDC%s_%s", category, clean.Value());
   const char * pattrRecent = attr.Value();
   const char * pattr = pattrRecent + 6; // skip prefix "Recent"

   int cRecent = this->RecentWindowMax / dc_stats_window_quantum;
   void * ret = NULL;

   switch (as)
      {
      case AS_COUNT | IS_RINGBUF:
         {
         stats_entry_recent<int>* probe = new stats_entry_recent<int>(cRecent);
         //probe->SetRecentMax(cRecent);
         Named.Add(name, as, probe, probe->unit, pattr);
         Named.Add(pattrRecent, as, probe, probe->unit, pattrRecent, &probe->PublishRecent);
         //attr += "Debug";
         //Named.Add(attr.Value(), as, probe, probe->unit, attr.Value(), &probe->PublishDebug);
         ret = probe;
         }
         break;

      case AS_ABSTIME | IS_RINGBUF:
      case AS_RELTIME | IS_RINGBUF:
         {
         stats_entry_recent<time_t>* probe = new stats_entry_recent<time_t>(cRecent);
         //probe->SetRecentMax(cRecent);
         Named.Add(name, as, probe, probe->unit, pattr);
         Named.Add(pattrRecent, as, probe, probe->unit, pattrRecent, &probe->PublishRecent);
         //attr += "Debug";
         //Named.Add(attr.Value(), as, probe,  probe->unit, attr.Value(), &probe->PublishDebug);
         ret = probe;
         }
         break;

      case AS_COUNT | IS_RINGBUF | IS_PROBE:
      case AS_RELTIME | IS_RINGBUF | IS_PROBE:
         {
         stats_recent_counter_timer * probe = new stats_recent_counter_timer(cRecent);
         Named.Add(name, AS_COUNT | IS_RINGBUF | IS_PROBE, probe, probe->unit, pattr, &probe->PublishValue);
         }
         break;

      case -1:
         {
         // force references to the unit members, g++ is giving a linker error if I don't.
         stats_entry_recent<int>* foo = Named.Get<stats_entry_recent<int>*>(name);
         stats_entry_recent<time_t>* bar = Named.Get<stats_entry_recent<time_t>*>(name);
         ret = (void*)(foo->unit + bar->unit);
         }
         break;

      default:
         EXCEPT("invalid probe type\r");
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
