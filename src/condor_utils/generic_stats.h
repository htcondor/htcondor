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
//   * declare your probes as class (or struct) members
//     * use stats_entry_abs<T>    for probes that need a value and a max value (i.e. number of shadows processes)
//     * use stats_entry_recent<T> for probes that need a value and a recent value (i.e. number of jobs that have finished)
//     * use stats_entry_probe<T>  for general statistics value (min,max,avg,std)
//     * use stats_recent_counter_timer for runtime accumulators (int count, double runtime with overall and recent)
//     * use stats_ema for exponential moving averages
//     * use stats_sum_ema_rate for computing a running total and exponential moving averages of the rate of change
//   * use Add() or Set() methods of the probes (+= and =) to update the probe
//       probes will calculate total value, as well as windowed values and/or min/max,std, or peak values. 
//   * Then EITHER
//      * call the Publish, Clear, and AdvanceBy methods of the counters as needed
//     OR
//      * add your probes to an instance of StatisticsPool 
//      * use the Publish,Clear and AdvanceBy methods of the StatisticsPool
//
// For example:
//
// typedef struct MyStats {
//     time_t                  UpdateTime                     
//     stats_entry_abs<int>    JobsRunning;  // keep track of Peak value as well as absolete value
//     stats_entry_recent<int> JobsRun;      // keep track of Recent values as well as accumulated value
//
//     StatisticsPool Pool;                  // optional
//     void Publish(ClassAd & ad) const;
// } MyStats;
// 
// EITHER this way works if you have only a few probes
//
// void MyStats::Publish(ClassAd & ad) const
// {
//    JobsRunning.Publish(ad, "JobsRunning", 0);
//    JobsRunning.Publish(ad, "JobsRun", 0);
// }
//
// OR otherwise this way is better
//
// void MyStats::Init()
// {
//     Pool.AddProbe("JobsRunning", JobsRunning);
//     Pool.AddProbe("JobsRun", JobsRun);
// }
// void MyStats::Publish(ClassAd & ad) const
// {
//     Pool.Publish(ad);
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

// this is used to identify the fundamental type of a statistics entry so
// that we can use generic data driven code to publish and clear collections
// of statistics. It works with the template specialization below to let us
// capture the type T inside a template expansion as an integer value. 
//
constexpr const int STATS_ENTRY_TYPE_INT32 = 1;
constexpr const int   STATS_ENTRY_TYPE_INT64 = 2;
constexpr const int   STATS_ENTRY_TYPE_FLOAT = 1 | 4;
constexpr const int   STATS_ENTRY_TYPE_DOUBLE = 2 | 4;
constexpr const int   STATS_ENTRY_TYPE_UNSIGNED = 8;
constexpr const int   STATS_ENTRY_TYPE_UINT32 = STATS_ENTRY_TYPE_INT32 | STATS_ENTRY_TYPE_UNSIGNED;
constexpr const int   STATS_ENTRY_TYPE_UINT64 = STATS_ENTRY_TYPE_INT64 | STATS_ENTRY_TYPE_UNSIGNED;

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
   IS_CLS_COUNT   = 0x0000, // is stats_entry_count (has simple value)
   IS_CLS_ABS     = 0x0100, // is stats_entry_abs (has max)
   IS_CLS_PROBE   = 0x0200, // is stats_entry_probe class (has min/max/avg/stddev)
   IS_RECENT      = 0x0400, // is stats_entry_recent class (recent value derived from a ring_buffer)
   IS_RECENTTQ    = 0x0500, // is stats_entry_tq class (recent value derived from a timed_queue)
   IS_RCT         = 0x0600, // is stats_recent_counter_timer 
   IS_REPROBE     = 0x0700, // is stats_entry_recent<Probe> class
   IS_HISTOGRAM   = 0x0800, // is stats_entry_histgram class
   IS_CLS_EMA     = 0x0900, // is stats_entry_sum_ema_rate class
   IS_CLS_SUM_EMA_RATE = 0x0A00, // is stats_entry_sum_ema_rate class

   // values above AS_TYPE_MASK are flags
   //
   IF_ALWAYS     = 0x0000000, // publish regardless of publishing request
   IF_BASICPUB   = 0x0010000, // publish if 'basic' publishing is requested
   IF_VERBOSEPUB = 0x0020000, // publish if 'verbose' publishing is requested.
   IF_HYPERPUB   = 0x0030000, // publish if 'diagnostic' publishing is requested
   IF_RECENTPUB  = 0x0040000, // publish if 'recent' publishing is requested.
   IF_DEBUGPUB   = 0x0080000, // publish if 'debug' publishing is requested.
   IF_PUBLEVEL   = 0x0030000, // level bits
   IF_PUBKIND    = 0x0F00000, // category bits
   IF_NONZERO    = 0x1000000, // only publish non-zero values.
   IF_NOLIFETIME = 0x2000000, // don't publish lifetime values
   IF_RT_SUM     = 0x4000000, // publish probe Sum value as Runtime
   IF_PUBMASK    = 0x0FF0000, // bits that affect publication
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

   // ring buffer accepts negative or positive indexes and interprets them modulo MaxSize()
   // this[0] is the head item, and so is this[MaxSize()]
   // this[-1] is the previous item, so is this[MaxSize()-1]
   // this[1] is the last item in the ring, so is this[1-MaxSize()]
   // when Length() < MaxSize(), only items between [0] and [-Length()] will contain valid data
   // once Length() == MaxSize(), all indexes in the ring buffer are valid.
   T& operator[](int ix) { 
      // yes, we do want to segfault if pbuf==NULL
      MSC_SUPPRESS_WARNING_FOREVER(6011) // dereferencing null pointer.
      if ( ! pbuf || ! cMax) return pbuf[0];

      // pbuf[ixHead] is this[0], and we need to use only positive indexes into pbuf
      // we achieve that for all positive values and reasonable negative values (0 to -cMax)
      // by adding cMax before we mod. this is the expected case and is branch-free.
      int ixmod = (ixHead+ix+cMax) % cMax;
      // if the moded index is STILL negative, it must be now between 0 and -cMax, so just
      // add and mod again. This branch should be seldom (never?) be taken.
      if (ixmod < 0) ixmod = (ixmod+cMax) % cMax;
      return pbuf[ixmod];
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
      pbuf = NULL;
   }

   T Sum() {
      T tot(0);
      for (int ix = 0; ix > (0 - cItems); --ix)
         tot += (*this)[ix];
      return tot;
   }

   bool SetSize(int cSize) {

      if (cSize < 0) return false;
      if (cSize == 0) {
         this->Free();
         return true;
      }

      // quantize the requested size so that we don't have to reallocate as often
      // if the size is changing by a small amount. (i.e. incrementing or decrementing)
      const int cAlign = 5;
      int cQuantizedSize = (cSize % cAlign) ? (cSize + cAlign - (cSize % cAlign)) : cSize;

      // we must re-allocate and copy elements if the allocation size is changing
      bool fMustCopy = false;
      if ((cSize != cMax) && (cAlloc != cQuantizedSize))
         fMustCopy = true;

      // if current items are outside of the new ring buffer from [0 to cSize]
      // then we have to copy items, so we might as well allocate a new buffer
      // even if we are shrinking.
      if ((cItems > 0) && (ixHead >= cSize || ixHead - cItems + 1 < 0))
         fMustCopy = true;

      // allocate a new buffer and copy items from the old buffer, note that
      // we allocate the EXACT requested size the first time, but we use a
      // quantized size when asked to grow or shrink the buffer.
      if (fMustCopy) {
         int cNew = !cAlloc ? cSize : cQuantizedSize;
         T* p = new T[cNew];
         if ( ! p) return false;

         // if there is an existing buffer copy items from it to the new buffer
         int cCopy = 0;
         if (pbuf) {
            MSC_SUPPRESS_WARNING_FOREVER(6385) // Read overrun. ms analyze claims p can have readable range of 0 here, which isn't possible.
            MSC_SUPPRESS_WARNING_FOREVER(6386) // Write overrun. ms analyze claims p can have writable range of 0 here, which isn't possible.
            cCopy = MIN(cItems,cSize);
            for (int ix = 0; ix > 0 - cCopy; --ix)
               p[(ix+cCopy)%cSize] = (*this)[ix];
            delete[] pbuf;
         }

         pbuf    = p;
         cAlloc  = cNew;
         cMax    = cSize;
         ixHead  = cCopy % cSize;
         cItems  = cCopy;

      } else if (cSize < cMax) {

         // because of the mustcopy test above, we should only
         // get here if there if cItems is 0 or the current items
         // all fit within the new range from 0 to cSize
         // we shouldn't need to correct ixHead or cItems, but
         // just to be careful, fix them up anyway.
         if (cItems > 0) {
            ixHead = ixHead % cSize;
            if (cItems > cSize) 
               cItems = cSize;
         }
      }
      cMax = cSize;
      return true;
   }

   int Unexpected() {
      EXCEPT("Unexpected call to empty ring_buffer");
      return 0;
   }


   // push an empty item, this is more efficient
   // when pbuf is an array of classes.
   void PushZero() {
      if (cItems > cMax) {
         Unexpected();
         return;
      }

      if ( ! pbuf) SetSize(2);

      // advance the head item pointer
      ++ixHead;
      ixHead %= cMax;

      // if we have room to add an item without overwriting one
      // then also grow the counter.
      if (cItems < cMax) {
         ++cItems;
      }
      // clear the head item.
      pbuf[ixHead] = 0;
   }

