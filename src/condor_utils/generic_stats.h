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

#ifndef _GENERIC_STATS_H
#define _GENERIC_STATS_H

// To use generic statistics:
//   * create a structure or class to hold your counters, 
//     * use stats_entry_abs    for counters that are absolute values (i.e. number of jobs running)
//     * use stats_entry_recent for counters that always increase (i.e. number of jobs that have finished)
//   * use Add() or Set() methods of the counters to update the counters in your code
//       these methods will automatically keep track of total value, as well as windowed
//       values and/or peak values. 
//   * create a const array of GenericStatsEntry structures, one for each Attribute
//   * use generic_stats_PublishToClassAd passing the GenericStatsEntry and also your counters class 
//     if you use the GENERIC_STATS_ENTRY macros to initialize the GenericStatsEntry structures,
//     counters will be published as ClassAd attributes using their field names.
//
// For example:
//
// typedef struct MyStats {
//     time_t                  UpdateTime                     
//     stats_entry_abs<int>    JobsRunning;  // keep track of Peak value as well as absolete value
//     stats_entry_recent<int> JobsRun;      // keep track of Recent values as well as accumulated value
//
//     void Publish(ClassAd & ad) const;
// } MyStats;
// 
// static const GenericStatsEntry MyStatsTable[] = {
//   GENERIC_STATS_ENTRY(MyStats, UpdateTime,     AS_ABSTIME), // publish "UpdateTime"
//   GENERIC_STATS_ENTRY(MyStats, JobsRunning,      AS_COUNT), // publish "JobsRunning"
//   GENERIC_STATS_ENTRY_PEAK(MyStats, JobsRunning, AS_COUNT), // publish "JobsRunningPeak" from JobsRunning
//   GENERIC_STATS_ENTRY(MyStats, JobsRun,          AS_COUNT), // publish "JobsRun"
//   GENERIC_STATS_ENTRY_RECENT(MyStats, JobsRun,   AS_COUNT), // publish "RecentJobsRun" from JobsRun
//   };
//
// void MyStats::Publish(ClassAd & ad) const
// {
//    generic_stats_PublishToClassAd(ad, MyStatsTable, COUNTOF(MyStatsTable), (const char *)this);
// }
//
// include an instance of MyStats in the class to be measured
// 
//  class MyClass {
//      ...
//      MyStats stats;
//      ...
//      void MyMethod() {
//          stats.UpdateTime = time(NULL);
//
//           // this to update the absolute jobs running counter
//          stats.JobsRunning.Set(jobs_running);
//           // or this
//          stats.JobsRunning = jobs_running; 
//
//           // this to increment the jobs that have run counter
//          stats.JobsRun.Add(1);
//           // or this 
//          stats.JobsRun += 1;
//
//      }
//
//      void PublishMyStats(ClassAd & ad) const {
//          stats.Publish(ad);
//      }
//  };
//    
//

// this structure is used to describe one field of a statistics structure
// so that we can generically update and publish
//
typedef struct _generic_stats_entry {
   char * pattr;  // name to be used when publishing the value
   int    units;  // field type, AS_COUNT, AS_ABSTIME, etc. plus IS_xx, IF_xx flags
   int    off;    // offset to statistics data value from start of data set
   int    siz;    // size of statistics data value
   int    off2;  // if non-zero, indicates that this value is baked down from a timed_queue.
   } GenericStatsEntry;

typedef void (*fn_generic_stats_publish)(const char * me, ClassAd & ad, const char * pattr);
typedef struct _generic_stats_pubitem {
   char * pattr;  // name to be used when publishing the value
   int    units;  // one or more of AS_COUNT, etc and IS_RINGBUF, etc
   int    off;    // offset of the stats_entry_xxx field within the containing class/struct
   fn_generic_stats_publish pub; // static method for publishing
   } GenericStatsPubItem;

// this is used to identify the fundamental type of a statistics entry so
// that we can use generic data driven code to publish and clear collections
// of statistics. It works with the template specialization below to let us
// capture the type T inside a template expansion as an integer value. 
//
enum {
   STATS_ENTRY_TYPE_INT32 = 1,
   STATS_ENTRY_TYPE_INT64 = 2,
   STATS_ENTRY_TYPE_FLOAT = 1 | 4,
   STATS_ENTRY_TYPE_DOUBLE = 2 | 4,
   STATS_ENTRY_TYPE_UNSIGNED = 8,
   STATS_ENTRY_TYPE_UINT32 = STATS_ENTRY_TYPE_INT32 | STATS_ENTRY_TYPE_UNSIGNED,
   STATS_ENTRY_TYPE_UINT64 = STATS_ENTRY_TYPE_INT64 | STATS_ENTRY_TYPE_UNSIGNED,
   };

