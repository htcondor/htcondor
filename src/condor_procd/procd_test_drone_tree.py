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

# module providing the DroneTree class for managing a tree of processes
# for testing the ProcD. the program executed by the actual "drone"
# processes is in procd_test_drone.py. this module is used by the
# "controller" process to manage the drones

import os
import socket
import subprocess

class DroneTree:

    # creates a drone tree object and starts its initial drone
    def __init__(self):

        # initialize our listening socket for communication with drones
        self.listen_sock = socket.socket()
        self.listen_sock.bind(('127.0.0.1', 0))
        self.listen_sock.listen(5)

        # start the first drone
        self.listen_port = self.listen_sock.getsockname()[1]
        self.init_drone = subprocess.Popen(('python',
                                            'procd_test_drone.py',
                                            str(self.listen_port)),
                                           close_fds = True)

        # save the initial drone's PID
        self.init_drone_pid = self.init_drone.pid

        # establish our connection with the initial drone
        init_drone_sock = self.accept_new_drone()

        # for each drone we need to store our socket connecting
        # us to it and its parent PID (which is needed so that when
        # we tell a drone to die we can then tell its parent to reap
        # it)
        self.drone_info = {self.init_drone_pid: (init_drone_sock, os.getpid())}

    # this gets called when we are expecting a newly created drone to
    # connect to us; it returns a file object that wraps a socket
    # connected to the new drone
    def accept_new_drone(self):
        s = self.listen_sock.accept()[0]
        sock = s.makefile()
        s.close()
        return sock

    # get the PID of the initial drone
    def get_init_pid(self):
        return self.init_drone_pid

    # tell the drone with the given PID to spawn a new drone;
    # alternatively, a custom command line can be given to have
    # something else get launched (like the ProcD); returns the
    # PID of the newly created process
    def spawn(self, parent_pid, cmdline = None):

        if cmdline is None:
            is_drone = True
            cmdline = 'python procd_test_drone.py ' + str(self.listen_port)
        else:
            is_drone = False

        # send the spawn command to the parent drone then accept the
        # new drone, saving its socket connection and PID
        parent_sock = self.drone_info[parent_pid][0]
        parent_sock.write('SPAWN ' + cmdline + '\n')
        parent_sock.flush()

        # read the new process's PID from it's parent
        child_pid = int(parent_sock.readline())

        # if we're spawning a new drone, accept it and save its
        # information
        if is_drone:
            child_sock  = self.accept_new_drone()
            self.drone_info[child_pid] = (child_sock, parent_pid)

        # finally, return the new process's PID
        return child_pid

    # instruct the given drone to reap the given PID; note that a drone
    # is automatically told to get reaped by its parent if it is killed
    # via the kill() method; this is mostly just useful for processes
    # created via spawn() with a custom cmdline
    def reap(self, parent_pid, child_pid):
        parent_sock = self.drone_info[parent_pid][0]
        parent_sock.write('REAP ' + str(child_pid) + '\n')
        parent_sock.flush()
        parent_sock.readline()

    # kill the drone with the given PID and ensure it gets reaped
    # (TODO: come up with a way to do this for processes that have
    # been reparented to init)
    def kill(self, pid):

        # pop the drone's info
        (sock, ppid) = self.drone_info[pid]
        del self.drone_info[pid]

        # close the drone's connection, which tells it to exit
        sock.close()

        # reap it ourself if it's the initial drone; otherwise if its
        # parent is still around tell it to reap
        if ppid == os.getpid():
            self.init_drone.wait()
            del self.init_drone
        elif ppid in self.drone_info:
            self.reap(ppid, pid)

        # TODO: invoke a C++ tool that uses ProcAPI to block until
        # the process is really gone
        pass
