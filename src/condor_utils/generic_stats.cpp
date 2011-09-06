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

// specialization of time_t values because ClassAd
// doesn't currently handle 64 bit ints.
//
#if 0
template <>
void stats_entry_abs<time_t>::PublishLargest(const char * me, ClassAd & ad, const char * pattr) {
   const stats_entry_abs<time_t> * pthis = (const stats_entry_abs<time_t> *)me;
   ad.Assign(pattr, (int)pthis->largest);
   }

template <>
void stats_entry_recent<time_t>::PublishRecent(const char * me, ClassAd & ad, const char * pattr) {
   const stats_entry_recent<time_t> * pthis = (const stats_entry_recent<time_t> *)me;
   ad.Assign(pattr, (int)pthis->recent);
}

template <>
void stats_entry_count<time_t>::PublishValue(const char * me, ClassAd & ad, const char * pattr) {
   const stats_entry_count<time_t> * pthis = (const stats_entry_count<time_t> *)me;
   ad.Assign(pattr, (int)pthis->value);
}
#endif

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

   MyString str(pattr);
   MyString strR("Recent");
   strR += pattr;

   ClassAdAssign(ad, str.Value(), this->count.value);
   ClassAdAssign(ad, strR.Value(), this->count.recent);

   str += "Runtime";
   strR += "Runtime";
   ClassAdAssign(ad, str.Value(), this->runtime.value);
   ClassAdAssign(ad, strR.Value(), this->runtime.recent);

   //str.sprintf("Debug%s", pattr);
   //StatsPublishDebug(str.Value(), ad, this->count);
   //str += "Runtime";
   //StatsPublishDebug(str.Value(), ad, this->runtime);
}

#if 0
void stats_recent_counter_timer::PublishValue(const char * me, ClassAd & ad, const char * pattr)
{
   const stats_recent_counter_timer * pthis = (const stats_recent_counter_timer *)me;
   pthis->Publish(ad, pattr, 0);
}
#endif

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
  #if 0
   ClassAd * pad = NULL;
   stats_entry_count<time_t>::PublishValue("",*pad,"");
   stats_entry_abs<time_t>::PublishLargest("",*pad,"");
   stats_entry_recent<time_t>::PublishRecent("",*pad,"");
  #endif

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
// This is how you use the generic_stats_xxxx functions.
#if 0

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
