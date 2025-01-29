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
#include <cctype>
#include "timed_queue.h"
#include "generic_stats.h"
#include "classad_helpers.h" // for canStringForUseAsAttr
#include "condor_config.h"

// specialize AdvanceBy for simple types so that we can use a more efficient algorithm.
template <> void stats_entry_recent<int>::AdvanceBy(int cSlots) { this->AdvanceAndSub(cSlots); }
template <> void stats_entry_recent<int64_t>::AdvanceBy(int cSlots) { this->AdvanceAndSub(cSlots); }
template <> void stats_entry_recent<double>::AdvanceBy(int cSlots) { this->AdvanceAndSub(cSlots); }

// specialization of PublishDebug to dump the internal data storage
// of stats_entry_recent. 
//
template <class T>
void stats_entry_recent<T>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   std::string str;
      
   str += std::to_string(this->value);
   str += " ";
   str += std::to_string(this->recent);
   formatstr_cat(str, " {h:%d c:%d m:%d a:%d}",
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         str += !ix ? "[" : (ix == this->buf.cMax ? "|" : ",");
         str += std::to_string(this->buf.pbuf[ix]);
         }
      str += "]";
      }

   std::string attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

template <>
void stats_entry_recent<int64_t>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   std::string str;
   str += std::to_string( (long)this->value );
   str += " ";
   str += std::to_string( (long)this->recent );
   formatstr_cat(str, " {h:%d c:%d m:%d a:%d}",
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         str += !ix ? "[" : (ix == this->buf.cMax ? "|" : ",");
         str += std::to_string( (long)this->buf.pbuf[ix] );
         }
      str += "]";
      }

   std::string attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

template <>
void stats_entry_recent<double>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   std::string str;
   formatstr_cat(str, "%g %g", this->value, this->recent);
   formatstr_cat(str, " {h:%d c:%d m:%d a:%d}",
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         formatstr_cat(str, !ix ? "[%g" : (ix == this->buf.cMax ? "|%g" : ",%g"), this->buf.pbuf[ix]);
         }
      str += "]";
      }

   std::string attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

template <class T>
void stats_entry_recent_histogram<T>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const {
   std::string str("(");
   this->value.AppendToString(str);
   str += ") (";
   this->recent.AppendToString(str);
   formatstr_cat(str, ") {h:%d c:%d m:%d a:%d}",
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         // Note: this is tediously broken up into multiple lines because clang produces a format string
         // warning otherwise.
         if (!ix) {
            formatstr_cat(str, "[(");
         } else if (ix == this->buf.cMax) {
            formatstr_cat(str, ")|(");
         } else {
            formatstr_cat(str, ") (");
            }
         this->buf.pbuf[ix].AppendToString(str);
         }
      str += ")]";
      }

   std::string attr(pattr);
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
   time_t now, // now==0 means call time(NULL) yourself
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,
   time_t & RecentTickTime,
   time_t & Lifetime,
   time_t & RecentLifetime)
{
   if ( ! now) now = time(NULL);

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
         cTicks = (int) (delta / RecentQuantum);
         RecentTickTime = now - (delta % RecentQuantum);
         }

      time_t recent_time = (int)(RecentLifetime + now - LastUpdateTime);
      RecentLifetime = (recent_time < RecentMaxTime) ? recent_time : RecentMaxTime;
      LastUpdateTime = now;
      }

   Lifetime = now - InitTime;
   return cTicks;
}

