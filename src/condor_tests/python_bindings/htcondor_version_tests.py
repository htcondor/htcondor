#!/usr/bin/python3

import os
import sys
import errno
import platform
import unittest

class TestConfig(unittest.TestCase):

    def setUp(self):
        os.environ["_condor_FOO"] = "BAR"
        htcondor.reload_config()

    def test_config(self):
        self.assertEqual(htcondor.param["FOO"], "BAR")

    def test_reconfig(self):
        htcondor.param["FOO"] = "BAZ"
        self.assertEqual(htcondor.param["FOO"], "BAZ")
        os.environ["_condor_FOO"] = "1"
        htcondor.reload_config()
        self.assertEqual(htcondor.param["FOO"], "1")


class TestClassadExtensions(unittest.TestCase):

    def test_user_home(self):
        if platform.system() == 'Windows':
            self.assertEqual(classad.ExprTree('userHome("foo","bar")').eval(), classad.Value.Error)
            return
        import pwd
        pw = pwd.getpwuid(os.geteuid())
        user = pw.pw_name
        home = pw.pw_dir

        htcondor.param['CLASSAD_ENABLE_USER_HOME'] = 'true'
        self.assertRaises(TypeError, classad.ExprTree('userHome()').eval)
        self.assertRaises(TypeError, classad.ExprTree('userHome("a", "b", "c")').eval)
        self.assertEqual(classad.ExprTree('userHome("", "")').eval(), classad.Value.Undefined)
        self.assertEqual(classad.ExprTree('userHome("", 1)').eval(), classad.Value.Undefined)
        self.assertEqual(classad.ExprTree('userHome("")').eval(), classad.Value.Undefined)
        self.assertEqual(classad.ExprTree('userHome(%s)' % classad.quote(user)).eval(), home)
        self.assertEqual(classad.ExprTree('userHome(%s, "foo")' % classad.quote(user)).eval(), home)
        self.assertEqual(classad.ExprTree('userHome("", "foo")').eval(), "foo")
        self.assertEqual(classad.ExprTree('userHome(undefined)').eval(), classad.Value.Undefined)
        self.assertEqual(classad.ExprTree('userHome(undefined, "foo")').eval(), "foo")
        self.assertEqual(classad.ExprTree('userHome(1, "foo")').eval(), "foo")
        self.assertEqual(classad.ExprTree('userHome(1)').eval(), classad.Value.Error)
        htcondor.param['CLASSAD_ENABLE_USER_HOME'] = 'false'
        self.assertEqual(classad.ExprTree('userHome(%s)' % classad.quote(user)).eval(), classad.Value.Undefined)


class TestVersion(unittest.TestCase):

    def setUp(self):
        fd = os.popen("condor_version")
        self.lines = []
        for line in fd.readlines():
            self.lines.append(line.strip())
        if fd.close():
            raise RuntimeError("Unable to invoke condor_version")

    def test_version(self):
        self.assertEqual(htcondor.version(), self.lines[0])

    def test_platform(self):
        self.assertEqual(htcondor.platform(), self.lines[1])


def makedirs_ignore_exist(directory):
    try:
        os.makedirs(directory)
    except:
        exctype, oe = sys.exc_info()[:2]
        if not issubclass(exctype, OSError): raise
        if oe.errno != errno.EEXIST:
            raise


# Bootstrap condor
testdir = os.path.join(os.getcwd(), "tests_tmp")
logdir = os.path.join(testdir, "log")
makedirs_ignore_exist(testdir)
makedirs_ignore_exist(logdir)
config_file = os.path.join(testdir, "condor_config")
open(config_file, "w").close()
os.environ["CONDOR_CONFIG"] = config_file
os.environ["_condor_TOOL_LOG"] = os.path.join(logdir, "ToolLog")
import htcondor
import classad
htcondor.enable_classad_extensions()

if __name__ == '__main__':
    unittest.main()

