#!/usr/bin/python

#
# Copyright 2010 Red Hat, Inc.
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

import sys
import time

from optparse import OptionParser
from qmf.console import Session, Console
from qpid.disp import Display

class Trigger:
    def __init__(self, name, query, text, id=None):
        self.id = id
        self.name = name
        self.query = query 
        self.text = text
    
    def __repr__(self):
        return [self.id, self.name, self.query, self.text]

class TriggerTester(Console):
    def __init__(self):
        self.event_received = False

    def get_event_received(self):
        return self.event_received

    def reset_event_received(self):
        self.event_received = False

    def event(self, broker, event):
        cls = event.getClassKey().getClassName()
        if cls == "CondorTriggerNotify":
            self.event_received = True
            print "Received 'CondorTriggerNotify' event"
            for attr in event.getArguments():
                print '%s: %s' % (attr, event.arguments[attr])
        else:
            print "Received '%s' event, ignoring" % (cls)

class TriggerConfig(Session):
    def __init__(self, test=False):
        self.cts = None
        if test:
            self.tester = TriggerTester()
            Session.__init__(self,
                             console=self.tester,
                             rcvObjects=False,
                             rcvEvents=True,
                             rcvHeartbeats=False,
                             manageConnections=False)
        else:
            Session.__init__(self,
                             rcvObjects=False,
                             rcvEvents=False,
                             rcvHeartbeats=False,
                             manageConnections=False)

    def add_default_triggers(self):
        defaultTriggers = [
            Trigger("High CPU Usage", 
                    "(TriggerdLoadAvg1Min > 5)", 
                    "$(Machine) has a high load average ($(TriggerdLoadAvg1Min))"),
            Trigger("Low Free Mem", 
                    "(TriggerdMemFree <= 10240)", 
                    "$(Machine) has low free memory available ($(TriggerdMemFree )k)"),
            Trigger("Low Free Disk Space (/)", 
                    "(TriggerdFilesystem_slash_Free < 10240)", 
                    "$(Machine) has low disk space on the / partition ($(TriggerdFilesystem_slash_Free))"),
            Trigger("Busy and Swapping", 
                    "(State == \"Claimed\" && Activity == \"Busy\" && TriggerdSwapInKBSec > 1000 && TriggerdActivityTime > 300)", 
                    "$(Machine) has been Claimed/Busy for $(TriggerdActivityTime) seconds and is swapping in at $(TriggerdSwapInKBSec) K/sec"),
            Trigger("Busy but Idle", 
                    "(State == \"Claimed\" && Activity == \"Busy\" && CondorLoadAvg < 0.3 && TriggerdActivityTime > 300)", 
                    "$(Machine) has been Claimed/Busy for $(TriggerdActivityTime) seconds but only has a load of $(CondorLoadAvg)"),
            Trigger("Idle for long time", 
                    "(State == \"Claimed\" && Activity == \"Idle\" && TriggerdActivityTime > 300)", 
                    "$(Machine) has been Claimed/Idle for $(TriggerdActivityTime) seconds"),
            Trigger("Logs with ERROR entries", 
                    "(TriggerdCondorLogCapitalError != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogCapitalErrorCount) ERROR messages in the following log files: $(TriggerdCondorLogCapitalError)"),
            Trigger("Logs with error entries", 
                    "(TriggerdCondorLogLowerError != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogLowerErrorCount) error messages in the following log files: $(TriggerdCondorLogLowerError)"),
            Trigger("Logs with DENIED entries", 
                    "(TriggerdCondorLogCapitalDenied != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogCapitalDeniedCount) DENIED Error messages in the following log files: $(TriggerdCondorLogCapitalDenied)"),
            Trigger("Logs with denied entries", 
                    "(TriggerdCondorLogLowerDenied != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogLowerDeniedCount) denied Error messages in the following log files: $(TriggerdCondorLogLowerDenied)"),
            Trigger("Logs with WARNING entries", 
                    "(TriggerdCondorLogCapitalWarning != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogCapitalWarningCount) WARNING messages in the following log files: $(TriggerdCondorLogCapitalWarning)"),
            Trigger("Logs with warning entries", 
                    "(TriggerdCondorLogLowerWarning != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogLowerWarningCount) warning messages in the following log files: $(TriggerdCondorLogLowerWarning)"),
            Trigger("dprintf Logs", 
                    "(TriggerdCondorLogDPrintfs != \"\")", 
                    "$(Machine) has the following dprintf_failure log files: $(TriggerdCondorLogDPrintfs)"),
            Trigger("Logs with stack dumps", 
                    "(TriggerdCondorLogStackDump != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogStackDumpCount) stack dumps in the following log files: $(TriggerdCondorLogStackDump)"),
            Trigger("Core Files", 
                    "(TriggerdCondorCoreFiles != \"\")", 
                    "$(Machine) has $(TriggerdCondorLogStackDumpCount) core files in the following locations: $(TriggerdCondorCoreFiles)")
            ]

        cts = self._get_condortriggerservice()
        if cts is None: 
            return 1
        print "Initializing, adding default triggers..."
        for t in defaultTriggers:
            if self.add_trigger(t.name, t.query, t.text) > 0:
                return 1
        return 0

    def list_triggers(self):
        print "Listing currently installed triggers..."
        triggers = self.getObjects(_class="condortrigger")
        if len(triggers) > 0:
            disp = Display()
            title = "Currently installed triggers:"
            heads = ('ID', 'Name', 'Query', 'Text')
            rows = []
            for t in triggers:
                rows.append([t.TriggerId, t.TriggerName, t.TriggerQuery, t.TriggerText])
            disp.table(title, heads, rows)
        else:
            print "No triggers currently installed"
            
    def get_trigger_id_by_name(self, name):
        triggers = self.getObjects(_class="condortrigger")
        for t in triggers:
            if t.TriggerName == name:
                return t.TriggerId
        print "Trigger named '%s' not found" % (name)

    def _wait_for_event(self):
        while not self.tester.get_event_received():
            time.sleep(1)
            print "."
        self.tester.reset_event_received()

    def test_triggers(self):
        ret_val = 0

        print "Testing triggers..."
        # list currently installed triggers
        self.list_triggers()

        # set the evaluation interval to 5 seconds
        self.set_interval(5)
        interval = self.get_interval()
        if interval != 5:
            print "Error: Interval was not set to 5 seconds"
            ret_val = 1
        else:
            print "Interval correctly set to 5 seconds"

        cts = self._get_condortriggerservice()
        if cts is None: 
            ret_val = 1
            return ret_val

        # add a trigger and verify that an event is received
        if self.add_trigger("TestTrigger", "(SlotID == 1)", "$(Machine) has a slot 1") > 0:
            ret_val = 1
        self.list_triggers()

        self._wait_for_event()
        first_event = time.time()
        self._wait_for_event()
        second_event = time.time()
        if second_event - first_event > 6:
            print "Error: Trigger evaluations occurring greater than every 5 seconds"
            ret_val = 1
        else:
            print "Trigger evaluations correctly occurring every 5 seconds"

        # retrieve the id of the trigger just added
        trigger_id = self.get_trigger_id_by_name("TestTrigger")

        # change the trigger name and verify that an event is received
        self.edit_trigger(trigger_id, "name", "Changed Test Trigger")
        print "Expecting 1 trigger in the form of '$(Machine) has a slot 1'"
        self._wait_for_event()

        # retrieve the id of the trigger with new name
        new_trigger_id = self.get_trigger_id_by_name("Changed Test Trigger")

        # The two IDs should be the same
        if trigger_id == new_trigger_id:
            print "Name successfully changed"
        else:
            print "Error: Did not receive same trigger ID for new name (%d vs %s)" % (trigger_id, new_trigger_id)
            ret_val = 1

        # change the trigger query and text and verify that an event is received
        self.edit_trigger(trigger_id, "query", "(SlotID > 0)")
        self.edit_trigger(trigger_id, "text", "$(Machine) has a slot $(SlotID)")
        print "Expecting >= 1 trigger in the form of '$(Machine) has a slot $(SlotID)'"
        self._wait_for_event()

        # list currently installed triggers
        self.list_triggers()

        # remove the trigger
        if self.del_trigger(trigger_id) > 0:
            ret_val = 1

        return ret_val

    def add_trigger(self, name, query, text):
        cts = self._get_condortriggerservice()
        if cts is None: 
            return 1
        print "Adding trigger '%s'..." % (name)
        result = cts.AddTrigger(name, query, text)
        if result.status != 0:
            print "Error: Failed to add trigger '%s' (error message: %d - %s)" % (name, result.status, result.text)
            return 1
        return 0

    def edit_trigger(self, id, field, value):
        cts = self._get_condortriggerservice()
        if cts is None: 
            return 1
        print "Editing trigger '%s', setting '%s' to '%s'..." % (id, field, value)
        if field == "name":
            result = cts.SetTriggerName(id, value)
        elif field == "query":
            result = cts.SetTriggerQuery(id, value)
        elif field == "text":
            result = cts.SetTriggerText(id, value)
        else:
            print "Error: invalid field '%s' to be edited" % (field)
            return 1

        if result.status != 0:
            print "Error: Failed to edit trigger '%s' (error message: %d - %s)" % (id, result.status, result.text)
            return 1
        else:
            print "Trigger '%s' edited successfully" % (id)
        return 0

    def del_trigger(self, id):
        cts = self._get_condortriggerservice()
        if cts is None: 
            return 1
        if id > 0:
            print "Deleting trigger '%d'..." % (id)
            result = cts.RemoveTrigger(id)
            if result.status != 0:
                print "Error: Failed to delete trigger '%d' (error message: %d - %s)" % (id, result.status, result.text)
                return 1
        elif id == 0:
            print "Deleting all installed triggers..."
            triggers = self.getObjects(_class="condortrigger")
            if len(triggers) == 0:
                print "No triggers found"
            else:
                for t in triggers:
                    self.del_trigger(t.TriggerId)
        return 0

    def get_interval(self):
        cts = self._get_condortriggerservice()
        if cts is None: 
            return -1
        print "Retrieving interval..."
        result = cts.GetEvalInterval()
        if result.status != 0:
            print "Error: Failed to retrieve interval (error message: %d - %s)" % (result.status, result.text)
            return -1
        else:
            return result.outArgs["Interval"]

    def set_interval(self, interval):
        cts = self._get_condortriggerservice()
        if cts is None: 
            return 1
        print "Setting interval to '%d' seconds..." % (interval)
        result = cts.SetEvalInterval(interval)
        if result.status != 0:
            print "Error: Failed to set interval (error message: %d - %s)" % (result.status, result.text)
            return 1
        
    def _get_condortriggerservice(self):
        if self.cts == None:
            try:
                self.cts = self.getObjects(_class="condortriggerservice")[0]
            except IndexError:
                print "No condor trigger service found"
        return self.cts

