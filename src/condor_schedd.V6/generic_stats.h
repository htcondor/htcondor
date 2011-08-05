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

#ifndef NUMELMS
 #define NUMELMS(aa) (sizeof(aa)/sizeof((aa)[0]))
#endif

#ifndef FIELDOFF
 #ifdef WIN32
  #define FIELDOFF(st,fld) FIELD_OFFSET(st, fld)
 #else
  #define FIELDOFF(st,fld) ((int)(size_t)&(((st *)0)->fld))
 #endif
 #define FIELDSIZ(st,fld) ((int)(sizeof(((st *)0)->fld)))
#endif

template <class T> class ring_buffer {
public:
   ring_buffer() : cMax(0), cAlloc(0), ixFirst(0), cItems(0), pbuf(0) {};
   ~ring_buffer() { delete[] pbuf; };
   int cMax;
   int cAlloc;
   int ixFirst;
   int cItems;
   T*  pbuf;

   T& operator[](int ix) { 
      if ( ! pbuf || ! cMax) return pbuf[0];
      return pbuf[ix % cMax];
   }

   int Length() const { return cItems; } 
   int MaxSize() const { return cMax; }
   bool empty() const { return cItems == 0; }

   bool Clear() {
      bool ret = cItems > 0;
      ixFirst = 0;
      cItems = 0;
      return ret;
   }

   void Free() {
      ixFirst = 0;
      cItems = 0;
      cMax = 0;
      cAlloc = 0;
      delete[] pbuf;
   }

   T Sum() {
      T tot = 0;
      for (int ix = 0; ix < cItems; ++ix)
         tot += this[ix];
      return tot;
   }

   bool SetSize(int cSize) {

      if (cSize < 0) return false;

      if (cSize > cAlloc) {
         const int cAlign = 16;
         int cNew = !cAlloc ? cSize : cSize + (cAlign-1) - ((cSize) % cAlign);
         T* p = new T[cNew];
         if ( ! p) return false;

         // if there is an existing buffer copy items from it to the new buffer
         int cCopy = 0;
         if (pbuf) {
            cCopy = cSize < cItems ? cSize : cItems;
            for (int ix = 0; ix < cCopy; ++ix) {
               p[ix] = (*this)[ix];
            }
            delete[] pbuf;
         }

         pbuf    = p;
         cAlloc  = cNew;
         cMax    = cSize;
         ixFirst = 0;
         cItems  = cCopy;
   
      } else if (cSize < cMax) {

         // if we are shrinking, we need to move the elements
         // in the buffer so that they all still fit within [0 to cSize]
         // if we must discard items, we discard from the front

         int ix0   = ixFirst;
         int ixEnd = ixFirst + cItems;
         if (cSize < cItems) {
            ix0 += cItems - cSize;
         }

         for (int ix = ixEnd-1; ix >= ix0; --ix) {
            pbuf[ix % cSize] = pbuf[(ix - ix0 + ixFirst) % cMax];
         }
         ixFirst = ix0;
         cItems = ixEnd - ix0;
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


   // push a new latest item.
   T Push(T val) {
      if (cItems > cMax) return T(Unexpected());
      if ( ! pbuf) SetSize(2);
      if (cItems < cMax) {
         (*this)[cItems++] = val;
         return 0;
      }
      // if cItems == cMax, then we can't add an item
      // without removing one
      T tmp = pbuf[ixFirst];
      // ((ixFirst+cItems) % cMax) == ixFirst so we can just use ixFirst as here
      // and then increment ixFirst to advance through the ring buffer.
      pbuf[ixFirst] = val;
      ixFirst = (++ixFirst) % cMax;
      return tmp;
   }

   // remove the latest item and return it.
   T Pop() {
      if (cItems > 0) {
         T tmp = this[--cItems];
         if (0 == cItems) ixFirst = 0;
         return tmp;
      }
      return 0;
   }

   // add to the latest item.
   T Add(T val) {
      if ( ! pbuf || ! cMax) return T(Unexpected());
      int ix = (ixFirst + cItems) % cMax;
      pbuf[ix] += val;
      return pbuf[ix];
   }

   // advance to the next item in the ring buffer
   //
   T Advance() { 
      if (empty()) return 0;
      return Push(T(0)); 
   }
};

// this structure is used to describe one field of a statistics structure
// so that we can generically update and publish
//
typedef struct _generic_stats_entry {
   char * pattr;  // name to be used when publishing the value
   int    units;  // field type, AS_COUNT, AS_ABSTIME, etc.
   int    off;    // offset to statistics data value from start of data set
   int    siz;    // size of statistics data value
   int    off2;  // if non-zero, indicates that this value is baked down from a timed_queue.
   } GenericStatsEntry;

#define GENERIC_STATS_ENTRY(st,name, as) { #name, as, FIELDOFF(st, name), FIELDSIZ(st, name), 0}
#define GENERIC_STATS_ENTRY_TQ(st,name,as) { "Recent" #name, as | IS_TIMED_QUEUE, FIELDOFF(st, name.recent), FIELDSIZ(st, name.recent), FIELDOFF(st, name.tq) }
#define GENERIC_STATS_ENTRY_RECENT(st,name,as) { "Recent" #name, as | IS_RINGBUF, FIELDOFF(st, name.recent), FIELDSIZ(st, name.recent), FIELDOFF(st, name.buf) }
#define GENERIC_STATS_ENTRY_PEAK(st,name,as) { #name "Peak" , as, FIELDOFF(st, name.largest), FIELDSIZ(st, name.largest), 0 }

// These enums are for the units field of GenericStatEntry
enum {
   AS_COUNT   = 0,  // an int count of something
   AS_ABSTIME,      // a time_t absoute timestamp (i.e. from time(NULL))
   AS_RELTIME,      // a time_t time duration

   AS_TYPE_MASK   = 0x00FF,
   IS_HISTOGRAM   = 0x0100, // value is a histogram

   IS_TIMED_QUEUE = 0x1000, // recent value is derived from a timed_queue
   IS_RINGBUF     = 0x2000, // recent value is derived from a ring_buffer

   IF_NONZERO    = 0x10000, // only publish non-zero values.
   IF_NEVER      = 0x20000, // set this flag to disable publishing of the item.
   IF_PUBMASK    = IF_NEVER | IF_NONZERO,
   };

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


// stats_entry_xxx classes are used for statistics entries.
//

// use stats_entry_abs for entries that have an absolute value such as Number of jobs currently running.
// this entry keeps track of the largest value as the value changes.
//
template <class T> class stats_entry_abs {
public:
   stats_entry_abs() : value(0), largest(0) {}
   T value;
   T largest;

   T Set(T val) { 
      if (val > largest)
         largest = val;
      value = val;
      return value;
   }
   //T Set(T val, time_t now) { return Set(val); }

   T Add(T val) { return Set(value + val); }
   //T Add(T val, time_t now) { return Add(val); }

   // T Accumulate(time_t now, time_t window) { return value; }

   // operator overloads
   stats_entry_abs<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_abs<T>& operator+=(T val) { Add(val); return *this; }
};

// use stats_entry_recent for values that are constantly increasing, such 
// as number of jobs run.  this class keeps track of a the recent total
// as well as the overall total - new values are added to recent and old
// values are subtracted from it. 
//
template <class T> class stats_entry_recent {
public:
   stats_entry_recent() : value(0), recent(0), buf() {}
   T value;             // the total since the last time the entry was Clear()'d
   T recent;            // the up-to-date recent value (for publishing)
   ring_buffer<T> buf;  // use to store a buffer of older values

   void Clear() {
      value = 0;
      recent = 0;
      buf.Clear();
   }

   T Add(T val) { 
      value += val; 
      recent += val;
      if (val != 0) {
         if (buf.empty())
            buf.Push(val);
         else
            buf.Add(val);
      }
      return value; 
   }
   //T Add(T val, time_t now) { return Add(val); }

   // Advance to the next time slot and add a value.
   T Advance(T val) { 
      value += val; 
      recent -= buf.Push(val);
      recent += val;
      return value; 
   }

   T Set(T val) { 
      T delta = val - value;
      return Add(delta);
   }
   //T Set(T val, time_t now) { return Set(val); }

   //T Accumulate() {return recent;}
   //T Accumulate(time_t now, time_t window) {
   //   return Accumulate();
   //}

   void SetWindowSize(int window) {
      buf.SetSize(window);
   }

   // operator overloads
   stats_entry_recent<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_recent<T>& operator+=(T val) { Add(val); return *this; }
};

// use timed_queue to keep track of recent windowed values.
// obsolete: use stats_entry_tq for windowed values that need more time accuracy than
// can be provided by the ring_buffer
//
template <class T> class stats_entry_tq {
public:
   stats_entry_tq() : value(0), recent(0) {}
   T value;
   T recent;
   timed_queue<T> tq;

   T Add(T val, time_t now) { 
      value += val; 
      if (val != 0) {
         if (tq.empty() || tq.front().first != now) 
            tq.push_front(val, now);
         else
            tq.front().second += val;
      }
      return value; 
   }

   T Set(T val, time_t now) { 
      T delta = val - value;
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

   // can we get rid of this??
   void SetWindowSize(int size) { tq.max_time(size); }
};


#endif /* _GENERIC_STATS_H */
