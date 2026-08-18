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

#define TEST_CASE(fn) static void check_##fn(void)
#define INVOKE_TEST_CASE(fn) check_##fn()
#define CHECK(cond) REQUIRE(cond)
#define CHECK_EQUAL(value,result) REQUIRE((value) == (result))

template <typename T> void print_it(
    FILE * out,
    stats_entry_daily<T> & d,
    const char * attr,
    const char * label)
{
    std::string adbuf;
    ClassAd ad;
    d.Publish(ad, attr, d.PubTotal | d.PubDefault);
    //d.PublishDebug(ad, attr, d.PubTotal | d.PubDefault);
    adbuf.clear(); fprintf(out, "%s%s\n", label, formatAd(adbuf, ad));
}


static void test_daily_stat(void) {

    stats_entry_daily<double> d(6, 4);
    const char * attr = "Usage";

    print_it(stdout, d, attr, "--- \n");

    d += 3;

    print_it(stdout, d, attr, "--- 3\n");

    d += .5;

    print_it(stdout, d, attr, "--- 3.5\n");

    d.AdvanceBy(1);

    print_it(stdout, d, attr, "--- Advance(1)\n");

    d += 1.1;
    d += 1.2;

    print_it(stdout, d, attr, "--- 2.3, 3.5\n");

    d.AdvanceBy(2);

    print_it(stdout, d, attr, "--- Advance(2)\n");

    d += 7.42;

    print_it(stdout, d, attr, "--- 7.42, 0, 2.3, 3.5\n");

    d.AdvanceBy(1);

    d += 4.4;

    print_it(stdout, d, attr, "--- 4.4, 7.42, 0, 2.3, 3.5\n");

    d.AdvanceBy(2);

    print_it(stdout, d, attr, "--- Advance(2)\n");

    d += 99;

    print_it(stdout, d, attr, "--- 99.0, 0, 4.4, 7.42, 0, 2.3 / 3.5\n");

    d.AdvanceBy(1);

    print_it(stdout, d, attr, "--- Advance(1)\n");

    d += 2;

    print_it(stdout, d, attr, "--- 2.0, 99.0, 0, 4.4, 7.42, 0 / 5.8\n");

    d.AdvanceBy(3);

    print_it(stdout, d, attr, "--- Advance(3)\n");

    d += 3;

    print_it(stdout, d, attr, "--- 3.0, 0, 0, 2.0, 99.0, 0 / 17.62 \n");

    d.AdvanceBy(4);

    print_it(stdout, d, attr, "--- Advance(4)\n");

    d += 1;

    print_it(stdout, d, attr, "--- 0.1, 0, 0, 0.0, 3.0, 0 / 101.0, 17.62 \n");

    d.AdvanceBy(1,1);

    print_it(stdout, d, attr, "--- Advance(1,1)\n");

    d.AdvanceBy(1);

    print_it(stdout, d, attr, "--- Advance(1)\n");

    d += 93.3;

    print_it(stdout, d, attr, "--- 93.3, \n");

    d.AdvanceBy(6,1);

    print_it(stdout, d, attr, "--- Advance(6,1) 0.0,0.0,0.0,0.0,0.0,0.0 / 94.3,7.0,101.0,17.62 \n");
}

int main( int argc, const char ** argv) {

    union {
        unsigned int all;
        struct {
            unsigned int dwm:1;
        };
    } dash_flags{0};

    for (int ix = 0; ix < argc; ++ix) {
        if (is_dash_arg_prefix(argv[ix], "daily", 1)) {
            dash_flags.dwm = 1;
        }
    }

    if ( ! dash_flags.all) { dash_flags.all = 0xffff; }


    if (dash_flags.dwm) {
        test_daily_stat();
    }

	return fail_count;
}