// we don't use these, the semantics could easily lead to
// memory leaks for the ring buffers used by statistics code.
// 
#ifdef RING_BUFFER_PUSH_POP
   // push a new latest item, returns the item that was discarded
   T Push(T val) {
      if (cItems > cMax) {
         Unexpected();
         return T(0);
      }
      if ( ! pbuf) SetSize(2);

      // advance the head item pointer
      ++ixHead;
      ixHead %= cMax;

      // if we have room to add an item without overwriting one
      // then also grow the counter.
      if (cItems < cMax) {
         pbuf[ixHead] = val;
         ++cItems;
         return T(0);
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
      return T(0);
   }

   // advance to the head item to next slot in the ring buffer,
   // this is equivalent to PushZero except that it does NOT allocate
   // a ring buffer if there isn't one.
   //
   T Advance() { 
      if (empty()) return T(0);
      return Push(T(0));
   }
#endif // RING_BUFFER_PUSH_POP

   // add to the head item.
   const T& Add(T val) {
      if ( ! pbuf || ! cMax) {
         Unexpected();
         return pbuf[0];
      }
      pbuf[ixHead] += val;
      return pbuf[ixHead];
   }

   // advance the ring buffer by cAdvance slots if there the buffer
   // has been allocated, but don't treat an unallocated buffer as an error.
   // Sum the overwritten items and return them via pSumTo
   //
   void AdvanceBy(int cAdvance) { 
      if (cMax <= 0) 
         return;
      while (--cAdvance >= 0)
         PushZero(); 
   }

   // advance the ring buffer by cAdvance slots if there the buffer
   // has been allocated, but don't treat an unallocated buffer as an error.
   // Sum the overwritten items and return them via pSumTo
   //
   void AdvanceAccum(int cAdvance, T& accum) { 
      if (cMax <= 0) 
         return;
      while (--cAdvance >= 0) {
         if (cItems == cMax)
            accum += pbuf[(ixHead+1)%cMax];
         PushZero(); 
      }
   }
};

// templatize publishing a value to ClassAd's so that we can specialize on types
// that ClassAd's don't support and do the right thing.
//
template <class T>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, T value) {
   return ad.Assign(pattr, value);
}
/*
template <>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, int64_t value) {
   return ad.Assign(pattr, (int)value);
}
*/

template <class T>
inline int ClassAdAssign2(ClassAd & ad, const char * pattr1, const char * pattr2, T value) {
   std::string attr(pattr1);
   attr += pattr2;
   return ClassAdAssign(ad, attr.c_str(), value);
}
/*
template <>
inline int ClassAdAssign2(ClassAd & ad, const char * pattr1, const char * pattr2, int64_t value) {
   return ClassAdAssign2(ad, pattr1, pattr2, (int)value);
}
*/

// base class for all statistics probes
//
class stats_entry_base;
typedef void (stats_entry_base::*FN_STATS_ENTRY_PUBLISH)(ClassAd & ad, const char * pattr, int flags) const;
typedef void (stats_entry_base::*FN_STATS_ENTRY_UNPUBLISH)(ClassAd & ad, const char * pattr) const;
typedef void (stats_entry_base::*FN_STATS_ENTRY_ADVANCE)(int cAdvance);
typedef void (stats_entry_base::*FN_STATS_ENTRY_SETRECENTMAX)(int cRecent);
typedef void (stats_entry_base::*FN_STATS_ENTRY_CLEAR)(void);
typedef void (*FN_STATS_ENTRY_DELETE)(void* probe);

class stats_entry_base {
public:
   static const int unit = 0;

   // in derived templates that have Advance and SetRecentMax methods
   // replace the implementation of these with code that returns &AdvanceBy
   // and &SetRecentMax respectively.  we do this so that we can fill out
   // the callback table in the StatisticsPool::poolitems
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return NULL; };
   static FN_STATS_ENTRY_SETRECENTMAX GetFnSetRecentMax() { return NULL; };
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return NULL; }
};


// stats_entry_count holds a single value
// it is the simplist of all possible statistics probes
//
template <class T> class stats_entry_count : public stats_entry_base {
public:
   stats_entry_count() : value(0) {}
   T value;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      (void)flags;
      ClassAdAssign(ad, pattr, value); 
      };
   void Unpublish(ClassAd & ad, const char * pattr) const {
      ad.Delete(pattr);
      };
   static const int unit = IS_CLS_COUNT | stats_entry_type<T>::id;
   static void Delete(stats_entry_count<T> * probe) { delete probe; }
};