// parse a configuration string in the form "ALL:opt, CAT:opt, ALT:opt"
// where opt can be one or more of 
//   0-3   verbosity level, 0 is least and 3 is most. default is usually 1
//   NONE  disable all 
//   ALL   enable all
//   R     enable Recent (default)
//   !R    disable Recent
//   D     enable Debug
//   !D    disable Debug (default)
//   Z     don't publish values markerd IF_NONZERO when their value is 0
//   !Z    ignore IF_NONZERO publishing flag 
// 
// return value is the PublishFlags that should be passed in to StatisticsPool::Publish
// for this category.
//
int generic_stats_ParseConfigString(
   const char * config, // name of the string parameter to read from the config file
   const char * pool_name, // name of the stats pool/category of stats to look for 
   const char * pool_alt,  // alternate name of the category to look for
   int          flags_def)  // default value for publish flags for this pool
{
    // special case, if there is no string, or the string is just "default", then
    // return the default flags
    if ( ! config || MATCH == strcasecmp(config,"DEFAULT"))
       return flags_def;

    // special case, if the string is empty, or the string is just "none", then
    // return 0 (disable all)
    if ( ! config[0] || MATCH == strcasecmp(config,"NONE"))
       return 0;

    // if the config string is non-trivial, then it must contain either our pool_name
    // or pool_alt or "DEFAULT" or "ALL" or we do not publish this pool.
    int PublishFlags = 0;

    // walk the list, looking for items that match our pool name or the keyword DEFAULT or ALL
    //
    // tokenize the list on , or space
    for (auto& item : StringTokenIterator(config)) {
       const char * p = item.c_str();

       int flags = PublishFlags;
       const char * psep = strchr(p,':');
       if (psep) {
          size_t cch = psep - p;
          char sz[64];
          if (cch >= COUNTOF(sz)) 
             continue;
          strncpy(sz, p, cch);
          sz[cch] = 0;
          if (strcasecmp(sz,pool_name) && strcasecmp(sz,pool_alt) && strcasecmp(sz,"DEFAULT") && strcasecmp(sz,"ALL"))
             continue;
       } else {
          if (strcasecmp(p,pool_name) && strcasecmp(p,pool_alt) && strcasecmp(p,"DEFAULT") && strcasecmp(p,"ALL"))
             continue;
       }

       // if we get to here, we found our pool name or "DEFAULT" or "ALL"
       // so we begin with our default flags
       flags = flags_def;

       // if there are any options, then parse them and modify the flags
       if (psep) {
          const char * popt = psep+1;
          if (MATCH == strcasecmp(popt,"NONE")) {
             flags = 0;
          } else {
             bool bang = false;
             const char * parse_error = NULL;
             while (popt[0]) {
                char ch = popt[0];
                if (ch >= '0' && ch <= '3') {
                   int level = (atoi(popt) * IF_BASICPUB) & IF_PUBLEVEL;
                   flags = (flags & ~IF_PUBLEVEL) | level;
                } else if (ch == '!') {
                   bang = true;
                } else if (ch == 'd' || ch == 'D') {
                   flags = bang ? (flags & ~IF_DEBUGPUB) : (flags | IF_DEBUGPUB);
                } else if (ch == 'r' || ch == 'R') {
                   flags = bang ? (flags & ~IF_RECENTPUB) : (flags | IF_RECENTPUB);
                } else if (ch == 'z' || ch == 'Z') {
                   flags = bang ? (flags & ~IF_NONZERO) : (flags | IF_NONZERO);
                } else if (ch == 'l' || ch == 'L') {
                   flags = bang ? (flags | IF_NOLIFETIME) : (flags & ~IF_NOLIFETIME);
                } else {
                   if ( ! parse_error) parse_error = popt;
                }
                ++popt;
             }

             if (parse_error) {
                dprintf(D_ALWAYS, "Option '%s' invalid in '%s' when parsing statistics to publish. effect is %08X\n",
                        parse_error, p, flags);
             }
          }
       }

       PublishFlags = flags;
       dprintf(D_FULLDEBUG, "'%s' gives flags %08X for %s statistics\n", p, PublishFlags, pool_name);
    }

    return PublishFlags;
}

//----------------------------------------------------------------------------------------------
//
void stats_recent_counter_timer::Delete(stats_recent_counter_timer * probe) {
   delete probe;
}

void stats_recent_counter_timer::Unpublish(ClassAd & ad, const char * pattr) const
{
   ad.Delete(pattr);
   std::string attr;
   formatstr(attr, "Recent%s",pattr);
   ad.Delete(attr);
   formatstr(attr, "Recent%sRuntime",pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6); // +6 to skip "Recent" prefix
}

void stats_recent_counter_timer::Publish(ClassAd & ad, const char * pattr, int flags) const
{
   if ((flags & IF_NONZERO) && (this->count.value == 0) && (this->count.recent == 0))
      return;

   std::string attr(pattr);
   std::string attrR("Recent");
   attrR += pattr;

   ClassAdAssign(ad, attr.c_str(), this->count.value);
   ClassAdAssign(ad, attrR.c_str(), this->count.recent);

   attr += "Runtime";
   attrR += "Runtime";
   ClassAdAssign(ad, attr.c_str(), this->runtime.value);
   ClassAdAssign(ad, attrR.c_str(), this->runtime.recent);

}

void stats_recent_counter_timer::PublishDebug(ClassAd & ad, const char * pattr, int flags) const
{
   if ( ! IsValidAttrName(pattr))
      return;

   this->count.PublishDebug(ad, pattr, flags);

   std::string attr(pattr);
   attr += "Runtime";
   this->runtime.PublishDebug(ad, attr.c_str(), flags);
}

template <class T>
void stats_entry_probe<T>::Publish(ClassAd & ad, const char * pattr, int flags) const
{
   // value holds the count of samples.
   if ((flags & IF_NONZERO) && (this->value >= 0.0 && this->value <= 0.0))
      return;

   std::string base(pattr);
   std::string attr;

   // the IF_RT_SUM flag modifies the naming pattern for Miron probes so that the Sum is labeled
   // as accumulated runtime and the count of samples has no suffix.  This makes them interchangeable
   // with stats_recent_counter_timer probes.
   if (flags & IF_RT_SUM) {
      ad.Assign(base, (long long)this->value);
      base += "Runtime";
      ad.Assign(base, this->Sum);
   } else {
      // the default output for Miron probes is to use a std suffix for each bit of info.
      attr = base; attr += "Count";
      ad.Assign(attr, this->Count());
      attr = base; attr += "Sum";
      ad.Assign(attr, this->Sum);
   }
   if (this->Count() > 0 || ((flags & IF_PUBLEVEL) == IF_HYPERPUB)) {
      attr = base; attr += "Avg";
      ad.Assign(attr, this->Avg());

      attr = base; attr += "Min";
      ad.Assign(attr, this->Min);

      attr = base; attr += "Max";
      ad.Assign(attr, this->Max);

      attr = base; attr += "Std";
      ad.Assign(attr, this->Std());
   }
}