template <class T> struct stats_entry_type  {static const int id = 0;};
template<> struct stats_entry_type<int>     {static const int id = STATS_ENTRY_TYPE_INT32; };
template<> struct stats_entry_type<int64_t> {static const int id = STATS_ENTRY_TYPE_INT64; };
template<> struct stats_entry_type<unsigned int> {static const int id = STATS_ENTRY_TYPE_UINT32; };
template<> struct stats_entry_type<uint64_t>{static const int id = STATS_ENTRY_TYPE_UINT64; };
template<> struct stats_entry_type<float>   {static const int id = STATS_ENTRY_TYPE_FLOAT; };
template<> struct stats_entry_type<double>  {static const int id = STATS_ENTRY_TYPE_DOUBLE; };

// These enums are for the units field of GenericStatEntry
enum {
   AS_COUNT   = 0,      // an int or int64 count of something
   AS_ABSTIME = 0x10,   // a time_t absoute timestamp (i.e. from time(NULL))
   AS_RELTIME = 0x20,   // a time_t time duration

   // the fundamental type is determined by the compiler and mapped to 
   // the stats_entry_type<T>::id constants, which fit in the low 4 bits
   AS_FUNDAMENTAL_TYPE_MASK = 0x000F, // mask to get STATS_ENTRY_TYPE_xxx
   AS_TYPE_MASK   = 0x00FF, // mask the units field with this to get AS_xxx 

   // bits between 0x8000 and 0x0100 identify the probe class
   IS_CLASS_MASK  = 0xFF00, // 
   IS_ABS         = 0x0100, // value has largest
   IS_PROBE       = 0x0200, // value has min/max/avg/stddev
   IS_HISTOGRAM   = 0x0400, // value is a histogram
   IS_RINGBUF     = 0x1000, // has recent value derived from a ring_buffer
   IS_TIMED_QUEUE = 0x2000, // has recent value derived from a timed_queue

   // values above AS_TYPE_MASK are flags
   //
   IF_NONZERO    = 0x10000, // only publish non-zero values.
   IF_NEVER      = 0x20000, // set this flag to disable publishing of the item.
   IF_PUBMASK    = IF_NEVER | IF_NONZERO, // flags that affect publication
   };


