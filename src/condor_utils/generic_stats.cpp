 /***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include <map>
#include "timed_queue.h"
#include "generic_stats.h"
#include "classad_helpers.h" // for canStringForUseAsAttr

// use these to help initialize static const arrays arrays of 
//
#ifndef FIELDOFF
 #ifdef WIN32
  #define FIELDOFF(st,fld) FIELD_OFFSET(st, fld)
 #else
  //#define FIELDOFF(st,fld) ((int)(size_t)&(((st *)0)->fld))
  #define FIELDOFF(st,fld) offsetof(st,fld)
 #endif
 #define FIELDSIZ(st,fld) ((int)(sizeof(((st *)0)->fld)))
#endif
#define GS_FIELD(st,fld) (((st *)0)->fld)

// specialization of PublishDebug to dump the internal data storage
// of stats_entry_recent. 
//
template <class T>
void stats_entry_recent<T>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   MyString str;
      
   str += this->value;
   str += " ";
   str += this->recent;
   str.sprintf_cat(" {h:%d c:%d m:%d a:%d}", 
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         str += !ix ? "[" : (ix == this->buf.cMax ? "|" : ",");
         str += this->buf.pbuf[ix];
         }
      str += "]";
      }

   MyString attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

template <>
void stats_entry_recent<int64_t>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   MyString str;
   str += (long)this->value;
   str += " ";
   str += (long)this->recent;
   str.sprintf_cat(" {h:%d c:%d m:%d a:%d}", 
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         str += !ix ? "[" : (ix == this->buf.cMax ? "|" : ",");
         str += (long)this->buf.pbuf[ix];
         }
      str += "]";
      }

   MyString attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

template <>
void stats_entry_recent<double>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   MyString str;
   str.sprintf_cat("%g %g", this->value, this->recent);
   str.sprintf_cat(" {h:%d c:%d m:%d a:%d}", 
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         str.sprintf_cat(!ix ? "[%g" : (ix == this->buf.cMax ? "|%g" : ",%g"), this->buf.pbuf[ix]);
         }
      str += "]";
      }

   MyString attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

// Determine if enough time has passed to Advance the recent buffers,
// returns an advance count. 
// 
// note that the caller must insure that all 5 time_t values are preserved
// between calls to this function.  
//
int generic_stats_Tick(
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,
   time_t & RecentTickTime,
   time_t & Lifetime,
   time_t & RecentLifetime)
{
   time_t now = time(NULL);

   // when working from freshly initialized stats, the first Tick should not Advance.
   //
   if (LastUpdateTime == 0)
      {
      LastUpdateTime = now;
      RecentTickTime = now;
      RecentLifetime = 0;
      return 0;
      }

   // whenever 'now' changes, we want to check to see how much time has passed
   // since the last Advance, and if that time exceeds the quantum, we advance
   // the recent buffers and update the PrevUpdateTime
   //
   int cTicks = 0;
   if (LastUpdateTime != now) 
      {
      time_t delta = now - RecentTickTime;

      // if delta from the previous update time exceeds the quantum, advance
      // and update the prev update time.
      if (delta >= RecentQuantum)
         {
         cTicks = delta / RecentQuantum;
         RecentTickTime = now - (delta % RecentQuantum);
         }

      time_t recent_time = (int)(RecentLifetime + now - LastUpdateTime);
      RecentLifetime = (recent_time < RecentMaxTime) ? recent_time : RecentMaxTime;
      LastUpdateTime = now;
      }

   Lifetime = now - InitTime;
   return cTicks;
}

//----------------------------------------------------------------------------------------------
//
void stats_recent_counter_timer::Delete(stats_recent_counter_timer * probe) {
   delete probe;
}

void stats_recent_counter_timer::Publish(ClassAd & ad, const char * pattr, int flags) const
{
   if ((flags & IF_NONZERO) && (this->count.value == 0) && (this->count.recent == 0))
      return;

   MyString attr(pattr);
   MyString attrR("Recent");
   attrR += pattr;

   ClassAdAssign(ad, attr.Value(), this->count.value);
   ClassAdAssign(ad, attrR.Value(), this->count.recent);

   attr += "Runtime";
   attrR += "Runtime";
   ClassAdAssign(ad, attr.Value(), this->runtime.value);
   ClassAdAssign(ad, attrR.Value(), this->runtime.recent);

}

void stats_recent_counter_timer::PublishDebug(ClassAd & ad, const char * pattr, int flags) const
{
   if ( ! canStringBeUsedAsAttr(pattr))
      return;

   this->count.PublishDebug(ad, pattr, flags);

   MyString attr(pattr);
   attr += "Runtime";
   this->runtime.PublishDebug(ad, attr.Value(), flags);
}

// the Probe class is designed to be instantiated with
// the stats_entry_recent template,  i.e. stats_entry_recent<Probe>
// creates a probe that does full statistical sampling (min,max,avg,std)
// with an overall value and a recent-window'd value.
//
void Probe::Clear() 
{
   Count = 0; // value is use to store the count of samples.
   Max = std::numeric_limits<double>::min();
   Min = std::numeric_limits<double>::max();
   SumSq = Sum = 0.0;
}

double Probe::Add(double val) 
{ 
   Count += 1; // value is use to store the count of samples.
   if (val > Max) Max = val;
   if (val < Min) Min = val;
   Sum += val;
   SumSq += val*val;
   return Sum;
}

Probe & Probe::Add(const Probe & val) 
{ 
   if (val.Count >= 1) {
      Count += val.Count;
      if (val.Max > Max) Max = val.Max;
      if (val.Min < Min) Min = val.Min;
      Sum += val.Sum;
      SumSq += val.SumSq;
      }
   return *this;
}

double Probe::Avg() const 
{
   if (Count > 0) {
      return this->Sum / this->Count;
   } else {
      return this->Sum;
   }
}

double Probe::Var() const
{
   if (Count <= 1) {
      return this->Min;
   } else {
      // Var == (SumSQ - count*Avg*Avg)/(count -1)
      return (this->SumSq - this->Sum * (this->Sum / this->Count))/(this->Count - 1);
   }
}

double Probe::Std() const
{
   if (Count <= 1) {
      return this->Min;
   } else {
      return sqrt(this->Var());
   }
}

template <> void stats_entry_recent<Probe>::AdvanceBy(int cSlots) { 
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

template <> int ClassAdAssign(ClassAd & ad, const char * pattr, const Probe& probe) {
   MyString attr(pattr);
   ad.Assign(pattr, probe.Count);
   attr += "Runtime";
   int ret = ad.Assign(attr.Value(), probe.Sum);
   if (probe.Count > 0)
      {
      int pos = attr.Length();
      attr += "Avg";
      ad.Assign(attr.Value(), probe.Avg());

      attr.replaceString("Avg","Min",pos);
      ad.Assign(attr.Value(), probe.Min);

      attr.replaceString("Min","Max",pos);
      ad.Assign(attr.Value(), probe.Max);

      attr.replaceString("Max","Std",pos);
      ad.Assign(attr.Value(), probe.Std());
      }
   return ret;
}

#include "utc_time.h"
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

//----------------------------------------------------------------------------------------------
// methods for the StatisticsPool class.  StatisticsPool is a collection of statistics that 
// share a recent time quantum and are intended to be published/Advanced/Cleared
// together.
//
StatisticsPool::~StatisticsPool()
{
   // first delete all of the publish entries.
   MyString name;
   pubitem item;
   pub.startIterations();
   while (pub.iterate(name,item))
      {
      pub.remove(name);
      if (item.fOwnedByPool && item.pattr)
         free((void*)item.pattr);
      }

   // then all of the probes. 
   void* probe;
   poolitem pi;
   pool.startIterations();
   while (pool.iterate(probe,pi))
      {
      pool.remove(probe);
      if (pi.Delete)
         pi.Delete(probe);
      }
}

int StatisticsPool::RemoveProbe (const char * name)
{
   pubitem item;
   if (pub.lookup(name, item) < 0)
      return 0;
   int ret =  pub.remove(name);

   void * probe = item.pitem;
   bool fOwnedByPool = item.fOwnedByPool;
   if (fOwnedByPool) {
      if (item.pattr) free((void*)item.pattr);
   }

   // remove the probe from the pool (if it's still there)
   poolitem pi;
   if (pool.lookup(probe, pi) >= 0) {
      pool.remove(probe);
      if (pi.Delete) {
         pi.Delete(probe);
      }
   }

   // clear out any dangling references to the probe in the pub list.
   // 
   /*
   StatisticsPool * pthis = const_cast<StatisticsPool*>(this);
   MyString key;
   pthis->pub.startIterations();
   while (pthis->pub.iterate(key,item)) {
      if (item.pitem == probe)
         pthis->pub.remove(key);
   }

   if (fOwnedByPool) {
      delete pitem;
   }
   */
   return ret;
}

