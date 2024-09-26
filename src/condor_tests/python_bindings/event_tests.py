
import os
import time
import unittest
import threading

import classad2 as classad
import htcondor2 as htcondor

SAMPLE_EVENT_TEXT = """\
006 (23515.000.000) 04/09 17:55:39 Image size of job updated: 260
        1  -  MemoryUsage of job (MB)
        252  -  ResidentSetSize of job (KB)
...
"""

class TestEventReader(unittest.TestCase):

    def setUp(self):
        # Note we cannot use a temporary file here; the event reader
        # is based on *filenames* (which are not visible for TemporaryFile),
        # not file descriptors.
        self.testname = "tests/test_event_reader.log"
        self.testfile = open(self.testname, "w")
        self.testfile.write(open("tests/job.log", "r").read())
        self.testfile.flush()
        self.testfile = open(self.testname, "r")
        self.reader = htcondor.read_events(self.testfile)
        self.sampleEvent = { \
            'MyType': "JobImageSizeEvent",
            'EventTypeNumber': 6,
            'Subproc': 0,
            'Cluster': 23515,
            'Proc': 0,
            'MemoryUsage': 1, 
            'Size': 260,
            'ResidentSetSize': 252,
            'EventTime': 0,
        }
        self.sampleEventText = SAMPLE_EVENT_TEXT

    def test_event_count(self):
        events = list(self.reader)
        #for event in events: print event
        self.assertEqual(len(events), 6)

    def test_block(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        self.reader.setBlocking(True)
        t.start()
        new_event = next(self.reader)
        self.compareEvents(new_event, self.sampleEvent)
        t.join()

    def test_inotify(self):
        if not self.reader.use_inotify:
            print "Not using inotify"
            return
        self.assertTrue(self.reader.watch() >= 0)

    def test_wait(self):
        events = list(self.reader)
        self.assertEqual(len(events), 6)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        t.setDaemon(True)
        t.start()
        self.reader.wait()
        self.compareEvents(next(self.reader), self.sampleEvent)

    def test_wait2(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        t.setDaemon(True)
        t.start()
        time.sleep(1)
        self.compareEvents(next(self.reader), self.sampleEvent)

    def test_nochange(self):
        events = list(self.reader)
        self.assertRaises(StopIteration, lambda: next(self.reader))

    def test_poll(self):
        events = list(self.reader)
        self.assertEqual(self.reader.poll(1000), None)

    def targetSleepAndWrite(self):
        time.sleep(.4)
        testfile = open(self.testname, "a")
        testfile.write(self.sampleEventText)
        testfile.flush()
        #print "Wrote event to %s." % self.testfile

    def test_invalid(self):
        events = list(self.reader)
        self.assertEqual(len(events), 6)
        file2 = open(self.testname, "a")
        file2.write("...\n...\n")
        file2.flush()
        file2.close()
        self.assertRaises(ValueError, lambda: next(self.reader))

    def compareEvents(self, one, two):
        keys1 = list(one.keys())
        keys1.sort()
        keys1 = tuple(keys1)
        keys2 = list(two.keys())
        keys2.sort()
        keys2 = tuple(keys2)
        self.assertEqual(keys1, keys2)
        for key in keys1:
            if key in ['EventTime', 'CurrentTime']:
                continue
            self.assertEqual(one[key], two[key])


if __name__ == '__main__':
    unittest.main()

