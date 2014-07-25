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
#include "condor_attributes.h"

#include "consumption_policy.h"

#include <map>

#define BOOST_TEST_MAIN
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>

#define BOOST_TEST_MODULE consumption_policy

using std::string;
using std::map;

// fixture for building a resource (slot) ad and a request (job) ad
struct cpfix {
    cpfix() {
        string attr;

        // currently (circa 8.1.1) there is some weird interaction between
        // classad caching and calls to quantize() where 2nd arg is a list,
        // so just disabling caching for the time being.
        classad::ClassAdSetExpressionCaching(false);

        // construct request (job) ad with requested asset values    
        formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_CPUS);
        request.Assign(attr.c_str(), 1);

        formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_MEMORY);
        request.Assign(attr.c_str(), 2);

        formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_DISK);
        request.Assign(attr.c_str(), 3);

        formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
        request.Assign(attr.c_str(), 4);


        // construct a resource with consumption policy
        resource.Assign(ATTR_SLOT_PARTITIONABLE, true);
        resource.Assign(ATTR_MACHINE_RESOURCES, "cpus memory disk actuators");

        formatstr(attr, "%s%s = quantize(target.%s%s, {2})", ATTR_CONSUMPTION_PREFIX, ATTR_CPUS, ATTR_REQUEST_PREFIX, ATTR_CPUS);
        resource.Insert(attr.c_str());

        formatstr(attr, "%s%s = quantize(target.%s%s, {128})", ATTR_CONSUMPTION_PREFIX, ATTR_MEMORY, ATTR_REQUEST_PREFIX, ATTR_MEMORY);
        resource.Insert(attr.c_str());

        formatstr(attr, "%s%s = quantize(target.%s%s, {1024})", ATTR_CONSUMPTION_PREFIX, ATTR_DISK, ATTR_REQUEST_PREFIX, ATTR_DISK);
        resource.Insert(attr.c_str());

        formatstr(attr, "%s%s = ifthenelse(target.%s%s =?= 0, 3, quantize(target.%s%s, {7}))", ATTR_CONSUMPTION_PREFIX, "Actuators", ATTR_REQUEST_PREFIX, "Actuators", ATTR_REQUEST_PREFIX, "Actuators");
        resource.Insert(attr.c_str());

        // provision it with some assets
        resource.Assign(ATTR_CPUS, 3);
        resource.Assign(ATTR_MEMORY, 200);
        resource.Assign(ATTR_DISK, 2000);
        resource.Assign("Actuators", 10);
    }
    virtual ~cpfix() {}

    ClassAd request;
    ClassAd resource;
};


BOOST_AUTO_TEST_CASE(cp_supports_policy_1) {
    ClassAd ad;
    string attr;

    // empty ad, doesn't support
    BOOST_CHECK(!cp_supports_policy(ad));

    // necessary, but not sufficient
    ad.Assign(ATTR_SLOT_PARTITIONABLE, true);
    ad.Assign(ATTR_MACHINE_RESOURCES, "cpus memory disk");
    BOOST_CHECK(!cp_supports_policy(ad));

    // not yet
    formatstr(attr, "%s%s", ATTR_CONSUMPTION_PREFIX, "Cpus");
    ad.Assign(attr.c_str(), 0);
    BOOST_CHECK(!cp_supports_policy(ad));
 
    // sooo close
    formatstr(attr, "%s%s", ATTR_CONSUMPTION_PREFIX, "Memory");
    ad.Assign(attr.c_str(), 0);
    BOOST_CHECK(!cp_supports_policy(ad));
   
    // this ad should support consumption policy:
    formatstr(attr, "%s%s", ATTR_CONSUMPTION_PREFIX, "Disk");
    ad.Assign(attr.c_str(), 0);
    BOOST_CHECK(cp_supports_policy(ad));

    // test with an extensible resource
    ad.Assign(ATTR_MACHINE_RESOURCES, "cpus memory disk railgun");
    
    // fail
    BOOST_CHECK(!cp_supports_policy(ad));

    // now it should support:
    formatstr(attr, "%s%s", ATTR_CONSUMPTION_PREFIX, "Railgun");
    ad.Assign(attr.c_str(), 0);
    BOOST_CHECK(cp_supports_policy(ad));

    // insufficient!
    ClassAd ad2 = ad;
    ad2.Delete(ATTR_SLOT_PARTITIONABLE);
    BOOST_CHECK(!cp_supports_policy(ad2));

    // insufficient!
    ad2 = ad;
    ad2.Delete(ATTR_MACHINE_RESOURCES);
    BOOST_CHECK(!cp_supports_policy(ad2));
}


