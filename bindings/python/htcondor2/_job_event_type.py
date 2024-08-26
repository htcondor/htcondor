import sys
import enum


class JobEventType(enum.IntEnum):
    """
    An enumeration of the types of user log events;
    corresponds to ``ULogEventNumber`` in the C++ source.

    .. attribute:: SUBMIT
    .. attribute:: EXECUTE
    .. attribute:: EXECUTABLE_ERROR
    .. attribute:: CHECKPOINTED
    .. attribute:: JOB_EVICTED
    .. attribute:: JOB_TERMINATED
    .. attribute:: IMAGE_SIZE
    .. attribute:: SHADOW_EXCEPTION
    .. attribute:: GENERIC
    .. attribute:: JOB_ABORTED
    .. attribute:: JOB_SUSPENDED
    .. attribute:: JOB_UNSUSPENDED
    .. attribute:: JOB_HELD
    .. attribute:: JOB_RELEASED
    .. attribute:: NODE_EXECUTE
    .. attribute:: NODE_TERMINATED
    .. attribute:: POST_SCRIPT_TERMINATED
    .. attribute:: GLOBUS_SUBMIT
    .. attribute:: GLOBUS_SUBMIT_FAILED
    .. attribute:: GLOBUS_RESOURCE_UP
    .. attribute:: GLOBUS_RESOURCE_DOWN
    .. attribute:: REMOTE_ERROR
    .. attribute:: JOB_DISCONNECTED
    .. attribute:: JOB_RECONNECTED
    .. attribute:: JOB_RECONNECT_FAILED
    .. attribute:: GRID_RESOURCE_UP
    .. attribute:: GRID_RESOURCE_DOWN
    .. attribute:: GRID_SUBMIT
    .. attribute:: JOB_AD_INFORMATION
    .. attribute:: JOB_STATUS_UNKNOWN
    .. attribute:: JOB_STATUS_KNOWN
    .. attribute:: JOB_STAGE_IN
    .. attribute:: JOB_STAGE_OUT
    .. attribute:: ATTRIBUTE_UPDATE
    .. attribute:: PRESKIP
    .. attribute:: CLUSTER_SUBMIT
    .. attribute:: CLUSTER_REMOVE
    .. attribute:: FACTORY_PAUSED
    .. attribute:: FACTORY_RESUMED
    .. attribute:: NONE
    .. attribute:: FILE_TRANSFER
    .. attribute:: RESERVE_SPACE
    .. attribute:: RELEASE_SPACE
    .. attribute:: FILE_COMPLETE
    .. attribute:: FILE_USED
    .. attribute:: FILE_REMOVED
    .. attribute:: DATAFLOW_JOB_SKIPPED

    """

    SUBMIT                  = 0
    EXECUTE                 = 1
    EXECUTABLE_ERROR        = 2
    CHECKPOINTED            = 3
    JOB_EVICTED             = 4
    JOB_TERMINATED          = 5
    IMAGE_SIZE              = 6
    SHADOW_EXCEPTION        = 7
    GENERIC                 = 8
    JOB_ABORTED             = 9
    JOB_SUSPENDED           = 10
    JOB_UNSUSPENDED         = 11
    JOB_HELD                = 12
    JOB_RELEASED            = 13
    NODE_EXECUTE            = 14
    NODE_TERMINATED         = 15
    POST_SCRIPT_TERMINATED  = 16
    GLOBUS_SUBMIT           = 17
    GLOBUS_SUBMIT_FAILED    = 18
    GLOBUS_RESOURCE_UP      = 19
    GLOBUS_RESOURCE_DOWN    = 20
    REMOTE_ERROR            = 21
    JOB_DISCONNECTED        = 22
    JOB_RECONNECTED         = 23
    JOB_RECONNECT_FAILED    = 24
    GRID_RESOURCE_UP        = 25
    GRID_RESOURCE_DOWN      = 26
    GRID_SUBMIT             = 27
    JOB_AD_INFORMATION      = 28
    JOB_STATUS_UNKNOWN      = 29
    JOB_STATUS_KNOWN        = 30
    JOB_STAGE_IN            = 31
    JOB_STAGE_OUT           = 32
    ATTRIBUTE_UPDATE        = 33
    PRESKIP                 = 34
    CLUSTER_SUBMIT          = 35
    CLUSTER_REMOVE          = 36
    FACTORY_PAUSED          = 37
    FACTORY_RESUMED         = 38
    NONE                    = 39
    FILE_TRANSFER           = 40
    RESERVE_SPACE           = 41
    RELEASE_SPACE           = 42
    FILE_COMPLETE           = 43
    FILE_USED               = 44
    FILE_REMOVED            = 45
    DATAFLOW_JOB_SKIPPED    = 46