// Generic class for a ring buffer.  
// 
// A ring buffer does not grow except via the SetSize() method.
// Push() advances the location of the head through the buffer until 
// it gets to the last element at which point it wraps around to the first element.  
// 
// Once the size of the buffer is set, buffer entries can be accessed with the [] operator
// where [0] accesses the current entry, and [1-cItems] accesses the last (oldest) entry. 
// once the buffer is full, [1] also access the oldest entry because of the way the ring 
// buffer wraps around.
//
// the Add() method is used to add to the head element, it does not advance through the buffer.
//
// So after SetSize(6) and 6 or more Push() calls, we have 
//
//   pbuf:[aaa][bbb][ccc][ddd][eee][fff]
//              ^    ^
//              head tail
//   this[0] returns bbb
//
// After Add(2) we have
//
//   pbuf:[aaa][bbb+2][ccc][ddd][eee][fff]
//              ^      ^
//              head   tail
//   this[0] returns bbb+2
// 
// After Advance() or Push(0) we have
//
//   pbuf:[aaa][bbb][0  ][ddd][eee][fff]
//                   ^    ^
//                   head tail
//   this[0] returns 0
// 
//
template <class T> class ring_buffer {
public:
   ring_buffer(int cSize=0) : cMax(0), cAlloc(0), ixHead(0), cItems(0), pbuf(0) {
      if (cSize > 0) {
         pbuf = new T[cSize];
         cMax = cAlloc = cSize;
         }
      };
   ~ring_buffer() { delete[] pbuf; };
   int cMax;   // the maximum number of items in the ring, may be less than cAlloc but never more
   int cAlloc; // the allocation size of pbuf, should be 0 if pbuf is NULL
   int ixHead; // index of the head item (item[0]) within the ring buffer, advances with each Push
   int cItems; // number of items currently in the ring buffer, grows with each Push() until it hits cMax
   T*  pbuf;   // allocated buffer for the ring.

   T& operator[](int ix) { 
      if ( ! pbuf || ! cMax) return pbuf[0]; // yes, we do want to segfault if pbuf==NULL
      return pbuf[(ixHead+ix+cMax) % cMax];
   }

   int Length() const { return cItems; } 
   int MaxSize() const { return cMax; }
   bool empty() const { return cItems == 0; }

   bool Clear() {
      bool ret = cItems > 0;
      ixHead = 0;
      cItems = 0;
      return ret;
   }

   void Free() {
      ixHead = 0;
      cItems = 0;
      cMax = 0;
      cAlloc = 0;
      delete[] pbuf;
   }

   T Sum() {
      T tot = 0;
      for (int ix = 0; ix > (0 - cItems); --ix)
         tot += (*this)[ix];
      return tot;
   }

   bool SetSize(int cSize) {

      if (cSize < 0) return false;

      // if current items are outside of the new ring buffer from [0 to cSize]
      // then we have to copy items, so we might as well allocate a new buffer
      // even if we are shrinking.
      bool fMustCopy = (cItems > 0) && (ixHead > cSize || ixHead - cItems + 1 < 0);

      if ((cSize > cAlloc) || fMustCopy) {
         const int cAlign = 16;
         int cNew = !cAlloc ? cSize : cSize + (cAlign-1) - ((cSize) % cAlign);
         T* p = new T[cNew];
         if ( ! p) return false;

         // if there is an existing buffer copy items from it to the new buffer
         int cCopy = 0;
         if (pbuf) {
            cCopy = cItems;
            for (int ix = 0; ix > 0 - cCopy; --ix)
               p[(ix+cCopy)%cSize] = (*this)[ix];
            delete[] pbuf;
         }

         pbuf    = p;
         cAlloc  = cNew;
         cMax    = cSize;
         ixHead  = cCopy;
         cItems  = cCopy;
   
      } else if (cSize < cMax) {

         // because of the mustcopy test above, we should only
         // get here if there if cItems is 0 or the current items
         // all fit within the new range from 0 to cSize
         // we shouldn't need to correct ixHead or cItems, but
         // just to be careful, fix them up anyway.
         if (cItems > 0) {
            ixHead = (ixHead + cSize) % cSize;
            if (cItems > cSize) 
               cItems = cSize;
         }
      }
      cMax = cSize;
      return true;
   }

   int Unexpected() {
     #ifdef EXCEPT
      EXCEPT("Unexpected call to empty ring_buffer\n");
     #endif
      return 0;
   }


   // push a new latest item, returns the item that was discarded
   T Push(T val) {
      if (cItems > cMax) return T(Unexpected());
      if ( ! pbuf) SetSize(2);

      // advance the head item pointer
      ixHead = (++ixHead) % cMax;

      // if we have room to add an item without overwriting one
      // then also grow the counter.
      if (cItems < cMax) {
         pbuf[ixHead] = val;
         ++cItems;
         return 0;
      }

      // we get here if cItems == cMax. 
      // we can't add an item without removing one
      // so save off the one we will remove and return it.
      T tmp = pbuf[ixHead];
      pbuf[ixHead] = val;
      return tmp;
   }

   // remove the head item and return it.
   T Pop() {
      if (cItems > 0) {
         T tmp = pbuf[ixHead];
         --cItems;
         --ixHead;
         if (0 == cItems) 
            ixHead = 0;
         return tmp;
      }
      return 0;
   }

   // add to the head item.
   T Add(T val) {
      if ( ! pbuf || ! cMax) return T(Unexpected());
      pbuf[ixHead] += val;
      return pbuf[ixHead];
   }

   // advance to the head item to next slot in the ring buffer,
   // this is equivalent to Push(0) except that it does NOT allocate
   // a ring buffer if there isn't one.
   //
   T Advance() { 
      if (empty()) return 0;
      return Push(T(0)); 
   }
};

// templatize publishing a value to ClassAd's so that we can specialize on types
// that ClassAd's don't support and do the right thing.
//
template <class T>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, T value) {
   return ad.Assign(pattr, value);
}
template <>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, time_t value) {
   return ad.Assign(pattr, (int)value);
}

template <class T> class stats_entry_recent;
extern void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<int> & me);
extern void StatsPublishDebug(const char * pattr, ClassAd & ad, const stats_entry_recent<time_t> & me);

