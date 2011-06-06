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

#ifndef _timed_queue_h_
#define _timed_queue_h_ 1

#include "time.h"
#include <deque>
#include "condor_debug.h"

// A deque<> subclass that makes it convenient to time-stamp queue entries
// and maintain the queue with a configurable time-window and maximum length.
template <typename Data>
struct timed_queue : public std::deque<std::pair<time_t, Data> > {
    typedef std::deque<std::pair<time_t, Data> > base_type;

    typedef typename base_type::size_type size_type;
    typedef typename base_type::difference_type difference_type;
    typedef typename base_type::value_type value_type;
    typedef typename base_type::pointer pointer;
    typedef typename base_type::reference reference;
    typedef typename base_type::const_reference const_reference;

    typedef typename base_type::iterator iterator;
    typedef typename base_type::const_iterator const_iterator;
    typedef typename base_type::reverse_iterator reverse_iterator;
    typedef typename base_type::const_reverse_iterator const_reverse_iterator;

    timed_queue() : base_type(), _max_time(0), _max_len(0) {}
    virtual ~timed_queue() {}

    timed_queue(const timed_queue& src) : base_type(src), _max_time(src._max_time), _max_len(src._max_len) {}

    timed_queue& operator=(const timed_queue& src) {
        if (this == &src) return *this;
        base_type::operator=(src);
        _max_time = src._max_time;
        _max_len = src._max_len;
        return *this;
    }

    void push_front(const Data& d) {
        push_front(d, time(NULL));
    }

    void push_front(const Data& d, time_t t) {
        if (!base_type::empty() && (t < base_type::front().first)) {
            EXCEPT("timed_queue::push_front, timestamp %lu out of order", (unsigned long)(t));
        }
        base_type::push_front(value_type(t, d));
        if (max_len() > 0) trim_len(max_len());
        if (max_time() > 0) trim_time(base_type::front().first - max_time());
    }

    void max_time(size_type t) {
        _max_time = t;
        if ((!base_type::empty()) && (max_time() > 0)) trim_time(base_type::front().first - max_time());
    }
    size_type max_time() const {
        return _max_time;
    }
    void max_len(size_type l) {
        _max_len = l;
        if (max_len() > 0) trim_len(max_len());
    }
    size_type max_len() const {
        return _max_len;
    }

    void trim_time(time_t tmin) {
        while (!base_type::empty()) {
            if (base_type::back().first >= tmin) break;
            base_type::pop_back();
        }
    }

    void trim_len(size_type lmax) {
        while (base_type::size() > lmax) base_type::pop_back();        
    }

    protected:
    size_type _max_time;
    size_type _max_len;
};


// Utility function that will add (n) to the front of the queue if the
// current time-stamp is the same, otherwise it will push a new entry.
template <typename Num1, typename Num2>
void update(timed_queue<Num1>& tq, Num2 n, time_t t) {
    if (n <= 0) return;
    if (!tq.empty() && (tq.front().first == t)) {
        tq.front().second += n;
    } else {
        tq.push_front(Num1(n), t);
    }
}

// Utility function that will trim the queue and then sum up the entries
template <typename Num>
Num accumulate(timed_queue<Num>& tq, time_t t) {
    tq.trim_time(t - tq.max_time());
    Num r = 0;
    for (typename timed_queue<Num>::iterator j(tq.begin());  j != tq.end();  ++j) {
        r += j->second;
    }
    return r;
}


#endif // _timed_queue_h_
