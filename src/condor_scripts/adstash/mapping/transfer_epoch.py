# Copyright 2025 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from adstash.mapping.common import MAX_KEYWORD_LEN, METADATA_MAPPING, OTHER_MAPPING_SETTINGS


INDEXED_TEXT_ATTRS = set()

NON_INDEXED_TEXT_ATTRS = {
    "AttemptError",
    "ErrorString",
    "TransferError",
    "TransferError1",
}

INDEXED_KEYWORD_ATTRS = {
    "Endpoint",
    "ErrorType",
    "ErrorMessage",
    "FailedName",
    "FailedServer",
    "FailureType",
    "DebugErrorType",
    "GLIDEIN_ResourceName",
    "GLIDEIN_Site",
    "GlobalJobId",
    "HttpCacheHitOrMiss",
    "HttpCacheHost",
    "Owner",
    "PelicanClientVersion",
    "ProjectName",
    "PluginVersion",
    "RemoteHost",
    "ScheddName",
    "ServerVersion",
    "StartdName",
    "StartdSlot",
    "TransferHostName",
    "TransferLocalMachineName",
    "TransferProtocol",
    "TransferType",
    "TransferFileName",
    "TransferUrl",
    "TransferClass",
    "EpochAdType",
}

NON_INDEXED_KEYWORD_ATTRS = set()

FLOAT_ATTRS = {
    "AttemptTime",
    "ConnectionTimeSeconds",
    "TimeToFirstByte",
}

INT_ATTRS = {
    "RunInstanceId",
    "Attempt",
    "Attempts",
    "AttemptFileBytes",
    "ClusterId",
    "DataAge",
    "ErrorCode",
    "LibcurlReturnCode",
    "NumShadowStarts",
    "PelicanErrorCode",
    "ProcId",
    "TransferFileBytes",
    "TransferFileNumber",
    "TransferHttpStatusCode",
    "TransferTotalBytes",
    "TransferTries",
}

DATE_ATTRS = {
    "@timestamp",
    "AttemptEndTime",
    "EpochWriteDate",
    "RecordTime",
    "TransferEndTime",
    "TransferStartTime",
}

BOOL_ATTRS = {
    "CacheHit",
    "FinalAttempt",
    "IsRetryable",
    "IsRetryable1",
    "NoPluginResults",
    "PluginLaunched",
    "Retryable",
    "TransferSuccess",
}

OBJECT_ATTRS = {
    "ClientChecksums",
    "ServerChecksums",
    "DirectorDecision",
}

NESTED_ATTRS = {
    "InputPluginInvocations",
    "OutputPluginInvocations",
    "CheckpointPluginInvocations",
}

DYNAMIC_TEMPLATES = [
    {"raw_expression": {  # Attrs ending in "_EXPR" are generated during
        "match": r"*_EXPR",  # ad conversion for expressions that cannot be evaluated
        "mapping": {"type": "text", "norms": "false", "index": "false"},
    }},
    {"date_attrs": {  # Attrs ending in "Date" are usually timestamps
        "match": r"*Date",
        "mapping": {"type": "date", "format": "epoch_second||strict_date_optional_time"},
    }},
    {"target_bool_attrs": {  # Attrs starting with "Want", "Has", or
        "match_pattern": "regex",  # "Is" are usually boolean checks
        "match": r"(?i)^(Want|Has|Is).+$",
        "mapping": {"type": "boolean"},
    }},
    {"DEFAULT": {  # DEFAULT MAPPING - will be evaluated last
        "match_mapping_type": "string",  # Store unknown attrs as indexed keywords
        "mapping": {"type": "keyword", "ignore_above": MAX_KEYWORD_LEN},  # https://www.elastic.co/guide/en/elasticsearch/reference/7.17/tune-for-disk-usage.html#default-dynamic-string-mapping
    }}
]
