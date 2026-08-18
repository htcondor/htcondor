#!/usr/bin/env pytest

"""
Regression tests for condor_adstash JSON output, index setup, settings, and mapping merge logic.
Does not test HTCondor or search engine integrations.
"""

import argparse
import json
import sys
from pathlib import Path

import pytest

# Add the adstash package to the path
sys.path.insert(0, str(Path(__file__).parent.parent / "condor_scripts"))

from adstash.index_setup import init_index
from adstash.interfaces.json_file import JSONFileInterface
from adstash.mapping.functions import get_default_mappings, merge_properties, count_total_fields
from adstash.mapping import job
from adstash.settings import SearchEngineSettings, calculate_field_limit


class TestJSONFileMakeBulkBody:
    """#1. JSON file interface produces valid JSON output"""

    @pytest.fixture
    def interface(self):
        return JSONFileInterface(json_dir=Path("."))

    @pytest.fixture
    def sample_docs(self):
        return [
            ("doc1", {"Owner": "testuser", "ClusterId": 1}),
            ("doc2", {"Owner": "testuser", "ClusterId": 2}),
        ]

    def test_output_is_valid_json(self, interface, sample_docs):
        body = interface.make_bulk_body(sample_docs)
        parsed = json.loads(body)
        assert isinstance(parsed, list)
        assert len(parsed) == 2

    def test_doc_ids_included(self, interface, sample_docs):
        body = interface.make_bulk_body(sample_docs)
        parsed = json.loads(body)
        assert parsed[0]["_id"] == "doc1"
        assert parsed[1]["_id"] == "doc2"

    def test_metadata_attached(self, interface, sample_docs):
        body = interface.make_bulk_body(sample_docs, metadata={"source": "test"})
        parsed = json.loads(body)
        assert parsed[0]["metadata"]["source"] == "test"

    def test_empty_metadata_when_none(self, interface, sample_docs):
        body = interface.make_bulk_body(sample_docs)
        parsed = json.loads(body)
        assert parsed[0]["metadata"] == {}


class TestMergeProperties:
    """#2. merge_properties handles conflicts with multi-fields"""

    def test_no_conflict_adds_field(self):
        existing = {"Owner": {"type": "keyword"}}
        new = {"ClusterId": {"type": "long"}}
        result = merge_properties(existing, new)
        assert result["Owner"]["type"] == "keyword"
        assert result["ClusterId"]["type"] == "long"

    def test_same_type_merges(self):
        existing = {"Owner": {"type": "keyword", "ignore_above": 256}}
        new = {"Owner": {"type": "keyword", "ignore_above": 32766}}
        result = merge_properties(existing, new)
        assert result["Owner"]["ignore_above"] == 32766

    def test_format_is_immutable_on_existing(self):
        existing = {"CompletionDate": {"type": "date", "format": "epoch_second"}}
        new = {"CompletionDate": {"type": "date", "format": "epoch_second||strict_date_optional_time"}}
        result = merge_properties(existing, new)
        # Existing format should not be overwritten
        assert result["CompletionDate"]["format"] == "epoch_second"

    def test_type_conflict_creates_multi_field(self):
        existing = {"RemoteWallClockTime": {"type": "keyword"}}
        new = {"RemoteWallClockTime": {"type": "long"}}
        result = merge_properties(existing, new)
        assert result["RemoteWallClockTime"]["type"] == "keyword"
        assert "fields" in result["RemoteWallClockTime"]
        assert "long" in result["RemoteWallClockTime"]["fields"]
        assert result["RemoteWallClockTime"]["fields"]["long"]["type"] == "long"

    def test_object_type_conflict_skipped(self):
        existing = {"Stats": {"type": "object"}}
        new = {"Stats": {"type": "keyword"}}
        result = merge_properties(existing, new)
        # Object type conflicts can't create multi-fields, original preserved
        assert result["Stats"]["type"] == "object"
        assert "fields" not in result["Stats"]

    def test_existing_properties_not_mutated(self):
        existing = {"Owner": {"type": "keyword"}}
        original_owner = existing["Owner"].copy()
        new = {"Owner": {"type": "long"}}
        merge_properties(existing, new)
        # The original dict should not have been mutated
        assert existing["Owner"] == original_owner

    def test_requires_at_least_two_dicts(self):
        with pytest.raises(ValueError):
            merge_properties({"Owner": {"type": "keyword"}})


