#!/usr/bin/env pytest

"""
Regression tests for condor_adstash ClassAd-to-doc conversion.
Uses static history files with known contents.
"""

import sys
from pathlib import Path

import pytest

# Add the adstash package to the path
sys.path.insert(0, str(Path(__file__).parent.parent / "condor_scripts"))

from adstash.ad_sources.ad_file import FileAdSource
from adstash.ad_converters.job import JobClassAdConverter
from adstash.ad_converters.transfer_epoch import TransferEpochClassAdConverter
from adstash.mapping.functions import get_default_mappings
from adstash.mapping import job, transfer_epoch


JOB_HISTORY_FILE = Path(__file__).parent / "test_adstash_job_history_input"
TRANSFER_HISTORY_FILE = Path(__file__).parent / "test_adstash_transfer_epoch_history_input"


@pytest.fixture
def mappings():
    return get_default_mappings(job)


@pytest.fixture
def converter(mappings):
    return JobClassAdConverter(mapping=mappings)


@pytest.fixture
def ads():
    """Parse ads from the history file using the same path as condor_adstash --ad_file."""
    ad_source = FileAdSource()
    return list(ad_source.fetch_ads(JOB_HISTORY_FILE))


@pytest.fixture
def docs(converter, ads):
    """Convert all ads to docs using the full converter pipeline."""
    return [converter.convert_ad_to_doc(ad) for ad in ads]


@pytest.fixture
def completed_doc(docs):
    """Job 123.456, completed, vanilla universe."""
    return docs[0]


@pytest.fixture
def removed_doc(docs):
    """Job 123.457, removed, local universe."""
    return docs[1]


class TestGlobalJobIdParsing:
    """#1: GlobalJobId is parsed to ScheddName#ClusterId.ProcId#QDate"""

    def test_schedd_name(self, completed_doc):
        assert completed_doc["ScheddName"] == "ap.example.edu"

    def test_global_job_id(self, completed_doc):
        assert completed_doc["GlobalJobId"] == "ap.example.edu#123.456#1781010000"

    def test_cluster_proc(self, completed_doc):
        assert completed_doc["ClusterId"] == 123
        assert completed_doc["ProcId"] == 456


class TestCustomAttributes:
    """#2: Unknown attrs get lowercased field name and string value"""

    def test_custom_attr_lowercased(self, completed_doc):
        assert "customattrfoo" in completed_doc
        assert "CustomAttrFoo" not in completed_doc

    def test_custom_attr_string_value(self, completed_doc):
        # Input is integer 842, but unknown attrs are mapped as keyword (string)
        assert completed_doc["customattrfoo"] == "842"

    def test_custom_attr_unknown_without_mapping(self, completed_doc):
        # Without a custom mapping, CustomAttrKnown is also lowercased/stringified
        assert "customattrknown" in completed_doc
        assert completed_doc["customattrknown"] == "123"


class TestCustomAttributeWithMapping:
    """#2b: Custom attrs with an existing mapping preserve case and type"""

    @pytest.fixture
    def converter_with_custom_mapping(self):
        custom_mappings = get_default_mappings(job)
        custom_mappings["properties"]["CustomAttrKnown"] = {"type": "long"}
        return JobClassAdConverter(mapping=custom_mappings)

    @pytest.fixture
    def doc_with_custom_mapping(self, converter_with_custom_mapping, ads):
        return converter_with_custom_mapping.convert_ad_to_doc(ads[0])

    def test_known_custom_attr_preserves_case(self, doc_with_custom_mapping):
        assert "CustomAttrKnown" in doc_with_custom_mapping
        assert "customattrknown" not in doc_with_custom_mapping

    def test_known_custom_attr_preserves_type(self, doc_with_custom_mapping):
        assert doc_with_custom_mapping["CustomAttrKnown"] == 123
        assert isinstance(doc_with_custom_mapping["CustomAttrKnown"], int)


