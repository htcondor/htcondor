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

// advance the ring buffer and update the recent accumulator, this is called
// each xxxx_stats_window_quantum of seconds, in case an Advance is skipped, 
// cAdvance is the number of times to advance.
//
template <class T>
static stats_entry_abs<T>* GetAbs(const GenericStatsPubItem & entry, char * pstruct)
{
   if ((entry.units & IS_CLASS_MASK) != IS_CLS_ABS)
      return NULL;
   return (stats_entry_abs<T>*)(pstruct + entry.off);
}

template <class T>
static stats_entry_probe<T>* GetProbe(const GenericStatsPubItem & entry, char * pstruct)
{
   if ((entry.units & IS_CLASS_MASK) != IS_CLS_PROBE)
      return NULL;
   return (stats_entry_probe<T>*)(pstruct + entry.off);
}

template <class T>
static stats_entry_recent<T>* GetRecent(const GenericStatsPubItem & entry, char * pstruct)
{
   if ((entry.units & IS_CLASS_MASK) != IS_RECENT)
      return NULL;
   return (stats_entry_recent<T>*)(pstruct + entry.off);
}

#ifdef _timed_queue_h_
template <class T>
static stats_entry_tq<T>* GetTimedQ(const GenericStatsPubItem & entry, char * pstruct)
{
   if ((entry.units & IS_CLASS_MASK) != IS_RECENTTQ)
      return NULL;
   return (stats_entry_tq<T>*)(pstruct + entry.off);
}
#endif