class TestFieldLimitCalculation:
    """#3. Field limit is calculated as max(2x field count, previous limit)"""

    def test_basic_calculation(self):
        mappings = {"properties": {"a": {"type": "keyword"}, "b": {"type": "long"}}}
        assert calculate_field_limit(mappings) == 4  # 2 * 2 fields

    def test_respects_previous_limit(self):
        mappings = {"properties": {"a": {"type": "keyword"}}}
        # 2 * 1 = 2, but previous limit is 1000
        assert calculate_field_limit(mappings, previous_limit=1000) == 1000

    def test_multi_fields_counted(self):
        mappings = {"properties": {
            "field": {"type": "keyword", "fields": {"long": {"type": "long"}}},
        }}
        # 1 field + 1 multi-field = 2 fields, limit = 4
        assert calculate_field_limit(mappings) == 4

    def test_settings_object_bumps_limit(self):
        mappings = get_default_mappings(job)
        # With a ridiculously low existing limit, the computed limit should exceed it
        ses = SearchEngineSettings(
            index_name="test",
            mappings=mappings,
            existing_settings={"index": {"mapping": {"total_fields": {"limit": 10}}}},
        )
        computed_limit = ses.update_settings["index.mapping.total_fields.limit"]
        assert computed_limit > 10
        assert computed_limit == calculate_field_limit(mappings, previous_limit=10)

    def test_settings_object_preserves_adequate_limit(self):
        mappings = {"properties": {"a": {"type": "keyword"}}}
        ses = SearchEngineSettings(
            index_name="test",
            mappings=mappings,
            existing_settings={"index": {"mapping": {"total_fields": {"limit": 50000}}}},
        )
        # 2 * 1 = 2, but existing limit 50000 is higher
        assert ses.update_settings["index.mapping.total_fields.limit"] == 50000


class TestInitIndex:
    """#4. init_index writes ILM, template, index JSON, and README"""

    @pytest.fixture
    def init_args(self, tmp_path):
        return argparse.Namespace(
            se_index_name="htcondor",
            custom_field_properties=None,
            custom_dynamic_templates=None,
            custom_index_settings=None,
            init_output_directory=tmp_path,
            use_alias=True,
            use_ilm=True,
            use_template=True,
        )

    def test_creates_expected_files(self, init_args, tmp_path):
        init_index(ad_type="history", args=init_args)
        assert (tmp_path / "README").exists()
        assert (tmp_path / "htcondor-ilm.json").exists()
        assert (tmp_path / "htcondor-template.json").exists()
        assert (tmp_path / "htcondor-000001.json").exists()

    def test_ilm_policy_has_phases(self, init_args, tmp_path):
        init_index(ad_type="history", args=init_args)
        with open(tmp_path / "htcondor-ilm.json") as f:
            ilm = json.load(f)
        assert "hot" in ilm["policy"]["phases"]
        assert "warm" in ilm["policy"]["phases"]
        assert "cold" in ilm["policy"]["phases"]

    def test_template_has_index_patterns(self, init_args, tmp_path):
        init_index(ad_type="history", args=init_args)
        with open(tmp_path / "htcondor-template.json") as f:
            template = json.load(f)
        assert template["index_patterns"] == ["htcondor-*"]

    def test_initial_index_has_alias(self, init_args, tmp_path):
        init_index(ad_type="history", args=init_args)
        with open(tmp_path / "htcondor-000001.json") as f:
            index = json.load(f)
        assert index["aliases"]["htcondor"]["is_write_index"] is True

    def test_no_alias_no_ilm(self, tmp_path):
        args = argparse.Namespace(
            se_index_name="htcondor",
            custom_field_properties=None,
            custom_dynamic_templates=None,
            custom_index_settings=None,
            init_output_directory=tmp_path,
            use_alias=False,
            use_ilm=False,
            use_template=False,
        )
        init_index(ad_type="history", args=args)
        assert (tmp_path / "htcondor.json").exists()
        assert not (tmp_path / "htcondor-ilm.json").exists()
        assert not (tmp_path / "htcondor-template.json").exists()
