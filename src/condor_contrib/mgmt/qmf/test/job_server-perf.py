#!/usr/bin/env python
#
# Copyright 2009-2011 Red Hat, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
from qmf.console import Session, Console
from sys import exit, argv
import time
import threading

lock = threading.Lock()
condition = threading.Condition(lock)

class EmptyConsole(Console):
    def __init__(self):
        self.count = 0
    def methodResponse(self, broker, seq, response):
        condition.acquire()
        self.count += 1
        condition.notify()
        condition.release()
#        print broker, seq, response

url = len(argv) > 1 and argv[1] or "amqp://localhost:5672"
count = len(argv) > 2 and int(argv[2]) or 10000
submission = len(argv) > 3 and argv[3] or "submission"
klass =  "JobServer"
package = "com.redhat.grid"

console = EmptyConsole()

session = Session(console);
broker = session.addBroker(url)
servers = session.getObjects(_class=klass, _package=package)
server = servers[0]

print "Found server:", server
print "Iteractions:", count
print "Submission:", submission

start = time.time()

print "Start:", start

for i in xrange(count):
    r = session._sendMethodRequest(server.getBroker(),
                                   server.getClassKey(),
                                   server.getObjectId(),
                                   "GetJobAds",
                                   (submission,))

#    r = server.GetJobs(submission);
#    print r

#    r = echoer.echo(i, message, _async=True)
#    if r.status or r.sequence != i:
#        print "text:", r.text, "sequence:", r.sequence, "(expected:", i, ")"
#        count = i
#        break
#    if r.body != message:
#        print "message mismatch, received:", r.body
#        count = i
#        break

condition.acquire()
while console.count < count:
    condition.wait()
condition.release()

end = time.time()

print "End:", end
print "Total messages:", console.count, count
print "Total time:", end-start, "seconds"
print "Response rate:", count/(end-start), "Hz"

session.delBroker(broker)