class TestExpressionHandling:
    """#3: Expressions become _EXPR with string value when eval fails"""

    def test_unevaluable_expr_becomes_expr_field(self, completed_doc):
        # Requirements references TARGET which is not in the ad,
        # so it can't be fully evaluated
        assert "Requirements_EXPR" in completed_doc
        assert "Requirements" not in completed_doc

    def test_unevaluable_expr_is_string(self, completed_doc):
        assert isinstance(completed_doc["Requirements_EXPR"], str)

    def test_evaluable_expr_becomes_value(self, completed_doc):
        # PeriodicRemove = JobStatus == 4, and JobStatus is 4, so it evals to true
        assert completed_doc["PeriodicRemove"] is True

    def test_evaluable_expr_different_context(self, removed_doc):
        # PeriodicRemove = JobStatus == 4, but JobStatus is 3, so it evals to false
        assert removed_doc["PeriodicRemove"] is False


class TestNestedClassAds:
    """#4: Object-type ClassAds are flattened into dotted field names"""

    def test_transfer_input_stats_flattened(self, completed_doc):
        assert "TransferInputStats.PELICANSizeBytesTotal" in completed_doc
        assert "TransferInputStats.CedarFilesCountTotal" in completed_doc

    def test_transfer_input_stats_values(self, completed_doc):
        assert completed_doc["TransferInputStats.PELICANSizeBytesTotal"] == 5000000000
        assert completed_doc["TransferInputStats.CedarFilesCountTotal"] == 20


class TestKnownAttrTypes:
    """#5: Known attrs are converted to the correct types"""

    def test_date_attr(self, completed_doc):
        assert completed_doc["CompletionDate"] == 1781013600
        assert isinstance(completed_doc["CompletionDate"], int)

    def test_int_attr(self, completed_doc):
        assert completed_doc["RequestMemory"] == 123456
        assert isinstance(completed_doc["RequestMemory"], int)

    def test_float_attr(self, completed_doc):
        assert completed_doc["Rank"] == pytest.approx(12.34)
        assert isinstance(completed_doc["Rank"], float)

    def test_bool_attr(self, completed_doc):
        assert completed_doc["StreamIn"] is True
        assert completed_doc["StreamOut"] is False

    def test_keyword_attr(self, completed_doc):
        assert completed_doc["Owner"] == "testuser"
        assert isinstance(completed_doc["Owner"], str)

    def test_large_int_attr(self, completed_doc):
        assert completed_doc["RequestDisk"] == 123456789


class TestIgnoredAttributes:
    """#6: IGNORE_ATTRS are dropped from output"""

    def test_env_is_dropped(self, completed_doc):
        assert "Env" not in completed_doc
        assert "env" not in completed_doc


class TestDerivedFields:
    """#7: Derived fields computed correctly"""

    def test_status_completed(self, completed_doc):
        assert completed_doc["Status"] == "Completed"

    def test_status_removed(self, removed_doc):
        assert removed_doc["Status"] == "Removed"

    def test_universe_vanilla(self, completed_doc):
        assert completed_doc["Universe"] == "Vanilla"

    def test_universe_local(self, removed_doc):
        assert removed_doc["Universe"] == "Local"

    def test_startd_slot(self, completed_doc):
        assert completed_doc["StartdSlot"] == "slot1_2"

    def test_startd_name(self, completed_doc):
        assert completed_doc["StartdName"] == "ep.example.edu"


class TestDynamicTemplates:
    """#8: Unknown attrs matched and handled by the correct dynamic template"""

    def test_want_attr_becomes_bool(self, completed_doc):
        # WantFlocking is not in BOOL_ATTRS but matches target_bool_attrs template;
        # dynamic template matches preserve original case
        assert completed_doc["WantFlocking"] is True
        assert isinstance(completed_doc["WantFlocking"], bool)

    def test_provisioned_attr(self, completed_doc):
        # IoHeavyProvisioned matches provisioned_attrs template (preserves case)
        assert "IoHeavyProvisioned" in completed_doc

    def test_resource_request_attr(self, completed_doc):
        # RequestIoHeavy matches resource_request_attrs template (preserves case)
        assert "RequestIoHeavy" in completed_doc

    def test_num_attr(self, completed_doc):
        # NumCookies matches num_attrs template (preserves case)
        assert "NumCookies" in completed_doc

    def test_transformbody_attr(self, completed_doc):
        # TransformBody_RequestMemory matches transformbody_attrs template
        assert "TransformBody_RequestMemory" in completed_doc


