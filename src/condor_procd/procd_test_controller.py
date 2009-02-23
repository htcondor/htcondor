###########################################################################
#
#  Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
#  University of Wisconsin-Madison, WI.
#  
#  Licensed under the Apache License, Version 2.0 (the "License"); you
#  may not use this file except in compliance with the License.  You may
#  obtain a copy of the License at
#  
#     http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################

# this is the ProcD's test program. it is invoked with one argument: the
# name of an input file, which contains a list of commands for the test to
# execute. the commands have as arguments process "tags" which are
# an abstract way to refer to processes in the tree of "drone" processes
# that the test maintains (tags are used since the actual PIDs may change
# from one test run to another). commands include:
#   - SPAWN <child_tag> <parent_tag>
#   - KILL <tag>
#   - REGISTER <tag> [<watcher_tag>]
#   - UNREGISTER <tag>
# See procd_test_input for an example input file. Also, the
# procd_test_generate.py program can be used to take a list of commands
# without arguments and randomly generate arguments that work.

import os
import platform
import subprocess
import sys

from procd_test_drone_tree import DroneTree
from procd_test_state import ProcDState, ProcDReference, ProcDDump

# class for interacting with the ProcD we're testing
class ProcDInterface:

    def __init__(self, pipe_name):
        self.procd_ctl_path = './procd_ctl'
        self.procd_addr = pipe_name

    def register(self, proc, watcher):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'REGISTER_FAMILY',
                               str(proc),
                               str(watcher),
                               '-1'))
        if ret != 0:
            raise Exception('error result from procd_ctl REGISTER: ' +
                            str(ret))

    def unregister(self, proc):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'UNREGISTER_FAMILY',
                               str(proc)))
        if ret != 0:
            raise Exception('error result from procd_ctl UNREGISTER: ' +
                            str(ret))

    def snapshot(self):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'SNAPSHOT'))
        if ret != 0:
            raise Exception('error result from procd_ctl SNAPSHOT: ' +
                            str(ret))

    def quit(self):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'QUIT'))
        if ret != 0:
            raise Exception('error result from procd_ctl QUIT: ' +
                            str(ret))

    # sucks the state out of the ProcD and returns a ProcDState object
    def dump(self):
        p = subprocess.Popen((self.procd_ctl_path, self.procd_addr, 'DUMP'),
                             stdout = subprocess.PIPE)
        (root_pid,
         watcher_pid,
         proc_count) = map(int, p.stdout.readline().split()[1:])
        dump = ProcDDump(root_pid, watcher_pid)
        while True:
            for i in range(proc_count):
                pid = int(p.stdout.readline().split()[0])
                dump.new_proc(root_pid, pid)
            line = p.stdout.readline()
            if not line:
                break
            (parent_root_pid,
             root_pid,
             watcher_pid,
             proc_count) = map(int, line.split())
            dump.new_family(parent_root_pid, root_pid, watcher_pid)
        if p.wait() != 0:
            raise Exception('error result from procd_ctl DUMP: ' +
                            p.returncode)
        return dump

# top-level class to bring everything together
class ProcDTester:

    def __init__(self):

        if platform.system() == 'Windows':
            pipe_name = '\\\\.\\pipe\\procd_pipe.' + str(os.getpid())
        else:
            pipe_name = 'procd_pipe'

        self.drone_tree = DroneTree()

        init_pid = self.drone_tree.get_init_pid()
        procd_pid = self.drone_tree.spawn(init_pid,
            "./condor_procd -A " + pipe_name + " -L procd_log -S -1")

        self.procd_reference = ProcDReference(init_pid)
        self.procd_reference.spawn(procd_pid, init_pid)

        self.procd_interface = ProcDInterface(pipe_name)

        self.tag_mappings = {'INIT': init_pid, 'PROCD': procd_pid}

    def spawn(self, tag, parent_tag):
        ppid = self.tag_mappings[parent_tag]
        pid = self.drone_tree.spawn(ppid)
        self.procd_reference.spawn(pid, ppid)
        self.tag_mappings[tag] = pid

    def kill(self, tag):
        pid = self.tag_mappings[tag]
        self.drone_tree.kill(pid)
        self.procd_reference.kill(pid)

    def register(self, tag, watcher):
        pid = self.tag_mappings[tag]
        watcher_pid = 0
        if watcher is not None:
            watcher_pid = self.tag_mappings[watcher]
        self.procd_interface.register(pid, watcher_pid)
        self.procd_reference.register(pid, watcher_pid)

    def unregister(self, tag):
        pid = self.tag_mappings[tag]
        self.procd_interface.unregister(pid)
        self.procd_reference.unregister(pid)

    def snapshot(self):
        self.procd_interface.snapshot()

    def show_state(self, procd_dump = None):
        if procd_dump is None:
            procd_dump = self.procd_interface.dump()
        print '------ ProcD State ------'
        procd_dump.dump()
        print '------ Reference State ------'
        self.procd_reference.dump()
        print '------ Tag-to-PID Mappings ------'
        for tag, pid in sorted(self.tag_mappings.iteritems()):
            print '%s: %s' % (tag, pid)
        print '------------------------------------'

    def check_state(self):
        dump = self.procd_interface.dump()
        if not dump.equals(self.procd_reference):
            print 'ProcD state is incorrect!'
            self.show_state(dump)
            return False
        return True

    # run the test; commands are read from the given file;
    # if interactive is True, the test will wait for "Enter"
    # to be pressed in between each command
    def run(self, filename, interactive):
        ok = True
        f = open(filename, 'r')
        while True:
            line = f.readline()
            tokens = line.split()
            if interactive:
                self.show_state()
            if not tokens:
                break
            print 'Next command: ' + line.rstrip()
            if interactive:
                raw_input()
            if tokens[0] == 'SPAWN':
                self.spawn(tokens[1], tokens[2])
            elif tokens[0] == 'KILL':
                self.kill(tokens[1])
            elif tokens[0] == 'REGISTER':
                watcher = None
                if len(tokens) > 2:
                    watcher = tokens[2]
                self.register(tokens[1], watcher)
                ok = self.check_state()
            elif tokens[0] == 'UNREGISTER':
                self.unregister(tokens[1])
                ok = self.check_state()
            elif tokens[0] == 'SNAPSHOT':
                self.snapshot()
                ok = self.check_state()
            if not ok:
                break
        f.close()
        self.procd_interface.quit()
        return ok

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: %s <file>' % sys.argv[0]
        sys.exit(1)
    tester = ProcDTester()
    ok = tester.run(sys.argv[1], False)
    if not ok:
        sys.exit(1)
    sys.exit(0)
