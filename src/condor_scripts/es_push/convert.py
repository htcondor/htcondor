#!/usr/bin/python

import re
import json
import time
import logging
import zlib
import base64

import classad

AUTO_ATTRS = {
    "date_attrs": re.compile(r"^.*Date$"),
    "provisioned_attrs": re.compile(r"^.*Provisioned$"),
    "resource_request_attrs": re.compile(r"^Request[A-Z].*$"),
    "target_boolean_attrs": re.compile(r"^(Want|Has|Is)[A-Z_].*$"),
}

# TEXT_ATTRS should only contain attrs that we want full text search on,
# otherwise strings are stored as keywords.
TEXT_ATTRS = {} or set()

INDEXED_KEYWORD_ATTRS = {
    "AccountingGroup",
    "AcctGroup",
    "AcctGroupUser",
    "AutoClusterId",
    "BatchQueue",
    "CloudLabelNames",
    "ConcurrencyLimits",
    "CondorPlatform",
    "CondorVersion",
    "DAGNodeName",
    "DAGParentNodeNames",
    "DockerImage",
    "GLIDEIN_Entry_Name",
    "GlideinClient",
    "GlideinEntryName",
    "GlideinFactory",
    "GlideinFrontendName",
    "GlideinName",
    "GlobalJobId",
    "GridJobId",
    "GridJobStatus",
    "GridResource",
    "JobBatchName",
    "JobDescription",
    "JobKeyword",
    "JobState",
    "LastRemoteHost",
    "LastRemotePool",
    "MATCH_EXP_JOB_GLIDECLIENT_Name",
    "MATCH_EXP_JOB_GLIDEIN_ClusterId",
    "MATCH_EXP_JOB_GLIDEIN_Entry_Name",
    "MATCH_EXP_JOB_GLIDEIN_Factory",
    "MATCH_EXP_JOB_GLIDEIN_Name",
    "MATCH_EXP_JOB_GLIDEIN_SEs",
    "MATCH_EXP_JOB_GLIDEIN_Schedd",
    "MATCH_EXP_JOB_GLIDEIN_Site",
    "MATCH_EXP_JOB_GLIDEIN_SiteWMS",
    "MATCH_EXP_JOB_GLIDEIN_SiteWMS_JobId",
    "MATCH_EXP_JOB_GLIDEIN_SiteWMS_Queue",
    "MATCH_EXP_JOB_GLIDEIN_SiteWMS_Slot",
    "MyType",
    "NTDomain",
    "Owner",
    "ProjectName",
    "RemoteHost",
    "RemotePool",
    "ScheddName",
    "ShouldTransferFiles",
    "StartdName",
    "StartdSlot",
    "Status",
    "SubmitterGlobalJobId",
    "SubmitterGroup",
    "SubmitterNegotiatingGroup",
    "TargetType",
    "Universe",
    "User",
    "WhenToTransferOutput",
    "x509UserProxyEmail",
    "x509UserProxyFQAN",
    "x509UserProxyFirstFQAN",
    "x509UserProxySubject",
    "x509UserProxyVOName",
}

NOINDEX_KEYWORD_ATTRS = {
    "AllRemoteHosts",
    "Args",
    "Arguments",
    "Cmd",
    "DAGManNodesLog",
    "DAGManNodesMask",
    "Err",
    "ExitReason",
    "HoldReason",
    "In",
    "Iwd",
    "LastHoldReason",
    "LastRejMatchReason",
    "OtherJobRemoveRequirements",
    "Out",
    "OutputDestination",
    "PostCmd",
    "PreCmd",
    "ReleaseReason",
    "RemoteIwd",
    "RemoveReason",
    "Requirements",
    "RootDir",
    "StartdIpAddr",
    "StartdPrincipal",
    "StarterIpAddr",
    "StarterPrincipal",
    "SubmitEventNotes",
    "TransferCheckpoint",
    "TransferInput",
    "TransferOutput",
    "TransferOutputRemaps",
    "TransferPlugins",
    "UserLog",
}

FLOAT_ATTRS = {
    "CPUsUsage",
    "JobDuration",
    "NetworkInputMb",
    "NetworkOutputMb",
    "Rank",
}

