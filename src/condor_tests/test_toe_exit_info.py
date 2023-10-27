#!/usr/bin/env pytest

import os
import pytest
import htcondor2 as htcondor
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


#
# JobEventType.JOB_TERMINATED events that report the job terminating of its
# own accord (ToE.howCode of 0) may now have an optional part that includes
# if the job exited on a signal or normally, and what the signal or exit
# code was when it did so.
#
# This test verifies that the code can still read old events, and that the
# old and new events both round-trip through the system properly.
#

#
# If run against a build from hash 6a48d2aa4282eddc9ee6b57a840dc5fa9b8ad10a or
# earlier, this test will fail as written because old readers can't reproduce
# the new (exit) information.  This is OK; the point of the running the test
# against the older build is to verify that it doesn't crash.
#
# If we feel ambitious at some point, we could port this test to the stable
# branch and update the assertion to reflect that the only permissible
# difference is after the datestamp on the last line of each event.
#

EVENTS = [
('old',
"""005 (3817.000.000) 2021-04-26 10:22:28 Job terminated.
	(1) Normal termination (return value 0)
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
	0  -  Run Bytes Sent By Job
	0  -  Run Bytes Received By Job
	0  -  Total Bytes Sent By Job
	0  -  Total Bytes Received By Job
	Partitionable Resources :    Usage  Request Allocated 
	   Cpus                 :                 1         1 
	   Disk (KB)            :       40       40  28786355 
	   Memory (MB)          :        6        1      2011 

	Job terminated of its own accord at 2021-04-26T15:22:28Z.
"""),
('zero',
"""005 (3818.000.000) 2021-04-26 11:04:44 Job terminated.
	(1) Normal termination (return value 0)
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
	0  -  Run Bytes Sent By Job
	0  -  Run Bytes Received By Job
	0  -  Total Bytes Sent By Job
	0  -  Total Bytes Received By Job
	Partitionable Resources :    Usage  Request Allocated 
	   Cpus                 :                 1         1 
	   Disk (KB)            :       40       40  28786352 
	   Memory (MB)          :        6        1      2011 

	Job terminated of its own accord at 2021-04-26T16:04:44Z with exit-code 0.
"""),
('seventeen',
"""005 (3819.000.000) 2021-04-26 11:05:44 Job terminated.
	(1) Normal termination (return value 17)
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
	0  -  Run Bytes Sent By Job
	20  -  Run Bytes Received By Job
	0  -  Total Bytes Sent By Job
	20  -  Total Bytes Received By Job
	Partitionable Resources :    Usage  Request Allocated 
	   Cpus                 :                 1         1 
	   Disk (KB)            :        9        1  28786352 
	   Memory (MB)          :        6        1      2011 

	Job terminated of its own accord at 2021-04-26T16:05:44Z with exit-code 17.
"""),
(
'sigterm',
"""005 (3820.000.000) 2021-04-26 11:11:32 Job terminated.
	(0) Abnormal termination (signal 15)
	(0) No core file
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
		Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
	0  -  Run Bytes Sent By Job
	20  -  Run Bytes Received By Job
	0  -  Total Bytes Sent By Job
	20  -  Total Bytes Received By Job
	Partitionable Resources :    Usage  Request Allocated 
	   Cpus                 :                 1         1 
	   Disk (KB)            :        9        1  28786352 
	   Memory (MB)          :        6        1      2011 

	Job terminated of its own accord at 2021-04-26T16:11:32Z with signal 15.
"""),
]

def write_and_read_back_event(event):
    with open('test_toe_exit_info.event', mode="w") as f:
        f.write(event)
        f.write('...\n')

    jel = htcondor.JobEventLog('test_toe_exit_info.event')
    os.unlink('test_toe_exit_info.event')
    for e in jel.events(stop_after=0):
        return(str(e))


@pytest.mark.parametrize("event", [e[1] for e in EVENTS], ids=[e[0] for e in EVENTS])
def test_round_trip(event):
    roundTripEvent = write_and_read_back_event(event)
    assert event == roundTripEvent