template <class T> inline bool stats_entry_is_zero(const T& val) { return val == 0; }
template <> inline bool stats_entry_is_zero(const double& val) { return val >= 0.0 && val <= 0.0; }

// use stats_entry_abs for entries that have an absolute value such as Number of jobs currently running.
// this entry keeps track of the largest value as the value changes.
//
template <class T> class stats_entry_abs : public stats_entry_count<T> {
public:
   stats_entry_abs() : largest(0) {}
   T largest;

   static const int PubValue = 1;
   static const int PubLargest = 2;
   static const int PubDecorateAttr = 0x100;
   static const int PubDefault = PubValue | PubLargest | PubDecorateAttr;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value); 
      if (flags & this->PubLargest) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, pattr, "Peak", largest);
         else
            ClassAdAssign(ad, pattr, largest); 
      }
   }
   void Unpublish(ClassAd & ad, const char * pattr) const {
      ad.Delete(pattr);
      std::string attr(pattr);
      attr += "Peak";
      ad.Delete(attr);
      };

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
   T Sub(T val) { return Set(this->value - val); }

   // operator overloads
   stats_entry_abs<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_abs<T>& operator+=(T val) { Add(val); return *this; }
   stats_entry_abs<T>& operator-=(T val) { Sub(val); return *this; }

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_CLS_ABS | stats_entry_type<T>::id;
   static void Delete(stats_entry_abs<T> * probe) { delete probe; }
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_abs<T>::Unpublish; };
};

class stats_ema_config {
 public:
	void add(time_t horizon,char const *horizon_name);
	bool sameAs( stats_ema_config const *other);

	class horizon_config {
	public:
		horizon_config(time_t h,char const *h_name): horizon(h), horizon_name(h_name), cached_alpha(0.0), cached_interval(0) {}

		time_t horizon;
		std::string horizon_name;
		double cached_alpha;
		time_t cached_interval;
	};
	typedef std::vector< horizon_config > horizon_config_list;
	horizon_config_list horizons;
};

// exponential moving average class
class stats_ema {
 public:
	double ema;
	time_t total_elapsed_time;

	stats_ema():
		ema(0.0), total_elapsed_time(0) {}

	void Clear() {
		ema = 0.0;
		total_elapsed_time = 0;
	}

	void Update(double value,time_t interval,stats_ema_config::horizon_config &config) {
		if( config.cached_interval != interval ) {
			config.cached_interval = interval;
			config.cached_alpha = 1.0 - exp(-(double)interval/double(config.horizon));
		}
		this->ema = config.cached_alpha*value + (1.0-config.cached_alpha)*ema;
		this->total_elapsed_time += interval;
	}

	bool insufficientData(stats_ema_config::horizon_config &config) const {
		return total_elapsed_time < config.horizon;
	}
};

// collection of exponential moving averages
typedef std::vector<stats_ema> stats_ema_list;

	// Parse exponential moving average list configuration string of the form
	// "NAME1:HORIZON1 NAME2:HORIZON2 ..."
	// Where each horizon is specified as an integer number of seconds
	// Fills in ema_horizons, which can be fed to ConfigureEMAHorizons() of one or more
	// instances of this class to allocate EMA variables.
bool ParseEMAHorizonConfiguration(char const *ema_conf,std::shared_ptr<stats_ema_config> &ema_horizons,std::string &error_str);

// use stats_entry_sum_ema_rate to compute a running sum and rate of growth.
// The rate of growth is computed as an exponential moving average over
// a set of time horizons, like linux load average.  The specific horizons
// are configurable.  To avoid misinterpretation and clutter, only the EMAs
// for which at least one full horizon has elapsed will be published.
//
// Example: track sum of bytes transferred since startup
// and average bytes transferred per second over 1m, 5m, 1h, and 1d horizons.
//
template <class T> class stats_entry_ema_base : public stats_entry_count<T> {
public:
	stats_entry_ema_base() { Clear(); }

	stats_ema_list ema;
	time_t recent_start_time;
	std::shared_ptr<stats_ema_config> ema_config;

	void Clear() {
		this->value = 0;
		this->recent_start_time = time(NULL);
		for(stats_ema_list::iterator ema_itr = ema.begin();
			ema_itr != ema.end();
			++ema_itr )
		{
			ema_itr->Clear();
		}
	}
	void SkipInterval() {
			// Add 1 second of slop time so immediately calling Update() after this
			// will likely be a no-op.
		this->recent_start_time = time(NULL)+1;
	}

		// Allocate variables to track exponential moving averages.
		// Can be called to reconfigure or initialize this object.
		// Args:
		//   ema_config serves as a template; it can be created with ParseEMAHorizonConfiguration()
	void ConfigureEMAHorizons(std::shared_ptr<stats_ema_config> config);

		// Return the biggest EMA rate value
	double BiggestEMAValue() const;

		// Return the name of EMA value with the shortest horizon
	char const *ShortestHorizonEMAName() const;

		// Return the named EMA rate value
	double EMAValue(char const *horizon_name) const;

	bool HasEMAHorizonNamed(char const *horizon_name) const;

		// bits used in Publish() flags
	static const int PubValue = 1;
	static const int PubEMA = 2;
	static const int PubDecorateAttr = 0x100;
	static const int PubDecorateLoadAttr = 0x200;
	static const int PubSuppressInsufficientDataEMA = 0x300;

	void Publish(ClassAd & ad, const char * pattr, int flags) const;
	void Unpublish(ClassAd & ad, const char * pattr) const;
};