void StatisticsPool::InsertProbe (
   const char * name,       // unique name for the probe
   int          unit,       // identifies the probe class/type
   void*        probe,      // the probe, usually a member of a class/struct
   bool         fOwned,     // probe and pattr string are owned by the pool
   const char * pattr,      // publish attribute name
   int          flags,      // flags to control publishing
   FN_STATS_ENTRY_PUBLISH fnpub, // publish method
   FN_STATS_ENTRY_ADVANCE fnadv, // Advance method
   FN_STATS_ENTRY_CLEAR   fnclr,  // Clear method
   FN_STATS_ENTRY_SETRECENTMAX fnsrm,
   FN_STATS_ENTRY_DELETE  fndel) // Destructor
{
   pubitem item = { unit, flags, fOwned, probe, pattr, fnpub };
   pub.insert(name, item);

   poolitem pi = { unit, fOwned, fnadv, fnclr, fnsrm, fndel };
   pool.insert(probe, pi);
}

void StatisticsPool::InsertPublish (
   const char * name,       // unique name for the probe
   int          unit,       // identifies the probe class/type
   void*        probe,      // the probe, usually a member of a class/struct
   bool         fOwned,     // probe and pattr string are owned by the pool
   const char * pattr,      // publish attribute name
   int          flags,      // flags to control publishing
   FN_STATS_ENTRY_PUBLISH fnpub) // publish method
{
   pubitem item = { unit, flags, fOwned, probe, pattr, fnpub };
   pub.insert(name, item);
}