BOOST_FIXTURE_TEST_CASE(cp_compute_1, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    consumption_map_t consumption;
    cp_compute_consumption(request, resource, consumption);

    BOOST_CHECK_EQUAL((int)consumption["cpus"], 2);
    BOOST_CHECK_EQUAL((int)consumption["memory"], 128);
    BOOST_CHECK_EQUAL((int)consumption["disk"], 1024);
    BOOST_CHECK_EQUAL((int)consumption["actuators"], 7);
}


BOOST_FIXTURE_TEST_CASE(cp_compute_2, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    // remove this requested attribute, test defaulting behavior for
    // requested-asset attributes for resource consumption
    string attr;
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    request.Delete(attr.c_str());

    consumption_map_t consumption;
    cp_compute_consumption(request, resource, consumption);

    BOOST_CHECK_EQUAL((int)consumption["cpus"], 2);
    BOOST_CHECK_EQUAL((int)consumption["memory"], 128);
    BOOST_CHECK_EQUAL((int)consumption["disk"], 1024);
    BOOST_CHECK_EQUAL((int)consumption["actuators"], 3);

    // make sure missing status is restored - should not be present
    int v = 0;
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    BOOST_CHECK(!request.LookupInteger(attr.c_str(), v));
}


BOOST_FIXTURE_TEST_CASE(cp_compute_3, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    // test overriding behavior of _condor_RequestedXxx attributes
    string attr;
    formatstr(attr, "_condor_%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    request.Assign(attr.c_str(), 9);

    consumption_map_t consumption;
    cp_compute_consumption(request, resource, consumption);

    BOOST_CHECK_EQUAL((int)consumption["cpus"], 2);
    BOOST_CHECK_EQUAL((int)consumption["memory"], 128);
    BOOST_CHECK_EQUAL((int)consumption["disk"], 1024);
    BOOST_CHECK_EQUAL((int)consumption["actuators"], 14);

    // make sure original value is restored properly
    int v = 0;
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    BOOST_CHECK(request.LookupInteger(attr.c_str(), v));
    BOOST_CHECK_EQUAL(v, 4);
}


BOOST_FIXTURE_TEST_CASE(cp_override_1, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    consumption_map_t consumption;
    cp_override_requested(request, resource, consumption);

    string attr;
    int v;
    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_CPUS);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 1);
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_CPUS);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 2);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_MEMORY);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 2);
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_MEMORY);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 128);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_DISK);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 3);
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_DISK);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 1024);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 4);
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 7);
}


BOOST_FIXTURE_TEST_CASE(cp_restore_1, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    // override the values
    consumption_map_t consumption;
    cp_override_requested(request, resource, consumption);

    // now restore them and verify the result
    cp_restore_requested(request, consumption);
    
    string attr;
    int v;
    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_CPUS);
    BOOST_CHECK(!request.LookupInteger(attr.c_str(), v));
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_CPUS);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 1);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_MEMORY);
    BOOST_CHECK(!request.LookupInteger(attr.c_str(), v));
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_MEMORY);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 2);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, ATTR_DISK);
    BOOST_CHECK(!request.LookupInteger(attr.c_str(), v));
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, ATTR_DISK);
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 3);

    formatstr(attr, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    BOOST_CHECK(!request.LookupInteger(attr.c_str(), v));
    formatstr(attr, "%s%s", ATTR_REQUEST_PREFIX, "Actuators");
    request.LookupInteger(attr.c_str(), v);
    BOOST_CHECK_EQUAL(v, 4);
}


