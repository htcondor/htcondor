#!/usr/bin/env pytest

import pytest

import datetime

# The v1 implementation of the ClassAd language's AbsTime type is wrong,
# so even if you switch the other two paired comment/uncommented lines,
# this test will fail.  Because the (ClassAd) AbsTime value type requires
# a timezone, I prefer to convert naive (Python) datetime.datime values
# to UTC according to the convention the Python documentation mentions,
# which is that all naive types are in "local" (system) time.
#
# This is not bug-for-bug compatible, with version 1.

from htcondor2 import classad
# import classad

class TestClassAds:
    def test_ad_datetime(self):
        ad = classad.ClassAd()

        # Test an aware datetime
        then = datetime.datetime.now(tz=datetime.timezone.utc)
        ad["aware"] = then
        assert then.isoformat(sep=' ', timespec='seconds') == str(ad["aware"])
        # assert then.isoformat(sep=' ', timespec='seconds') == f'{str(ad["aware"])}+00:00'

        # Test a naive datetime
        now = datetime.datetime.now()
        ad["naive"] = now

        # ClassAd time attributes are always stored in UTC, so convert
        # the naive datetime before comparing.
        offset = now.astimezone().utcoffset()
        now = now - offset

        assert f"{now.isoformat(sep=' ', timespec='seconds')}+00:00" == str(ad["naive"])
        # assert f"{now.isoformat(sep=' ', timespec='seconds')}" == str(ad["naive"])