// reset all counters to 0, including Recent buffers.
void generic_stats_itemFree(int units, void * pitem)
{
#undef DELETE_ITEM
#define DELETE_ITEM(st,p) delete ((st*)p);
   int cls = (units & IS_CLASS_MASK);
   if (IS_CLS_ABS == cls)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            DELETE_ITEM(stats_entry_abs<int>, pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            DELETE_ITEM(stats_entry_abs<int64_t>, pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            DELETE_ITEM(stats_entry_abs<float>, pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            DELETE_ITEM(stats_entry_abs<double>, pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else if (IS_CLS_PROBE == cls)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            DELETE_ITEM(stats_entry_probe<int>, pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            DELETE_ITEM(stats_entry_probe<int64_t>, pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            DELETE_ITEM(stats_entry_probe<float>, pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            DELETE_ITEM(stats_entry_probe<double>, pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else if (IS_RECENT == cls)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            DELETE_ITEM(stats_entry_recent<int>, pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            DELETE_ITEM(stats_entry_recent<int64_t>, pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            DELETE_ITEM(stats_entry_recent<float>, pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            DELETE_ITEM(stats_entry_recent<double>, pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#ifdef _timed_queue_h_
   else if (IS_RECENTTQ == cls)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_INT32:
            DELETE_ITEM(stats_entry_tq<int>, pitem);
            break;
         case STATS_ENTRY_TYPE_INT64:
            DELETE_ITEM(stats_entry_tq<int64_t>, pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#endif
#undef DELETE_ITEM
}

void generic_stats_itemSetRecentMax(int units, void * pitem, int window, int quantum)
{
int cRecent = quantum ? window / quantum : window;
#undef SETSIZE_ITEM
#define SETSIZE_ITEM(st,p) ((st*)p)->SetRecentMax(cRecent)
   int cls = (units & IS_CLASS_MASK);
   if (cls == IS_RECENT)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            SETSIZE_ITEM(stats_entry_recent<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            SETSIZE_ITEM(stats_entry_recent<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            SETSIZE_ITEM(stats_entry_recent<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            SETSIZE_ITEM(stats_entry_recent<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#ifdef _timed_queue_h_
#undef SETSIZE_ITEM
#define SETSIZE_ITEM(st,p) ((st*)p)->SetMaxTime(window)
   else if (cls == IS_RECENTTQ)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_INT32:
            SETSIZE_ITEM(stats_entry_tq<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_INT64:
            SETSIZE_ITEM(stats_entry_tq<int64_t>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#endif
#undef SETSIZE_ITEM 
}

void generic_stats_SetRecentMax(
   const GenericStatsPubItem * pTable, 
   int    cTable, 
   char * pstruct, 
   int    window, 
   int    quantum)
{
   //int cRecent = quantum ? window / quantum : window;

   for (int ii = 0; ii < cTable; ++ii)
      {
      generic_stats_itemSetRecentMax(pTable[ii].units, pstruct + pTable[ii].off, window, quantum);
      }
}

// publish items into a old-style ClassAd
void generic_stats_PublishToClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if (pTable[ii].units & IF_NEVER)
         continue;

      if (pTable[ii].pub)
         pTable[ii].pub(pdata + pTable[ii].off, ad, pTable[ii].pattr);
      }
}

void generic_stats_DeleteInClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if (pTable[ii].pattr)
         ad.Delete(pTable[ii].pattr);
      }
}

void generic_stats_itemClear(int units, void * pitem)
{
#undef CLEAR_ITEM
#define CLEAR_ITEM(st,p) ((st*)p)->Clear()
   int cls = (units & IS_CLASS_MASK);
   if (cls == IS_CLS_ABS)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32: 
            CLEAR_ITEM(stats_entry_abs<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_abs<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_abs<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_abs<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else if (cls == IS_CLS_PROBE)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_probe<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_probe<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_probe<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_probe<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else if (cls == IS_RECENT)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_recent<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_recent<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_recent<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_recent<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#ifdef _timed_queue_h_
   else if (cls == IS_RECENTTQ)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_tq<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_tq<int64_t>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#endif
#undef CLEAR_ITEM
}

// reset all counters to 0, including Recent buffers.
void generic_stats_Clear(const GenericStatsPubItem * pTable, int cTable, char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      generic_stats_itemClear(pTable[ii].units, pdata + pTable[ii].off);
      }
}

void generic_stats_itemClearRecent(int units, char * pitem)
{
#undef CLEAR_ITEM
#define CLEAR_ITEM(st,p) ((st*)p)->ClearRecent()
   int cls = (units & IS_CLASS_MASK);
   /*
   if (cls == IS_CLS_ABS)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32: 
            CLEAR_ITEM(stats_entry_abs<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_abs<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_abs<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_abs<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else if (cls == IS_CLS_PROBE)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_probe<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_probe<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_probe<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_probe<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
   else 
   */
   if (cls == IS_RECENT)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_recent<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_recent<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            CLEAR_ITEM(stats_entry_recent<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            CLEAR_ITEM(stats_entry_recent<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#ifdef _timed_queue_h_
   else if (cls == IS_RECENTTQ)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_INT32:
            CLEAR_ITEM(stats_entry_tq<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_INT64:
            CLEAR_ITEM(stats_entry_tq<int64_t>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#endif
#undef CLEAR_ITEM
}
// clear the recent buffers
void generic_stats_ClearRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      generic_stats_itemClearRecent(pTable[ii].units, pdata + pTable[ii].off);
      }
}

// each time the time quantum has passed, we wan to Advance the recent buffers
void generic_stats_itemAdvanceRecent(int units, void * pitem, int cAdvance)
{
#undef ADVANCE_ITEM
#define ADVANCE_ITEM(st,p) ((st*)p)->AdvanceBy(cAdvance)
   int cls = (units & IS_CLASS_MASK);
   if (cls == IS_RECENT)
      {
      switch (units & AS_FUNDAMENTAL_TYPE_MASK)
         {
         case STATS_ENTRY_TYPE_UINT32: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT32:
            ADVANCE_ITEM(stats_entry_recent<int>,pitem);
            break;
         case STATS_ENTRY_TYPE_UINT64: // we can overload signed and unsigned because they are the same size.
         case STATS_ENTRY_TYPE_INT64:
            ADVANCE_ITEM(stats_entry_recent<int64_t>,pitem);
            break;
         case STATS_ENTRY_TYPE_FLOAT:
            ADVANCE_ITEM(stats_entry_recent<float>,pitem);
            break;
         case STATS_ENTRY_TYPE_DOUBLE:
            ADVANCE_ITEM(stats_entry_recent<double>,pitem);
            break;
         default: { EXCEPT("invalid value in statistics pub table\n"); }
         }
      }
#undef ADVANCE_ITEM
}

void generic_stats_AdvanceRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata, int cAdvance)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      generic_stats_itemAdvanceRecent(pTable[ii].units, pdata + pTable[ii].off, cAdvance);
      }
}


int generic_stats_Tick(
   const GenericStatsPubItem * pPub, 
   int    cPub, 
   char * pdata,
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
         if (pPub && cPub)
            generic_stats_AdvanceRecent(pPub, cPub, pdata, delta / RecentQuantum);
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

void stats_recent_counter_timer::PublishValue(const char * me, ClassAd & ad, const char * pattr)
{
   const stats_recent_counter_timer * pthis = (const stats_recent_counter_timer *)me;
   pthis->Publish(ad, pattr, 0);
}


//----------------------------------------------------------------------------------------------
// methods for the stats_pool class.  stats_pool is a collection of statistics that 
// share a recent time quantum and are intended to be published/Advanced/Cleared
// together.
//
int stats_pool::RemoveProbe (const char * name)
{
   pubitem item = { 0 };
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
   stats_pool * pthis = const_cast<stats_pool*>(this);
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

void stats_pool::InsertProbe (
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

void stats_pool::InsertPublish (
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

double stats_pool::SetSample(const char * probe_name, double sample)
{
   return sample;
}

int stats_pool::SetSample(const char * probe_name, int sample)
{
   return sample;
}

int64_t stats_pool::SetSample(const char * probe_name, int64_t sample)
{
   return sample;
}

int stats_pool::Advance(int cAdvance)
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

void stats_pool::Clear()
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

void stats_pool::ClearRecent()
{
}

void stats_pool::SetRecentMax(int window, int quantum)
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
      if (item.Publish) {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Publish))(ad, item.pattr ? item.pattr : name.Value(), item.flags);
         }
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

// some compiler/linkers need us to force template instantiation
// since the template code isn't in a header file.
// force the static members to be instantiated in the library.
//
void generic_stats_force_refs()
{
   ClassAd * pad = NULL;
   stats_entry_count<time_t>::PublishValue("",*pad,"");
   stats_entry_abs<time_t>::PublishLargest("",*pad,"");
   stats_entry_recent<time_t>::PublishRecent("",*pad,"");

   stats_entry_recent<int>* pi = NULL;
   stats_entry_recent<long>* pl = NULL;
   stats_entry_recent<int64_t>* pt = NULL;
   stats_entry_recent<double>* pd = NULL;
   stats_recent_counter_timer* pc = NULL;

   stats_pool dummy;
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

   void Init();
   void Clear();
   void Tick(); // call this when time may have changed to update StatsLastUpdateTime, etc.
   void SetWindowSize(int window);
   void Publish(ClassAd & ad) const;
   void Unpublish(ClassAd & ad) const;
};


#define TEST_STATS_PUB(name, as)        GENERIC_STATS_PUB(TestStats, "Test", name, as)
#define TEST_STATS_PUB_RECENT(name, as) GENERIC_STATS_PUB_RECENT(TestStats, "Test", name, as)
#define TEST_STATS_PUB_PEAK(name, as)   GENERIC_STATS_PUB_PEAK(TestStats, "Test", name, as)
#define TEST_STATS_PUB_TYPE(name,T,as)  GENERIC_STATS_PUB_TYPE(TestStats, "Test", name, as, T)

static const GenericStatsPubItem TestStatsPub[] = {
   TEST_STATS_PUB_TYPE(StatsLifetime,       time_t,  AS_RELTIME),
   TEST_STATS_PUB_TYPE(RecentStatsLifetime, time_t,  AS_RELTIME),

   TEST_STATS_PUB_TYPE(InitTime,            time_t,  AS_ABSTIME),

   TEST_STATS_PUB(Signals,               AS_COUNT),
   TEST_STATS_PUB(SignalRuntime,         AS_RELTIME),

   TEST_STATS_PUB_RECENT(Signals,        AS_COUNT),
   TEST_STATS_PUB_RECENT(SignalRuntime,  AS_RELTIME),
   };

const int test_stats_window_quantum = 60;

void TestStats::SetWindowSize(int window)
{
   this->RecentWindowMax = window;
   generic_stats_SetRecentMax(TestStatsPub, COUNTOF(TestStatsPub), (char*)this, 
                              window, test_stats_window_quantum);
}

void TestStats::Init()
{
   Clear();
   this->RecentWindowMax = test_stats_window_quantum; 
}

void TestStats::Clear()
{
   generic_stats_Clear(TestStatsPub, COUNTOF(TestStatsPub), (char*)this);
   this->InitTime = time(NULL);
   this->StatsLifetime = 0;
   this->StatsLastUpdateTime = 0;
   this->RecentStatsTickTime = 0;
   this->RecentStatsLifetime = 0;
}

void TestStats::Tick()
{
   generic_stats_Tick(
      TestStatsPub, COUNTOF(TestStatsPub), 
      (char*)this,
      this->RecentWindowMax,     // RecentMaxTime
      test_stats_window_quantum, // RecentQuantum
      this->InitTime,
      this->StatsLastUpdateTime,
      this->RecentStatsTickTime,
      this->StatsLifetime,
      this->RecentStatsLifetime);

}

void TestStats::Unpublish(ClassAd & ad) const
{
   generic_stats_DeleteInClassAd(ad, TestStatsPub, COUNTOF(TestStatsPub), (const char *)this);  
}


void TestStats::Publish(ClassAd & ad) const
{
   generic_stats_PublishToClassAd(ad, TestStatsPub, COUNTOF(TestStatsPub), (const char *)this);  
/*
   const GenericStatsPubItem * pTable = PubTable;
   int                         cTable = COUNTOF(PubTable);

   for (int ii = 0; ii < cTable; ++ii)
      {
      if (pTable[ii].pub)
         pTable[ii].pub((const char *)this + pTable[ii].off, ad, pTable[ii].pattr);
      }
*/
}
#endif
