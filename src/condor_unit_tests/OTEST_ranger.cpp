/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

/* Test the ranger implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "ranger.h"
#include "proc.h"


/////////////


enum Action { Erase, Insert };

struct testcase1 {
    Action action;
    const char *input;
    const char *expected_result;
};

// test cases for load, insert/erase, persist
const testcase1 test_table1[] = {
    {Insert, "2",                         "2"                          },
    {Insert, "2",                         "2"                          },
    {Insert, "5-10",                      "2;5-10"                     },
    {Insert, "4;7;10-20;44;50-60",        "2;4-20;44;50-60"            },
    {Erase,  "55",                        "2;4-20;44;50-54;56-60"      },
    {Erase,  "56",                        "2;4-20;44;50-54;57-60"      },
    {Insert, "61",                        "2;4-20;44;50-54;57-61"      },
    {Insert, "56",                        "2;4-20;44;50-54;56-61"      },
    {Erase,  "61",                        "2;4-20;44;50-54;56-60"      },
    {Insert, "55",                        "2;4-20;44;50-60"            },
    {Insert, "3-43",                      "2-44;50-60"                 },
    {Insert, "70-80",                     "2-44;50-60;70-80"           },
    {Insert, "10-75",                     "2-80"                       },
    {Erase,  "5;10;20-30;41;42;45;50-55", "2-4;6-9;11-19;31-40;"
                                          "43-44;46-49;56-80"          },
    {Insert, "5-35",                      "2-40;43-44;46-49;56-80"     },
    {Erase,  "5-35",                      "2-4;36-40;43-44;46-49;56-80"},
    {Erase,  "85",                        "2-4;36-40;43-44;46-49;56-80"},
    {Erase,  "85-90",                     "2-4;36-40;43-44;46-49;56-80"},
    {Erase,  "1-5",                       "36-40;43-44;46-49;56-80"    },
    {Erase,  "2-3",                       "36-40;43-44;46-49;56-80"    },
    {Erase,  "1-100",                     ""                           },
    {Erase,  "2",                         ""                           },
    {Erase,  "2-4",                       ""                           }
};


/////////////


struct testcase2 {
    const char *input;
    struct { int start, back; };
    const char *expected_result;
};

// test cases for load, persist slice
const testcase2 test_table2[] = {
    {"10-20;30-40;50-60", {1,100}, "10-20;30-40;50-60"},
    {"10-20;30-40;50-60", {1,35},  "10-20;30-35"      },
    {"10-20;30-40;50-60", {10,35}, "10-20;30-35"      },
    {"10-20;30-40;50-60", {11,35}, "11-20;30-35"      },
    {"10-20;30-40;50-60", {11,39}, "11-20;30-39"      },
    {"10-20;30-40;50-60", {11,40}, "11-20;30-40"      },
    {"10-20;30-40;50-60", {11,41}, "11-20;30-40"      },
    {"10-20;30-40;50-60", {25,45}, "30-40"            },
    {"10-20;30-40;50-60", {29,41}, "30-40"            },
    {"10-20;30-40;50-60", {30,41}, "30-40"            },
    {"10-20;30-40;50-60", {29,40}, "30-40"            },
    {"10-20;30-40;50-60", {31,40}, "31-40"            },
    {"10-20;30-40;50-60", {30,39}, "30-39"            },
    {"10-20;30-40;50-60", {31,39}, "31-39"            },
    {"10-20;30-40;50-60", {0,5},   ""                 },
    {"10-20;30-40;50-60", {0,9},   ""                 },
    {"10-20;30-40;50-60", {0,10},  "10"               },
    {"10-20;30-40;50-60", {21,29}, ""                 },
    {"10-20;30-40;50-60", {20,29}, "20"               },
    {"10-20;30-40;50-60", {20,30}, "20;30"            },
};


/////////////


struct testcase3 {
    const char *input;
    int element;
    bool expected_result;
};

// test cases for contains (element)
const testcase3 test_table3[] = {
    {"",                   123,  false},
    {"10-20",              123,  false},
    {"10-20",                5,  false},
    {"10-20",                9,  false},
    {"10-20",               10,  true },
    {"10-20",               11,  true },
    {"10-20",               19,  true },
    {"10-20",               20,  true },
    {"10-20",               21,  false},
    {"10-20",               22,  false},
    {"10-20;30-40",        123,  false},
    {"10-20;30-40",          5,  false},
    {"10-20;30-40",          9,  false},
    {"10-20;30-40",         10,  true },
    {"10-20;30-40",         11,  true },
    {"10-20;30-40",         19,  true },
    {"10-20;30-40",         20,  true },
    {"10-20;30-40",         21,  false},
    {"10-20;30-40",         22,  false},
    {"10-20;30-40",         29,  false},
    {"10-20;30-40",         30,  true },
    {"10-20;30-40",         31,  true },
    {"10-20;30-40",         39,  true },
    {"10-20;30-40",         40,  true },
    {"10-20;30-40",         41,  false},
    {"10-20;30-40",         42,  false},
    {"10-20;30-40;50-60",  123,  false},
    {"10-20;30-40;50-60",    5,  false},
    {"10-20;30-40;50-60",    9,  false},
    {"10-20;30-40;50-60",   10,  true },
    {"10-20;30-40;50-60",   11,  true },
    {"10-20;30-40;50-60",   19,  true },
    {"10-20;30-40;50-60",   20,  true },
    {"10-20;30-40;50-60",   21,  false},
    {"10-20;30-40;50-60",   22,  false},
    {"10-20;30-40;50-60",   29,  false},
    {"10-20;30-40;50-60",   30,  true },
    {"10-20;30-40;50-60",   31,  true },
    {"10-20;30-40;50-60",   39,  true },
    {"10-20;30-40;50-60",   40,  true },
    {"10-20;30-40;50-60",   41,  false},
    {"10-20;30-40;50-60",   42,  false},
    {"10-20;22-30",         19,  true },
    {"10-20;22-30",         20,  true },
    {"10-20;22-30",         21,  false},
    {"10-20;22-30",         22,  true },
    {"10-20;22-30",         23,  true }
};


/////////////


struct testcase4 {
    Action action;
    const char *input;
    int element;
    const char *expected_result;
};

const testcase4 test_table4[] = {
    {Insert, "",     2,  "2"    },
    {Insert, "2",    2,  "2"    },
    {Insert, "2",    3,  "2-3"  },
    {Insert, "2-3",  2,  "2-3"  },
    {Insert, "2-3",  3,  "2-3"  },
    {Insert, "2;4",  3,  "2-4"  },
    {Insert, "2;4",  1,  "1-2;4"},
    {Insert, "2;4",  5,  "2;4-5"},

    {Insert, "10-20;30-40;50-60",   5,  "5;10-20;30-40;50-60" },
    {Insert, "10-20;30-40;50-60",   9,  "9-20;30-40;50-60"    },
    {Insert, "10-20;30-40;50-60",  21,  "10-21;30-40;50-60"   },
    {Insert, "10-20;30-40;50-60",  22,  "10-20;22;30-40;50-60"},
    {Insert, "10-20;30-40;50-60",  29,  "10-20;29-40;50-60"   },
    {Insert, "10-20;30-40;50-60",  31,  "10-20;30-40;50-60"   },
    {Insert, "10-20;30-40;50-60",  41,  "10-20;30-41;50-60"   },
    {Insert, "10-20;30-40;50-60",  42,  "10-20;30-40;42;50-60"},
    {Insert, "10-20;30-40;50-60",  49,  "10-20;30-40;49-60"   },
    {Insert, "10-20;30-40;50-60",  60,  "10-20;30-40;50-60"   },
    {Insert, "10-20;30-40;50-60",  61,  "10-20;30-40;50-61"   },
    {Insert, "10-20;30-40;50-60",  62,  "10-20;30-40;50-60;62"},

    {Erase, "",       5, ""           },
    {Erase, "4",      5, "4"          },
    {Erase, "5",      5, ""           },
    {Erase, "6",      5, "6"          },
    {Erase, "10-12", 11, "10;12"      },
    {Erase, "10-20",  9, "10-20"      },
    {Erase, "10-20", 10, "11-20"      },
    {Erase, "10-20", 11, "10;12-20"   },
    {Erase, "10-20", 15, "10-14;16-20"},
    {Erase, "10-20", 19, "10-18;20"   },
    {Erase, "10-20", 20, "10-19"      },

    {Erase, "10-20;30-40;50-60", 30, "10-20;31-40;50-60"},
    {Erase, "10-20;30-40;50-60", 40, "10-20;30-39;50-60"},
    {Erase, "10-20;30-40;50-60", 50, "10-20;30-40;51-60"},
    {Erase, "10-20;30-40;50-60", 60, "10-20;30-40;50-59"}
};


/////////////


using testcase5 = testcase1;

// test cases for load, insert/erase, persist -- job ids
const testcase5 test_table5[] = {
    {Insert, "2.1",                    "2.1"                                },
    {Insert, "2.1",                    "2.1"                                },
    {Insert, "3.5-3.10",               "2.1;3.5-3.10"                       },
    {Insert, "3.11-3.20;4.1;5.1-5.10", "2.1;3.5-3.20;4.1;5.1-5.10"          },
    {Erase,  "3.10",                   "2.1;3.5-3.9;3.11-3.20;4.1;5.1-5.10" },
    {Erase,  "3.0-4.-1",               "2.1;4.1;5.1-5.10"                   },
    {Erase,  "5.1",                    "2.1;4.1;5.2-5.10"                   },
    {Erase,  "5.10",                   "2.1;4.1;5.2-5.9"                    },
    {Erase,  "4.1",                    "2.1;5.2-5.9"                        },
    {Erase,  "2.1",                    "5.2-5.9"                            }
};


/////////////


struct testcase6 {
    const char *input;
    JOB_ID_KEY element;
    bool expected_result;
};

// test cases for contains (element)
const testcase6 test_table6[] = {
    {"",                       {1,123},  false},
    {"1.10-1.20",              {1,123},  false},
    {"1.10-1.20",              {1,  5},  false},
    {"1.10-1.20",              {1,  9},  false},
    {"1.10-1.20",              {1, 10},  true },
    {"1.10-1.20",              {1, 11},  true },
    {"1.10-1.20",              {1, 19},  true },
    {"1.10-1.20",              {1, 20},  true },
    {"1.10-1.20",              {1, 21},  false},
    {"1.10-1.20",              {1, 22},  false},
    {"1.10-1.20;1.30-1.40",    {1,123},  false},
    {"1.10-1.20;1.30-1.40",    {1,  5},  false},
    {"1.10-1.20;1.30-1.40",    {1,  9},  false},
    {"1.10-1.20;1.30-1.40",    {1, 10},  true },
    {"1.10-1.20;1.30-1.40",    {1, 11},  true },
    {"1.10-1.20;1.30-1.40",    {1, 19},  true },
    {"1.10-1.20;1.30-1.40",    {1, 20},  true },
    {"1.10-1.20;1.30-1.40",    {1, 21},  false},
    {"1.10-1.20;1.30-1.40",    {1, 22},  false},
    {"1.10-1.20;1.30-1.40",    {1, 29},  false},
    {"1.10-1.20;1.30-1.40",    {1, 30},  true },
    {"1.10-1.20;1.30-1.40",    {1, 31},  true },
    {"1.10-1.20;1.30-1.40",    {1, 39},  true },
    {"1.10-1.20;1.30-1.40",    {1, 40},  true },
    {"1.10-1.20;1.30-1.40",    {1, 41},  false},
    {"1.10-1.20;1.30-1.40",    {1, 42},  false},
};


//////////////////////////////////////////////


// do some template magic to automatically register one
// test function per test case in the above tables

// one templated function gets generated for every test case, so we keep
// each one very light weight as a wrapper around the main test function
// for each table

#define ARRAY_LEN(arr) (sizeof arr / sizeof arr[0])

#define TEST_TABLE_SETUP(n, funcname)                                         \
                                                                              \
static const int TEST_TABLE##n##_COUNT = ARRAY_LEN(test_table##n);            \
                                                                              \
static bool funcname(int N);                                                  \
                                                                              \
template <int N>                                                              \
static bool funcname##_tmpl() { return funcname(N); }                         \
                                                                              \
template <int N>                                                              \
static void driver_register_all##n(FunctionDriver &driver)                    \
{                                                                             \
    driver.register_function(funcname##_tmpl<N>);                             \
    driver_register_all##n<N+1>(driver);                                      \
}                                                                             \
                                                                              \
template <>                                                                   \
void driver_register_all##n<TEST_TABLE##n##_COUNT>(FunctionDriver &driver)    \
{                                                                             \
    (void) driver;                                                            \
}                                                                             \
                                                                              \

// one for each test_tableN and corresponding test function
TEST_TABLE_SETUP(1, test_ranger_misc_persist_load)
TEST_TABLE_SETUP(2, test_ranger_misc_load_persist_slice)
TEST_TABLE_SETUP(3, test_ranger_misc_contains)
TEST_TABLE_SETUP(4, test_ranger_misc_element)
TEST_TABLE_SETUP(5, test_ranger_misc_jobid_persist_load)
TEST_TABLE_SETUP(6, test_ranger_misc_contains_jobid)


//////////////////////////////////////////////


// misc individual tests

static bool test_ranger_initializer_list_elements();
static bool test_ranger_initializer_list_ranges();
static bool test_ranger_initializer_list_jobid_elements();
static bool test_ranger_initializer_list_jobid_ranges();


bool OTEST_ranger(void) {
    emit_object("ranger");
    emit_comment("This module contains functions for manipulating range "
        "mask objects, testing the load and persist functionality.");

    FunctionDriver driver;

    // one of these per TEST_TABLE_SETUP(N, ...) above
    driver_register_all1<0>(driver);
    driver_register_all2<0>(driver);
    driver_register_all3<0>(driver);
    driver_register_all4<0>(driver);
    driver_register_all5<0>(driver);
    driver_register_all6<0>(driver);

    driver.register_function(test_ranger_initializer_list_elements);
    driver.register_function(test_ranger_initializer_list_ranges);
    driver.register_function(test_ranger_initializer_list_jobid_elements);
    driver.register_function(test_ranger_initializer_list_jobid_ranges);

    return driver.do_all_functions();
}

//////////////////

template <class T>
static int ranger_load_insert_persist(const char *prev, const char *input,
                                      std::string &s)
{
    ranger<T> r;
    if (r.load(prev) < 0 || r.load(input)) {
        emit_alert("Unexpected error loading range spec for insert");
        return -1;
    }
    r.persist(s);
    return 0;
}

template <class T>
static int ranger_load_erase_persist(const char *prev, const char *input,
                                     std::string &s)
{
    ranger<T> r, e;
    if (r.load(prev) < 0 || e.load(input)) {
        emit_alert("Unexpected error loading range spec for erase");
        return -1;
    }
    for (auto &range : e)
        r.erase(range);
    r.persist(s);
    return 0;
}


// test a row in the test case table (load, insert/erase, persist)

static bool test_ranger_misc_persist_load(int N)
{
    const char *prev = N > 0 ? test_table1[N-1].expected_result : "";
    const testcase1 &t = test_table1[N];
    std::string s;
    s.reserve(256);

    emit_test("Test misc ranger manipulation");
    emit_input_header();
    emit_param("State", prev);
    emit_param("Input", t.input);
    emit_param("Action", t.action == Insert ? "insert" : "erase");

    int ret = (t.action == Insert ? ranger_load_insert_persist<int>
                                  : ranger_load_erase_persist<int>)
              (prev, t.input, s);

    if (ret != 0)
        FAIL;

    emit_output_expected_header();
    emit_retval(t.expected_result);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != t.expected_result)
        FAIL;
    PASS;
}


//////////////////


static int ranger_load_persist_slice(const char *input, int start, int back,
                                     std::string &s)
{
    ranger<int> r;
    if (r.load(input)) {
        emit_alert("Unexpected error loading range spec for persist slice");
        return -1;
    }
    r.persist_slice(s, start, back);

    return 0;
}


// test a row in the test case table (load, persist slice)

static bool test_ranger_misc_load_persist_slice(int N)
{
    char slicebuf[64];
    const testcase2 &t = test_table2[N];
    std::string s;
    s.reserve(256);

    sprintf(slicebuf, "%d-%d", t.start, t.back);

    emit_test("Test misc ranger load/persist slice");
    emit_input_header();
    emit_param("Input", t.input);
    emit_param("Slice", slicebuf);

    int ret = ranger_load_persist_slice(t.input, t.start, t.back, s);

    if (ret != 0)
        FAIL;

    emit_output_expected_header();
    emit_retval(t.expected_result);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != t.expected_result)
        FAIL;
    PASS;

}



//////////////////


static const char *boolstr(bool truth)
{
    return truth ? "true" : "false";
}

// test a row in the test case table (contains)

static bool test_ranger_misc_contains(int N)
{
    char elementbuf[64];
    const testcase3 &t = test_table3[N];

    sprintf(elementbuf, "%d", t.element);

    emit_test("Test misc ranger contains element");
    emit_input_header();
    emit_param("Input", t.input);
    emit_param("Element", elementbuf);

    ranger<int> r;
    if (r.load(t.input)) {
        emit_alert("Unexpected error loading range spec for contains");
        FAIL;
    }
    bool contains_element = r.contains(t.element);

    emit_output_expected_header();
    emit_retval(boolstr(t.expected_result));

    emit_output_actual_header();
    emit_retval(boolstr(contains_element));

    if (contains_element != t.expected_result)
        FAIL;
    PASS;
}




//////////////////


static int ranger_load_insert_persist_elt(const char *input, int elt,
                                          std::string &s)
{
    ranger<int> r;
    if (r.load(input) < 0) {
        emit_alert("Unexpected error loading range spec for element insert");
        return -1;
    }
    r.insert(elt);
    r.persist(s);
    return 0;
}

static int ranger_load_erase_persist_elt(const char *input, int elt,
                                         std::string &s)
{
    ranger<int> r, e;
    if (r.load(input) < 0) {
        emit_alert("Unexpected error loading range spec for element erase");
        return -1;
    }
    r.erase(elt);
    r.persist(s);
    return 0;
}


static bool test_ranger_misc_element(int N)
{
    char elementbuf[64];
    const testcase4 &t = test_table4[N];

    sprintf(elementbuf, "%d", t.element);

    std::string s;

    emit_test("Test misc ranger element-wise manipulation");
    emit_input_header();
    emit_param("Input", t.input);
    emit_param("Action", t.action == Insert ? "insert" : "erase");
    emit_param("Element", elementbuf);

    int ret = (t.action == Insert ? ranger_load_insert_persist_elt
                                  : ranger_load_erase_persist_elt)
              (t.input, t.element, s);

    if (ret != 0)
        FAIL;

    emit_output_expected_header();
    emit_retval(t.expected_result);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != t.expected_result)
        FAIL;
    PASS;
}



//////////////////


static bool test_ranger_initializer_list_elements()
{
    ranger<int> r = {1, 2, 3, 5, 10};

    const char *expected = "1-3;5;10";

    std::string s;
    r.persist(s);

    emit_test("Test initializer lists of elements working as expected");
    emit_input_header();
    emit_param("Input", "{1, 2, 3, 5, 10}");

    emit_output_expected_header();
    emit_retval(expected);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != expected)
        FAIL;
    PASS;
}

static bool test_ranger_initializer_list_ranges()
{
    ranger<int> r = {{20,30}, {40,50}, {60,80}};

    const char *expected = "20-29;40-49;60-79";

    std::string s;
    r.persist(s);

    emit_test("Test initializer lists of ranges working as expected");
    emit_input_header();
    emit_param("Input", "{{20,30}, {40,50}, {60,80}}");

    emit_output_expected_header();
    emit_retval(expected);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != expected)
        FAIL;
    PASS;
}


static bool test_ranger_initializer_list_jobid_elements()
{
    ranger<JOB_ID_KEY> r = {{1,1}, {1,2}, {1,3}, {5,5}, {10,10}};

    const char *expected = "1.1-1.3;5.5;10.10";

    std::string s;
    r.persist(s);

    emit_test("Test initializer lists of jobid elements working as expected");
    emit_input_header();
    emit_param("Input", "{{1,1}, {1,2}, {1,3}, {5,5}, {10,10}}");

    emit_output_expected_header();
    emit_retval(expected);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != expected)
        FAIL;
    PASS;
}

static bool test_ranger_initializer_list_jobid_ranges()
{
    ranger<JOB_ID_KEY> r = {{{1,20},{1,30}}, {{2,40},{2,50}}, {{3,60},{3,80}}};

    const char *expected = "1.20-1.29;2.40-2.49;3.60-3.79";

    std::string s;
    r.persist(s);

    emit_test("Test initializer lists of jobid ranges working as expected");
    emit_input_header();
    emit_param("Input", "{{{1,20},{1,30}}, {{2,40},{2,50}}, {{3,60},{3,80}}}");

    emit_output_expected_header();
    emit_retval(expected);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != expected)
        FAIL;
    PASS;
}


//////////////////


// test a row in the test case table (load, insert/erase, persist)

static bool test_ranger_misc_jobid_persist_load(int N)
{
    const char *prev = N > 0 ? test_table5[N-1].expected_result : "";
    const testcase5 &t = test_table5[N];
    std::string s;
    s.reserve(256);

    emit_test("Test misc ranger manipulation");
    emit_input_header();
    emit_param("State", prev);
    emit_param("Input", t.input);
    emit_param("Action", t.action == Insert ? "insert" : "erase");

    int ret = (t.action == Insert ? ranger_load_insert_persist<JOB_ID_KEY>
                                  : ranger_load_erase_persist<JOB_ID_KEY>)
              (prev, t.input, s);

    if (ret != 0)
        FAIL;

    emit_output_expected_header();
    emit_retval(t.expected_result);

    emit_output_actual_header();
    emit_retval(s.c_str());

    if (s != t.expected_result)
        FAIL;
    PASS;
}



//////////////////


// test a row in the test case table (contains jobid)

static bool test_ranger_misc_contains_jobid(int N)
{
    char elementbuf[64];
    const testcase6 &t = test_table6[N];

    sprintf(elementbuf, "%d.%d", t.element.cluster, t.element.proc);

    emit_test("Test misc ranger contains jobid element");
    emit_input_header();
    emit_param("Input", t.input);
    emit_param("Element", elementbuf);

    ranger<JOB_ID_KEY> r;
    if (r.load(t.input)) {
        emit_alert("Unexpected error loading range spec for contains");
        FAIL;
    }
    bool contains_element = r.contains(t.element);

    emit_output_expected_header();
    emit_retval(boolstr(t.expected_result));

    emit_output_actual_header();
    emit_retval(boolstr(contains_element));

    if (contains_element != t.expected_result)
        FAIL;
    PASS;
}



//////////////////