template <class T>
void stats_entry_probe<T>::Unpublish(ClassAd & /*ad*/, const char * /*pattr*/) const
{
   /*
   std::string attr;
   std::string base(pattr);
   if (flags & IF_RT_SUM) {
      ad.Delete(base);
      base += "Runtime";
      ad.Delete(base);
   } else {
      attr = base; attr += "Count";
      ad.Delete(attr);
      attr = base; attr += "Sum";
      ad.Delete(attr);
   }
   attr = base; attr += "Avg";
   ad.Delete(attr);
   attr = base; attr += "Min";
   ad.Delete(attr);
   attr = base; attr += "Max";
   ad.Delete(attr);
   attr = base; attr += "Std";
   ad.Delete(attr);
   */
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


void ProbeToStringDebug(std::string & str, const Probe& probe)
{
   formatstr(str, "%d M:%g m:%g S:%g s2:%g",
             probe.Count, probe.Max, probe.Min, probe.Sum, probe.SumSq);
}

int ClassAdAssign(ClassAd & ad, const char * pattr, const Probe& probe) 
{
   std::string attr;
   formatstr(attr, "%sCount", pattr);
   ad.Assign(attr, probe.Count);

   formatstr(attr, "%sSum", pattr);
   int ret = ad.Assign(attr, probe.Sum);

   if (probe.Count > 0)
      {
      formatstr(attr, "%sAvg", pattr);
      ad.Assign(attr, probe.Avg());

      formatstr(attr, "%sMin", pattr);
      ad.Assign(attr, probe.Min);

      formatstr(attr, "%sMax", pattr);
      ad.Assign(attr, probe.Max);

      formatstr(attr, "%sStd", pattr);
      ad.Assign(attr, probe.Std());
      }
   return ret;
}

int ClassAdAssign(ClassAd & ad, const char * pattr, const Probe& probe, int DetailMode, bool if_nonzero)
{
   if ( ! DetailMode) {	 // ProbeDetailMode_Normal
      return ClassAdAssign(ad, pattr, probe);
   }

   int ret = -1;

   std::string attr;
   if (DetailMode == ProbeDetailMode_Brief) {
      // for Brief, publish the Avg without a suffix, and the Min & Max using their normal names
      double avg = probe.Avg();
      ret = ad.Assign(pattr, avg);
      double val = MIN(avg, probe.Min);
      if ( ! if_nonzero || val > 0.0 || val < 0.0) {
         formatstr(attr, "%sMin", pattr);
         ad.Assign(attr, val);
      }
      val = MAX(avg, probe.Max);
      if ( ! if_nonzero || val > 0.0 || val < 0.0) {
         formatstr(attr, "%sMax", pattr);
         ad.Assign(attr, val);
      }
   } else if (DetailMode == ProbeDetailMode_RT_SUM) {
      // for RT_SUM, publish the count without a suffix and the Sum as "Runtime"
      // this gives sort of probe is useful for function/process timings.
      ret = ad.Assign(pattr, probe.Count);
      formatstr(attr, "%sRuntime", pattr);
      ad.Assign(attr, probe.Sum);
   } else if (DetailMode == ProbeDetailMode_Tot) {
      // for Totals, publish the Sum without a suffix
      ret = ad.Assign(pattr, (long long)probe.Sum);
   } else if (DetailMode == ProbeDetailMode_CAMM) {
      // for CAMM, publish the Avg and the Min & Max and Count using their normal names
      // publish Avg, Min and Max only if count is non-zero
      formatstr(attr, "%sCount", pattr);
      ret = ad.Assign(attr, probe.Count);

      if (probe.Count != 0) {
         formatstr(attr, "%sAvg", pattr);
         ad.Assign(attr, probe.Avg());

         formatstr(attr, "%sMin", pattr);
         ad.Assign(attr, probe.Min);

         formatstr(attr, "%sMax", pattr);
         ad.Assign(attr, probe.Max);
      }
   }
   return ret;
}

template <> void stats_entry_recent<Probe>::Unpublish(ClassAd& ad, const char * pattr) const
{
   std::string attr;
   ad.Delete(pattr);
   formatstr(attr, "Recent%s", pattr);
   ad.Delete(attr);

   formatstr(attr, "Recent%sCount", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
   formatstr(attr, "Recent%sSum", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
   formatstr(attr, "Recent%sAvg", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
   formatstr(attr, "Recent%sMin", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
   formatstr(attr, "Recent%sMax", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
   formatstr(attr, "Recent%sStd", pattr);
   ad.Delete(attr);
   ad.Delete(attr.c_str()+6);
}

template <> void stats_entry_recent<Probe>::Publish(ClassAd& ad, const char * pattr, int flags) const
{
   if ( ! flags) flags = PubDefault;
   if ((flags & IF_NONZERO) && this->value.Count == 0) return;

   if ( !(flags & this->PubDetailMask) && (flags & IF_PUBLEVEL) <= IF_BASICPUB) {
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value.Avg());
      if (flags & this->PubRecent) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, this->recent.Avg());
         else
            ClassAdAssign(ad, pattr, this->recent.Avg());
      }
      return;
   }

   ClassAdAssign(ad, pattr, this->value, flags & this->PubDetailMask, flags & IF_NONZERO);

   if (flags & this->PubRecent) {
      std::string attr(pattr);
      if (flags & this->PubDecorateAttr) {
         formatstr(attr, "Recent%s", pattr);
      }
      ClassAdAssign(ad, attr.c_str(), recent, flags & this->PubDetailMask, flags & IF_NONZERO);
   }
}

template <>
void stats_entry_recent<Probe>::PublishDebug(ClassAd & ad, const char * pattr, int flags) const
{
   std::string str;
   std::string var1;
   std::string var2;
   ProbeToStringDebug(var1, this->value);
   ProbeToStringDebug(var2, this->recent);

   formatstr_cat(str, "(%s) (%s)", var1.c_str(), var2.c_str());
   formatstr_cat(str, " {h:%d c:%d m:%d a:%d}",
                   this->buf.ixHead, this->buf.cItems, this->buf.cMax, this->buf.cAlloc);
   if (this->buf.pbuf) {
      for (int ix = 0; ix < this->buf.cAlloc; ++ix) {
         ProbeToStringDebug(var1, this->buf.pbuf[ix]);
         formatstr_cat(str, !ix ? "[%s" : (ix == this->buf.cMax ? "|%s" : ",%s"), var1.c_str());
         }
      str += "]";
      }

   std::string attr(pattr);
   if (flags & this->PubDecorateAttr)
      attr += "Debug";

   ad.Assign(pattr, str);
}

#include "utc_time.h"
void TestProbe()
{
   struct {
      stats_entry_recent<Probe> Runtime;
   } stats;

   stats.Runtime.SetRecentMax(5);

   double runtime = _condor_debug_get_time_double();

   sleep(2);
   double now = _condor_debug_get_time_double();
   stats.Runtime += (now - runtime);
   now = runtime;

   stats.Runtime.AdvanceBy(1);
}

#if 0
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
       #ifdef EXCEPT
		EXCEPT("attempt to add histogram of %d items to histogram of %d items", 
				sh.cLevels, this->cLevels);
       #else
        return *this;
       #endif
	}

	if (this->levels != sh.levels) {
       #ifdef EXCEPT
		EXCEPT("Histogram level pointers are not the same.");
       #else
        return *this;
       #endif
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
#endif

#define is_space(ch) isspace((unsigned char)(ch))
#define is_digit(ch) isdigit((unsigned char)(ch))

// Parse a string of the form "NNN Kb, NNN Mb" and return an array of sizes in bytes
// the valid scaling factors are b B Kb KB Mb MB Gb GB Tb TB
// commas and the scaling factors (Kb, Mb, etc) are parsed, but not required. 
// return value is number of entries needed in pSizes array to fully parse. 
//
int stats_histogram_ParseSizes(
   const char * psz, // in: string to parse
   int64_t * pSizes, // out: parsed values
   int cMaxSizes)    // in: size of pSizes array.
{
   int cSizes = 0;
   int64_t size = 0;
   for (const char* p = psz; p && *p; ++p) {

      while (is_space(*p))
         ++p;

      if ( ! is_digit(*p)) {
         EXCEPT("Invalid input to ParseSizes at offset %d in '%s'", (int)(p-psz), psz);
         break;
      }

      bool saw_digit = false;
      while (is_digit(*p)) {
         saw_digit = true;
         size *= 10;
         size += *p - '0';
         ++p;
      }

      if (saw_digit) {

         while (is_space(*p))
            ++p;

         int64_t scale = 1;
         if (*p == 'K') ++p, scale = 1024;
         else if (*p == 'M') ++p, scale = 1024*1024;
         else if (*p == 'G') ++p, scale = 1024*1024*1024;
         else if (*p == 'T') ++p, scale = (int64_t)1024*1024*1024*1024;
         if (*p == 'b' || *p == 'B') ++p;

         while (is_space(*p))
            ++p;

         if (*p == ',') ++p;

         if (cSizes < cMaxSizes)
            pSizes[cSizes] = size * scale;

         ++cSizes;
         size = 0;
      }

      while (is_space(*p))
         ++p;
   }

   return cSizes; 
}

void stats_histogram_PrintSizes(std::string &  /*str*/, const  int64_t *  /*pSizes*/, int  /*cSizes*/)
{
   EXCEPT("stats_histogram::PrintSizes not implemented");
   // tj: WRITE THIS
}

// Parse a string of the form "NNN Sec, NNN Min" and return an array of times in seconds
// the valid scaling factors are S Sec M Min H HR D Day (case insensitive)
// commas and the scaling factors (Sec, Min etc) are parsed, but not required. 
// note that Sec is a scaling factor of 1 
// return value is size of pTimes array necessary to parse all of the input.
//
int stats_histogram_ParseTimes(
   const char * psz, // in: string to parse
   time_t * pTimes,  // out: parsed values
   int cMaxTimes)    // in: size of pTimes array
{
   int cTimes = 0;
   time_t time = 0;
   for (const char* p = psz; p && *p; ++p) {

      while (is_space(*p))
         ++p;

      if ( ! is_digit(*p)) {
         EXCEPT("Invalid input to ParseTimes at offset %d in '%s'", (int)(p-psz), psz);
         break;
      }

      bool saw_digit = false;
      while (is_digit(*p)) {
         saw_digit = true;
         time *= 10;
         time += *p - '0';
         ++p;
      }

      if (saw_digit) {

         while (is_space(*p))
            ++p;

         time_t scale = 1;
         if (toupper(*p) == 'S') {
            scale = 1;
            ++p;
            if (toupper(*p) == 'E') {
               ++p; if (toupper(*p) == 'C') ++p;
            }
         } else if (toupper(*p) == 'M') {
            scale = 60;
            ++p;
            if (toupper(*p) == 'I') {
               ++p; if (toupper(*p) == 'N') ++p;
            }
         } else if (toupper(*p) == 'H') {
            scale = 60*60;
            ++p;
            if (toupper(*p) == 'R') ++p;
         } else if (toupper(*p) == 'D') { 
            scale = 24*60*60;
            if (toupper(*p) == 'A') {
               ++p; if (toupper(*p) == 'Y') ++p;
            }
         }

         while (is_space(*p))
            ++p;

         if (*p == ',') ++p;

         if (cTimes < cMaxTimes)
            pTimes[cTimes] = time * scale;

         ++cTimes;
         time = 0;
      }

      while (is_space(*p))
         ++p;
   }

   return cTimes; 
}

void stats_histogram_times_PrintTimes(std::string &  /*str*/, const time_t *  /*pTimes*/, int /*cTimes*/)
{
   EXCEPT("stats_histogram::PrintTimes not implemented");
   // tj: WRITE THIS
}

/*
int stats_histogram_double::ParseLimits(const char * psz, double * pLimits, int cMax)
{
   // tj: WRITE THIS
   return 0; 
}

void stats_histogram_sizes::PrintLimits(std::string & str, const double * pLimits, int cLimits)
{
   // tj: WRITE THIS
}

int stats_histogram_int::ParseLimits(const char * psz, int * pLimits, int cMax)
{
   // tj: WRITE THIS
   return 0; 
}

void stats_histogram_int::PrintLimits(std::string & str, const int * pLimits, int cTimes)
{
   // tj: WRITE THIS
}
*/

//----------------------------------------------------------------------------------------------
// methods for the StatisticsPool class.  StatisticsPool is a collection of statistics that 
// share a recent time quantum and are intended to be published/Advanced/Cleared
// together.
//
StatisticsPool::~StatisticsPool()
{
   // first delete all of the publish entries.
   for (auto& [name, item] : zpub)
      {
      if (item.fOwnedByPool && item.pattr)
         free((void*)(const_cast<char*>(item.pattr)));
      }
   zpub.clear();

   // then all of the probes.
   for (auto& [probe, pi] : pool)
      {
      if (pi.Delete)
         pi.Delete(probe);
      }
   pool.clear();
}

int StatisticsPool::RemoveProbe (const char * name)
{
   auto zpub_itr = zpub.find(name);
   if (zpub_itr == zpub.end())
      return 0;
   pubitem item = zpub_itr->second;
   zpub.erase(zpub_itr);

   void * probe = item.pitem;
   bool fOwnedByPool = item.fOwnedByPool;
   if (fOwnedByPool) {
      if (item.pattr) free((void*)(const_cast<char*>(item.pattr)));
   }

   // remove the probe from the pool (if it's still there)
   auto pool_itr = pool.find(probe);
   if (pool_itr != pool.end()) {
      if (pool_itr->second.Delete) {
         pool_itr->second.Delete(probe);
      }
      pool.erase(pool_itr);
   }

   // clear out any dangling references to the probe in the pub list.
   // 
   /*
   StatisticsPool * pthis = const_cast<StatisticsPool*>(this);
   std::string key;
   pthis->pub.startIterations();
   while (pthis->pub.iterate(key,item)) {
      if (item.pitem == probe)
         pthis->pub.remove(key);
   }

   if (fOwnedByPool) {
      delete pitem;
   }
   */
   return 0;
}

int StatisticsPool::RemoveProbesByAddress(void* first, void *last)
{
   int cRemoved = 0;

   // first remove from the pub list
   auto zpub_itr = zpub.begin();
   while (zpub_itr != zpub.end()) {
      if (zpub_itr->second.pitem < first || zpub_itr->second.pitem > last) {
         zpub_itr++;
      } else {
         zpub_itr = zpub.erase(zpub_itr);
      }
   }

   // then remove from the pool
   auto pool_itr = pool.begin();
   while (pool_itr != pool.end()) {
      auto probe = pool_itr->first;
	  const auto &item2 = pool_itr->second;
      if (probe < first || probe > last) {
         pool_itr++;
	  } else {
         ASSERT (!item2.fOwnedByPool);
         if (item2.Delete) { item2.Delete(probe); }

         pool_itr = pool.erase(pool_itr);
         ++cRemoved;
      }
   }

   return cRemoved;
}

void StatisticsPool::InsertProbe (
   const char * name,       // unique name for the probe
   int          unit,       // identifies the probe class/type
   void*        probe,      // the probe, usually a member of a class/struct
   bool         fOwned,     // probe and pattr string are owned by the pool
   const char * pattr,      // publish attribute name
   int          flags,      // flags to control publishing
   FN_STATS_ENTRY_PUBLISH fnpub, // publish method
   FN_STATS_ENTRY_UNPUBLISH fnunp, // unpublish method
   FN_STATS_ENTRY_ADVANCE fnadv, // Advance method
   FN_STATS_ENTRY_CLEAR   fnclr,  // Clear method
   FN_STATS_ENTRY_SETRECENTMAX fnsrm,
   FN_STATS_ENTRY_DELETE  fndel) // Destructor
{
   pubitem item = { unit, flags, fOwned, false, 0, probe, pattr, fnpub, fnunp };
   zpub[name] = item;

   poolitem pi = { unit, fOwned, fnadv, fnclr, fnsrm, fndel };
   pool[probe] = pi;
}

void StatisticsPool::InsertPublish (
   const char * name,       // unique name for the probe
   int          unit,       // identifies the probe class/type
   void*        probe,      // the probe, usually a member of a class/struct
   bool         fOwned,     // probe and pattr string are owned by the pool
   const char * pattr,      // publish attribute name
   int          flags,      // flags to control publishing
   FN_STATS_ENTRY_PUBLISH fnpub, // publish method
   FN_STATS_ENTRY_UNPUBLISH fnunp) // unpublish method
{
   pubitem item = { unit, flags, fOwned, false, 0, probe, pattr, fnpub, fnunp };
   zpub[name] = item;
}

/* tj: IMPLEMENT THIS
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
*/

int StatisticsPool::Advance(int cAdvance)
{
   if (cAdvance <= 0)
      return cAdvance;

   for (auto& [pitem, item] : pool)
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
   for (auto& [pitem, item] : pool)
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
   int cRecent = (quantum > 0) ? (window / quantum) : window;

   for (auto& [pitem, item] : pool)
      {
      if (pitem && item.SetRecentMax) {
         stats_entry_base * probe = (stats_entry_base *)pitem;
         (probe->*(item.SetRecentMax))(cRecent);
         }
      }
}

/* set probe verbosites using a whitelist as a comma, and/or space separated list of attributes.
 * probes that publish attributes that match the list are set to pub_flags,
 * if set_nonmatching is true, then probes that don't match the are set to nonmatch_pub_flags
 */
int StatisticsPool::SetVerbosities(const char * attrs_list, int pub_flags, bool restore_nonmatching /*= false*/)
{
   if ( ! attrs_list || ! attrs_list[0]) return 0;

   classad::References attrs;
   StringTokenIterator list(attrs_list);
   const std::string * attr;
   while ((attr = list.next_string())) { attrs.insert(*attr); }
   return SetVerbosities(attrs, pub_flags, restore_nonmatching);
}

/* call this to set verbosites using a whitelist.
 * probes that publish attributes that match the list are set to pub_flags,
 * if set_nonmatching is true, then probes that don't match the are set to nonmatch_pub_flags
 */
int StatisticsPool::SetVerbosities(classad::References & attrs, int pub_flags, bool restore_nonmatching /*= false*/)
{
   ClassAd tmp;

   for (auto& [name, item] : zpub) {
      if ( ! item.Publish) continue;

      // if the base attribute name matches, the it's a match
      const char * pattr = item.pattr ? item.pattr : name.c_str();
      bool attr_match = attrs.find(pattr) != attrs.end();

      // if this is a multi-attribute probe we have to check against actual
      // publication attributes to be sure that it's not a match.
      int probe_class = item.units & IS_CLASS_MASK;
      bool multi_attrib = (probe_class == IS_CLS_PROBE) || (probe_class > IS_RECENTTQ);
      if ( ! attr_match && multi_attrib) {
         // publish attribute so we can match them against the whitelist
         tmp.Clear();
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Publish))(tmp, pattr, (item.flags & ~IF_NONZERO) | IF_HYPERPUB);

         // look to see if any of the published attributes match the whitelist.
         for (classad::AttrList::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
            if (attrs.find(it->first) != attrs.end()) { attr_match = true; break; }
         }
      }

      // if we have a match, adjust the pub flags
      if (attr_match) {
         int new_flags = (item.flags & ~IF_PUBLEVEL) | (pub_flags & IF_PUBLEVEL);
         if ( ! item.fWhitelisted && (new_flags != item.flags)) {
            item.fWhitelisted = true;
            item.def_verbosity = (unsigned short)(item.flags >> 16);
         }
         item.flags = new_flags;
      } else if (restore_nonmatching && item.fWhitelisted) {
         item.flags = (item.flags & ~IF_PUBLEVEL) | (((unsigned int)item.def_verbosity << 16) & IF_PUBLEVEL);
         item.fWhitelisted = false;
      }
   }
   return 0;
}


void StatisticsPool::Publish(ClassAd & ad, int flags) const
{
   for (auto [name, item] : zpub)
      {
      // check various publishing flags to decide whether to call the Publish method
      if (!(flags & IF_DEBUGPUB) && (item.flags & IF_DEBUGPUB)) continue;
      if (!(flags & IF_RECENTPUB) && (item.flags & IF_RECENTPUB)) continue;
      if ((flags & IF_PUBKIND) && (item.flags & IF_PUBKIND) && !(flags & item.flags & IF_PUBKIND)) continue;
      if ((item.flags & IF_PUBLEVEL) > (flags & IF_PUBLEVEL)) continue;

      // don't pass the item's IF_NONZERO flag through unless IF_NONZERO is enabled
      int item_flags = (flags & IF_NONZERO) ? item.flags : (item.flags & ~IF_NONZERO);

      if (item.Publish) {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Publish))(ad, item.pattr ? item.pattr : name.c_str(), item_flags);
         }
      }
}