// stats_entry_count holds a single value, that can only count up,
// it is the simplist of all possible statistics values because
//
template <class T> class stats_entry_count {
public:
   stats_entry_count() : value(0) {}
   T value;
   void Publish(ClassAd & ad, const char * pattr) const { 
      ClassAdAssign(ad, pattr, value); 
      };

   static const int unit = AS_COUNT | stats_entry_type<T>::id;
   static void PublishValue(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_count<T> * pthis = (const stats_entry_count<T> *)me;
      ClassAdAssign(ad, pattr, pthis->value);
      }
};

// specialize for time_t because ClassAd doesn't currently handle 64 bit ints.
//template <> void stats_entry_count<time_t>::Publish(ClassAd & ad, const char * pattr) const;
//template <> void stats_entry_count<time_t>::PublishValue(const char * me, ClassAd & ad, const char * pattr);


// use stats_entry_abs for entries that have an absolute value such as Number of jobs currently running.
// this entry keeps track of the largest value as the value changes.
//
template <class T> class stats_entry_abs : public stats_entry_count<T> {
public:
   stats_entry_abs() : largest(0) {}
   T largest;

   void Clear() {
      this->value = 0;
      largest = 0;
   }

   T Set(T val) { 
      if (val > largest)
         largest = val;
      this->value = val;
      return this->value;
   }

   T Add(T val) { return Set(this->value + val); }

   // operator overloads
   stats_entry_abs<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_abs<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_ABS | stats_entry_type<T>::id;
   static void PublishLargest(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_abs<T> * pthis = (const stats_entry_abs<T> *)me;
      ClassAdAssign(ad, pattr, pthis->largest);
      }
};

// specialize for time_t because ClassAd doesn't handle 64 bit ints
//template <> void stats_entry_abs<time_t>::PublishLargest(const char * me, ClassAd & ad, const char * pattr);

// use stats_entry_recent for values that are constantly increasing, such 
// as number of jobs run.  this class keeps track of a the recent total
// as well as the overall total - new values are added to recent and old
// values are subtracted from it. 
//
template <class T> class stats_entry_recent : public stats_entry_count<T> {
public:
   stats_entry_recent(int cRecentMax=0) : recent(0), buf(cRecentMax) {}
   T recent;            // the up-to-date recent value (for publishing)
   ring_buffer<T> buf;  // use to store a buffer of older values

   void Clear() {
      this->value = 0;
      recent = 0;
      buf.Clear();
   }
   void ClearRecent() {
      recent = 0;
      buf.Clear();
   }

   T Add(T val) { 
      this->value += val; 
      recent += val;
      if (val != 0 && buf.MaxSize() > 0) {
         if (buf.empty())
            buf.Push(val);
         else
            buf.Add(val);
      }
      return this->value; 
   }

   // Advance to the next time slot and add a value.
   T Advance(T val) { 
      this->value += val; 
      recent -= buf.Push(val);
      recent += val;
      return this->value; 
   }

   // Advance by cSlots time slots
   T AdvanceBy(int cSlots) { 
      while (cSlots > 0) {
         recent -= buf.Advance();
         --cSlots;
         }
      return this->value; 
   }

   T Set(T val) { 
      T delta = val - this->value;
      return Add(delta);
   }

   void SetWindowSize(int size) {
      buf.SetSize(size);
   }
   void SetRecentMax(int cRecentMax) {
      buf.SetSize(cRecentMax);
   }

   // operator overloads
   stats_entry_recent<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_recent<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_RINGBUF | stats_entry_type<T>::id;
   static void PublishRecent(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_recent<T> * pthis = (const stats_entry_recent<T> *)me;
      ClassAdAssign(ad, pattr, pthis->recent);
      }
   static void PublishDebug(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_recent<T> * pthis = (const stats_entry_recent<T> *)me;
      StatsPublishDebug(pattr, ad, *pthis);
      }
};

// specialize for time_t because ClassAd doesn't handle 64 bit ints
//template <> void stats_entry_recent<time_t>::PublishRecent(const char * me, ClassAd & ad, const char * pattr);

// use timed_queue to keep track of recent windowed values.
// obsolete: use stats_entry_tq for windowed values that need more time accuracy than
// can be provided by the ring_buffer
//
#ifdef _timed_queue_h_