INT_ATTRS = {
    "AutoClusterId",
    "BlockReadKbytes",
    "BlockReads",
    "BlockWriteKbytes",
    "BlockWrites",
    "BytesRecvd",
    "BytesSent",
    "ClusterId",
    "CommittedSlotTime",
    "CommittedSuspensionTime",
    "CommittedTime",
    "CoreSize",
    "CpusProvisioned",
    "CumulativeRemoteSysCpu",
    "CumulativeRemoteUserCpu",
    "CumulativeSlotTime",
    "CumulativeSuspensionTime",
    "CumulativeTransferTime",
    "CurrentHosts",
    "DAGManJobId",
    "DataLocationsCount",
    "DelegatedProxyExpiration",
    "DiskProvisioned",
    "DiskUsage",
    "DiskUsage_RAW",
    "ErrSize",
    "ExecutableSize",
    "ExecutableSize_RAW",
    "ExitCode",
    "ExitSignal",
    "ExitStatus",
    "GpusProvisioned",
    "HoldReasonCode",
    "HoldReasonSubCode",
    "ImageSize",
    "ImageSize_RAW",
    "IOWait",
    "JobLeaseDuration",
    "JobMaxRetries",
    "JobMaxVacateTime",
    "JobPid",
    "JobPrio",
    "JobRunCount",
    "JobStatus",
    "JobSuccessExitCode",
    "JobUniverse",
    "KeepClaimIdle",
    "KillSig",
    "LastHoldReasonCode",
    "LastHoldReasonSubCode",
    "LastJobStatus",
    "LocalSysCpu",
    "LocalUserCpu",
    "MATCH_EXP_JOB_GLIDEIN_Job_Max_Time",
    "MATCH_EXP_JOB_GLIDEIN_MaxMemMBs",
    "MATCH_EXP_JOB_GLIDEIN_Max_Walltime",
    "MATCH_EXP_JOB_GLIDEIN_Memory",
    "MATCH_EXP_JOB_GLIDEIN_ProcId",
    "MATCH_EXP_JOB_GLIDEIN_ToDie",
    "MATCH_EXP_JOB_GLIDEIN_ToRetire",
    "MaxHosts",
    "MaxJobRetirementTime",
    "MaxTransferInputMB",
    "MaxTransferOutputMB",
    "MaxWallTimeMins",
    "MaxWallTimeMins_RAW",
    "MemoryProvisioned",
    "MemoryUsage",
    "MinHosts",
    "NextJobStartDelay",
    "NumCkpts",
    "NumCkpts_RAW",
    "NumJobCompletions",
    "NumJobMatches",
    "NumJobReconnects",
    "NumJobStarts",
    "NumPids",
    "NumRestarts",
    "NumShadowExceptions",
    "NumShadowStarts",
    "NumSystemHolds",
    "OrigMaxHosts",
    "OutSize",
    "PilotRestLifeTimeMins",
    "PostCmdExitCode",
    "PostCmdExitSignal",
    "PostJobPrio1",
    "PostJobPrio2",
    "PreCmdExitCode",
    "PreCmdExitSignal",
    "PreJobPrio1",
    "PreJobPrio2",
    "ProcId",
    "ProportionalSetSizeKb",
    "RecentBlockReadKbytes",
    "RecentBlockReads",
    "RecentBlockWriteKbytes",
    "RecentBlockWrites",
    "RecentStatsLifetimeStarter",
    "RemoteSlotID",
    "RemoteSysCpu",
    "RemoteUserCpu",
    "RemoteWallClockTime",
    "RequestCpus",
    "RequestDisk",
    "RequestGpus",
    "RequestMemory",
    "RequestVirtualMemory",
    "ResidentSetSize",
    "ResidentSetSize_RAW",
    "ScratchDirFileCount",
    "StackSize",
    "StatsLifetimeStarter",
    "SuccessCheckpointExitCode",
    "SuccessCheckpointExitSignal",
    "SuccessPostExitCode",
    "SuccessPostExitSignal",
    "SuccessPreExitCode",
    "SuccessPreExitSignal",
    "TotalSubmitProcs",
    "TotalSuspensions",
    "TransferInputSizeMB",
    "WallClockCheckpoint",
    "WindowsBuildNumber",
    "WindowsMajorVersion",
    "WindowsMinorVersion",
}

DATE_ATTRS = {
    "CompletionDate",
    "EnteredCurrentStatus",
    "GLIDEIN_ToDie",
    "GLIDEIN_ToRetire",
    "JobCurrentFinishTransferInputDate",
    "JobCurrentFinishTransferOutputDate",
    "JobCurrentStartDate",
    "JobCurrentStartExecutingDate",
    "JobCurrentStartTransferInputDate",
    "JobCurrentStartTransferOutputDate",
    "JobDisconnectedDate",
    "JobFinishedHookDone",
    "JobLastStartDate",
    "JobLeaseExpiration",
    "JobQueueBirthdate",
    "JobStartDate",
    "LastJobLeaseRenewal",
    "LastMatchTime",
    "LastRejMatchTime",
    "LastRemoteStatusUpdate",
    "LastSuspensionTime",
    "LastVacateTime",
    "LastVacateTime_RAW",
    "MATCH_GLIDEIN_ToDie",
    "MATCH_GLIDEIN_ToRetire",
    "QDate",
    "RecordTime",
    "ShadowBday",
    "StageInFinish",
    "StageInStart",
    "StageOutFinish",
    "StageOutStart",
    "TransferInFinished",
    "TransferInQueued",
    "TransferInStarted",
    "TransferOutFinished",
    "TransferOutQueued",
    "TransferOutStarted",
}