class TestTimestampSelection:
    """#9: CompletionDate preferred, fallback to EnteredCurrentStatus"""

    def test_completed_job_uses_completion_date(self, completed_doc):
        assert completed_doc["@timestamp"] == 1781013600
        assert completed_doc["RecordTime"] == 1781013600

    def test_removed_job_falls_back_to_entered_current_status(self, removed_doc):
        assert removed_doc["@timestamp"] == 1781013590
        assert removed_doc["RecordTime"] == 1781013590


class TestUndefinedValues:
    """#10: Undefined values should be dropped"""

    def test_undefined_attr_dropped(self, completed_doc):
        assert "TransferOutput" not in completed_doc
        assert "transferoutput" not in completed_doc


class TestDocId:
    """#11: Doc IDs are deterministic"""

    def test_same_ad_same_id(self, converter, ads):
        id1 = converter.get_unique_doc_id(converter.convert_ad_to_doc(ads[0]))
        id2 = converter.get_unique_doc_id(converter.convert_ad_to_doc(ads[0]))
        assert id1 == id2

    def test_different_ads_different_ids(self, converter, ads):
        id1 = converter.get_unique_doc_id(converter.convert_ad_to_doc(ads[0]))
        id2 = converter.get_unique_doc_id(converter.convert_ad_to_doc(ads[1]))
        assert id1 != id2


# --- Transfer Epoch History Tests ---


@pytest.fixture
def transfer_mappings():
    return get_default_mappings(transfer_epoch)


@pytest.fixture
def transfer_converter(transfer_mappings):
    return TransferEpochClassAdConverter(mapping=transfer_mappings)


@pytest.fixture
def transfer_ads():
    """Parse ads from the transfer epoch history file."""
    ad_source = FileAdSource()
    return list(ad_source.fetch_ads(TRANSFER_HISTORY_FILE))


@pytest.fixture
def transfer_all_docs(transfer_converter, transfer_ads):
    """Convert all transfer ads, returning a list of lists (multiple docs per ad)."""
    return [list(transfer_converter.convert_transfer_ad_to_docs(ad)) for ad in transfer_ads]


@pytest.fixture
def single_attempt_docs(transfer_all_docs):
    """ProcId 456: single attempt, successful transfer."""
    return transfer_all_docs[0]


@pytest.fixture
def multi_attempt_success_docs(transfer_all_docs):
    """ProcId 457: 3 attempts, first two fail, third succeeds."""
    return transfer_all_docs[1]


@pytest.fixture
def multi_attempt_failure_docs(transfer_all_docs):
    """ProcId 458: 3 attempts, all fail with permission denied."""
    return transfer_all_docs[2]


class TestTransferEpochAdSplitting:
    """#12: A single transfer epoch ad expands into the correct number of docs."""

    def test_single_attempt_yields_one_doc(self, single_attempt_docs):
        assert len(single_attempt_docs) == 1

    def test_multi_attempt_success_yields_three_docs(self, multi_attempt_success_docs):
        assert len(multi_attempt_success_docs) == 3

    def test_multi_attempt_failure_yields_three_docs(self, multi_attempt_failure_docs):
        assert len(multi_attempt_failure_docs) == 3