class TriggerConfigOptionParser(OptionParser):
    def __init__(self, usage):
        OptionParser.__init__(self, usage)

        self.add_option("-a", "--add", action="store_true", help="add a trigger (name, query, text required)")
        self.add_option("-n", "--name", action="store")
        self.add_option("-q", "--query", action="store")
        self.add_option("-t", "--text", action="store")
        self.add_option("-d", "--delete", action="store", type="int", help="delete a trigger by id (id=0 deletes all triggers)")
        self.add_option("-i", "--init", action="store_true", help="add default triggers")
        self.add_option("-l", "--list", action="store_true", help="list installed triggers")
        self.add_option("-s", "--test", action="store_true", help="test triggers")
        self.add_option("-U", "--user", action="store", help="The username used to authenticate with the broker")
        self.add_option("-P", "--password", action="store", help="The password used to authenticate with the broker")
        self.add_option("-m", "--auth-mechanism", action="store", help="A comma separated list of authentication mechanisms to use when communicating with the broker.  Supported mechanisms are: ANONYMOUS, PLAIN, GSSAPI", default="ANONYMOUS,PLAIN,GSSAPI")

    def is_valid(self, opts, args):
        valid = False
        if len(args) != 1:
            return False
        if opts.add and not opts.delete and not opts.init and not opts.list and not opts.test and opts.name != None and opts.query != None and opts.text != None:
            valid = True
        elif not opts.add and opts.delete >= 0 and not opts.init and not opts.list and not opts.test:
            valid = True
        elif not opts.add and not opts.delete and opts.init and not opts.list and not opts.test:
            valid = True
        elif not opts.add and not opts.delete and not opts.init and opts.list and not opts.test:
            valid = True
        elif not opts.add and not opts.delete and not opts.init and not opts.list and opts.test:
            valid = True
        return valid

def main():
    ret_val = 0
    parser = TriggerConfigOptionParser(usage="usage: %s [options] broker" % (sys.argv[0],))

    opts, args = parser.parse_args()

    if not parser.is_valid(opts, args):
        parser.print_help()
        sys.exit(1)
    else:
        target = str(args[0])

    session = TriggerConfig(opts.test)

    print "Connecting to broker '%s'..." % target
    if opts.user != None and opts.password != None:
       target = "%s/%s@%s" % (opts.user, opts.password, target)
    elif opts.user != None:
       target = "%s@%s" % (opts.user, target)

    try:
        broker = session.addBroker(target, mechanisms = opts.auth_mechanism.replace(',', ' '))
    except Exception, e:
        print e
        sys.exit(1)

    try:
        if opts.init:
            ret_val = session.add_default_triggers()
        elif opts.add:
            ret_val = session.add_trigger(opts.name, opts.query, opts.text)
        elif opts.delete != None:
            ret_val = session.del_trigger(opts.delete)
        elif opts.list:
            session.list_triggers()
        elif opts.test:
            ret_val = session.test_triggers()
    finally:
        session.delBroker(broker)
    
    sys.exit(ret_val)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass

