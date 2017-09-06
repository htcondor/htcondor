
import os
import time
import tempfile
import unittest
import threading

import classad
import htcondor

class TestLogReader(unittest.TestCase):

    def setUp(self):
        self.testfile = tempfile.NamedTemporaryFile()
        self.testfile.write(open("tests/job_queue.log", "r").read())
        self.testfile.flush()
        self.reader = htcondor.LogReader(self.testfile.name)
        self.sampleEvent = {'value': "bar", 'event': htcondor.EntryType.SetAttribute, 'key': '23605.0', 'name': 'Foo'}

    def test_event_count(self):
        events = list(self.reader)
        #for event in events: print event
        self.assertEqual(len(events), 16)

    def test_block(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        self.reader.setBlocking(True)
        t.start()
        new_event = self.reader.next()
        self.compareEvents(new_event, self.sampleEvent)
        t.join()

    def test_inotify(self):
        if not self.reader.use_inotify:
            print "Not using inotify"
            return
        self.assertTrue(self.reader.watch() >= 0)

    def test_wait(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        t.setDaemon(True)
        t.start()
        self.reader.wait()
        self.assertEqual(self.reader.next(), self.sampleEvent)

    def test_wait2(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        t.setDaemon(True)
        t.start()
        time.sleep(1.0)
        print self.reader.next()

    def test_nochange(self):
        events = list(self.reader)
        self.assertRaises(StopIteration, self.reader.next, )

    def test_poll(self):
        events = list(self.reader)
        self.assertEqual(self.reader.poll(1000), None)

    def targetSleepAndWrite(self):
        time.sleep(.4)
        self.testfile.write('103 23605.0 Foo "bar"\n')
        self.testfile.flush()
        #print "Wrote event to %s." % self.testfile

    def test_invalid(self):
        self.testfile.write('103 23605.0 Foo ::\n')
        self.testfile.flush()
        events = list(self.reader)
        #print "LastEvent", events[-1]
        self.assertEqual(classad.Function("isError", events[-1]['value']).eval(), True)

    def compareEvents(self, one, two):
        keys1 = one.keys()
        keys1.sort()
        keys1 = tuple(keys1)
        keys2 = two.keys()
        keys2.sort()
        keys2 = tuple(keys2)
        self.assertEqual(keys1, keys2)
        for key in keys1:
            self.assertEqual(one[key], two[key])


if __name__ == '__main__':
    unittest.main()