void StatisticsPool::Publish(ClassAd & ad, const char * prefix, int flags) const
{
   for (auto& [name, item] : zpub)
      {
      // check various publishing flags to decide whether to call the Publish method
      if (!(flags & IF_DEBUGPUB) && (item.flags & IF_DEBUGPUB)) continue;
      if (!(flags & IF_RECENTPUB) && (item.flags & IF_RECENTPUB)) continue;
      if ((flags & IF_PUBKIND) && (item.flags & IF_PUBKIND) && !(flags & item.flags & IF_PUBKIND)) continue;
      if ((item.flags & IF_PUBLEVEL) > (flags & IF_PUBLEVEL)) continue;

      // don't pass the item's IF_NONZERO flag through unless IF_NONZERO is enabled
      int item_flags = (flags & IF_NONZERO) ? item.flags : (item.flags & ~IF_NONZERO);

      if (item.Publish) {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         std::string attr(prefix);
         attr += (item.pattr ? item.pattr : name.c_str());
         (probe->*(item.Publish))(ad, attr.c_str(), item_flags);
         }
      }
}

void StatisticsPool::Unpublish(ClassAd & ad) const
{
   for (auto& [name, item] : zpub)
      {
      const char * pattr = item.pattr ? item.pattr : name.c_str();
      if (item.Unpublish) 
         {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Unpublish))(ad, pattr);
         }
      else
         ad.Delete(pattr);
      }
}