template <class T> class stats_entry_sum_ema_rate : public stats_entry_ema_base<T> {
public:
	T recent_sum = {};


	void Clear() {
		this->recent_sum = 0;
		stats_entry_ema_base<T>::Clear();
	}
	T Set(T val) { 
		this->recent_sum = val - this->value;
		this->value = val;
		return this->value;
	}

	T Add(T val) {
		this->recent_sum += val;
		this->value += val;
		return this->value;
	}

		// update the exponential moving averages of the rate of growth
	void Update(time_t now) {
			// Throw out data points during which time jumps into the past.
			// We could be more correct and use clock_gettime(CLOCK_MONOTONIC),
			// but that is overkill for our current uses of this code.
		if( now > this->recent_start_time ) {
			time_t interval = now - this->recent_start_time;
			double recent_rate = (double)this->recent_sum/interval;

			for(size_t i = this->ema.size(); i--; ) {
				this->ema[i].Update(recent_rate,interval,this->ema_config->horizons[i]);
			}
		}

		this->recent_sum = 0;
		this->recent_start_time = now;
	}
		// We don't have a ring buffer, so cSlots has no meaning, but we still define AdvanceBy()
		// so that our Update() function will get called automatically by the statistics pool.
	void AdvanceBy(int cSlots) { 
		if (cSlots <= 0) 
			return;
		Update( time(NULL) );
	}


	static const int PubDefault =
		stats_entry_sum_ema_rate<T>::PubValue |
		stats_entry_sum_ema_rate<T>::PubEMA |
		stats_entry_sum_ema_rate<T>::PubDecorateAttr |
		stats_entry_sum_ema_rate<T>::PubDecorateLoadAttr |
		stats_entry_sum_ema_rate<T>::PubSuppressInsufficientDataEMA;

	void Publish(ClassAd & ad, const char * pattr, int flags) const;
	void Unpublish(ClassAd & ad, const char * pattr) const;

		// callback methods/fetchers for use by the StatisticsPool class
	static const int unit = IS_CLS_SUM_EMA_RATE | stats_entry_type<T>::id;
	static void Delete(stats_entry_sum_ema_rate<T> * probe) { delete probe; }
	static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_sum_ema_rate<T>::Unpublish; };
	static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_entry_sum_ema_rate<T>::AdvanceBy; };

		// operator overloads
	stats_entry_sum_ema_rate<T>& operator=(T val)  { this->Set(val); return *this; }
	stats_entry_sum_ema_rate<T>& operator+=(T val) { this->Add(val); return *this; }
};

template <class T> class stats_entry_ema : public stats_entry_ema_base<T> {
public:
	T Set(T val) { 
		this->value = val;
		return this->value;
	}

	T Add(T val) {
		this->value += val;
		return this->value;
	}

		// update the exponential moving averages of the rate of growth
	void Update(time_t now) {
			// Throw out data points during which time jumps into the past.
			// We could be more correct and use clock_gettime(CLOCK_MONOTONIC),
			// but that is overkill for our current uses of this code.
		if( now > this->recent_start_time ) {
			time_t interval = now - this->recent_start_time;

			for(size_t i = this->ema.size(); i--; ) {
				this->ema[i].Update(this->value,interval,this->ema_config->horizons[i]);
			}
		}
		this->recent_start_time = now;
	}
		// We don't have a ring buffer, so cSlots has no meaning, but we still define AdvanceBy()
		// so that our Update() function will get called automatically by the statistics pool.
	void AdvanceBy(int cSlots) { 
		if (cSlots <= 0) 
			return;
		Update( time(NULL) );
	}


	static const int PubDefault =
		stats_entry_sum_ema_rate<T>::PubEMA |
		stats_entry_sum_ema_rate<T>::PubDecorateAttr |
		stats_entry_sum_ema_rate<T>::PubSuppressInsufficientDataEMA;


	void Publish(ClassAd & ad, const char * pattr, int flags) const;
	void Unpublish(ClassAd & ad, const char * pattr) const;

		// callback methods/fetchers for use by the StatisticsPool class
	static const int unit = IS_CLS_EMA | stats_entry_type<T>::id;
	static void Delete(stats_entry_ema<T> * probe) { delete probe; }
	static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_ema<T>::Unpublish; };
	static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_entry_ema<T>::AdvanceBy; };

		// operator overloads
	stats_entry_ema<T>& operator=(T val)  { this->Set(val); return *this; }
	stats_entry_ema<T>& operator+=(T val) { this->Add(val); return *this; }
};

// use stats_entry_recent for values that are constantly increasing, such 
// as number of jobs run.  this class keeps track of a the recent total
// as well as the overall total - new values are added to recent and old
// values are subtracted from it. 
//
GCC_DIAG_OFF(float-equal)
template <typename T> 
class stats_entry_recent : public stats_entry_count<T> {
public:
   stats_entry_recent(int cRecentMax=0) : recent(0), buf(cRecentMax) {}
   T recent;            // the up-to-date recent value (for publishing)
   ring_buffer<T> buf;  // use to store a buffer of older values

   static const int PubValue = 1;
   static const int PubRecent = 2;
   static const int PubDetailMask = 0x7C; // control visibility of internal structure, use when T is Probe
   static const int PubDebug = 0x80;
   static const int PubDecorateAttr = 0x100;
   static const int PubValueAndRecent = PubValue | PubRecent | PubDecorateAttr;
   static const int PubDefault = PubValueAndRecent;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if ((flags & IF_NONZERO) && stats_entry_is_zero(this->value)) return;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value);
      if (flags & this->PubRecent) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, recent);
         else
            ClassAdAssign(ad, pattr, recent); 
      }
      if (flags & this->PubDebug) {
         PublishDebug(ad, pattr, flags);
      }
   }
   void Unpublish(ClassAd & ad, const char * pattr) const {
      ad.Delete(pattr);
      std::string attr;
      formatstr(attr, "Recent%s", pattr);
      ad.Delete(attr);
      };

   void PublishDebug(ClassAd & ad, const char * pattr, int flags) const;

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
      if (buf.MaxSize() > 0) {
         if (buf.empty())
            buf.PushZero();
         buf.Add(val);
      }
      return this->value; 
   }

#ifdef RING_BUFFER_PUSH_POP // depends on ring_buffer::Push
   // Advance to the next time slot and add a value.
   T Advance(T val) { 
      this->value += val; 
      if (buf.MaxSize() > 0) {
         recent -= buf.Push(val);
         recent += val;
      } else {
         recent = val;
      }
      return this->value; 
   }
#endif // RING_BUFFER_PUSH_POP

   // Advance by cSlots time slots, then update recent by summing the remaining
   void AdvanceBy(int cSlots) { 
      if (cSlots <= 0) 
         return;
      buf.AdvanceBy(cSlots); 
      recent = buf.Sum();
   }

   // Advance by cSlots time slots, keeping a running total, more efficient
   // than AdvandBy, but can only be used for types that support -=
   void AdvanceAndSub(int cSlots) { 
      if (cSlots < buf.MaxSize()) {
         T accum(0);
         buf.AdvanceAccum(cSlots, accum);
         recent -= accum;
      } else {
         recent = 0;
         buf.Clear();
      }
   }

   T Set(T val) { 
      T delta = val - this->value;
      return Add(delta);
   }

   void SetWindowSize(int size) {
      if (buf.MaxSize() != size) {
         buf.SetSize(size);
         recent = buf.Sum(); // recalculate the recent sum in case the size changed.
      }
   }
   void SetRecentMax(int cRecentMax) {
      SetWindowSize(cRecentMax);
   }

   // operator overloads
   stats_entry_recent<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_recent<T>& operator+=(T val) { Add(val); return *this; }

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_RECENT | stats_entry_type<T>::id;
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_entry_recent<T>::AdvanceBy; };
   static FN_STATS_ENTRY_SETRECENTMAX GetFnSetRecentMax() { return (FN_STATS_ENTRY_SETRECENTMAX)&stats_entry_recent<T>::SetRecentMax; };
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_recent<T>::Unpublish; };
   static void Delete(stats_entry_recent<T> * probe) { delete probe; }
};

GCC_DIAG_ON(float-equal)

