from __future__ import absolute_import

import classad
import htcondor
import os
import time

from .Globals import *
from .Utils import Utils
from .EventMemory import EventMemory

from htcondor import JobEventLog
from htcondor import JobEventType

class CondorCluster(object):

    def __init__(self, job_args, schedd=None):
        self._cluster_id = None
        self._job_args = job_args
        self._log = None
        self._callbacks = { }
        self._count = 0
        self._schedd = schedd

    @property
    def cluster_id(self):
        return self._cluster_id

    def Remove(self):
        if self._schedd is None:
            self._schedd = htcondor.Schedd()
        constraint = "ClusterId == {0}".format(self._cluster_id)
        ad = self._schedd.act(htcondor.JobAction.Remove, constraint)
        if ad["TotalSuccess"] != self._count:
            return False
        return True

    # @return The corresponding CondorCluster object or None.
    def Submit(self, count=1):
        # It's easier to smash the case of the keys (since ClassAds and the
        # submit language don't care) than to do the case-insensitive compare.
        self._job_args = dict([(k.lower(), v) for k, v in self._job_args.items()])

        # Extract the event log filename, or insert one if none.
        self._log = self._job_args.setdefault( "log", "test-{0}.log".format( os.getpid() ) )
        self._log = os.path.abspath( self._log )

        # Submit the job defined by submit_args
        Utils.TLog("Submitting job with arguments: " + str(self._job_args))
        if self._schedd is None:
            self._schedd = htcondor.Schedd()
        submit = htcondor.Submit(self._job_args)
        try:
            with self._schedd.transaction() as txn:
                self._cluster_id = submit.queue(txn, count)
                self._count = count
        except Exception as e:
            Utils.TLog( "Job submission failed: " + str(e) )
            raise e

        Utils.TLog("Job submitted succeeded with cluster ID " + str(self._cluster_id))

        # We probably don't need self._log, but it seems like it may be
        # handy for log messages at some point.
        self._jel = JobEventLog( self._log )

    def Schedd(self):
        return self._schedd


    #
    # The timeout for these functions is in seconds, and applies to the
    # whole process, not any individual read.
    #

    def WaitUntilJobTerminated( self, timeout = 240, proc = 0, count = 0 ):
        return self.WaitUntil( [ JobEventType.JOB_TERMINATED ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.FILE_TRANSFER ], timeout, proc, count )

    def WaitUntilExecute( self, timeout = 240, proc = 0, count = 0 ):
        return self.WaitUntil( [ JobEventType.EXECUTE ],
            [ JobEventType.SUBMIT, JobEventType.FILE_TRANSFER ], timeout, proc, count )

    def WaitUntilJobHeld( self, timeout = 240, proc = 0, count = 0 ):
        return self.WaitUntil( [ JobEventType.JOB_HELD ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.SHADOW_EXCEPTION,
              JobEventType.FILE_TRANSFER ],
            timeout, proc, count )

        # FIXME: "look ahead" five seconds to see if we find the
        # hold event, o/w fail.  TODO: file a bug to prevent the
        # shadow exception event from entering the user log when
        # a job goes on hold.

    def WaitUntilJobEvicted( self, timeout = 240, proc = 0, count = 0 ):
        return self.WaitUntil( [ JobEventType.JOB_EVICTED ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.FILE_TRANSFER ],
            timeout, proc, count )

    # WaitUntilAll*() won't work with 'advanced queue statements' because we
    # don't know how many procs to expect.  That's OK for now, since this
    # class doesn't support AQSs... yet.
    def WaitUntilAllJobsTerminated( self, timeout = 240 ):
        return self.WaitUntil( [ JobEventType.JOB_TERMINATED ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.FILE_TRANSFER ], timeout, -1, self._count )

    def WaitUntilAllExecute( self, timeout = 240 ):
        return self.WaitUntil( [ JobEventType.EXECUTE ],
            [ JobEventType.SUBMIT, JobEventType.IMAGE_SIZE, JobEventType.FILE_TRANSFER ],
            timeout, -1, self._count )

    def WaitUntilAllJobsHeld( self, timeout = 240 ):
        return self.WaitUntil( [ JobEventType.JOB_HELD ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.SHADOW_EXCEPTION,
              JobEventType.FILE_TRANSFER ],
            timeout, -1, self._count )

    def WaitUntilAllJobsEvicted( self, timeout = 240 ):
        return self.WaitUntil( [ JobEventType.JOB_EVICTED ],
            [ JobEventType.EXECUTE, JobEventType.SUBMIT,
              JobEventType.IMAGE_SIZE, JobEventType.FILE_TRANSFER ],
            timeout, -1, self._count )

    # An event type not listed in successEvents or ignoreeEvents is a failure.
    def WaitUntil( self, successEvents, ignoreEvents, timeout = 240, proc = 0, count = 0 ):
        Utils.TLog( "[cluster " + str(self._cluster_id) + "] Waiting for " + ",".join( [str(x) for x in successEvents] ) )

        successes = 0
        self._memory = EventMemory()
        for event in self._jel.events( timeout ):
            # Record all events in case we need them later.
            self._memory.Append( event )

            if event.cluster == self._cluster_id and (count > 0 or event.proc == proc):
                if self._callbacks.get( event.type ) is not None:
                    self._callbacks[ event.type ]()

                #  This first clause should no longer be necessary.
                if( event.type == JobEventType.NONE ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (ignore)" )
                    pass
                elif( event.type in successEvents ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (succeed)" )
                    successes += 1
                    if count <= 0 or successes == count:
                        return True
                elif( event.type in ignoreEvents ):
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of type " + str(event.type) + " (ignore)" )
                    pass
                else:
                    Utils.TLog( "[cluster " + str(self._cluster_id) + "] Found relevant event of disallowed type " + str(event.type) + " (fail)" )
                    return False

        Utils.TLog( "[cluster " + str(self._cluster_id) + "] Timed out waiting for " + ",".join( [str(x) for x in successEvents] ) )


    #
    # The callback-based interface.  The first set respond to events.  We'll
    # add semantic callbacks if it turns out those are useful to anyone.
    #

    def RegisterSubmit( self, submit_callback_fn ):
        self._callbacks[ JobEventType.SUBMIT ] = submit_callback_fn

    def RegisterJobTerminated( self, job_terminated_callback_fn ):
        self._callbacks[ JobEventType.JOB_TERMINATED ] = job_terminated_callback_fn

    def RegisterJobHeld( self, job_held_callback_fn ):
        self._callbacks[ JobEventType.JOB_HELD ] = job_held_callback_fn


    # A convenience function.
    def QueryForJobAd(self, proc=0):
        if self._schedd is None:
            self._schedd = htcondor.Schedd()
        try:
            return next(self._schedd.xquery( requirements = "ClusterID == {0} && ProcID == {1}".
                format( self._cluster_id, proc ) ))
        except StopIteration as si:
            return None


    #
    # History.
    #
    # @return The reverse-chronological (newest-first) list of events,
    # filtered by cluster if specified and by proc if specified.  If proc
    # is specified, but cluster is not, assumes self._cluster_id.
    def GetPrecedingEvents(self, cluster=None, proc=None):
        if cluster is None and proc is not None:
            cluster = self._cluster_id
        return reversed(self.memory.trace(cluster=cluster, proc=proc))
