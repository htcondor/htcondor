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
#include "match_prefix.h"

// override the EXCEPT macro in generic stats so we can test it.
#undef EXCEPT
#define EXCEPT(msg,...) {}
#include "generic_stats.h"

int fail_count = 0;

#define REQUIRE( condition ) \
	if(! ( condition )) { \
		fprintf( stderr, "Failed %5d: %s\n", __LINE__, #condition ); \
		++fail_count; \
	}

#define TEST_CASE(fn) static void test_ring_buffer_##fn(void)
#define INVOKE_TEST_CASE(fn) test_ring_buffer_##fn()
#define CHECK(cond) REQUIRE(cond)
#define CHECK_EQUAL(value,result) REQUIRE((value) == (result))


TEST_CASE(default_constructor) {
    ring_buffer<int> b;

    CHECK(b.empty());
    CHECK_EQUAL(b.Length(), 0);
    CHECK_EQUAL(b.MaxSize(), 0);
    CHECK_EQUAL(b.Sum(), 0);
}


TEST_CASE(sized_constructor) {
    ring_buffer<int> b(10);

    CHECK(b.empty());
    CHECK_EQUAL(b.Length(), 0);
    CHECK_EQUAL(b.MaxSize(), 10);
    CHECK_EQUAL(b.Sum(), 0);
}


TEST_CASE(PushZero) {
    ring_buffer<int> b(2);
    
    b.PushZero();
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.Sum(), 0);

    b.PushZero();
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 0);

    // should stop growing at 2:
    b.PushZero();
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 0);
}


TEST_CASE(Add) {
    ring_buffer<int> b(2);
    
    b.PushZero();
    CHECK_EQUAL(b.Add(2), 2);

    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.Sum(), 2);

    CHECK_EQUAL(b.Add(40), 42);
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.Sum(), 42);

    b.PushZero();
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 42);

    CHECK_EQUAL(b.Add(10), 10);
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 52);

    // wrap around:
    b.PushZero();
    b.Add(2);
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 12);

    b.PushZero();
    b.Add(40);
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.Sum(), 42);
}


TEST_CASE(index_operator) {
    ring_buffer<int> b(3);
    const int M = b.MaxSize();
    
    b.PushZero();
    CHECK_EQUAL(b[0], 0);

    b.Add(1);
    CHECK_EQUAL(b[0], 1);
    CHECK_EQUAL(b[0+M], 1);
    CHECK_EQUAL(b[0-M], 1);

    b.PushZero();
    b.Add(2);
    CHECK_EQUAL(b[0], 2);
    CHECK_EQUAL(b[-1], 1);
    CHECK_EQUAL(b[0+M], 2);
    CHECK_EQUAL(b[0-M], 2);
    CHECK_EQUAL(b[-1+M], 1);
    CHECK_EQUAL(b[-1-M], 1);

    b.PushZero();
    b.Add(3);
    CHECK_EQUAL(b[0], 3);
    CHECK_EQUAL(b[-1], 2);
    CHECK_EQUAL(b[-2], 1);
    CHECK_EQUAL(b[0+M], 3);
    CHECK_EQUAL(b[0-M], 3);
    CHECK_EQUAL(b[-1+M], 2);
    CHECK_EQUAL(b[-1-M], 2);
    CHECK_EQUAL(b[-2+M], 1);
    CHECK_EQUAL(b[-2-M], 1);

    // wrap:
    b.PushZero();
    b.Add(4);
    CHECK_EQUAL(b[0], 4);
    CHECK_EQUAL(b[-1], 3);
    CHECK_EQUAL(b[-2], 2);
    CHECK_EQUAL(b[0+M], 4);
    CHECK_EQUAL(b[0-M], 4);
    CHECK_EQUAL(b[-1+M], 3);
    CHECK_EQUAL(b[-1-M], 3);
    CHECK_EQUAL(b[-2+M], 2);
    CHECK_EQUAL(b[-2-M], 2);

    CHECK_EQUAL(b[-3], 4);
    CHECK_EQUAL(b[-2], 2);
    CHECK_EQUAL(b[-1], 3);
    CHECK_EQUAL(b[0], 4);
    CHECK_EQUAL(b[1], 2);
    CHECK_EQUAL(b[2], 3);
    CHECK_EQUAL(b[3], 4);
}


TEST_CASE(Clear) {
    ring_buffer<int> b(3);

    b.PushZero();
    b.Add(1);

    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.MaxSize(), 3);
    CHECK_EQUAL(b.Sum(), 1);

    CHECK(b.Clear());
    CHECK(b.empty());
    CHECK_EQUAL(b.Length(), 0);
    CHECK_EQUAL(b.MaxSize(), 3);
    CHECK_EQUAL(b.Sum(), 0);

    b.PushZero();
    b.Add(7);
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.MaxSize(), 3);
    CHECK_EQUAL(b.Sum(), 7);    
}