template <class T> class stats_entry_tq : public stats_entry_count<T> {
public:
   stats_entry_tq() : recent(0) {}
   T recent;
   timed_queue<T> tq;

   void Clear() {
      this->value = 0;
      recent = 0;
      tq.clear();
   }
   void ClearRecent() {
      recent = 0;
      tq.clear();
   }

   T Add(T val, time_t now) { 
      this->value += val; 
      if (val != 0) {
         if (tq.empty() || tq.front().first != now) 
            tq.push_front(val, now);
         else
            tq.front().second += val;
      }
      return this->value; 
   }

   T Set(T val, time_t now) { 
      T delta = val - this->value;
      return Add(delta, now);
   }

   T Accumulate(time_t now, time_t window) {
      tq.trim_time(now - window);
      T ret(0);
      for (typename timed_queue<T>::iterator jj(tq.begin());  jj != tq.end();  ++jj) {
         ret += jj->second;
         }
      recent = ret;
      return ret;
   }

   // the the max size of the 
   void SetMaxTime(int size) { tq.max_time(size); }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_TIMED_QUEUE | stats_entry_type<T>::id;
   static void PublishRecent(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_tq<T> * pthis = (const stats_entry_tq<T> *)me;
      ClassAdAssign(ad, pattr, pthis->recent);
      }
};

// specialize for time_t because ClassAd doesn't handle 64 bit ints
// template <> void stats_entry_tq<time_t>::PublishRecent(const char * me, ClassAd & ad, const char * pattr);

#endif // _timed_queue_h_

#undef min
#undef max
#include <limits>

// stats_entry_probe is derived from Miron Livny's Probe class,
// it counts and sums samples as they arrive and can publish
// Min,Max,Average,Variance and Standard Deviation for the data set.
//
// NOTE: the probe derives from the simple counter template and uses
// its 'value' field to hold the count of samples.  the value of the
// samples themselves are not stored, only the sum, min and max are stored.
//
template <class T> class stats_entry_probe : protected stats_entry_count<T> {
public:
   stats_entry_probe() 
      : Max(std::numeric_limits<T>::min())
      , Min(std::numeric_limits<T>::max())
      , Sum(0)
      , SumSq(0) 
   {
   }

protected:
   T Max;        // max sample so far
   T Min;        // min sample so far
   T Sum;        // Sum of samples
   T SumSq;      // Sum of samples squared

public:
   void Clear() {
      this->value = 0; // value is use to store the count of samples.
      Max = std::numeric_limits<T>::min();
      Min = std::numeric_limits<T>::max();
      Sum = 0;
      SumSq = 0;
   }

   T Add(T val) { 
      this->value += 1; // value is use to store the count of samples.
      if (val > Max) Max = val;
      if (val < Max) Min = val;
      Sum += val;
      SumSq += val*val;
      return Sum;
   }

   T Count() { return this->value; }

   T Avg() {
      if (Count() > 0) {
         return this->Sum / Count();
      } else {
         return this->Sum;
      }
   }

   T Var() {
      if (Count() <= 1) {
         return this->Min;
      } else {
         // Var == (SumSQ - count*Avg*Avg)/(count -1)
         return (SumSq - this->Sum * (this->Sum / Count()))/(Count() - 1);
      }
   }

   T Std() {
      if (Count() <= 1) {
         return this->Min;
      } else {
         return sqrt(Var());
      }
   }

   // operator overloads
   stats_entry_probe<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_PROBE | stats_entry_type<T>::id;
   static void PublishLargest(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_probe<T> * pthis = (const stats_entry_probe<T> *)me;
      ClassAdAssign(ad, pattr, pthis->Max);
      }
   static void PublishAverage(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_probe<T> * pthis = (const stats_entry_probe<T> *)me;
      ClassAdAssign(ad, pattr, pthis->Avg());
      }
};

/* ----------------------------------------------------------------------------- 
 * helper functions for dealing with collections of statistics 
 * these functions expect to be passed and array of GenericStatsPubItem
 * with one entry for each statistics value that you wish to publish and or manipulate
 * and a pointer to the base address of the statistics data. 
 */

// publish items into a old-style ClassAd
void generic_stats_PublishToClassAd(ClassAd & ad, const GenericStatsEntry * pTable, int cTable, const char * pdata);
void generic_stats_DeleteInClassAd(ClassAd & ad, const GenericStatsEntry * pTable, int cTable, const char * pdata);

// reset all counters to 0, including Recent buffers.
void generic_stats_ClearAll(const GenericStatsEntry * pTable, int cTable, char * pdata);
// clear the recent buffers
void generic_stats_ClearRecent(const GenericStatsEntry * pTable, int cTable, char * pdata);

// set the window size, and accumulate IS_TIMED_QUEUE entries from generic stats.
void generic_stats_SetTQMax(const GenericStatsEntry * pTable, int cTable, char * pdata, int window);
void generic_stats_AccumulateTQ(const GenericStatsEntry * pTable, int cTable, char * pdata, time_t tmin);

