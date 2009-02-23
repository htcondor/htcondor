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

import subprocess
import sys

# simple "abstract" class to represent ProcD state; provides
# methods for printing out the state and comparing two state
# objects; it's the reponsibilty of derived classes to set up
# the state member so it obeys the expected form
class ProcDState:

    class Family:
        pass

    def __init__(self):
        self.state = self.Family()

    def dump(self):
        def f(fam, indent):
            parent = None
            if fam.parent is not None:
                parent = fam.parent.root_pid
            print '%s%s: %s (W: %s, P: %s)' % (indent,
                                               fam.root_pid,
                                               sorted(fam.procs),
                                               fam.watcher,
                                               parent)
            for subfam in fam.children:
                f(subfam, indent + '    ')
        f(self.state, '')

    def equals(self, other):
        def f(mine, yours):
            if mine.root_pid != yours.root_pid:
                return False
            if mine.watcher != yours.watcher:
                return False
            if len(mine.children) != len(yours.children):
                return False
            key = lambda x: x.root_pid
            my_children = sorted(mine.children, key = key)
            your_children = sorted(yours.children, key = key)
            for (my_child, your_child) in zip(my_children, your_children):
                if not f(my_child, your_child):
                    return False
            return True
        return f(self.state, other.state)

# class for maintaining a reference of what the ProcD's state
# should be; used by the ProcD test to compare against the actual
# ProcD's state in order to detect errors
class ProcDReference(ProcDState):

    def __init__(self, init_proc):

        ProcDState.__init__(self)

        self.state.parent = None
        self.state.children = set()
        self.state.root_pid = init_proc
        self.state.watcher = init_proc
        self.state.procs = set([init_proc])

        # initialize some dictionaries for easy lookup of families
        self.families_by_root_pid = {init_proc: self.state}
        self.families_by_proc = {init_proc: self.state}

    def spawn(self, proc, parent_proc):

        # add proc to same family as parent
        fam = self.families_by_proc[parent_proc]
        fam.procs.add(proc)

        self.families_by_proc[proc] = fam

    def kill(self, proc):

        # remove proc from its family
        self.families_by_proc[proc].procs.remove(proc)

        # keep lookup-by-proc dict up to date
        del self.families_by_proc[proc]

        # unregister any now-unwatched families
        def f(fam):
            if fam.watcher != 0:
                if fam.watcher not in self.families_by_proc:
                    self.unregister(fam.root_pid)
            for subfam in fam.children:
                f(subfam)
        f(self.state)

    def register(self, proc, watcher):

        old_fam = self.families_by_proc[proc]

        new_fam = ProcDState()
        new_fam.root_pid = proc
        new_fam.procs = set([proc])
        new_fam.parent = old_fam
        new_fam.children = set()
        new_fam.watcher = watcher

        old_fam.procs.remove(proc)
        old_fam.children.add(new_fam)

        self.families_by_root_pid[proc] = new_fam
        self.families_by_proc[proc] = new_fam

    def unregister(self, root_pid):

        fam = self.families_by_root_pid[root_pid]

        # parent family inherits unregistered family's procs, children
        fam.parent.procs |= fam.procs
        for orphan in fam.children:
            orphan.parent = fam.parent
            fam.parent.children.add(orphan)
        fam.children.clear()

        fam.parent.children.remove(fam)

        # keep lookup tables consistent
        del self.families_by_root_pid[root_pid]
        for proc in fam.procs:
            self.families_by_proc[proc] = fam.parent

    def get_all_procs(self):
        return self.families_by_proc.keys()

    def get_all_families(self):
        return self.families_by_root_pid.keys()

# class for storing a dump of the ProcD's state; used to represent the
# actual ProcD's state. a ProcDDump object is built up using the output
# from the "procd_ctl DUMP" command
class ProcDDump(ProcDState):

    def __init__(self, root_pid, watcher_pid):
        ProcDState.__init__(self)
        self.state.parent = None
        self.state.children = set()
        self.state.root_pid = root_pid
        self.state.watcher = watcher_pid
        self.state.procs = set()
        self.families_by_root_pid = {root_pid: self.state}

    def new_family(self, parent_root_pid, root_pid, watcher_pid):
        child = ProcDState.Family()
        parent = self.families_by_root_pid[parent_root_pid]
        parent.children.add(child)
        child.parent = parent
        child.children = set()
        child.root_pid = root_pid
        child.watcher = watcher_pid
        child.procs = set()
        self.families_by_root_pid[root_pid] = child

    def new_proc(self, family_root_pid, pid):
        self.families_by_root_pid[family_root_pid].procs.add(pid)