// specialize AdvanceBy for simple types so that we can use a more efficient algorithm.
template <> void stats_entry_recent<int>::AdvanceBy(int cSlots);
template <> void stats_entry_recent<int64_t>::AdvanceBy(int cSlots);
template <> void stats_entry_recent<double>::AdvanceBy(int cSlots);

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

   static const int PubValue = 1;
   static const int PubRecent = 2;
   static const int PubDebug = 4;
   static const int PubDecorateAttr = 0x100;
   static const int PubValueAndRecent = PubValue | PubRecent | PubDecorateAttr;
   static const int PubDefault = PubValueAndRecent;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value); 
      if (flags & this->PubRecent) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, recent);
         else
            ClassAdAssign(ad, pattr, recent); 
      }
   }
   void Unpublish(ClassAd & ad, const char * pattr) const {
      ad.Delete(pattr);
      std::string attr;
      formatstr(attr, "Recent%s", pattr);
      ad.Delete(attr);
      };

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
      if (val != T(0)) {
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

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_RECENTTQ | stats_entry_type<T>::id;
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_tq<T>::Unpublish; };
   static void Delete(stats_entry_tq<T> * probe) { delete probe; }
};


#endif // _timed_queue_h_

#include <limits>

// stats_entry_probe is derived from Miron Livny's Probe class,
// it counts and sums samples as they arrive and can publish
// Min,Max,Average,Variance and Standard Deviation for the data set.
//
// NOTE: the probe derives from the simple counter template and uses
// its 'value' field to hold the count of samples.  the value of the
// samples themselves are not stored, only the sum, min and max are stored.
//
GCC_DIAG_OFF(float-equal)
template <typename T> class stats_entry_probe : public stats_entry_count<T> {
public:
   stats_entry_probe() 
      : Max(-(std::numeric_limits<T>::max()))
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
   void Publish(ClassAd & ad, const char * pattr, int flags) const;
   void Unpublish(ClassAd & ad, const char * pattr) const;

   void Clear() {
      this->value = 0; // value is use to store the count of samples.
      Max = -(std::numeric_limits<T>::max());
      Min = std::numeric_limits<T>::max();
      Sum = 0;
      SumSq = 0;
   }

   T Add(T val) { 
      this->value += 1; // value is use to store the count of samples.
      if (val > Max) Max = val;
      if (val < Min) Min = val;
      Sum += val;
      SumSq += val*val;
      return Sum;
   }

   T Count() const { return this->value; }

   T Total() const { return this->Sum; }

   T Avg() const {
      if (Count() > 0) {
         return this->Sum / Count();
      } else {
         return this->Sum;
      }
   }

   T Var() const {
      if (Count() <= 1) {
         return this->Min;
      } else {
         // Var == (SumSQ - count*Avg*Avg)/(count -1)
         return (SumSq - this->Sum * (this->Sum / Count()))/(Count() - 1);
      }
   }

   T Std() const {
      if (Count() <= 1) {
         return this->Min;
      } else {
         return sqrt(Var());
      }
   }

   // operator overloads
   stats_entry_probe<T>& operator+=(T val) { Add(val); return *this; }

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_CLS_PROBE | stats_entry_type<T>::id;
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_entry_probe<T>::Unpublish; };
   static void Delete(stats_entry_probe<T> * probe) { delete probe; }
};
GCC_DIAG_ON(float-equal)

// --------------------------------------------------------------------
//   Full Min/Max/Avg/Std Probe class for use with stats_entry_recent
//
class Probe {
public:
   Probe(int=0) 
      : Count(0)
      , Max(-std::numeric_limits<double>::max())
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
   void Clear();
   double Add(double val);
   Probe & Add(const Probe & val);
   double Avg() const;
   double Var() const;
   double Std() const;

   // operator overloads
   Probe& operator+=(double val) { Add(val); return *this; }
   Probe& operator+=(const Probe & val) { Add(val); return *this; }
   // NOT actually functional - however, this is necessary for template instantiation.
   Probe& operator-=(const Probe & /*val*/) { return *this; }
   Probe& operator-(const Probe & /*val*/) { return *this; }

   // comparison to int(0) is used to detect if the probe is 'empty'
   bool operator==(const int &ii) const { return ii ? false : this->Count == ii; }
   bool operator!=(const int &ii) const { return !(*this == ii); }

   static const int unit = IS_CLS_PROBE;
};

//template <> void stats_entry_recent<Probe>::AdvanceBy(int cSlots);
template <> void stats_entry_recent<Probe>::Publish(ClassAd& ad, const char * pattr, int flags) const;
template <> void stats_entry_recent<Probe>::Unpublish(ClassAd& ad, const char * pattr) const;
int ClassAdAssign(ClassAd & ad, const char * pattr, const Probe& probe);
// detail mode must fit inside the stats_entry_recent<Probe>::PubDetailMask which is 0x7C, so we shift up by 2 bits
const int ProbeDetailMode_Normal  = (0<<2); // show all 6 fields Count, Sum, Avg, Min, Max, Std
const int ProbeDetailMode_Tot     = (1<<2); // show Sum as integer value without tag
const int ProbeDetailMode_Brief   = (2<<2); // show Avg without tag, Min, Max
const int ProbeDetailMode_RT_SUM  = (3<<2); // show runtime fields, Sum=Runtime, Count published without tag
const int ProbeDetailMode_CAMM    = (4<<2); // show 4 fields. Count, Avg, Min, Max

// --------------------------------------------------------------------
//  statistcs probe for histogram data.
//  Holds a set of integer counters indexed by level
//  where level is defined by an array of constants of increasing size.
//  When a data value is added to an instance of this class, a index is chosen
//  such that level[index-1] <= value < level[index], then counter[index] is incremented
//
template <class T>
class stats_histogram : public stats_entry_base {
public:
   stats_histogram(const T* ilevels = 0, int num_levels = 0) 
      : cLevels(num_levels)
      , levels(ilevels)
      , data(0)
      {
         if (cLevels) {
            data = new int[cLevels+1];
            Clear();
         }
      }
   stats_histogram(const stats_histogram<T> &sh)
      : cLevels(sh.cLevels)
      , levels(sh.levels)
      , data(0)
      {
         if (cLevels) {
            data = new int[cLevels+1];
            for (int ii = 0; ii <= cLevels; ++ii) {
               data[ii] = sh.data[ii];
            }
         }
      }

   ~stats_histogram() { delete [] data; data = 0, cLevels = 0; }

public:
   int      cLevels;    // number of levels
   const T* levels;    // upper size bound for each member of data
   int*     data;      // data array is cLevels+1 in size

   bool set_levels(const T* ilevels, int num_levels);

   void Clear() { if (data) for (int ii = 0; ii <= cLevels; ++ii) data[ii] = 0; }

