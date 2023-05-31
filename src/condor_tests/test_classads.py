#!/usr/bin/env pytest

import pytest

import datetime

from htcondor2 import classad
# import classad

class TestClassAds:

    #
    # As of ddf0f0569a1825bc1d04ad8e90acd53bd5ed3e07, we need to test:
    #
    #  * classad.ClassAd(), .ClassAd(string), .ClassAd(dictionary)
    #  * __repr__, __str__, and round-trips
    #  * __delitem__
    #  * __getitem__, __setitem__ for/with:
    #    * ClassAds, lists (and recursion/intermixing), dictionaries
    #    - booleans, strings, integers, floats, byte strings
    #    - datetime.datetime (see below)
    #    - error, undefined [classad.Value]
    #    * ExprTrees / expressions
    #

    def test_data_types(self):
        c = classad.ClassAd()

        c["a"] = True
        assert c["a"] is True

        c["b"] = False
        assert c["b"] is False

        c["c"] = -1.1
        assert c["c"] == -1.1

        c["v"] = classad.Value.Undefined
        assert c["v"] == classad.Value.Undefined

        c["w"] = classad.Value.Error
        assert c["w"] == classad.Value.Error

        c["x"] = 7
        assert c["x"] == 7

        c["y"] = "eight"
        assert c["y"] == "eight"

        # In version 1, we converted byte strings to unicode strings.
        c["z"] = b"seventy-five"
        assert c["z"] == "seventy-five"

        d = classad.ClassAd()
        c["nested_ad"] = d
        assert c["nested_ad"] == d
        assert c["nested_ad"] is not d

        d["a"] = "a"
        assert c["nested_ad"] != d
        assert c["nested_ad"] is not d

        e = classad.ClassAd()
        e["c"] = c
        assert e["c"] == c
        assert e["c"] is not c


    # The v1 implementation of the ClassAd language's AbsTime type stores
    # all datetime.datetime objects as AbsTimes in UTC, which is wrong.
    # Instead, we adopt the Python convention that naive datetime.datetime
    # values are in the local timezone, and adjust them as appropriate.
    #
    # This is not bug-for-bug compatible, with version 1.
    #
    # For development convenience, this test detects the implementation and
    # makes the appropriate assertions.

    def test_ad_datetime(self):
        ad = classad.ClassAd()

        # Test an aware datetime
        then = datetime.datetime.now(tz=datetime.timezone.utc)
        ad["aware"] = then
        if 'classad2' in str(type(ad)):
            assert then.isoformat(sep=' ', timespec='seconds') == str(ad["aware"])
        else:
            assert then.isoformat(sep=' ', timespec='seconds') == f'{str(ad["aware"])}+00:00'

        # Test a naive datetime
        now = datetime.datetime.now()
        ad["naive"] = now

        if 'classad2' in str(type(ad)):
            # ClassAd time attributes are always stored in UTC, so convert
            # the naive datetime before comparing.
            offset = now.astimezone().utcoffset()
            now = now - offset

            assert f"{now.isoformat(sep=' ', timespec='seconds')}+00:00" == str(ad["naive"])
        else:
            assert f"{now.isoformat(sep=' ', timespec='seconds')}" == str(ad["naive"])

