#!/usr/bin/env pytest

import logging
import htcondor2 as htcondor

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

class TestHTCondorSubmitConstructor:
    def test_passing_none(self):
        # This used to segfault in version 1.
        try:
            s = htcondor.Submit({"attribute": None})
        except TypeError:
            pass
