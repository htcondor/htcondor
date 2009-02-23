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

# This is a program to assist in generating input files for the ProcD
# test program (in procd_test_controller.py). It takes as input a list
# of commands, and randomly creates an output file that consists of the
# given commands with valid arguments. See procd_test_input_template for
# an example of a file that can be used as input to this script.

import random
import sys

from procd_test_state import ProcDReference

def random_choice(command, choices):
    if not choices:
        sys.stderr.write('WARNING: ' + command + ' not possible; skipping\n')
        return None
    return random.choice(choices)

if __name__ == '__main__':

    ref = ProcDReference('INIT')
    next_proc = 0
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.rstrip()
        if line == 'SPAWN':
            parent = random_choice(line, ref.get_all_procs())
            if not parent:
                continue
            child = 'P' + str(next_proc)
            next_proc += 1
            sys.stdout.write('SPAWN ' + child + ' ' + parent + '\n')
            ref.spawn(child, parent)
        elif line == 'KILL':
            procs = ref.get_all_procs()
            procs.remove('INIT')
            victim = random_choice(line, procs)
            if not victim:
                continue
            sys.stdout.write('KILL ' + victim + '\n')
            ref.kill(victim)
        elif line == 'REGISTER':
            root_pid = 'P' + str(next_proc - 1)
            sys.stdout.write('REGISTER ' + root_pid + '\n')
            ref.register(root_pid, 0)
        elif line == 'UNREGISTER':
            families = ref.get_all_families()
            families.remove('INIT')
            root_pid = random_choice(line, families)
            if not root_pid:
                continue
            sys.stdout.write('UNREGISTER ' + root_pid + '\n')
            ref.unregister(root_pid)
        elif line == 'SNAPSHOT':
            sys.stdout.write('SNAPSHOT\n') 
        else:
            raise Exception('invalid command: ' + line)
