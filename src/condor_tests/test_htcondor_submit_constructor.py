#!/usr/bin/env pytest

import logging
import htcondor

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

class TestHTCondorSubmitConstructor:
    def test_passing_none(self):
        # This used to segfault.
        s = htcondor.Submit({"attribute": None})