   static const int PubValue = 1;
   static const int PubDefault = PubValue;
   void AppendToString(std::string & str) const {
      if (this->cLevels > 0) {
         str += std::to_string( this->data[0] );
         for (int ix = 1; ix <= this->cLevels; ++ix) {
            str += ", ";
            str += std::to_string( this->data[ix] );
            }
         }
      }
   void Publish(ClassAd & ad, const char * pattr, int  /*flags*/) const {
      std::string str;
      this->AppendToString(str);
      ad.Assign(pattr, str);
      }
   void Unpublish(ClassAd & ad, const char * pattr) { ad.Delete(pattr); }

   T Add(T val);
   T Remove(T val);
   stats_histogram<T>& Accumulate(const stats_histogram<T>& sh);

   // operator overloads
   stats_histogram<T>& operator+=(const stats_histogram<T>& sh) { return Accumulate(sh); }
   T operator+=(T val) { return Add(val); }
   T operator-=(T val) { return Remove(val); }
   stats_histogram& operator=(const stats_histogram<T>& sh);
   stats_histogram& operator=(int val) {
      if (val != 0) {
          EXCEPT("Clearing operation on histogram with non-zero value");
      }
      Clear();
      return *this;
   }

   // comparison to int(0) is used to detect if the histogram is 'empty'
   bool operator==(const int &val) const { return val ? false : this->cLevels == val; }
   bool operator!=(const int &val) const { return !(*this == val); }

   // helper methods for parsing configuration
   //static int ParseLimits(const char * psz, T * pLimits, int cMax);
   //static int PrintLimits(std::string & str, const T * pLimits, int cLimits);

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_HISTOGRAM | stats_entry_type<T>::id;
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_histogram<T>::Unpublish; };
   static void Delete(stats_entry_recent<T> * probe) { delete probe; }
};

// stats_histogram code
//
template <class T>
bool stats_histogram<T>::set_levels(const T* ilevels, int num_levels)
{
   bool ret = false;
   if(cLevels == 0 && ilevels != NULL) {
      cLevels = num_levels;
      levels = ilevels;
      data = new int[cLevels + 1];
      Clear();
      ret = true;
   }
   return ret;
}

template<class T>
stats_histogram<T>& stats_histogram<T>::Accumulate(const stats_histogram<T>& sh)
{
   // if the input histogram is null, there is nothing to do.
   if (sh.cLevels <= 0) {
      return *this;
   }

   // if the current histogram is null, take on the size and levels of the input
   if (this->cLevels <= 0) {
      this->set_levels(sh.levels, sh.cLevels);
   }

   // to add histograms, they must both be the same size (and have the same
   // limits array as well, should we check that?)
   if (this->cLevels != sh.cLevels) {
       EXCEPT("attempt to add histogram of %d items to histogram of %d items",
              sh.cLevels, this->cLevels);
       return *this;
   }

   if (this->levels != sh.levels) {
       EXCEPT("Histogram level pointers are not the same.");
       return *this;
   }

   for (int i = 0; i <= cLevels; ++i) {
      this->data[i] += sh.data[i];
   }
   
   return *this;
}

GCC_DIAG_OFF(float-equal)
template<class T>
stats_histogram<T>& stats_histogram<T>::operator=(const stats_histogram<T>& sh)
{
   if(sh.cLevels == 0){
      Clear();
   } else if(this != &sh) {
      if(this->cLevels > 0 && this->cLevels != sh.cLevels){
         EXCEPT("Tried to assign different sized histograms");
      return *this;
      } else if(this->cLevels == 0) {
         this->cLevels = sh.cLevels;
         this->data = new int[this->cLevels+1];
         this->levels = sh.levels;
         for(int i=0;i<=cLevels;++i){
            this->data[i] = sh.data[i];
         }
      } else {
         for(int i=0;i<=cLevels;++i){
            this->data[i] = sh.data[i];
            if(this->levels[i] < sh.levels[i] || this->levels[i] > sh.levels[i]){
               EXCEPT("Tried to assign different levels of histograms");
               return *this;
            }
         }
      }
      this->data[this->cLevels] = sh.data[sh.cLevels];
   }
   return *this;
}
GCC_DIAG_ON(float-equal)

template<class T>
T stats_histogram<T>::Add(T val)
{
    int ix = 0;
    while (ix < cLevels && val >= levels[ix])
        ++ix;
    data[ix] += 1;
    /* the above code should give the same result as this...
     if(val < levels[0]){
        data[0] += 1;
     } else if(val >= levels[cLevels - 1]){
        data[cLevels] += 1;
     } else {
        for(int i=1;i<cLevels;++i){
           if(val >= levels[i-1] && val < levels[i]){
              data[i] += 1;
           }
        }
     }
    */

    return val;
}

template<class T>
T stats_histogram<T>::Remove(T val)
{
   int ix = 0;
   while (ix < cLevels && val >= levels[ix])
      ++ix;
   data[ix] -= 1;
   return val;
}


// helper functions for parsing configuration for histogram limits
int stats_histogram_ParseSizes(const char * psz, int64_t * pSizes, int cMaxSizes);
void stats_histogram_PrintSizes(std::string & str, const int64_t * pSizes, int cSizes);
int stats_histogram_ParseTimes(const char * psz, time_t * pTimes, int cMaxTimes);
void stats_histogram_PrintTimes(std::string & str, const time_t * pTimes, int cTimes);

// --------------------------------------------------------------------
//  statistics probe combining a histogram class with Recent ring buffer
//
template <typename T> 
class stats_entry_recent_histogram : public stats_entry_recent< stats_histogram<T> > {
public:
   stats_entry_recent_histogram(const T* vlevels = 0, int num_levels = 0) 
      : recent_dirty(false) 
      {
      if (num_levels && vlevels) {
         this->value.set_levels(vlevels, num_levels);
         this->recent.set_levels(vlevels, num_levels);
         }
      };
      
   bool recent_dirty;
   
   bool set_levels(const T* vlevels, int num_levels) {
      this->recent.set_levels(vlevels, num_levels);
      return this->value.set_levels(vlevels, num_levels);
   }

   T Add(T val) { 
      this->value.Add(val); 
      if (this->buf.MaxSize() > 0) {
         if (this->buf.empty())
            this->buf.PushZero(); 
         if (this->buf[0].cLevels <= 0)
            this->buf[0].set_levels(this->value.levels, this->value.cLevels);
         this->buf[0] += val;
      }
      recent_dirty = true;
      return val; 
   }

   void AdvanceBy(int cSlots) { 
      if (cSlots <= 0) 
         return;
      this->buf.AdvanceBy(cSlots); 
      recent_dirty = true;
   }

   void UpdateRecent() {
      if (recent_dirty) {
         this->recent.Clear();
         for (int ix = 0; ix > (0 - this->buf.cItems); --ix)
            this->recent.Accumulate(this->buf[ix]);
         recent_dirty = false;
      }
   }

