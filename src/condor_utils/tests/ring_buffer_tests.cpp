/***************************************************************
 *
 * Copyright (C) 1990-2013, Red Hat Inc.
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
#include "condor_config.h"
#include "condor_random_num.h"
#include <deque>
#include <cstdlib>

#define RING_BUFFER_UNIT_TESTING
#include "generic_stats.h"

#define BOOST_TEST_MAIN
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>

#define BOOST_TEST_MODULE ring_buffer


using std::deque;


BOOST_AUTO_TEST_CASE(default_constructor) {
    ring_buffer<int> b;

    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 0);
    BOOST_CHECK_EQUAL(b.MaxSize(), 0);
    BOOST_CHECK_EQUAL(b.Sum(), 0);
}


BOOST_AUTO_TEST_CASE(sized_constructor) {
    ring_buffer<int> b(10);

    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 0);
    BOOST_CHECK_EQUAL(b.MaxSize(), 10);
    BOOST_CHECK_EQUAL(b.Sum(), 0);
}


BOOST_AUTO_TEST_CASE(PushZero) {
    ring_buffer<int> b(2);
    
    b.PushZero();
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.Sum(), 0);

    b.PushZero();
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 0);

    // should stop growing at 2:
    b.PushZero();
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 0);
}


BOOST_AUTO_TEST_CASE(Add) {
    ring_buffer<int> b(2);
    
    b.PushZero();
    BOOST_CHECK_EQUAL(b.Add(2), 2);

    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.Sum(), 2);

    BOOST_CHECK_EQUAL(b.Add(40), 42);
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.Sum(), 42);

    b.PushZero();
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 42);

    BOOST_CHECK_EQUAL(b.Add(10), 10);
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 52);

    // wrap around:
    b.PushZero();
    b.Add(2);
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 12);

    b.PushZero();
    b.Add(40);
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 42);
}


BOOST_AUTO_TEST_CASE(index_operator) {
    ring_buffer<int> b(3);
    const int M = b.MaxSize();
    
    b.PushZero();
    BOOST_CHECK_EQUAL(b[0], 0);

    b.Add(1);
    BOOST_CHECK_EQUAL(b[0], 1);
    BOOST_CHECK_EQUAL(b[0+M], 1);
    BOOST_CHECK_EQUAL(b[0-M], 1);

    b.PushZero();
    b.Add(2);
    BOOST_CHECK_EQUAL(b[0], 2);
    BOOST_CHECK_EQUAL(b[-1], 1);
    BOOST_CHECK_EQUAL(b[0+M], 2);
    BOOST_CHECK_EQUAL(b[0-M], 2);
    BOOST_CHECK_EQUAL(b[-1+M], 1);
    BOOST_CHECK_EQUAL(b[-1-M], 1);

    b.PushZero();
    b.Add(3);
    BOOST_CHECK_EQUAL(b[0], 3);
    BOOST_CHECK_EQUAL(b[-1], 2);
    BOOST_CHECK_EQUAL(b[-2], 1);
    BOOST_CHECK_EQUAL(b[0+M], 3);
    BOOST_CHECK_EQUAL(b[0-M], 3);
    BOOST_CHECK_EQUAL(b[-1+M], 2);
    BOOST_CHECK_EQUAL(b[-1-M], 2);
    BOOST_CHECK_EQUAL(b[-2+M], 1);
    BOOST_CHECK_EQUAL(b[-2-M], 1);

    // wrap:
    b.PushZero();
    b.Add(4);
    BOOST_CHECK_EQUAL(b[0], 4);
    BOOST_CHECK_EQUAL(b[-1], 3);
    BOOST_CHECK_EQUAL(b[-2], 2);
    BOOST_CHECK_EQUAL(b[0+M], 4);
    BOOST_CHECK_EQUAL(b[0-M], 4);
    BOOST_CHECK_EQUAL(b[-1+M], 3);
    BOOST_CHECK_EQUAL(b[-1-M], 3);
    BOOST_CHECK_EQUAL(b[-2+M], 2);
    BOOST_CHECK_EQUAL(b[-2-M], 2);

    BOOST_CHECK_EQUAL(b[-3], 4);
    BOOST_CHECK_EQUAL(b[-2], 2);
    BOOST_CHECK_EQUAL(b[-1], 3);
    BOOST_CHECK_EQUAL(b[0], 4);
    BOOST_CHECK_EQUAL(b[1], 2);
    BOOST_CHECK_EQUAL(b[2], 3);
    BOOST_CHECK_EQUAL(b[3], 4);
}


BOOST_AUTO_TEST_CASE(Clear) {
    ring_buffer<int> b(3);

    b.PushZero();
    b.Add(1);

    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.MaxSize(), 3);
    BOOST_CHECK_EQUAL(b.Sum(), 1);

    BOOST_CHECK(b.Clear());
    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 0);
    BOOST_CHECK_EQUAL(b.MaxSize(), 3);
    BOOST_CHECK_EQUAL(b.Sum(), 0);

    b.PushZero();
    b.Add(7);
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.MaxSize(), 3);
    BOOST_CHECK_EQUAL(b.Sum(), 7);    
}


BOOST_AUTO_TEST_CASE(Free) {
    ring_buffer<int> b(3);

    b.PushZero();
    b.Add(1);

    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 1);
    BOOST_CHECK_EQUAL(b.MaxSize(), 3);
    BOOST_CHECK_EQUAL(b.Sum(), 1);

    b.Free();
    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 0);
    BOOST_CHECK_EQUAL(b.MaxSize(), 0);
    BOOST_CHECK_EQUAL(b.Sum(), 0);
}


BOOST_AUTO_TEST_CASE(SetSize_negative) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    BOOST_CHECK(!b.SetSize(-1));
    
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.MaxSize(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 3);
}


BOOST_AUTO_TEST_CASE(SetSize_zero) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    BOOST_CHECK(b.SetSize(0));

    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 0);
    BOOST_CHECK_EQUAL(b.MaxSize(), 0);
    BOOST_CHECK_EQUAL(b.Sum(), 0);
}


BOOST_AUTO_TEST_CASE(SetSize_same) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    BOOST_CHECK(b.SetSize(2));
    
    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.Length(), 2);
    BOOST_CHECK_EQUAL(b.MaxSize(), 2);
    BOOST_CHECK_EQUAL(b.Sum(), 3);    
}


BOOST_AUTO_TEST_CASE(SetSize_larger) {
    ring_buffer<int> b(5);
    
    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);
    b.PushZero();
    b.Add(3);
    b.PushZero();
    b.Add(4);
    b.PushZero();
    b.Add(5);
    
    BOOST_CHECK(b.SetSize(7));

    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.MaxSize(), 7);
    BOOST_CHECK_EQUAL(b.Length(), 5);
    BOOST_CHECK_EQUAL(b.Sum(), 15);

    BOOST_CHECK_EQUAL(b[0], 5);
    BOOST_CHECK_EQUAL(b[-1], 4);
    BOOST_CHECK_EQUAL(b[-2], 3);
    BOOST_CHECK_EQUAL(b[-3], 2);
    BOOST_CHECK_EQUAL(b[-4], 1);
}


BOOST_AUTO_TEST_CASE(SetSize_smaller) {
    ring_buffer<int> b(5);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);
    b.PushZero();
    b.Add(3);
    b.PushZero();
    b.Add(4);
    b.PushZero();
    b.Add(5);
    
    BOOST_CHECK(b.SetSize(3));

    BOOST_CHECK(!b.empty());
    BOOST_CHECK_EQUAL(b.MaxSize(), 3);
    BOOST_CHECK_EQUAL(b.Length(), 3);
    BOOST_CHECK_EQUAL(b.Sum(), 12);

    BOOST_CHECK_EQUAL(b[0], 5);
    BOOST_CHECK_EQUAL(b[-1], 4);
    BOOST_CHECK_EQUAL(b[-2], 3);
}


BOOST_AUTO_TEST_CASE(SetSize_random) {
    const int ntrials = 100000;
    ring_buffer<int> b;

    // actually important to be repeatable
    srand(777);

    for (int j=0;  j < ntrials;  ++j) {
        int oldsiz = b.Length();
        int newsiz = rand() % 10;
        int addsiz = rand() % 10;
        BOOST_TEST_CHECKPOINT("test iteration " << j << "oldsiz= " << oldsiz << " newsiz= " << newsiz << "addsiz= " << addsiz);
        deque<int> comp;
        for (int k = 0;  k < b.Length();  ++k) comp.push_back(b[-k]);
        BOOST_CHECK(b.SetSize(newsiz));
        for (int k = 0;  k < MIN(oldsiz, newsiz);  ++k) {
            BOOST_CHECK_EQUAL(b[-k], comp[k]);
        }
        while (int(comp.size()) > b.Length()) comp.pop_back();
        for (int k = 0;  k < addsiz;  ++k) {
            int v = rand() % 100;
            b.PushZero();
            b.Add(v);
            comp.push_front(v);
            if (int(comp.size()) > b.MaxSize()) comp.pop_back();
        }
        BOOST_CHECK_EQUAL(int(comp.size()), b.Length());
        for (int k = 0;  k < int(comp.size());  ++k) {
            BOOST_CHECK_EQUAL(b[-k], comp[k]);
        }
    }
}
