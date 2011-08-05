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

template <class T>
static ring_buffer<T>* GetRB(const GenericStatsEntry & entry, char * pdata)
{
   if ( ! entry.off2 || ! (entry.units & IS_RINGBUF))
      return NULL;
   return (ring_buffer<T>*)(pdata + entry.off2);
}

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