void StatisticsPool::Unpublish(ClassAd & ad, const char * prefix) const
{
   for (auto& [name, item] : zpub)
      {
      std::string attr(prefix);
      attr += (item.pattr ? item.pattr : name.c_str());
      if (item.Unpublish) 
         {
         stats_entry_base * probe = (stats_entry_base *)item.pitem;
         (probe->*(item.Unpublish))(ad, attr.c_str());
         }
      else
         ad.Delete(attr);
      }
}

bool
stats_ema_config::sameAs( stats_ema_config const *other )
{
	if( !other ) {
		return false;
	}

	horizon_config_list::const_iterator my_itr = horizons.begin();
	horizon_config_list::const_iterator other_itr = other->horizons.begin();
	while( my_itr != horizons.end() || other_itr != other->horizons.end() ) {
		if( my_itr == horizons.end() || other_itr == other->horizons.end() ) {
			return false; // unequal list length
		}
		if( my_itr->horizon != other_itr->horizon ) {
			return false;
		}
		++my_itr;
		++other_itr;
	}
	return true;
}

template <class T>
void stats_entry_ema_base<T>::ConfigureEMAHorizons(std::shared_ptr<stats_ema_config> new_config) {
	std::shared_ptr<stats_ema_config> old_config(ema_config);
	ema_config = new_config;

	if( new_config->sameAs(old_config.get()) ) {
		return;
	}

	stats_ema_list old_ema = ema;
	ema.clear();
	ema.resize(new_config->horizons.size());

	for(size_t new_idx = new_config->horizons.size(); new_idx--; ) {
		if( old_config.get() ) {
			for(size_t old_idx = old_config->horizons.size(); old_idx--; ) {
				if( old_config->horizons[old_idx].horizon == new_config->horizons[new_idx].horizon ) {
					ema[new_idx] = old_ema[old_idx];
					break;
				}
			}
		}
	}
}

