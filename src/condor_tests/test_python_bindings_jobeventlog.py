#!/usr/bin/env pytest

#
# Test the JobEventLog code.
#
# To make sure we test each event type, run x_write_joblog.exe.  Then
# instantiate a JobEventLog and make sure that the JobEvents it returns
# are what we expect them to be.
#

import pytest
import logging

import time
import pickle

import htcondor2 as htcondor

from ornithology import (
	action,
	run_command
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# To make sure we test each event type, run x_write_joblog.exe.
@action
def logfile(pytestconfig, test_dir):
	logFilePath = test_dir / "local.pybind-jel.log"
	exeFilePath = pytestconfig.invocation_dir/ "x_write_joblog.exe"
	completedProcess = run_command([exeFilePath, logFilePath.as_posix()])
	assert(completedProcess.returncode == 0)
	return logFilePath.as_posix()


@action
def synthetic(logfile):
	new = ""
	jel = htcondor.JobEventLog(logfile)
	for event in jel.events(stop_after=0):
		new = new + str(event) + "...\n"
	return new


@action
def original(logfile):
	old = ""
	log = open(logfile, "r")
	for line in log:
		old += line
	return old


standardHost = "<128.105.165.12:32779>"
standardUsage = "Usr 1 02:03:04, Sys 5 06:07:08"

expectedEvents = [
	( 0, {
		"UserNotes": "User info", "LogNotes": "DAGMan info",
		"SubmitHost": standardHost,
	} ),
	( 1, { "ExecuteHost": standardHost } ),
	( 2, { "ExecuteErrorType": 1 } ),
	( 3, {
		"RunRemoteUsage": standardUsage, "RunLocalUsage": standardUsage,
		"SentBytes": 11,
	} ),
	( 4, {
		"RunRemoteUsage": standardUsage, "RunLocalUsage": standardUsage, "SentBytes": 1,
		"TerminatedAndRequeued": True,
		"TerminatedNormally": True, "ReceivedBytes": 2,
		"Checkpointed": False, "Reason": "It misbehaved!",
	} ),
	( 5, {
		"TotalLocalUsage": standardUsage, "TotalRemoteUsage": standardUsage,
		"RunRemoteUsage": standardUsage, "RunLocalUsage": standardUsage,
		"TotalReceivedBytes": 800000, "SentBytes": 400000,
		"TerminatedBySignal": 9, "TotalSentBytes": 900000,
		"CoreFile": "badfilecore", "ReceivedBytes": 200000,
		"TerminatedNormally": False,
	} ),
	( 6, { "Size": 128, "ResidentSetSize": 129, "MemoryUsage": 131, "ProportionalSetSize": 130 } ),
	( 7, { "SentBytes": 4096, "Message": "shadow message", "ReceivedBytes": 4096 } ),
	( 8, { "Info": "Lorem ipsum dolor sit amet, in duo prima aeque principes euismod." } ),
	( 9, { "Reason": "cause I said so!" } ),
	(10, { "NumberOfPIDs": 99 } ),
	(11, { } ),
	(12, { "HoldReasonCode": 404, "HoldReasonSubCode": 255, "HoldReason": 'CauseWeCan' } ),
	(13, { "Reason": "MessinWithYou" } ),
	(14, { "ExecuteHost": standardHost, "Node": 49 } ),
	(15, { "SentBytes": 400000, "RequestMemory": 44, "RequestPets": 1,
		"Memory": 55, "TerminatedNormally": False, "ReturnValue": -1,
		      "PetsUsage": 0.5, "ReceivedBytes": 200000,
		      "TotalRemoteUsage": standardUsage, "Node": 44, "Pets": 1,
		      "CoreFile": "badfilecore", "TotalSentBytes": 900000,
		      "TotalLocalUsage": standardUsage, "TerminatedBySignal": 9,
		      "RunLocalUsage": standardUsage, "RunRemoteUsage": standardUsage,
		      "AssignedPets": "Spot", "MemoryUsage": 33,
		      "TotalReceivedBytes": 800000 } ),
	(16, { "TerminatedBySignal": 9, "TerminatedNormally": False } ),
	#17: { "RestartableJM": True, "RMContact": "ResourceManager", "JMContact": "JobManager" },
	#18: { "Reason": "Cause it could" },
	#19: { "RMContact": "ResourceUp" },
	#20: { "RMContact": "ResourceDown" },
	(21, { "ErrorMsg": "this is the write test error string",
		   "Daemon": "<write job log test>",
		   "ExecuteHost": standardHost } ),
	(22, { "EventDescription": "Job disconnected, attempting to reconnect",
	      "StartdName": "ThatMachine", "DisconnectReason": "TL;DR",
	      "StartdAddr": "<128.105.165.12:32780>" }),
	(23, { "EventDescription": "Job reconnected", "StartdName": "ThatMachine",
	      "StarterAddr": "<128.105.165.12:32780>", "StartdAddr": standardHost } ),
	(24, { "EventDescription": "Job reconnect impossible: rescheduling job",
	      "Reason": "The're just not into you", "StartdName": "ThatMachine" } ),
	(25, { "GridResource": "Resource Name" } ),
	(26, { "GridResource": "Resource Name" } ),
	(27, { "GridResource": "Resource Name", "GridJobId": "100.1" } ),
	(28, { "JobStatus": 2, "BILLReal" : 66.66,
	      "BillString": "lorem ipsum dolor", "BILLBool": True, "BILLInt": 1000 } ),
	(29, { }),
	(30, { }),
	(31, { "EventHead": "Job is performing stage-in of input files" } ),
	(32, { "EventHead": "Job is performing stage-out of output files" } ),
	(33, { "Attribute": "PrivateAttr", "Value": "1" } ),
	(34, { "SkipEventLogNotes": "DAGMan info" } ),
	(35, { "SubmitHost": standardHost } ),
	(36, { "Completion": 1, "NextProcId": 100, "NextRow": 10 } ),
	(37, { "Reason": "Hang on a second", "HoldCode": 24, "PauseCode": 42 } ),
	(38, { "Reason": "just messin wit' ya" } ),
	# 39: ULOG_NONE,
	(40, { "Type": htcondor.FileTransferEventType.IN_QUEUED } ),
	(40, { "Type": htcondor.FileTransferEventType.IN_STARTED } ),
	(40, { "Type": htcondor.FileTransferEventType.IN_FINISHED } ),
	(40, { "Type": htcondor.FileTransferEventType.OUT_QUEUED } ),
	(40, { "Type": htcondor.FileTransferEventType.OUT_STARTED } ),
	(40, { "Type": htcondor.FileTransferEventType.OUT_FINISHED } ),
	# 41: ULOG_RESERVE_SPACE,
	# 42: ULOG_RELEASE_SPACE,
	# 43: ULOG_FILE_COMPLETE,
	# 44: ULOG_FILE_USED,
	# 45: ULOG_FILE_REMOVED,
	# 46 : ULOG_DATAFLOW_JOB_SKIPPED,
	# FIXME: add CommonFilesEventType enumeration.
	(47, { "Type": "TransferStarted" } ),
	(47, { "Type": "TransferFinished" } ),
	(47, { "Type": "WaitStarted" } ),
]

boringAttrs = ("Subproc", "Proc", "Cluster", "EventTime", "EventTypeNumber", "MyType", "timestamp", "type")


def compareEvent(event, count):
	if event.type != expectedEvents[count][0]:
		print("Event {0} has wrong type".format(count))
		return False

	if event.cluster != 14:
		print("Event {0} has wrong cluster ID".format(count))
		return False

	if event.proc != 55:
		print("Event {0} has wrong proc ID".format(count))
		return False

	if time.time() - event.timestamp > 30:
		print("Event {0}'s timestamp is from more than thirty seconds ago.".format(count))
		return False

	expectedEvent = expectedEvents[count]
	if expectedEvent is not None:
		expectedType = expectedEvent[0]
		expectedAttrs = expectedEvent[1]

		for (attr, value) in expectedAttrs.items():
			if attr not in event:
				print("Event {0}'s {1} is missing".format(count, attr))
				return False
			if event[attr] != value:
				print("Event {0}'s {1} was '{2}', not '{3}'".format(count, attr, event[attr], value))
				return False
		d = set(event.keys()) - set(expectedAttrs.keys()) - set(boringAttrs)
		if d:
			print("Found extra attributes in event {0}: {1}".format(count, ", ".join(sorted(d))))
			return False
	else:
		# To help add new events to the test.
		print("Unknown event {0}:".format(count))
		for attr in event:
			if attr in boringAttrs:
				continue
			print("{0} = '{1}'".format(attr, event[attr]))
		return False

	return True

@action
def negative_cluster_log(test_dir):
	log = test_dir / "negative_cluster.log"
	with open(log, "w") as f:
		f.write("""008 (-01.-01.-01) 01/28 06:28:38 Global JobLog: ctime=1738067318 id=0.3192890.1738067309.170263.12.1738067318.512105 sequence=12 size=0 events=0 offset=23622537909 event_off=0 max_rotation=1 creator_name=<>
...
""")
	return log.as_posix()

class TestJobEventLog:

	# Instantiate a JobEventLog and make sure that the JobEvents it returns
	# are what we expect them to be.
	def test_correct_events_read(self, logfile):
		count = 0
		jel = htcondor.JobEventLog(logfile)
		for event in jel.events(stop_after=0):
			assert(compareEvent(event, count))
			count += 1

		assert(count == len(expectedEvents))


	# To check that __exit__() closes the log, we need to leave at least one
	# event in the log for the log to not return.
	def test_enter_and_exit(self, logfile):
		with htcondor.JobEventLog(logfile) as jel:
			for i in range(0, 30):
				event = next(jel)
		try:
			event = next(jel)
			assert(False)
		except StopIteration as si:
			pass


	# To test __str__(), use it to reconstruct the original log string and
	# then compare to the value of the log file.
	def test_string_conversion(self, synthetic, original):
		assert(synthetic == original)


	def test_close(self, logfile):
		with htcondor.JobEventLog(logfile) as jel:
			e = next(jel)
			jel.close()
			try:
				e = next(jel)
				assert(False)
			except StopIteration as si:
				pass


	def test_pickle(self, logfile):
		p = None
		with htcondor.JobEventLog(logfile) as jel:
			e = next(jel)
			assert(e["UserNotes"] == "User info")
			p = pickle.dumps(jel)

		# Make sure we throw away the pickled JobEventLog.
		del jel

		new_jel = pickle.loads(p)
		e = next(new_jel)

		assert(e["ExecuteHost"] == "<128.105.165.12:32779>")

	def test_non_job_event_handling(self, negative_cluster_log):
		with htcondor.JobEventLog(negative_cluster_log) as jel:
			for event in jel.events(stop_after=0):
				assert event.cluster == -1
