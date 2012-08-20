/***************************************************************
 *
 * Copyright (C) 1990-2012, Red Hat Inc.
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

#define BOOST_TEST_MAIN
#define BOOST_AUTO_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <string.h>

/////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_SUITE( collector_auto )
#define BOOST_TEST_MODULE GRNN test suite

// --------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( example_test )
{
    BOOST_CHECK( strcmp( "foo", "foo" ) == 0 );


    BOOST_CHECK_NO_THROW(); 
}

//BOOST_AUTO_TEST_SUITE_END()
//////////////////////////////////////////////////////////////////