double StatisticsPool::SetSample(const char * probe_name, double sample)
{
   return sample;
}

int StatisticsPool::SetSample(const char * probe_name, int sample)
{
   return sample;
}

int64_t StatisticsPool::SetSample(const char * probe_name, int64_t sample)
{
   return sample;
}

int StatisticsPool::Advance(int cAdvance)
{
   if (cAdvance <= 0)
      return cAdvance;

   void* pitem;
   poolitem item;
   pool.startIterations();
   while (pool.iterate(pitem,item))
      {
      if (pitem && item.Advance) {
         stats_entry_base * probe = (stats_entry_base *)pitem;
         (probe->*(item.Advance))(cAdvance);
         }
      }
   return cAdvance;
}

void StatisticsPool::Clear()
{
   void* pitem;
   poolitem item;
   pool.startIterations();
   while (pool.iterate(pitem,item))
      {
      if (pitem && item.Clear) {
         stats_entry_base * probe = (stats_entry_base *)pitem;
         (probe->*(item.Clear))();
         }
      }
}

void StatisticsPool::ClearRecent()
{
   EXCEPT("StatisticsPool::ClearRecent has not been implemented");
}

void StatisticsPool::SetRecentMax(int window, int quantum)
{
   int cRecent = quantum ? window / quantum : window;

   void* pitem;
   poolitem item;
   pool.startIterations();
   while (pool.iterate(pitem,item))
      {
      if (pitem && item.SetRecentMax) {
         stats_entry_base * probe = (stats_entry_base *)pitem;
         (probe->*(item.SetRecentMax))(cRecent);
         }
      }
}

void StatisticsPool::Publish(ClassAd & ad) const
{
   pubitem item;
   MyString name;

   // boo! HashTable doesn't support const, so I have to remove const from this
   // to make the compiler happy.
   StatisticsPool * pthis = const_cast<StatisticsPool*>(this);
   pthis->pub.startIterations();
   while (pthis->pub.iterate(name,item)) 
      {
      if (item.Publish) {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Publish))(ad, item.pattr ? item.pattr : name.Value(), item.flags);
         }
      }
}

