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
/*
template <>
void stats_entry_count<time_t>::Publish(ClassAd & ad, const char * pattr) const {
   ad.Assign(pattr, (int)value);
}

template <>
void stats_entry_count<time_t>::PublishValue(const char * me, ClassAd & ad, const char * pattr) {
   const stats_entry_count<time_t> * pthis = (const stats_entry_count<time_t> *)me;
   ad.Assign(pattr, (int)pthis->value);
   }
*/

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

// force the static members to be instantiated in the library.
//
void generic_stats_force_refs()
{
   ClassAd * pad = NULL;
   stats_entry_count<time_t>::PublishValue("",*pad,"");
   stats_entry_abs<time_t>::PublishLargest("",*pad,"");
   stats_entry_recent<time_t>::PublishRecent("",*pad,"");
};

/*
template <> 
void stats_entry_recent<time_t>::Publish(ClassAd &ad, const char *pattr) const
{
   ad.Assign(pattr, (int)value);
}

template <> 
void stats_entry_abs<time_t>::Publish(ClassAd &ad, const char *pattr) const
{
   ad.Assign(pattr, (int)value);
}

template <>
void stats_entry_tq<time_t>::Publish(ClassAd & ad, const char * pattr) const
{
   ad.Assign(pattr, (int)value);
}
*/


// advance the ring buffer and update the recent accumulator, this is called
// each xxxx_stats_window_quantum of seconds, in case an Advance is skipped, 
// cAdvance is the number of times to advance.
//
template <class T>
static T AdvanceRecent(const GenericStatsEntry & entry, char * pdata, int cAdvance)
{
   if ( ! entry.off2)
      return T(0);

   T& recent = *(T*)(pdata + entry.off);
   T ret(recent);
   if (entry.units & IS_TIMED_QUEUE) {
      // nothing to do.
   } else if (entry.units & IS_RINGBUF) {
      ring_buffer<T>& rb = *(ring_buffer<T>*)(pdata + entry.off2);
      if ( ! rb.empty()) {
         for (int ii = 0; ii < cAdvance; ++ii)
            ret -= rb.Push(0);
      }
   }
   recent = ret;
   return ret;
}

void generic_stats_PublishToClassAd(ClassAd & ad, const GenericStatsEntry * pTable, int cTable, const char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if (pTable[ii].units & IF_NEVER)
         continue;

      switch (pTable[ii].units & AS_TYPE_MASK)
         {
         case AS_COUNT: {
            int val = ((const int*)(pdata + pTable[ii].off))[0];
            if ((pTable[ii].units & IF_NONZERO) && ! val)
               ad.Delete(pTable[ii].pattr);
            else
               ad.Assign(pTable[ii].pattr, val);
            break;
            }

         case AS_RELTIME:
         case AS_ABSTIME: {
            // for old class ads, we publish time_t as an int because we can't use the
            // TIME types (new classads only) and we don't have access to int64 types.
            int val = ((const int*)(pdata + pTable[ii].off))[0];
            if ((pTable[ii].units & IF_NONZERO) && ! val)
               ad.Delete(pTable[ii].pattr);
            else
               ad.Assign(pTable[ii].pattr, val);
            break;
            }
         }
      }
}

void generic_stats_DeleteInClassAd(ClassAd & ad, const GenericStatsEntry * pTable, int cTable, const char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      switch (pTable[ii].units & AS_TYPE_MASK)
         {
         case AS_COUNT:
            ad.Delete(pTable[ii].pattr);
            break;

         case AS_RELTIME:
         case AS_ABSTIME:
            ad.Delete(pTable[ii].pattr);
            break;
         }
      }
}