BOOL_ATTRS = {
    "CurrentStatusUnknown",
    "EncryptExecuteDirectory",
    "ExitBySignal",
    "GlobusResubmit",
    "IsNoopJob",
    "JobCoreDumped",
    "LeaveJobInQueue",
    "NiceUser",
    "Nonessential",
    "OnExitHold",
    "OnExitRemove",
    "PeriodicHold",
    "PeriodicRelease",
    "PeriodicRemove",
    "PostCmdExitBySignal",
    "PreCmdExitBySignal",
    "PreserveRelativeExecutable",
    "PreserveRelativePaths",
    "RunAsOwner",
    "SendCredential",
    "StreamErr",
    "StreamOut",
    "SuccessCheckpointExitBySignal",
    "SuccessPostExitBySignal",
    "SuccessPreExitBySignal",
    "TerminationPending",
    "TransferIn",
    "TransferOut",
    "TransferQueued",
    "TransferringInput",
    "TransferringOutput",
    "Use_x509UserProxy",
    "UserLogUseXML",
    "WantAdRevaluate",
    "WantCheckpoint",
    "WantCheckpointSignal",
    "WantClaiming",
    "WantCompletionVisaFromSchedD",
    "WantDelayedUpdates",
    "WantExecutionVisaFromStarter",
    "WantFTOnCheckpoint",
    "WantGracefulRemoval",
    "WantIOProxy",
    "WantMatchDiagnostics",
    "WantMatching",
    "WantParallelScheduling",
    "WantParallelSchedulingGroups",
    "WantPslotPreemption",
    "WantRemoteIO",
    "WantRemoteSyscalls",
    "WantRemoteUpdates",
    "WantResAd",
}

IGNORE_ATTRS = {
    "BoincAuthenticatorFile",
    "ClaimId",
    "CmdHash",
    "EC2AccessKeyId",
    "EC2KeyPair",
    "EC2KeyPairFile",
    "EC2SecretAccessKey",
    "EC2SecurityGroups",
    "EC2SecurityIDs",
    "EC2UserData",
    "EC2UserDataFile",
    "Env",
    "EnvDelim",
    "Environment",
    "ExecutableSize",
    "GceAuthFile",
    "GceJsonFile",
    "GceMetadataFile",
    "GlideinCredentialIdentifier",
    "GlideinSecurityClass",
    "JobCoreFileName",
    "JobNotification",
    "LastPublicClaimId",
    "PostArgs",
    "PostArguments",
    "PostEnv",
    "PostEnvironment",
    "PreArgs",
    "PreArguments",
    "PreEnv",
    "PreEnvironment",
    "PublicClaimId",
    "ScitokensFile",
    "SpooledOutputFiles",
    "orig_environment",
    "osg_environment",
}

STATUS = {
    0: "Unexpanded",
    1: "Idle",
    2: "Running",
    3: "Removed",
    4: "Completed",
    5: "Held",
    6: "Error",
}

UNIVERSE = {
    1: "Standard",
    2: "Pipe",
    3: "Linda",
    4: "PVM",
    5: "Vanilla",
    6: "PVMD",
    7: "Scheduler",
    8: "MPI",
    9: "Grid",
    10: "Java",
    11: "Parallel",
    12: "Local",
}

_LAUNCH_TIME = int(time.time())


def to_json(ad, return_dict=False, reduce_data=False):
    if ad.get("TaskType") == "ROOT":
        return None

    result = {}

    result["RecordTime"] = record_time(ad)

    result["ScheddName"] = ad.get("GlobalJobId", "UNKNOWN").split("#")[0]
    result["StartdSlot"] = ad.get(
        "RemoteHost", ad.get("LastRemoteHost", "UNKNOWN@UNKNOWN")
    ).split("@")[0]
    result["StartdName"] = ad.get(
        "RemoteHost", ad.get("LastRemoteHost", "UNKNOWN@UNKNOWN")
    ).split("@")[-1]

    result["Status"] = STATUS.get(ad.get("JobStatus"), "Unknown")
    result["Universe"] = UNIVERSE.get(ad.get("JobUniverse"), "Unknown")

    bulk_convert_ad_data(ad, result)

    if return_dict:
        return result
    else:
        return json.dumps(result)


