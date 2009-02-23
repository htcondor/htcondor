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

# this is a helper program used to execute the "drones" managed by the
# procd_test_drone_tree module. the basic idea is to make a TCP
# connection back to the the process using the procd_test_drone_tree
# module), then loop processing commands until the connection is closed,
# at which point we exit

import socket
import subprocess
import sys

controller_port = int(sys.argv[1])

s = socket.socket()
s.connect(('127.0.0.1', controller_port))
sock = s.makefile()
s.close()

procs = {}

while True:

    cmd_argv = sock.readline().split()

    if not cmd_argv:
        sys.exit(0)

    elif cmd_argv[0] == 'SPAWN':
        if len(cmd_argv) < 2:
            raise Exception('usage: SPAWN <arg> [<arg> ...]')
        proc = subprocess.Popen(cmd_argv[1:], close_fds = True)
        procs[proc.pid] = proc
        sock.write(str(proc.pid) + '\n')
        sock.flush()

    elif cmd_argv[0] == 'REAP':
        if len(cmd_argv) != 2:
            raise Exception('usage: REAP <pid>')
        pid = int(cmd_argv[1])
        procs[pid].wait()
        del procs[pid]
        sock.write('\n')
        sock.flush()
        
    else:
        raise Exception('invalid command: ' + cmd_argv[0])