class TestTransferEpochSingleAttempt:
    """#13: Single-attempt successful transfer has correct fields."""

    def test_transfer_success(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["TransferSuccess"] is True

    def test_transfer_file_info(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["TransferFileName"] == "input.txt"
        assert doc["TransferProtocol"] == "osdf"
        assert doc["TransferType"] == "download"
        assert doc["TransferFileBytes"] == 1000000

    def test_attempt_fields(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["Attempts"] == 1
        assert doc["Attempt"] == 0
        assert doc["FinalAttempt"] is True

    def test_timestamp(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["@timestamp"] == 1781013601
        assert doc["RecordTime"] == 1781013601

    def test_derived_fields(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["ClusterId"] == 123
        assert doc["ProcId"] == 456
        assert doc["StartdSlot"] == "slot1_2"
        assert doc["StartdName"] == "ep.example.edu"

    def test_schedd_name_missing_without_live_schedd(self, single_attempt_docs):
        # ScheddName is populated from the schedd ad during process_ads,
        # which doesn't happen when reading from a file
        doc = single_attempt_docs[0]
        assert doc["ScheddName"] is None

    def test_custom_attr_on_transfer_epoch(self, single_attempt_docs):
        doc = single_attempt_docs[0]
        assert doc["customattrfoo"] == "842"


class TestTransferEpochMultiAttemptSuccess:
    """#14: Multi-attempt transfer with eventual success."""

    def test_non_final_attempts_have_limited_fields(self, multi_attempt_success_docs):
        # Non-final attempts only get identifying attrs + per-attempt data
        doc0 = multi_attempt_success_docs[0]
        assert doc0["Attempt"] == 0
        assert doc0["FinalAttempt"] is False
        assert doc0["TransferProtocol"] == "osdf"
        assert doc0["TransferType"] == "download"
        # Non-final attempts should NOT have the full result fields
        assert "TransferFileBytes" not in doc0

    def test_non_final_attempt_has_error(self, multi_attempt_success_docs):
        doc0 = multi_attempt_success_docs[0]
        assert "AttemptError" in doc0
        assert "connection timed out" in doc0["AttemptError"]

    def test_final_attempt_has_full_result(self, multi_attempt_success_docs):
        doc2 = multi_attempt_success_docs[2]
        assert doc2["Attempt"] == 2
        assert doc2["FinalAttempt"] is True
        assert doc2["TransferSuccess"] is True
        assert doc2["TransferFileBytes"] == 5000000
        assert doc2["TransferFileName"] == "bigfile.dat"

    def test_per_attempt_endpoint(self, multi_attempt_success_docs):
        assert multi_attempt_success_docs[0]["Endpoint"] == "cache-a.example.edu:8443"
        assert multi_attempt_success_docs[1]["Endpoint"] == "cache-b.example.edu:8443"
        assert multi_attempt_success_docs[2]["Endpoint"] == "cache-c.example.edu:8443"

    def test_attempts_count(self, multi_attempt_success_docs):
        for doc in multi_attempt_success_docs:
            assert doc["Attempts"] == 3


class TestTransferEpochMultiAttemptFailure:
    """#15: Multi-attempt transfer where all attempts fail."""

    def test_all_attempts_fail(self, multi_attempt_failure_docs):
        doc2 = multi_attempt_failure_docs[2]
        assert doc2["FinalAttempt"] is True
        assert doc2["TransferSuccess"] is False
        assert doc2["TransferFileBytes"] == 0

    def test_error_data_merged_on_final_attempt(self, multi_attempt_failure_docs):
        doc2 = multi_attempt_failure_docs[2]
        # ErrorType exists in both the error ad and DeveloperData, so it gets Debug prefix
        assert doc2["DebugErrorType"] == "Authorization"
        # Retryable and ErrorMessage only exist in DeveloperData, no prefix needed
        assert doc2["Retryable"] is False
        assert doc2["ErrorMessage"] == "Permission denied"

    def test_transfer_error_string(self, multi_attempt_failure_docs):
        doc2 = multi_attempt_failure_docs[2]
        assert "Permission denied" in doc2["TransferError"]

    def test_non_final_attempts_have_error_data(self, multi_attempt_failure_docs):
        # Each non-final attempt is paired with an error from TransferErrorData
        doc0 = multi_attempt_failure_docs[0]
        assert doc0["FinalAttempt"] is False
        assert doc0["DebugErrorType"] == "Authorization"


class TestTransferEpochDocId:
    """#16: Transfer epoch doc IDs are unique per attempt."""

    def test_unique_ids_across_attempts(self, transfer_converter, multi_attempt_success_docs):
        ids = [transfer_converter.get_unique_doc_id(doc) for doc in multi_attempt_success_docs]
        assert len(set(ids)) == 3

    def test_unique_ids_across_ads(self, transfer_converter, single_attempt_docs, multi_attempt_success_docs):
        id1 = transfer_converter.get_unique_doc_id(single_attempt_docs[0])
        id2 = transfer_converter.get_unique_doc_id(multi_attempt_success_docs[0])
        assert id1 != id2
