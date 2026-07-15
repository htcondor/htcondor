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

import re
import json
import math
import time
import logging

from functools import lru_cache
from collections import defaultdict, OrderedDict

from adstash.utils import classad_json_serializer
from adstash.mapping.common import MAX_KEYWORD_LEN
from adstash.mapping.functions import get_ignore_attrs, merge_properties, merge_dynamic_templates

import classad2 as classad


_LAUNCH_TIME = int(time.time())


def _normalize_target_bool_attr(attr: str) -> str:
    attr = attr.lower()
    prefix = next((x for x in ["want", "has", "is"] if attr.startswith(x)), "")
    middle = "_" if len(attr) > len(prefix) and attr[len(prefix)] == "_" else ""
    right = attr[len(prefix) + len(middle):]
    return f"{prefix.capitalize()}{middle}{right.capitalize()}"


DYNAMIC_TEMPLATE_FIELD_NAME_NORMALIZERS = {
    "target_bool_attrs": _normalize_target_bool_attr,
}


def coerce_int(i):
    try:
        return int(i)
    except (ValueError, TypeError):
        pass
    return round(float(i))


def strict_bool(i):
    if isinstance(i, bool):
        return bool(i)
    if isinstance(i, str):
        if i.lower() in {"1", "t", "true"}:
            return True
        if i.lower() in {"0", "f", "false"}:
            return False
        try:  # "1.0" / "0.0"
            return strict_bool(float(i))
        except ValueError:
            pass
    if isinstance(i, (int, float)):
        if abs(i - 1) < 1e-8:  # allow for some tiny error
            return True
        if abs(i) < 1e-8:
            return False
    raise ValueError(f"The truth value of {i} is ambiguous")


def finite_float(x):
    v = float(x)
    if not math.isfinite(v):
        raise ValueError(f"Non-finite float value: {v}")
    return v


FIELD_TYPE_MAP = {
    "text": str,
    "keyword": str,
    "float": finite_float,
    "double": finite_float,
    "long": coerce_int,
    "date": coerce_int,
    "boolean": strict_bool,
    "object": dict,
    "nested": list,
}