// Accumulate (sum) the Recent data buffer and use that to set the .recent field
// for a generic statistics entry.  Note that only timed_queues need to be
// Accumulated, ring_buffers keep track of accumulated values automatically.
//
template <class T>
static T Accumulate(const GenericStatsEntry & entry, char * pdata, time_t first)
{
   if ( ! entry.off2)
      return T(-1);

   T ret(0);
   T& recent = *(T*)(pdata + entry.off);

   if (entry.units & IS_TIMED_QUEUE) {
      timed_queue<T>& tq = *(timed_queue<T>*)(pdata + entry.off2);

      tq.trim_time(first);

      for (typename timed_queue<T>::iterator jj(tq.begin());  jj != tq.end();  ++jj) {
         ret += jj->second;
      }
   } else if (entry.units & IS_RINGBUF) {
      // we don't really need to Accumulate for ring buffers.
      ret = recent;
   }

   recent = ret;
   return ret;   

}

// Accumulate all of the timed_queue buffers and update their corresponding .recent fields
// for the given set of statistics data.
//
void generic_stats_AccumulateTQ(const GenericStatsEntry * pTable, int cTable, char * pdata, time_t tmin)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if ( ! pTable[ii].off2)
         continue;

      switch (pTable[ii].siz)
         {
         case sizeof(int): {
            Accumulate<int>(pTable[ii], pdata, tmin);
            break;
            }
         case sizeof(int64_t): {
            Accumulate<int64_t>(pTable[ii], pdata, tmin);
            break;
            }
         }
      }
}

// get the timed_queue member of a generic statistics entry. 
// returns NULL if the entry does not have a timed_queue
//
template <class T>
static timed_queue<T>* GetTQ(const GenericStatsEntry & entry, char * pdata)
{
   if ( ! entry.off2 || ! (entry.units & IS_TIMED_QUEUE))
      return NULL;
   return (timed_queue<T>*)(pdata + entry.off2);
}

void generic_stats_SetTQMax(const GenericStatsEntry * pTable, int cTable, char * pdata, int window)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if ( ! pTable[ii].off2)
         continue;

      if (pTable[ii].units & IS_TIMED_QUEUE)
         {
         switch (pTable[ii].siz)
            {
            case sizeof(int): {
               timed_queue<int>* pTQ = GetTQ<int>(pTable[ii], pdata);
               pTQ->max_time(window);
               break;
               }
            case sizeof(int64_t): {
               timed_queue<int64_t>* pTQ = GetTQ<int64_t>(pTable[ii], pdata);
               pTQ->max_time(window);
               break;
               }
            }
         }
      }
}

// get the ring_buffer member of a generic statistics entry. 
// returns NULL if the entry does not have a ring_buffer
//
template <class T>
static ring_buffer<T>* GetRB(const GenericStatsEntry & entry, char * pdata)
{
   if ( ! entry.off2 || ! (entry.units & IS_RINGBUF))
      return NULL;
   return (ring_buffer<T>*)(pdata + entry.off2);
}

// set the size of the ring_buffers used to store Recent data for statistics entries.
//
void generic_stats_SetRBMax(const GenericStatsEntry * pTable, int cTable, char * pdata, int cMax)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if ( ! pTable[ii].off2)
         continue;
      if (pTable[ii].units & IS_RINGBUF)
         {
         switch (pTable[ii].siz)
            {
            case sizeof(int): {
               ring_buffer<int>* prb = GetRB<int>(pTable[ii], pdata);
               prb->SetSize(cMax);
               break;
               }
            case sizeof(int64_t): {
               ring_buffer<int64_t>* prb = GetRB<int64_t>(pTable[ii], pdata);
               prb->SetSize(cMax);
               break;
               }
            }
         }
      }
}

// clear the Recent data for a statistics entry. works with timed_queue
// and ring_buffer style Recent data. this does not free the ring_buffer
//
template <class T>
static void ClearRecent(const GenericStatsEntry & entry, char * pdata)
{
   if ( ! entry.off2)
      return;

   T& recent = *(T*)(pdata + entry.off);
   if (entry.units & IS_TIMED_QUEUE) {
      timed_queue<T>& tq = *(timed_queue<T>*)(pdata + entry.off2);
      tq.clear();
   } else if (entry.units & IS_RINGBUF) {
      ring_buffer<T>& rb = *(ring_buffer<T>*)(pdata + entry.off2);
      rb.Clear();
   }
   recent = 0;
}