void StatisticsPool::Unpublish(ClassAd & ad) const
{
   pubitem item;
   MyString name;

   // boo! HashTable doesn't support const, so I have to remove const from this
   // to make the compiler happy.
   StatisticsPool * pthis = const_cast<StatisticsPool*>(this);
   pthis->pub.startIterations();
   while (pthis->pub.iterate(name,item)) 
      {
      ad.Delete(item.pattr ? item.pattr : name.Value());
      }
}

// some compiler/linkers need us to force template instantiation
// since the template code isn't in a header file.
// force the static members to be instantiated in the library.
//
void generic_stats_force_refs()
{
   stats_entry_recent<int>* pi = NULL;
   stats_entry_recent<long>* pl = NULL;
   stats_entry_recent<int64_t>* pt = NULL;
   stats_entry_recent<double>* pd = NULL;
   stats_recent_counter_timer* pc = NULL;

   StatisticsPool dummy;
   dummy.GetProbe<stats_entry_recent<int> >("");
   dummy.GetProbe<stats_entry_recent<int64_t> >("");
   dummy.GetProbe<stats_entry_recent<long> >("");
   dummy.GetProbe<stats_entry_recent<double> >("");
   dummy.GetProbe<stats_recent_counter_timer>("");

   dummy.NewProbe<stats_entry_recent<int> >("");
   dummy.NewProbe<stats_entry_recent<int64_t> >("");
   dummy.NewProbe<stats_entry_recent<long> >("");
   dummy.NewProbe<stats_entry_recent<double> >("");
   dummy.NewProbe<stats_recent_counter_timer>("");
   //dummy.Add<stats_entry_recent<int>*>("",0,pi,0,"",NULL);
   //dummy.Add<stats_entry_recent<long>*>("",0,pl,0,"",NULL);
   //dummy.Add<stats_entry_recent<int64_t>*>("",0,pt,0,"",NULL);
   //dummy.Add<stats_recent_counter_timer*>("",0,pc,0,"",NULL);
   dummy.AddProbe("",pi,NULL,0);
   dummy.AddProbe("",pl,NULL,0);
   dummy.AddProbe("",pt,NULL,0);
   dummy.AddProbe("",pc,NULL,0);
   dummy.AddProbe("",pd,NULL,0);
};

//
// This is how you use the generic_stats functions.
#ifdef UNIT_TESTS

class TestStats {
public:
   // InitTime should be the first data member, and RecentWindowMax the last data member..
   time_t InitTime;            // last time we init'ed the structure

   time_t StatsLastUpdateTime; // the prior time that statistics were updated. 
   int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
   time_t RecentStatsTickTime; // time of the latest recent buffer Advance

   // published values
   time_t StatsLifetime;       // last time we init'ed the structure
   time_t RecentStatsLifetime; // actual time span of current DCRecentXXX data.

   stats_entry_recent<int>    Signals; 
   stats_entry_recent<time_t> SignalRuntime;

   StatisticsPool Pool;           // pool of statistics probes and Publish attrib names

   void Init();
   void Clear();
   void Tick(); // call this when time may have changed to update StatsLastUpdateTime, etc.
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Unpublish(ClassAd & ad) const;
};


const int test_stats_window_quantum = 60;

void TestStats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   Pool.SetRecentMax(window, test_stats_window_quantum);
}

void TestStats::Init()
{
   Clear();
   this->RecentWindowMax = test_stats_window_quantum; 

   STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "Test", Signals, 0);
   STATS_POOL_ADD_VAL_PUB_RECENT(Pool, "Test", SignalRuntime, 0);
}

void TestStats::Clear()
{
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
}

void TestStats::Tick()
{
   int cAdvance = generic_stats_Tick(
      this->RecentWindowMax,     // RecentMaxTime
      test_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);
   if (cAdvance > 0)
      Pool.Advance(cAdvance);
}

void TestStats::Unpublish(ClassAd & ad) const
{
   Pool.Publish(ad);
}


void TestStats::Publish(ClassAd & ad) const
{
   Pool.Unpublish(ad);
}
#endif