class GenericClassAdConverter():

    def __init__(
            self,
            mapping={},
            projection=set(),
            ignore_attrs=set(),
            required_attrs=set(),
            timestamp_fields=["EnteredCurrentStatus"],
            doc_id_fields=["RecordTime"],
            ):
        self.mapping = mapping
        self.ignore_attrs = get_ignore_attrs(mapping, ignore_attrs)
        self.required_attrs = required_attrs
        self.timestamp_fields = timestamp_fields
        self.doc_id_fields = doc_id_fields
        if len(projection) > 0:
            self.projection = {attr.lower() for attr in projection | required_attrs}
        else:
            self.projection = None
        self.known_field_types = self.get_known_field_types(self.mapping)
        self.dynamic_templates_matchers = self.get_dynamic_template_matchers(self.mapping)

    def update_mapping(self, mapping: dict):
        """
        Update the mapping with an incoming mapping
        """
        mapping = mapping.copy()  # make a copy since we're mutating the incoming mapping
        # pop off the properties and dynamic_templates
        self.mapping["properties"] = merge_properties(self.mapping.get("properties", {}), mapping.pop("properties", {}))
        self.mapping["dynamic_templates"] = merge_dynamic_templates(self.mapping.get("dynamic_templates", OrderedDict()), mapping.pop("dynamic_templates", OrderedDict()))
        # update anything else that happens to be in the new mapping
        self.mapping.update(mapping)
        # refresh the known_field_types and dynamic_templates_matchers
        self.known_field_types = self.get_known_field_types(self.mapping)
        self.dynamic_templates_matchers = self.get_dynamic_template_matchers(self.mapping)

    @lru_cache(maxsize=2048)
    def log_once(self, msg, handle=logging.warning):
        """
        Only print the same warning (approximately) once.
        """
        handle(msg)

    def get_known_field_types(self, mapping: dict, parent_field_names=[]) -> defaultdict:
        '''
        Build up a map of known field names and types,
        keyed on the lowercased attribute names. For example,
        if "lastremotewallclocktime" and "LastRemoteWallClockTime"
        are both defined in the mapping as keyword and long,
        respectively, the map should include:
        {
            ...
            "lastremotewallclocktime": {
                "lastremotewallclocktime": str,
                "LastRemoteWallClockTime": int,
            }
            ...
        }

        Also flatten any objects' subproperty names, e.g.:
        {
            ...
            "numholdsbyreason.failedtocheckpoint": {
                "NumHoldsByReason.FailedToCheckpoint": int,
            }
            ...
        }
        '''
        known_field_types = defaultdict(dict)
        for base_field_name, field_properties in mapping["properties"].items():
            field_name_heirarchy = parent_field_names + [base_field_name]
            flattened_field_name = ".".join(field_name_heirarchy)
            if self.projection is not None and flattened_field_name.lower() not in self.projection:
                continue
            field_type = FIELD_TYPE_MAP[field_properties.get("type", "object")]
            known_field_types[flattened_field_name.lower()][flattened_field_name] = field_type
            if field_type is dict and "properties" in field_properties:
                known_field_types = known_field_types | self.get_known_field_types(field_properties, field_name_heirarchy)
        return known_field_types

    def get_dynamic_template_matchers(self, mapping: dict) -> OrderedDict:
        """
        Return an ordered dict with keys containing the names of dynamic templates
        and values containing information on how to match and map field names.
        """
        matchers = OrderedDict()
        for dynamic_template in mapping.get("dynamic_templates", []):
            dt_name = list(dynamic_template.keys())[0]
            dt = dynamic_template[dt_name]
            match_type = "wildcard"
            match_pattern = dt.get("match", "")
            if dt.get("match_pattern") == "regex":
                match_type = "regex"
                match_pattern = re.compile(dt.get("match", r"^$"))
            elif dt_name == "DEFAULT":
                match_type = "default"
                match_pattern = ""
            elif match_pattern.count("*") > 1:
                # Only a single * wildcard is supported in our implementation, even though
                # multiple * wildcards are valid in Elasticsearch dynamic templates.
                logging.warning(f"Dynamic template {dt_name} has multiple * wildcards in match pattern '{match_pattern}', which is not supported and will never match.")
            matchers[dt_name] = {
                "match_type": match_type,
                "match_pattern": match_pattern,
                "field_type": FIELD_TYPE_MAP[dt.get("mapping", {}).get("type", "keyword")],
            }
        return matchers

    @lru_cache(maxsize=2048)
    def map_unknown_field_type(self, attr: str):
        """
        Test to see if attr fits any dynamic templates,
        otherwise fall back to whatever the "DEFAULT"
        dynamic template is. Always return the *first*
        match (if any).
        """
        field_name = attr.lower()  # fallback to lowercase attr name if no match
        field_type = self.dynamic_templates_matchers["DEFAULT"]["field_type"]
        for dt_name, dt in self.dynamic_templates_matchers.items():
            if dt["match_type"] == "regex" and dt["match_pattern"].match(attr):
                field_name = DYNAMIC_TEMPLATE_FIELD_NAME_NORMALIZERS.get(dt_name, lambda x: x)(attr)
                field_type = dt["field_type"]
                self.log_once(f"Attr {attr} matched dynamic template {dt_name}", logging.info)
                break
            if dt["match_type"] == "wildcard" and "*" in dt["match_pattern"]:
                left, right = dt["match_pattern"].split("*", maxsplit=1)
                if attr.startswith(left) and attr.endswith(right):
                    field_name = DYNAMIC_TEMPLATE_FIELD_NAME_NORMALIZERS.get(dt_name, lambda x: x)(attr)
                    field_type = dt["field_type"]
                    self.log_once(f"Attr {attr} matched dynamic template {dt_name}", logging.info)
                    break
        else:  # Alert if we've never seen this before
            if not ({field_name, f"{field_name}_EXPR"} & self.mapping["properties"].keys()):
                self.log_once(f"Encountered new/unknown attr {attr}")
        return field_name, field_type

    def convert_attr_to_dict(self, attr: str, value, full_ad, preserve_case: bool = False, plain_dict: bool = False) -> dict:
        """
        Convert the given ClassAd attribute-value pair to a dict
        which can be merged into a document.

        If preserve_case is True, the original attr name casing is used as the
        field name when no mapping is found (instead of the lowercased DEFAULT).
        Use this when recursing into a known object field.
        """
        doc = {}

        # 1. Get the field name and field type mappings if known
        known_mappings = True
        field_names_types = self.known_field_types[attr.lower()]

        # 2. Get the field name and field type mappings if unknown
        if not field_names_types:
            known_mappings = False
            mapped_name, mapped_type = self.map_unknown_field_type(attr)
            if preserve_case:
                mapped_name = attr
            field_names_types = {mapped_name: mapped_type}

        # 3. Map attr to all matching fields
        for field_name, field_type in field_names_types.items():

            field_value = None

            # 4. Handle lists (nested fields) separately
            if isinstance(value, list):

                if known_mappings and field_type is list:
                    converted = []
                    failed = False
                    for item in value:
                        if not plain_dict:
                            try:
                                item = json.loads(json.dumps(item, default=classad_json_serializer))
                            except Exception:
                                self.log_once(f"Failed to serialize item in {attr} to JSON")
                                doc[field_name.lower()] = self.truncate(field_name.lower(), str(value))
                                failed = True
                                continue
                        converted.append(item)
                    if not (failed and field_name == field_name.lower()):
                        doc[field_name] = converted
                else:
                    try:
                        field_value = json.dumps(value, default=classad_json_serializer)
                    except Exception:
                        self.log_once(f"Failed to convert list in {attr} to JSON")
                        field_value = str(value)
                    doc[field_name.lower()] = self.truncate(field_name.lower(), field_value)
                continue

            # 5. Handle objects separately
            elif isinstance(value, (dict, classad.ClassAd)):

                # Make sure the mapping is expected
                if known_mappings and field_type not in (dict, str,):
                    self.log_once(f"Could not convert {field_name} to {field_type}, got a dict-like")
                    continue

                # If we know this to be a string, try to make it a JSON blob
                if known_mappings and field_type is str:
                    try:
                        field_value = json.dumps(value, default=classad_json_serializer)
                    except Exception:
                        self.log_once(f"Failed to convert dict-like object in {attr} to JSON")
                        continue

                else:  # Otherwise recursively convert it, flattening the namespace
                    if not known_mappings:  # Preserve original case if we don't know what this is
                        field_name = attr  # because it might match a dynamic template later.
                    doc.update(self.convert_ad_to_dict(value, field_name, preserve_case=known_mappings, plain_dict=plain_dict))
                    continue  # Then at this point, this mapping is already done

            # 6. Handle everything else
            else:

                if not plain_dict:
                    # Evaluate any ClassAd expressions
                    # (in the context of their ad if possible)
                    if isinstance(value, classad.ExprTree):
                        try:
                            if isinstance(full_ad, classad.ClassAd):
                                eval_value = value.eval(full_ad)
                            else:
                                eval_value = value.eval()
                        except Exception:
                            self.log_once(f"Failed to evaluate {attr} in the context of its ClassAd")
                            eval_value = classad.Value.Error

                        # If eval doesn't work, store the expr as a string if possible
                        if isinstance(eval_value, (classad.Value, classad.ExprTree)):
                            field_name = f"{field_name}_EXPR"
                            field_type = str
                            try:
                                field_value = field_type(value)
                            except Exception:
                                self.log_once(f"Failed to get string repr of expr in {attr}")
                                continue
                        else:
                            value = eval_value

                    # Prevent Error or Undefined ClassAd values from getting through,
                    # for example, classad.Value.Undefined acts a literal 2 for
                    # any type casting done on it, which we don't want.
                    if isinstance(value, classad.Value):
                        self.log_once(f"Got ClassAd value {value.name} for {attr}", logging.info)
                        continue

                try:
                    if field_type is list and not isinstance(value, list):
                        raise TypeError(f"expected a list")
                    field_value = field_type(value)
                except Exception:
                    self.log_once(f"Failed to cast {attr} = {value} as a {field_type.__name__}")
                    continue

            if field_value is None:  # Somehow we didn't get a value? This shouldn't happen.
                self.log_once(f"Failed to get a usable value for {attr}", logging.error)
                continue

            # 7. Truncate strings if necessary
            field_value = self.truncate(field_name, field_value)

            # 8. Store value
            doc[field_name] = field_value

        return doc

    def truncate(self, field_name: str, value) -> str:
        """
        Shorten strings to MAX_KEYWORD_LEN if necessary
        """
        if isinstance(value, str) and len(value) > MAX_KEYWORD_LEN:
            self.log_once(f"Had to truncate value of {field_name} (original length {len(value)})")
            return f"{value[:MAX_KEYWORD_LEN-3]}..."
        return value

    def convert_ad_to_dict(self, ad, parent_attr="", preserve_case: bool = False, plain_dict: bool = False) -> dict:
        """
        Convert a ClassAd (or plain dict) to a document (dict) with flattened objects.

        If preserve_case is True, unknown subfield names retain their original
        casing rather than being lowercased by the DEFAULT dynamic template fallback.

        If plain_dict is True, classad-specific processing (ExprTree evaluation,
        classad.Value guards, JSON roundtrip on list items) is skipped, which is
        more efficient when the input is already a plain Python dict.
        """
        doc = {}

        # 1. Loop over all attrs
        for attr, value in ad.items():

            # 2. Flatten the namespace
            if parent_attr:
                attr = f"{parent_attr}.{attr}"

            # 3. Skip any ignored attrs
            if attr in self.ignore_attrs:
                continue

            # 4. Skip any attrs not in the projection.
            # Use a prefix check so that sub-properties of projected object fields
            # (e.g. TransferInputStats.FileCount when TransferInputStats is projected)
            # are not filtered out during recursion.
            if self.projection is not None:
                attr_lower = attr.lower()
                if attr_lower not in self.projection and not any(attr_lower.startswith(p + ".") for p in self.projection):
                    continue

            # 5. Convert attr
            doc.update(self.convert_attr_to_dict(attr, value, ad, preserve_case=preserve_case, plain_dict=plain_dict))

        return doc

    def get_timestamp(self, doc: dict, use_launch=False, fallback_to_launch=True) -> int:
        """
        Return the timestamp field for the document using the first
        timestamp field found in the provided doc (unless adstash
        launch time is preferred).
        """
        if use_launch:
            return _LAUNCH_TIME

        for field in self.timestamp_fields:
            if doc.get(field, 0) > 0:
                return doc[field]

        if fallback_to_launch:
            self.log_once(f"Could not find valid value for any timestamp attr ({', '.join(self.timestamp_fields)}), falling back to adstash launch date")
            return _LAUNCH_TIME

        self.log_once(f"Could not find valid value for any timestamp attr ({', '.join(self.timestamp_fields)}), timestamp will be 0!")
        return 0

    def get_unique_doc_id(self, doc: dict) -> str:
        """
        Return a unique id for the document
        """
        doc_id_list = [str(v) for v in [doc.get(field) for field in self.doc_id_fields] if v is not None]
        return "#".join(doc_id_list)


    def add_additional_fields(self, doc: dict, ad: classad.ClassAd):
        """
        Add additional fields from the ad to the doc.
        """
        # doc["field"] = ad.get("field")
        return


    def convert_ad_to_doc(self, ad: classad.ClassAd) -> dict:

        # Do the bulk of the conversions
        doc = self.convert_ad_to_dict(ad)

        # Add timestamps
        doc["@timestamp"] = doc["RecordTime"] = self.get_timestamp(ad)

        # Add additional fields
        self.add_additional_fields(doc, ad)

        return doc
