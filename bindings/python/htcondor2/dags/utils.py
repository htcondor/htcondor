# Copyright 2020 HTCondor Team, Computer Sciences Department,
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

from typing import (
    Iterable,
    Any,
    Callable,
    Dict,
    Mapping,
    Iterator,
    TypeVar,
    Tuple,
)
import logging

import itertools
import re

import htcondor2 as htcondor

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


T = TypeVar("T")


def flatten(nested_iterable: Iterator[Iterator[T]]) -> Iterator[T]:
    yield from itertools.chain.from_iterable(nested_iterable)


def grouper(iterable, n, fill=None):
    args = [iter(iterable)] * n
    return itertools.zip_longest(*args, fillvalue=fill)


def make_repr(obj, attrs):
    entries = ", ".join("{} = {}".format(k, getattr(obj, k)) for k in attrs)
    return "{}({})".format(type(obj).__name__, entries)


def table(
    headers: Iterable[str],
    rows: Iterable[Iterable[Any]],
    fill: str = "",
    header_fmt: Callable[[str], str] = None,
    row_fmt: Callable[[str], str] = None,
    alignment: Dict[str, str] = None,
) -> str:
    """
    Return a string containing a simple table created from headers and rows of entries.

    Parameters
    ----------
    headers
        The column headers for the table.
    rows
        The entries for each row, for each column.
        Should be an iterable of iterables or mappings, with the outer level containing the rows,
        and each inner iterable containing the entries for each column.
        An iterable-type row is printed in order.
        A mapping-type row uses the headers as keys to align the stdout and can have missing values,
        which are filled using the ```fill`` value.
    fill
        The string to print in place of a missing value in a mapping-type row.
    header_fmt
        A function to be called on the header string.
        The return value is what will go in the output.
    row_fmt
        A function to be called on each row string.
        The return value is what will go in the output.
    alignment
        If ``True``, the first column will be left-aligned instead of centered.

    Returns
    -------
    table :
        A string containing the table.
    """
    if header_fmt is None:
        header_fmt = lambda _: _
    if row_fmt is None:
        row_fmt = lambda _: _
    if alignment is None:
        alignment = {}

    headers = tuple(headers)
    lengths = [len(h) for h in headers]

    align_methods = [alignment.get(h, "center") for h in headers]

    processed_rows = []
    for row in rows:
        if isinstance(row, Mapping):
            processed_rows.append([str(row.get(key, fill)) for key in headers])
        else:
            processed_rows.append(
                [str(entry) if entry is not None else fill for entry in row]
            )

    for row in processed_rows:
        lengths = [max(curr, len(entry)) for curr, entry in zip(lengths, row)]

    header = header_fmt(
        "  ".join(
            getattr(h, a)(l) for h, l, a in zip(headers, lengths, align_methods)
        ).rstrip()
    )

    lines = (
        row_fmt(
            "  ".join(getattr(f, a)(l) for f, l, a in zip(row, lengths, align_methods))
        )
        for row in processed_rows
    )

    output = "\n".join((header, *lines))

    return output


VERSION_RE = re.compile(
    r"^(\d+) \. (\d+) (\. (\d+))? ([ab](\d+))?$", re.VERBOSE | re.ASCII
)


def parse_version(v: str) -> Tuple[int, int, int, str, int]:
    match = VERSION_RE.match(v)
    if match is None:
        raise Exception("Could not determine version info from {}".format(v))

    (major, minor, micro, prerelease, prerelease_num) = match.group(1, 2, 4, 5, 6)

    out = (
        int(major),
        int(minor),
        int(micro or 0),
        prerelease[0] if prerelease is not None else None,
        int(prerelease_num) if prerelease_num is not None else None,
    )

    return out


EXTRACT_HTCONDOR_VERSION_RE = re.compile(r"(\d+\.\d+\.\d+)", flags=re.ASCII)

BINDINGS_VERSION_INFO = parse_version(
    EXTRACT_HTCONDOR_VERSION_RE.search(htcondor.version()).group(0)
)
