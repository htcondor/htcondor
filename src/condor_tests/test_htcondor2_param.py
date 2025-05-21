#!/usr/bin/env pytest

import os
import htcondor2

class TestHTCondor2Param:

    def test_param(self):
        os.environ['_CONDOR_FOO'] = 'BAR'
        os.environ['_CONDOR_EMPTY'] = '';
        htcondor2.reload_config()

        assert "FOO" in htcondor2.param.keys()
        assert "FOO" in htcondor2.param
        assert htcondor2.param["FOO"] == "BAR"

        assert not "EMPTY" in htcondor2.param.keys()
        assert not "EMPTY" in htcondor2.param

        assert not "OOF" in htcondor2.param.keys()
        assert not "OOF" in htcondor2.param.keys()