// set the ring buffer size for IS_RINGBUF type generic stats. 
void generic_stats_SetRBMax(const GenericStatsEntry * pTable, int cTable, char * pdata, int cMax);

// each time the time quantum has passed, we wan to Advance the recent buffers
void generic_stats_AdvanceRecent(const GenericStatsEntry * pTable, int cTable, char * pdata, int cAdvance);

// functions for working with a statistics item when you have the type
// encoded in 'units', but don't have a typed pointer.
//
// destroy and free a statistics item 
// (calls delete (type*)pitem) where type is determined from the units code
void generic_stats_itemFree(int units, void * pitem);
void generic_stats_itemSetRecentMax(int units, void * pitem, int window, int quantum);
void generic_stats_itemClear(int units, void * pitem);
void generic_stats_itemClearRecent(int units, void * pitem);
void generic_stats_itemAdvanceRecent(int units, void * pitem, int cAdvance);

// set the ring buffer size for IS_RINGBUF type stats. and time window size for IS_TIMED_QUEUE stats
void generic_stats_SetRecentMax(
   const GenericStatsPubItem * pTable, 
   int    cTable, 
   char * pstruct, 
   int    window, 
   int    quantum);

// publish items into ClassAds using the static Publish methods
void generic_stats_PublishToClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata);
void generic_stats_DeleteInClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata);

// reset all counters, and flush the Recent buffers.
void generic_stats_Clear(const GenericStatsPubItem * pTable, int cTable, char * pdata);
// flush the Recent buffers
void generic_stats_ClearRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata);
// advance the Recent buffers to the next slot
void generic_stats_AdvanceRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata, int cAdvance);

// determine of enough time has passed to advance the Recent buffers and if so, Advance
// this function will modify LastUpdateTime, Lifetime and may modify RecentTickTime and
// RecentLifetime.
// returns number of Quantums that stats were/should be Advanced
//
int generic_stats_Tick(
   const GenericStatsPubItem * pPub, 
   int    cPub, 
   char * pdata,
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,  // in,out
   time_t & RecentTickTime,  // in,out
   time_t & Lifetime,        // in,out
   time_t & RecentLifetime); // in,out



#ifndef FIELDOFF
 #ifdef WIN32
  #define FIELDOFF(st,fld) FIELD_OFFSET(st, fld)
 #else
  //#define FIELDOFF(st,fld) ((int)(size_t)&(((st *)0)->fld))
  #define FIELDOFF(st,fld) offsetof(st,fld)
 #endif
 #define FIELDSIZ(st,fld) ((int)(sizeof(((st *)0)->fld)))
#endif

// use these to help initialize arrays of GenericStatsPubItem's
//
template<class T>
void GenericStatsPublish(const char * pdata, ClassAd & ad, const char * pattr) {
   ClassAdAssign(ad, pattr, *(T*)pdata);
}
#define GS_FIELD(st,fld) (((st *)0)->fld)
#define GENERIC_STATS_PUB_TYPE(st,pre,name,as,T) { pre #name, as | stats_entry_type<T>::id, FIELDOFF(st,name), &GenericStatsPublish<T> }
#define GENERIC_STATS_PUB(st,pre,name,as)        { pre #name, as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishValue }
#define GENERIC_STATS_PUB_RECENT(st,pre,name,as) { pre "Recent" #name, as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishRecent }
#define GENERIC_STATS_PUB_PEAK(st,pre,name,as)   { pre #name "Peak", as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishLargest }

// use these to help initialize arrays of GenericStatsEntry's
//
#define GENERIC_STATS_ENTRY(st,pre,name,as) { pre #name, as, FIELDOFF(st, name), FIELDSIZ(st, name), 0}
#define GENERIC_STATS_ENTRY_TQ(st,pre,name,as) { pre "Recent" #name, as | IS_TIMED_QUEUE, FIELDOFF(st, name.recent), FIELDSIZ(st, name.recent), FIELDOFF(st, name.tq) }
#define GENERIC_STATS_ENTRY_RECENT(st,pre,name,as) { pre "Recent" #name, as | IS_RINGBUF, FIELDOFF(st, name.recent), FIELDSIZ(st, name.recent), FIELDOFF(st, name.buf) }
#define GENERIC_STATS_ENTRY_PEAK(st,pre,name,as) { pre #name "Peak" , as, FIELDOFF(st, name.largest), FIELDSIZ(st, name.largest), 0 }

#endif /* _GENERIC_STATS_H */