void
stats_ema_config::add(time_t horizon,char const *horizon_name)
{
	horizons.push_back( horizon_config(horizon,horizon_name) );
}

bool ParseEMAHorizonConfiguration(char const *ema_conf,std::shared_ptr<stats_ema_config> &ema_horizons,std::string &error_str) {
		// expected format of ema_conf:
		// "name1:horizon1 name2:horizon2 ..."
		// Example: "1m:60 1h:3600 1d:86400"

	ASSERT( ema_conf );
	ema_horizons = std::make_shared<stats_ema_config>();

	while( *ema_conf ) {
		while( is_space(*ema_conf) || *ema_conf == ',' ) ema_conf++;
		if( *ema_conf == '\0' ) break;

		char const *colon = strchr(ema_conf,':');
		if( !colon ) {
			error_str = "expecting NAME1:SECONDS1 NAME2:SECONDS2 ...";
			return false;
		}
		std::string horizon_name;
		horizon_name.append(ema_conf,colon-ema_conf);
		char *horizon_end=NULL;
		time_t horizon = (time_t)strtol(colon+1,&horizon_end,10);
		if( horizon_end == colon+1 || (!is_space(*horizon_end) && *horizon_end != ',' && *horizon_end) ) {
			error_str = "expecting NAME1:SECONDS1 NAME2:SECONDS2 ...";
			return false;
		}
		ema_horizons->add(horizon,horizon_name.c_str());

		ema_conf = horizon_end;
	}
	return true;
}