BOOST_FIXTURE_TEST_CASE(cp_sufficient_1, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));
    BOOST_CHECK(cp_sufficient_assets(request, resource));

    resource.Assign(ATTR_CPUS, 1);
    BOOST_CHECK(!cp_sufficient_assets(request, resource));

    resource.Assign(ATTR_CPUS, 3);
    resource.Assign(ATTR_MEMORY, 127);
    BOOST_CHECK(!cp_sufficient_assets(request, resource));

    resource.Assign(ATTR_MEMORY, 200);
    resource.Assign(ATTR_DISK, 1023);
    BOOST_CHECK(!cp_sufficient_assets(request, resource));

    resource.Assign(ATTR_DISK, 2000);
    resource.Assign("Actuators", 6);
    BOOST_CHECK(!cp_sufficient_assets(request, resource));

    resource.Assign("Actuators", 10);
    BOOST_CHECK(cp_sufficient_assets(request, resource));
}


BOOST_FIXTURE_TEST_CASE(cp_sufficient_2, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    consumption_map_t consumption;
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(cp_sufficient_assets(resource, consumption));

    resource.Assign(ATTR_CPUS, 1);
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(!cp_sufficient_assets(resource, consumption));

    resource.Assign(ATTR_CPUS, 3);
    resource.Assign(ATTR_MEMORY, 127);
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(!cp_sufficient_assets(resource, consumption));

    resource.Assign(ATTR_MEMORY, 200);
    resource.Assign(ATTR_DISK, 1023);
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(!cp_sufficient_assets(resource, consumption));

    resource.Assign(ATTR_DISK, 2000);
    resource.Assign("Actuators", 6);
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(!cp_sufficient_assets(resource, consumption));

    resource.Assign("Actuators", 10);
    cp_compute_consumption(request, resource, consumption);
    BOOST_CHECK(cp_sufficient_assets(resource, consumption));
}


BOOST_FIXTURE_TEST_CASE(cp_deduct_1, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    resource.AssignExpr(ATTR_SLOT_WEIGHT, "Cpus");
    BOOST_CHECK_EQUAL((int)cp_deduct_assets(request, resource), 2);

    int v;
    BOOST_CHECK(resource.LookupInteger(ATTR_CPUS, v));
    BOOST_CHECK_EQUAL(v, 3-2);
    BOOST_CHECK(resource.LookupInteger(ATTR_MEMORY, v));
    BOOST_CHECK_EQUAL(v, 200-128);
    BOOST_CHECK(resource.LookupInteger(ATTR_DISK, v));
    BOOST_CHECK_EQUAL(v, 2000-1024);
    BOOST_CHECK(resource.LookupInteger("Actuators", v));
    BOOST_CHECK_EQUAL(v, 10-7);
}


BOOST_FIXTURE_TEST_CASE(cp_deduct_2, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    resource.AssignExpr(ATTR_SLOT_WEIGHT, "Actuators");
    BOOST_CHECK_EQUAL((int)cp_deduct_assets(request, resource), 7);

    int v;
    BOOST_CHECK(resource.LookupInteger(ATTR_CPUS, v));
    BOOST_CHECK_EQUAL(v, 3-2);
    BOOST_CHECK(resource.LookupInteger(ATTR_MEMORY, v));
    BOOST_CHECK_EQUAL(v, 200-128);
    BOOST_CHECK(resource.LookupInteger(ATTR_DISK, v));
    BOOST_CHECK_EQUAL(v, 2000-1024);
    BOOST_CHECK(resource.LookupInteger("Actuators", v));
    BOOST_CHECK_EQUAL(v, 10-7);
}


BOOST_FIXTURE_TEST_CASE(cp_deduct_3, cpfix) {
    BOOST_CHECK(cp_supports_policy(resource));

    resource.AssignExpr(ATTR_SLOT_WEIGHT, "Actuators");
    BOOST_CHECK_EQUAL((int)cp_deduct_assets(request, resource, true), 7);

    int v;
    BOOST_CHECK(resource.LookupInteger(ATTR_CPUS, v));
    BOOST_CHECK_EQUAL(v, 3);
    BOOST_CHECK(resource.LookupInteger(ATTR_MEMORY, v));
    BOOST_CHECK_EQUAL(v, 200);
    BOOST_CHECK(resource.LookupInteger(ATTR_DISK, v));
    BOOST_CHECK_EQUAL(v, 2000);
    BOOST_CHECK(resource.LookupInteger("Actuators", v));
    BOOST_CHECK_EQUAL(v, 10);
}