   void Publish(ClassAd & ad, const char * pattr, int flags) { 
      if ( ! flags) flags = this->PubDefault;
      if ((flags & IF_NONZERO) && this->value.cLevels <= 0) return;
      if (flags & this->PubValue) {
       	 std::string str;
         this->value.AppendToString(str);
         ClassAdAssign(ad, pattr, str); 
      }
      if (flags & this->PubRecent) {
         UpdateRecent();
       	 std::string str;
         this->recent.AppendToString(str);
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, str);
         else
            ClassAdAssign(ad, pattr, str); 
      }
      if (flags & this->PubDebug) {
         PublishDebug(ad, pattr, flags);
      }
   }

   void PublishDebug(ClassAd & ad, const char * pattr, int flags) const;

   T operator+=(T val) { return Add(val); }

   static const int unit = IS_HISTOGRAM | IS_RECENT | stats_entry_type<T>::id;
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_entry_recent_histogram<T>::AdvanceBy; };
};

//-----------------------------------------------------------------------------
// A statistics probe designed to keep track of accumulated running time
// of a data set.  keeps a count of times that time was added and
// a running total of time
//
class stats_recent_counter_timer : public stats_entry_base {
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
   time_t Add(time_t time)    { count += 1; runtime += double(time); return (time_t)runtime.value; }
   void Clear()              { count.Clear(); runtime.Clear();}
   void ClearRecent()        { count.ClearRecent(); runtime.ClearRecent(); }
   void AdvanceBy(int cSlots) { count.AdvanceBy(cSlots); runtime.AdvanceBy(cSlots); }
   void SetRecentMax(int cMax)    { count.SetRecentMax(cMax); runtime.SetRecentMax(cMax); }
   double operator+=(double val)    { return Add(val); }

   static const int PubValue = 1;     // publish overall count and runtime
   static const int PubRecent = 2;    // publish recnet count and runtime
   static const int PubDebug = 4;
   static const int PubCount = 0x10;  // modify PubValue & PubRecent to mean publish count only
   static const int PubRuntime = 0x20; // modify PubValue & PubRecent to meanpublish runtime only
   static const int PubDecorateAttr = 0x100;
   static const int PubValueAndRecent = PubValue | PubRecent | PubDecorateAttr;
   static const int PubDefault = PubValueAndRecent;
   void Publish(ClassAd & ad, const char * pattr, int flags) const;
   void PublishDebug(ClassAd & ad, const char * pattr, int flags) const;
   void Unpublish(ClassAd & ad, const char * pattr) const;

   // callback methods/fetchers for use by the StatisticsPool class
   static const int unit = IS_RCT | stats_entry_type<int>::id;
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_recent_counter_timer::AdvanceBy; };
   static FN_STATS_ENTRY_SETRECENTMAX GetFnSetRecentMax() { return (FN_STATS_ENTRY_SETRECENTMAX)&stats_recent_counter_timer::SetRecentMax; };
   static FN_STATS_ENTRY_UNPUBLISH GetFnUnpublish() { return (FN_STATS_ENTRY_UNPUBLISH)&stats_recent_counter_timer::Unpublish; };
   static void Delete(stats_recent_counter_timer * pthis);
};

//-----------------------------------------------------------------------------------
// a helper function for determining if enough time has passed so that we
// should Advance the recent buffers.  returns an Advance count that you
// should pass to the AdvancBy methods of your stats_entry_recent<T> counters
// or pass to the Advance() method of your StatisticsPool (which will pass it on to
// the counters).
//
int generic_stats_Tick(
   time_t now,              // In, if 0 time(NULL) is called inside generic_stats_Tick
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,  // in,out
   time_t & RecentTickTime,  // in,out
   time_t & Lifetime,        // in,out
   time_t & RecentLifetime); // in,out

// parse a configuration string in the form "ALL:opt, CAT:opt, ALT:opt"
// where opt can be one or more of 
//   0-3   verbosity level, 0 is least and 3 is most. default is usually 1
//   NONE  disable all 
//   ALL   enable all
//   R     enable Recent (default)
//   !R    disable Recent
//   D     enable Debug
//   !D    disable Debug (default)
//   Z     publish values even when stats pool has IF_NONZERO set and value is 0
//   !Z    honor the IF_NONZERO publishing option.
//   L     publish lifetime values (default)
//   !L    don't publish lifetime values
// 
// return value is the PublishFlags that should be passed in to StatisticsPool::Publish
// for this category.
int generic_stats_ParseConfigString(
   const char * pconfig, // name of the string parameter to read from the config file
   const char * ppool_name, // name of the stats pool/category of stats to look for 
   const char * ppool_alt,  // alternate name of the category to look for
   int          def_flags); // default value for publish flags for this pool

// the StatisticsPool class is used to hold a collection of statistics probes of various types
// probes in the pool can be Cleared, Advanced and Published as a group. 
//
// Probes may be created by the pool, in which case they will be deleted when the pool is 
// destroyed.  Or they can be created externally and added to the pool, in which case the
// creator is responsible for destroying them.  This allows probes to be defined as member
// variables to a class, then added to the pool to get the benefit of pool Publish, Advance
// and Clear methods.
//

class StatisticsPool {
public:
   StatisticsPool()
      {
      };
   ~StatisticsPool();

   // allocate a probe and insert it into the pool.
   //
   template <typename T> T* NewProbe(
      const char * name,       // unique name for the probe
      const char * pattr=NULL, // publish attribute name
      int          flags=0)    // flags to control publishing
   {
      T* probe = GetProbe<T>(name);
      if (probe)
         return probe;

      probe = new T();
      bool fOwnedByPool = true;
      InsertProbe(name, T::unit, (void*)probe, 
                  fOwnedByPool,
                  pattr ? strdup(pattr) : NULL, 
                  flags,
                  (FN_STATS_ENTRY_PUBLISH)&T::Publish,
                  (FN_STATS_ENTRY_UNPUBLISH)&T::Unpublish,
                  T::GetFnAdvance(), //(FN_STATS_ENTRY_ADVANCE)&T::AdvanceBy, 
                  (FN_STATS_ENTRY_CLEAR)&T::Clear,
                  T::GetFnSetRecentMax(), //(FN_STATS_ENTRY_SETRECENTMAX)&T::SetRecentMax,
                  (FN_STATS_ENTRY_DELETE)&T::Delete);
      return probe;
   }

   // lookup a probe by name
   //
   template <typename T> T* GetProbe(const char * name)
   {
      auto itr = zpub.find(name);
      if (itr != zpub.end())
         return (T*)itr->second.pitem;
      return 0;
   }

   stats_entry_base* GetProbe(const char * name, int & units)
   {
      auto itr = zpub.find(name);
      if (itr != zpub.end()) {
         units = itr->second.units;
         return (stats_entry_base*)itr->second.pitem;
      }
      return 0;
   }