template <class T>
void stats_entry_sum_ema_rate<T>::Publish(ClassAd & ad, const char * pattr, int flags) const { 
	if ( ! flags) flags = PubDefault;
	if (flags & this->PubValue) {
		ClassAdAssign(ad, pattr, this->value);
	}
	if (flags & this->PubEMA) {
		for(size_t i = this->ema.size(); i--; ) {
			stats_ema_config::horizon_config &config = this->ema_config->horizons[i];
			if( (flags & stats_entry_ema_base<T>::PubSuppressInsufficientDataEMA) && this->ema[i].insufficientData(config)
				&& ((flags & IF_PUBLEVEL) < IF_HYPERPUB)) {
				continue;
			}
			if( !(flags & this->PubDecorateAttr) ) {
				ClassAdAssign(ad, pattr, this->ema[i].ema);
			}
			else {
				std::string attr_name;
				size_t pattr_len;
				if( (flags & this->PubDecorateLoadAttr) && (pattr_len=strlen(pattr)) >= 7 && strcmp(pattr+pattr_len-7,"Seconds")==0 ) {
						// Instead of reporting BlahSecondsPerSecond, report BlahLoad
					formatstr(attr_name,"%.*sLoad_%s",(int)(pattr_len-7),pattr,config.horizon_name.c_str());
				}
				else {
					formatstr(attr_name,"%sPerSecond_%s",pattr,config.horizon_name.c_str());
				}
				ClassAdAssign(ad, attr_name.c_str(), this->ema[i].ema);
			}
		}
	}
}