def record_time(ad):
    """
    RecordTime falls back to launch time as last-resort and for jobs in the queue

    For Completed/Removed/Error jobs, try to update it:
        - to CompletionDate if present
        - else to EnteredCurrentStatus if present
        - else fall back to launch time
    """
    if ad["JobStatus"] in [3, 4, 6]:
        if ad.get("CompletionDate", 0) > 0:
            return ad["CompletionDate"]

        elif ad.get("EnteredCurrentStatus", 0) > 0:
            return ad["EnteredCurrentStatus"]

    return _LAUNCH_TIME


def case_normalize(attr):
    """
    Given a ClassAd attr name, check to see if it's known. If so, normalize the
    attr name's casing to the known value. Otherwise, make the key lowercase.
    (Elasticsearch field names are case-sensitive.)
    """
    # Get the union of all known attrs
    known_attrs = (
        TEXT_ATTRS
        | INDEXED_KEYWORD_ATTRS
        | NOINDEX_KEYWORD_ATTRS
        | FLOAT_ATTRS
        | INT_ATTRS
        | DATE_ATTRS
        | BOOL_ATTRS
        | IGNORE_ATTRS
    )

    # Build a dict of lowercased attrs -> known attrs
    known_attrs = {x.casefold(): x for x in known_attrs}

    # We can also make auto-mapped fields more readable
    auto_attrs = [
        re.compile(r"^(.*)(Date)$"),
        re.compile(r"^(.*)(Provisioned)$"),
        re.compile(r"^(Request)([A-Za-df-z].*)$"),  # ignore "Requested"
        re.compile(r"^(Want|Has|Is)([A-Z_].*)$", re.IGNORECASE),
    ]

    if attr.casefold() in known_attrs:
        # Known attr
        return known_attrs[attr.casefold()]

    for attr_re in auto_attrs:
        match = attr_re.match(attr)
        if match:
            # Auto-mapped attr
            return "".join([x.capitalize() for x in match.groups()])

    # Unknown attr
    return attr.casefold()


def bulk_convert_ad_data(ad, result):
    """
    Given a ClassAd, bulk convert to a python dictionary.
    """
    keys = set(ad.keys())
    for key in keys:
        key = case_normalize(key)

        # Do not return ignored attrs
        if key in IGNORE_ATTRS:
            continue

        # Do not return invalid expressions
        try:
            value = ad.eval(key)
        except:
            continue

        if isinstance(value, classad.Value):
            if (value is classad.Value.Error) or (value is classad.Value.Undefined):
                # Could not evaluate expression, store raw expression
                value = str(ad.get(key))
                key = f"{key}_EXPR"
            else:
                continue
        elif (
            key in TEXT_ATTRS
            or key in INDEXED_KEYWORD_ATTRS
            or key in NOINDEX_KEYWORD_ATTRS
        ):
            value = str(value)
            if len(value) > 256:  # truncate strings longer than 256 characters
                value = f"{value[:253]}..."
        elif key in FLOAT_ATTRS:
            try:
                value = float(value)
            except ValueError:
                logging.warning(
                    f"Failed to convert key {key} with value {repr(value)} to float"
                )
                continue
        elif (
            key in INT_ATTRS
            or AUTO_ATTRS["provisioned_attrs"].match(key)
            or AUTO_ATTRS["resource_request_attrs"].match(key)
        ):
            try:
                value = int(value)
            except ValueError:
                logging.warning(
                    f"Failed to convert key {key} with value {repr(value)} to int"
                )
                continue
        elif key in BOOL_ATTRS or AUTO_ATTRS["target_boolean_attrs"].match(key):
            try:
                value = bool(value)
            except ValueError:
                logging.warning(
                    f"Failed to convert key {key} with value {repr(value)} to bool"
                )
                continue
        elif key in DATE_ATTRS or AUTO_ATTRS["date_attrs"].match(key):
            try:
                value = int(value)
                if value == 0:
                    continue
            except ValueError:
                logging.warning(
                    f"Failed to convert key {key} with value {repr(value)} to int for a date field"
                )
                continue

        result[key] = value


def unique_doc_id(doc):
    """
    Return a string of format "<GlobalJobId>#<RecordTime>"
    To uniquely identify documents (not jobs)

    Note that this uniqueness breaks if the same jobs are submitted
    with the same RecordTime
    """
    return f"{doc['GlobalJobId']}#{doc['RecordTime']}"