// clear all Recent data for a set of statistics. works with timed_queue
// and ring_buffer style Recent data.  this does not free the ring_buffer
//
void generic_stats_ClearAll(const GenericStatsEntry * pTable, int cTable, char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      switch (pTable[ii].units & AS_TYPE_MASK)
         {
         case AS_COUNT: {
            ((int*)(pdata + pTable[ii].off))[0] = 0;
            if (pTable[ii].off2) ClearRecent<int>(pTable[ii], pdata);
            break;
            }

         case AS_RELTIME:
         case AS_ABSTIME: {
            ((time_t*)(pdata + pTable[ii].off))[0] = 0;
            if (pTable[ii].off2) ClearRecent<time_t>(pTable[ii], pdata);
            break;
            }
         }
      }
}

// throw away all Recent (ring_buffer) data.  Note that this does not free or resize
// the ring buffers.
//
void generic_stats_ClearRecent(const GenericStatsEntry * pTable, int cTable, char * pdata)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if ( ! pTable[ii].off2)
         continue;

      switch (pTable[ii].siz)
         {
         case sizeof(int): {
            ClearRecent<int>(pTable[ii], pdata);
            break;
            }
         case sizeof(int64_t): {
            ClearRecent<int64_t>(pTable[ii], pdata);
            break;
            }
         }
      }
}

// advance the ring buffers to the next slot.  we do this when time is larger than the quantum
//
void generic_stats_AdvanceRecent(const GenericStatsEntry * pTable, int cTable, char * pdata, int cAdvance)
{
   for (int ii = 0; ii < cTable; ++ii)
      {
      if ( ! pTable[ii].off2)
         continue;

      if (pTable[ii].units & IS_RINGBUF)
         {
         switch (pTable[ii].siz)
            {
            case sizeof(int): {
               AdvanceRecent<int>(pTable[ii], pdata, cAdvance);
               break;
               }
            case sizeof(int64_t): {
               AdvanceRecent<int64_t>(pTable[ii], pdata, cAdvance);
               break;
               }
            }
         }
      }
}

template <class T>
static stats_entry_abs<T>* GetAbs(const GenericStatsPubItem & entry, char * pstruct)
{
   if (!(entry.units & IS_ABS))
      return NULL;
   return (stats_entry_abs<T>*)(pstruct + entry.off);
}

template <class T>
static stats_entry_probe<T>* GetProbe(const GenericStatsPubItem & entry, char * pstruct)
{
   if (!(entry.units & IS_PROBE))
      return NULL;
   return (stats_entry_probe<T>*)(pstruct + entry.off);
}

template <class T>
static stats_entry_recent<T>* GetRecent(const GenericStatsPubItem & entry, char * pstruct)
{
   if (!(entry.units & IS_RINGBUF))
      return NULL;
   return (stats_entry_recent<T>*)(pstruct + entry.off);
}

#ifdef _timed_queue_h_
template <class T>
static stats_entry_tq<T>* GetTimedQ(const GenericStatsPubItem & entry, char * pstruct)
{
   if (!(entry.units & IS_TIMED_QUEUE))
      return NULL;
   return (stats_entry_tq<T>*)(pstruct + entry.off);
}
#endif

// reset all counters to 0, including Recent buffers.
void generic_stats_itemFree(int units, void * pitem)
{
#undef DELETE_ITEM
#define DELETE_ITEM(st,p) delete ((st*)p);
   if (units & IS_ABS)
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
   else if (units & IS_PROBE)
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
   else if (units & IS_RINGBUF)
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
   else if (units & IS_TIMED_QUEUE)
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
   if (units & IS_RINGBUF)
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
   else if (units & IS_TIMED_QUEUE)
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
   int cRecent = quantum ? window / quantum : window;

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
   if (units & IS_ABS)
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
   else if (units & IS_PROBE)
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
   else if (units & IS_RINGBUF)
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
   else if (units & IS_TIMED_QUEUE)
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
   /*
   if (units & IS_ABS)
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
   else if (units & IS_PROBE)
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
   if (units & IS_RINGBUF)
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
   else if (units & IS_TIMED_QUEUE)
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
   if (units & IS_RINGBUF)
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