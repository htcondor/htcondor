
import os
import time
import tempfile
import unittest
import threading

import classad
import htcondor

SAMPLE_EVENT_TEXT = """\
006 (23515.000.000) 04/09 17:55:39 Image size of job updated: 260
        1  -  MemoryUsage of job (MB)
        252  -  ResidentSetSize of job (KB)
...
"""

class TestLogReader(unittest.TestCase):

    def setUp(self):
        self.testfile = tempfile.TemporaryFile()
        self.testfile.write(open("tests/job.log", "r").read())
        self.testfile.flush()
        self.testfile.seek(0)
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
            'CurrentTime': 0,
            'EventTime': 0,
        }
        self.sampleEventText = SAMPLE_EVENT_TEXT

    def test_event_count(self):
        events = list(self.reader)
        #for event in events: print event
        self.assertEquals(len(events), 6)

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
        self.compareEvents(self.reader.next(), self.sampleEvent)

    def test_wait2(self):
        events = list(self.reader)
        t = threading.Thread(target=self.targetSleepAndWrite, name="Sleep and write event")
        t.setDaemon(True)
        t.start()
        time.sleep(1)
        self.compareEvents(self.reader.next(), self.sampleEvent)

    def test_nochange(self):
        events = list(self.reader)
        self.assertRaises(StopIteration, self.reader.next, )

    def test_poll(self):
        events = list(self.reader)
        self.assertEquals(self.reader.poll(1000), None)

    def targetSleepAndWrite(self):
        time.sleep(.4)
        testfile = os.fdopen(os.dup(self.testfile.fileno()), "a")
        testfile.write(self.sampleEventText)
        testfile.flush()
        #print "Wrote event to %s." % self.testfile

    def test_invalid(self):
        self.testfile.write("...\n...\n")
        self.testfile.flush()
        self.assertRaises(ValueError, self.reader.next)

    def compareEvents(self, one, two):
        keys1 = list(one.keys())
        keys1.sort()
        keys1 = tuple(keys1)
        keys2 = list(two.keys())
        keys2.sort()
        keys2 = tuple(keys2)
        self.assertEquals(keys1, keys2)
        for key in keys1:
            if key in ['EventTime', 'CurrentTime']:
                continue
            self.assertEquals(one[key], two[key])


if __name__ == '__main__':
    unittest.main()