TEST_CASE(Free) {
    ring_buffer<int> b(3);

    b.PushZero();
    b.Add(1);

    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 1);
    CHECK_EQUAL(b.MaxSize(), 3);
    CHECK_EQUAL(b.Sum(), 1);

    b.Free();
    CHECK(b.empty());
    CHECK_EQUAL(b.Length(), 0);
    CHECK_EQUAL(b.MaxSize(), 0);
    CHECK_EQUAL(b.Sum(), 0);
}


TEST_CASE(SetSize_negative) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    CHECK(!b.SetSize(-1));
    
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.MaxSize(), 2);
    CHECK_EQUAL(b.Sum(), 3);
}


TEST_CASE(SetSize_zero) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    CHECK(b.SetSize(0));

    CHECK(b.empty());
    CHECK_EQUAL(b.Length(), 0);
    CHECK_EQUAL(b.MaxSize(), 0);
    CHECK_EQUAL(b.Sum(), 0);
}


TEST_CASE(SetSize_same) {
    ring_buffer<int> b(2);

    b.PushZero();
    b.Add(1);
    b.PushZero();
    b.Add(2);

    CHECK(b.SetSize(2));
    
    CHECK(!b.empty());
    CHECK_EQUAL(b.Length(), 2);
    CHECK_EQUAL(b.MaxSize(), 2);
    CHECK_EQUAL(b.Sum(), 3);    
}


TEST_CASE(SetSize_larger) {
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
    
    CHECK(b.SetSize(7));

    CHECK(!b.empty());
    CHECK_EQUAL(b.MaxSize(), 7);
    CHECK_EQUAL(b.Length(), 5);
    CHECK_EQUAL(b.Sum(), 15);

    CHECK_EQUAL(b[0], 5);
    CHECK_EQUAL(b[-1], 4);
    CHECK_EQUAL(b[-2], 3);
    CHECK_EQUAL(b[-3], 2);
    CHECK_EQUAL(b[-4], 1);
}


TEST_CASE(SetSize_smaller) {
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
    
    CHECK(b.SetSize(3));

    CHECK(!b.empty());
    CHECK_EQUAL(b.MaxSize(), 3);
    CHECK_EQUAL(b.Length(), 3);
    CHECK_EQUAL(b.Sum(), 12);

    CHECK_EQUAL(b[0], 5);
    CHECK_EQUAL(b[-1], 4);
    CHECK_EQUAL(b[-2], 3);
}


TEST_CASE(SetSize_random) {
    const int ntrials = 100000;
    ring_buffer<int> b;

    // actually important to be repeatable
    srand(777);

    for (int j=0;  j < ntrials;  ++j) {
        int oldsiz = b.Length();
        int newsiz = rand() % 10;
        int addsiz = rand() % 10;
        //BOOST_TEST_CHECKPOINT("test iteration " << j << "oldsiz= " << oldsiz << " newsiz= " << newsiz << "addsiz= " << addsiz);
        std::deque<int> comp;
        for (int k = 0;  k < b.Length();  ++k) comp.push_back(b[-k]);
        CHECK(b.SetSize(newsiz));
        for (int k = 0;  k < MIN(oldsiz, newsiz);  ++k) {
            CHECK_EQUAL(b[-k], comp[k]);
        }
        while (int(comp.size()) > b.Length()) comp.pop_back();
        for (int k = 0;  k < addsiz;  ++k) {
            int v = rand() % 100;
            b.PushZero();
            b.Add(v);
            comp.push_front(v);
            if (int(comp.size()) > b.MaxSize()) comp.pop_back();
        }
        CHECK_EQUAL(int(comp.size()), b.Length());
        for (int k = 0;  k < int(comp.size());  ++k) {
            CHECK_EQUAL(b[-k], comp[k]);
        }
    }
}


int main( int /*argc*/, const char ** /*argv*/) {

	INVOKE_TEST_CASE(default_constructor);
	INVOKE_TEST_CASE(sized_constructor);
	INVOKE_TEST_CASE(PushZero);
	INVOKE_TEST_CASE(Add);
	INVOKE_TEST_CASE(index_operator);
	INVOKE_TEST_CASE(Clear);
	INVOKE_TEST_CASE(Free);
	INVOKE_TEST_CASE(SetSize_negative);
	INVOKE_TEST_CASE(SetSize_zero);
	INVOKE_TEST_CASE(SetSize_same);
	INVOKE_TEST_CASE(SetSize_larger);
	INVOKE_TEST_CASE(SetSize_smaller);
	INVOKE_TEST_CASE(SetSize_random);

	return fail_count;
}