template <class T>
void stats_entry_sum_ema_rate<T>::Unpublish(ClassAd & ad, const char * pattr) const {
	ad.Delete(pattr);
	for(size_t i=this->ema.size(); i--;) {
		stats_ema_config::horizon_config &config = this->ema_config->horizons[i];
		std::string attr_name;
		size_t pattr_len;
		if( (pattr_len=strlen(pattr)) >= 7 && strcmp(pattr+pattr_len-7,"Seconds")==0 ) {
			formatstr(attr_name,"%.*sLoad_%s",(int)(pattr_len-7),pattr,config.horizon_name.c_str());
		}
		else {
			formatstr(attr_name,"%sPerSecond_%s",pattr,config.horizon_name.c_str());
		}
		ad.Delete(attr_name);
	}
}

template <class T>
void stats_entry_ema<T>::Publish(ClassAd & ad, const char * pattr, int flags) const { 
	if ( ! flags) flags = PubDefault;
	if (flags & this->PubValue) {
		ClassAdAssign(ad, pattr, this->value);
	}
	if (flags & this->PubEMA) {
		for(size_t i = this->ema.size(); i--; ) {
			stats_ema_config::horizon_config &config = this->ema_config->horizons[i];
			if( (flags & stats_entry_ema_base<T>::PubSuppressInsufficientDataEMA) && this->ema[i].insufficientData(config) 
				&& ((flags & IF_PUBLEVEL) < IF_HYPERPUB)) {
				continue;
			}
			if( !(flags & this->PubDecorateAttr) ) {
				ClassAdAssign(ad, pattr, this->ema[i].ema);
			}
			else {
				std::string attr_name;
				formatstr(attr_name,"%s_%s",pattr,config.horizon_name.c_str());
				ClassAdAssign(ad, attr_name.c_str(), this->ema[i].ema);
			}
		}
	}
}

template <class T>
void stats_entry_ema<T>::Unpublish(ClassAd & ad, const char * pattr) const {
	ad.Delete(pattr);
	for(size_t i=this->ema.size(); i--;) {
		stats_ema_config::horizon_config &config = this->ema_config->horizons[i];
		std::string attr_name;
		formatstr(attr_name,"%s_%s",pattr,config.horizon_name.c_str());
		ad.Delete(attr_name);
	}
}

template <class T>
double stats_entry_ema_base<T>::BiggestEMAValue() const {
	double biggest = 0.0;
	bool first = true;
	for(stats_ema_list::const_iterator ema_itr = ema.begin();
		ema_itr != ema.end();
		++ema_itr )
	{
		if( first || ema_itr->ema > biggest ) {
			biggest = ema_itr->ema;
			first = false;
		}
	}
	return biggest;
}

template <class T>
char const *stats_entry_ema_base<T>::ShortestHorizonEMAName() const {
	char const *shortest_horizon_name = NULL;
	time_t shortest_horizon = 0;
	bool first = true;
	for(size_t i = ema.size(); i--; ) {
		stats_ema_config::horizon_config &config = ema_config->horizons[i];
		if( first || config.horizon < shortest_horizon ) {
			shortest_horizon_name = config.horizon_name.c_str();
			shortest_horizon = config.horizon;
			first = false;
		}
	}
	return shortest_horizon_name;
}

template <class T>
double stats_entry_ema_base<T>::EMAValue(char const *horizon_name) const {
	for(size_t i = ema.size(); i--; ) {
		stats_ema_config::horizon_config &config = ema_config->horizons[i];
		if( config.horizon_name == horizon_name ) {
			return ema[i].ema;
		}
	}
	return 0.0;
}

template <class T>
bool stats_entry_ema_base<T>::HasEMAHorizonNamed(char const *horizon_name) const {
	for(size_t i = ema.size(); i--; ) {
		stats_ema_config::horizon_config &config = ema_config->horizons[i];
		if( config.horizon_name == horizon_name ) {
			return true;
		}
	}
	return false;
}

// Force template instantiation
// C++ note:
// We used to have a dummy function that make various templated objects
// in order to force the compiler to generate the template code so other
// objects could link against it.  That is not the correct way to do things
// because the compiler could decide to inline class member functions and not
// generate the functions for the object file.
//
// Note that we instantiate more than we have to on most platforms.  This is
// because things like stats_entry_recent<time_t> may or may not be equal
// to stats_entry_recent<int64_t>, depending on the platform.
template class stats_entry_recent<long long>;
template class stats_entry_recent<long>;
template class stats_entry_recent<int>;
template class stats_entry_recent<double>;
template class stats_entry_recent<Probe>;
template class stats_entry_recent_histogram<long long>;
template class stats_entry_recent_histogram<long>;
template class stats_entry_recent_histogram<int>;
template class stats_entry_recent_histogram<double>;
template class stats_entry_ema_base<int>;
template class stats_entry_ema_base<double>;
template class stats_entry_ema_base<uint64_t>;
template class stats_entry_sum_ema_rate<int>;
template class stats_entry_sum_ema_rate<double>;
template class stats_entry_sum_ema_rate<uint64_t>;
template class stats_entry_ema<int>;
template class stats_entry_ema<double>;
template class stats_entry_probe<double>;

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
   time_t now = time(NULL);
   int cAdvance = generic_stats_Tick(
      now,
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
   Pool.Publish(ad, 0);
}


void TestStats::Publish(ClassAd & ad) const
{
   Pool.Unpublish(ad);
}
#endif