   // add an externally created probe to the pool
   // so we can use pool functions to Advance/Clear/Publish
   //
   template <typename T> T* AddProbe (
      const char * name,       // unique name for the probe
      T*           probe,      // the probe, usually a member of a class/struct
      const char * pattr=NULL, // publish attribute name
      int          flags=0,    // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub=NULL,  // publish method
      FN_STATS_ENTRY_UNPUBLISH fnunp=NULL)
   {
      T* probeExist = GetProbe<T>(name);
      if (probeExist)
         return probeExist;

      bool fOwnedByPool = false;
      InsertProbe(name, T::unit, (void*)probe, 
                  fOwnedByPool,
                  pattr, 
                  flags,
                  fnpub ? fnpub : (FN_STATS_ENTRY_PUBLISH)&T::Publish,
                  fnunp ? fnunp : (FN_STATS_ENTRY_UNPUBLISH)&T::Unpublish,
                  T::GetFnAdvance(), //(FN_STATS_ENTRY_ADVANCE)&T::AdvanceBy, 
                  (FN_STATS_ENTRY_CLEAR)&T::Clear,
                  T::GetFnSetRecentMax(), //(FN_STATS_ENTRY_SETRECENTMAX)&T::SetRecentMax,
                  NULL);
      return probe;
   }

   // add an additional publishing entry for a probe that is already in the pool
   //
   template <typename T> T* AddPublish (
      const char * name,       // unique name for the probe
      T*           probe,      // the probe, usually a member of a class/struct
      const char * pattr,      // unique attr, must not be the same as a probe name.
      int          flags=0,    // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub=NULL, // publish method
      FN_STATS_ENTRY_UNPUBLISH fnunp=NULL) // unpublish method
   {
      T* probeExist = GetProbe<T>(name);
      if (probeExist)
         return probeExist;

      bool fOwnedByPool = false;
      InsertPublish(name, T::unit, (void*)probe, 
                    fOwnedByPool,
                    pattr, 
                    flags,
                    fnpub ? fnpub : (FN_STATS_ENTRY_PUBLISH)&T::Publish,
                    fnunp ? fnunp : (FN_STATS_ENTRY_UNPUBLISH)&T::Unpublish);
      return probe;
   }

   int RemoveProbe (const char * name); // remove from pool, will delete if owned by pool
   int RemoveProbesByAddress(void * first, void * last); // remove all probes that point to between first & last (inclusive)

   /* call this to set verbosites using a whitelist
    * probes that publish attributes that match the list are set to pub_flags,
    * if set_nonmatching is true, then probes that don't match the are set to nonmatch_pub_flags
    */
   int SetVerbosities(const char * attrs_list, int pub_flags, bool restore_nonmatching = false);
   int SetVerbosities(classad::References & attrs, int pub_flags, bool restore_nonmatching = false);

   /* tj: IMPLEMENT THIS
   double  SetSample(const char * probe_name, double sample);
   int     SetSample(const char * probe_name, int sample);
   int64_t SetSample(const char * probe_name, int64_t sample);
   */

   void Clear();
   void ClearRecent();
   void SetRecentMax(int window, int quantum);
   int  Advance(int cAdvance);
   void Publish(ClassAd & ad, int flags) const;
   void Publish(ClassAd & ad, const char * prefix, int flags) const;
   void Unpublish(ClassAd & ad) const;
   void Unpublish(ClassAd & ad, const char * prefix) const;

private:
   struct pubitem {
      int    units;    // copied from the class->unit, identifies the class and type of probe
      int    flags;    // passed to Publish
      bool   fOwnedByPool;
      bool   fWhitelisted;
      unsigned short def_verbosity;
      void * pitem;    // pointer to stats_entry_base derived class instance class/struct
      const char * pattr; // if non-null passed to Publish, if null name is passed.
      FN_STATS_ENTRY_PUBLISH Publish;
      FN_STATS_ENTRY_UNPUBLISH Unpublish;
   };
   struct poolitem {
      int units;
      int fOwnedByPool; // true if created and owned by this, otherise owned by some other code.
      FN_STATS_ENTRY_ADVANCE Advance;
      FN_STATS_ENTRY_CLEAR   Clear;
      FN_STATS_ENTRY_SETRECENTMAX SetRecentMax;
      FN_STATS_ENTRY_DELETE  Delete;
   };
   // table of values to publish, possibly more than one for each probe
   std::map<std::string, pubitem, std::less<>> zpub;

   // table of unique probes counters, used to Advance and Clear the items.
   std::map<void*,poolitem> pool;

   void InsertProbe (
      const char * name,       // unique name for the probe
      int          unit,       // identifies the probe class/type
      void*        probe,      // the probe, usually a member of a class/struct
      bool         fOwned,     // probe and pattr string are owned by the pool
      const char * pattr,      // publish attribute name
      int          flags,      // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub, // publish method
      FN_STATS_ENTRY_UNPUBLISH fnunp, // Unpublish method
      FN_STATS_ENTRY_ADVANCE fnadv, // Advance method
      FN_STATS_ENTRY_CLEAR   fnclr, // Clear method
      FN_STATS_ENTRY_SETRECENTMAX fnsrm,
      FN_STATS_ENTRY_DELETE  fndel); // static Delete method

   void InsertPublish (
      const char * name,       // unique name for the probe
      int          unit,       // identifies the probe class/type
      void*        probe,      // the probe, usually a member of a class/struct
      bool         fOwned,     // probe and pattr string are owned by the pool
      const char * pattr,      // publish attribute name
      int          flags,      // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub, // publish method
      FN_STATS_ENTRY_UNPUBLISH fnunp); // Unpublish method

};

// the macros help to add statistics probe defined as class or struct members to
// a StatisticsPool. use STATS_POOL_ADD or STATS_POOL_ADD_VAL to add a probe to the pool
// then use STATS_POOL_PUB_xxx to add additional Publish entries as needed.
#define STATS_POOL_ADD(pool,pre,name,as)        (pool).AddProbe(#name, &name, pre #name, as | name.PubDefault)
#define STATS_POOL_ADD_VAL(pool,pre,name,as)    (pool).AddProbe(#name, &name, pre #name, as | name.PubValue)
#define STATS_POOL_PUB_PEAK(pool,pre,name,as)   (pool).AddPublish(#name "Peak", &name, pre #name "Peak", as | name.PubLargest)
#define STATS_POOL_PUB_RECENT(pool,pre,name,as) (pool).AddPublish("Recent" #name, &name, "Recent" pre #name, IF_RECENTPUB | as | name.PubRecent)
#define STATS_POOL_PUB_DEBUG(pool,pre,name,as)  (pool).AddPublish(#name "Debug", &name, pre #name "Debug", IF_DEBUGPUB | as | name.PubDebug)
#define STATS_POOL_ADD_VAL_PUB_RECENT(pool,pre,name,as) \
   (pool).AddProbe(#name, &name, pre #name, as | name.PubValue); \
   (pool).AddPublish("Recent" #name, &name, "Recent" pre #name, IF_RECENTPUB | as | name.PubRecent)




#endif /* _GENERIC_STATS_H */
