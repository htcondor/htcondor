import classad
import htcondor
import os
import time

from PersonalCondor import PersonalCondor
from Globals import *
from Utils import Utils

from htcondor import JobEventLog
from htcondor import JobEventType

class CondorJob(object):

    def __init__(self, job_args):
        self._cluster_id = None
        self._job_args = job_args
        self._old_status = None
        self._log = None

    def clusterID(self):
        return self._cluster_id

    def Submit(self, wait=False, count=1):
        # It's easier to smash the case of the keys (since ClassAds and the
        # submit language don't care) than to do the case-insensitive compare.
        self._job_args = dict([(k.lower(), v) for k, v in self._job_args.items()])

        # Extract the event log filename, or insert one if none.
        self._log = self._job_args.setdefault( "log", "FIXME.log" )
        self._log = os.path.abspath( self._log )

        # Submit the job defined by submit_args
        Utils.TLog("Submitting job with arguments: " + str(self._job_args))
        schedd = htcondor.Schedd()
        submit = htcondor.Submit(self._job_args)
        try:
            with schedd.transaction() as txn:
                self._cluster_id = submit.queue(txn, count)
        except:
            print("Job submission failed for an unknown error")
            return JOB_FAILURE

        Utils.TLog("Job submitted succeeded with cluster ID " + str(self._cluster_id))

        # We probably don't need self._log, but it seems like it may be
        # handy for log messages at some point.
        self._jel = JobEventLog( self._log )
        if not self._jel.isInitialized():
            print( "Unable to initialize job event log " + self._log )
            return JOB_FAILURE

        # Wait until job has finished running?
        if wait is True:
            submit_result = self.WaitForFinish()
            return submit_result

        # If we aren't waiting for finish, return None
        return None

    #
    # The timeout for these functions is in seconds, and applies to the
    # whole process, not any individual read.
    #

    def WaitForFinish( self, timeout = 240 ):
        return WaitUntil( [ JobEventType.TERMINATED ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE ], timeout, proc )

    def WaitUntilRunning( self, timeout = 240, proc = 0 ):
        return self.WaitUntil( [ JobEventType.EXECUTE ],
            [ JobEventType.SUBMIT ], timeout, proc )

    def WaitUntilHeld( self, timeout = 240, proc = 0 ):
        return self.WaitUntil( [ JobEventType.JOB_HELD ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.SHADOW_EXCEPTION ],
            timeout, proc )

        # FIXME: "look ahead" five seconds to see if we find the
        # hold event, o/w fail.  TODO: file a bug to prevent the
        # shadow exception event from entering the user log when
        # a job goes on hold.


    # An event type not listed in successEvents or ignoreeEvents is a failure.
    def WaitUntil( self, successEvents, ignoreEvents, timeout = 240, proc = 0 ):
        deadline = time.time() + timeout;
        Utils.TLog( "[cluster " + str(self._cluster_id) + "] Waiting for " + ",".join( [str(x) for x in successEvents] ) )

        for event in self._jel.follow( int(timeout * 1000) ):
            if event.cluster == self._cluster_id and event.proc == proc:
                if( event.type == JobEventType.NONE ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (ignore)" )
                    pass
                elif( event.type in successEvents ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (succeed)" )
                    return True
                elif( event.type in ignoreEvents ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (ignore)" )
                    pass
                else:
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found disallowed event type " + str(event.type) + " (fail)" )
                    return False

            difference = deadline - time.time();
            if difference <= 0:
                Utils.TLog( "[cluster " + str(self._cluster_id) + "] Timed out waiting for " + ",".join( [str(x) for x in successEvents] ) )
                return False
            else:
                self._jel.setFollowTimeout( int(difference * 1000) )
                continue
