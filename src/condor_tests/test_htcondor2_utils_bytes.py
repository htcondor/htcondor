#!/usr/bin/env pytest

# Pure python unit test of htcondor2._utils.bytes; no daemons required

import pytest

from htcondor2._utils.bytes import (
    BytesConversionBase,
    BytesUnit,
    best_fit_unit,
    pretty_fmt_bytes,
)

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


IEC = BytesConversionBase.IEC_BINARY
METRIC = BytesConversionBase.METRIC


UNIT_STRINGS = [
    (BytesUnit.BYTE, IEC, "B"),
    (BytesUnit.BYTE, METRIC, "B"),
    (BytesUnit.KILO, IEC, "KiB"),
    (BytesUnit.KILO, METRIC, "kB"),
    (BytesUnit.MEGA, IEC, "MiB"),
    (BytesUnit.GIGA, METRIC, "GB"),
    (BytesUnit.YOTTA, IEC, "YiB"),
    (BytesUnit.YOTTA, METRIC, "YB"),
]


CONVERSIONS = [
    # (value, from, to, base, expected)
    (5, BytesUnit.KILO, BytesUnit.KILO, IEC, 5),
    (1, BytesUnit.KILO, BytesUnit.BYTE, IEC, 1024),
    (1, BytesUnit.KILO, BytesUnit.BYTE, METRIC, 1000),
    (1, BytesUnit.GIGA, BytesUnit.KILO, IEC, 1024 ** 2),
    (3, BytesUnit.TERA, BytesUnit.MEGA, METRIC, 3 * 1000 ** 2),
    (2048, BytesUnit.BYTE, BytesUnit.KILO, IEC, 2),
    (1024 ** 3, BytesUnit.KILO, BytesUnit.TERA, IEC, 1),
    (1500, BytesUnit.MEGA, BytesUnit.GIGA, METRIC, 1.5),
    (512, BytesUnit.MEGA, BytesUnit.GIGA, IEC, 0.5),
]


PRETTY = [
    # (value, unit, base, precision, expected)
    (0, BytesUnit.BYTE, IEC, 2, "0 B"),
    (1, BytesUnit.BYTE, IEC, 2, "1 B"),
    (1023, BytesUnit.BYTE, IEC, 2, "1023 B"),
    (1024, BytesUnit.BYTE, IEC, 2, "1 KiB"),
    (1536, BytesUnit.BYTE, IEC, 2, "1.50 KiB"),
    (999, BytesUnit.BYTE, METRIC, 2, "999 B"),
    (1500000, BytesUnit.BYTE, METRIC, 2, "1.50 MB"),
    # Non-byte starting units
    (3, BytesUnit.KILO, IEC, 2, "3 KiB"),
    (2048, BytesUnit.MEGA, IEC, 2, "2 GiB"),
    (512, BytesUnit.MEGA, IEC, 2, "512 MiB"),
    (10485760, BytesUnit.KILO, IEC, 2, "10 GiB"),
    (0.5, BytesUnit.GIGA, IEC, 2, "512 MiB"),
    # Fractional byte values still use precision
    (0.5, BytesUnit.KILO, IEC, 2, "512 B"),
    (1.5, BytesUnit.BYTE, IEC, 2, "1.50 B"),
    # Whole numbers drop the zero fraction, including after rounding
    (2047.9, BytesUnit.BYTE, IEC, 2, "2 KiB"),
    (1.999, BytesUnit.KILO, IEC, 2, "2 KiB"),
    (1.25, BytesUnit.KILO, IEC, 1, "1.2 KiB"),
    (1.05, BytesUnit.KILO, IEC, 1, "1.1 KiB"),
    # Rounding up to the base moves to the next unit
    (1048575, BytesUnit.BYTE, IEC, 2, "1 MiB"),
    (1023.999, BytesUnit.KILO, IEC, 2, "1 MiB"),
    (1023.4, BytesUnit.KILO, IEC, 0, "1023 KiB"),
    (999999, BytesUnit.BYTE, METRIC, 2, "1 MB"),
    (1023.999, BytesUnit.YOTTA, IEC, 2, "1024 YiB"),
    (1500, BytesUnit.BYTE, METRIC, 1, "1.5 kB"),
    # Precision
    (1536, BytesUnit.BYTE, IEC, 0, "2 KiB"),
    (1500, BytesUnit.BYTE, IEC, 4, "1.4648 KiB"),
    # Negative values scale by magnitude
    (-2048, BytesUnit.BYTE, IEC, 2, "-2 KiB"),
    (-1023, BytesUnit.BYTE, IEC, 2, "-1023 B"),
    # Largest unit is capped at YOTTA without mislabeling
    (1, BytesUnit.YOTTA, IEC, 2, "1 YiB"),
    (5 * 1024, BytesUnit.YOTTA, IEC, 2, "5120 YiB"),
    (5 * 1000, BytesUnit.YOTTA, METRIC, 2, "5000 YB"),
]


BEST_FIT = [
    # (value, unit, base, expected value, expected unit)
    (0, BytesUnit.BYTE, IEC, 0, BytesUnit.BYTE),
    (1023, BytesUnit.BYTE, IEC, 1023, BytesUnit.BYTE),
    (1024, BytesUnit.BYTE, IEC, 1, BytesUnit.KILO),
    (3072, BytesUnit.KILO, IEC, 3, BytesUnit.MEGA),
    (0.5, BytesUnit.GIGA, IEC, 512, BytesUnit.MEGA),
    (-2048, BytesUnit.BYTE, IEC, -2, BytesUnit.KILO),
    (1500, BytesUnit.MEGA, METRIC, 1.5, BytesUnit.GIGA),
    (5 * 1024, BytesUnit.YOTTA, IEC, 5 * 1024, BytesUnit.YOTTA),
]


class TestBytesUtils:

    @pytest.mark.parametrize("unit, base, expected", UNIT_STRINGS)
    def test_unit_string(self, unit, base, expected):
        assert unit.unit(base) == expected


    def test_unit_string_default_base_is_iec(self):
        assert BytesUnit.MEGA.unit() == "MiB"


    @pytest.mark.parametrize("value, src, dst, base, expected", CONVERSIONS)
    def test_convert(self, value, src, dst, base, expected):
        assert src.convert(value, dst, base) == pytest.approx(expected)


    @pytest.mark.parametrize("src", list(BytesUnit))
    @pytest.mark.parametrize("dst", list(BytesUnit))
    def test_convert_round_trip(self, src, dst):
        value = 12345
        assert dst.convert(src.convert(value, dst), src) == pytest.approx(value)


    def test_convert_to_smaller_unit_stays_integer(self):
        # Converting to a smaller unit only multiplies, so ints stay exact
        result = BytesUnit.GIGA.convert(7, BytesUnit.BYTE)
        assert isinstance(result, int)
        assert result == 7 * 1024 ** 3


    @pytest.mark.parametrize("value, unit, base, precision, expected", PRETTY)
    def test_pretty_fmt_bytes(self, value, unit, base, precision, expected):
        assert pretty_fmt_bytes(value, unit, base, precision) == expected


    @pytest.mark.parametrize("value, unit, expected", [
        (1024, BytesUnit.BYTE, "1.00 KiB"),
        (2047.9, BytesUnit.BYTE, "2.00 KiB"),
        (2048, BytesUnit.MEGA, "2.00 GiB"),
        (1536, BytesUnit.BYTE, "1.50 KiB"),
        (1023, BytesUnit.BYTE, "1023.00 B"),
    ])
    def test_pretty_fmt_bytes_keep_zero_fraction(self, value, unit, expected):
        assert pretty_fmt_bytes(value, unit, drop_zero_fraction=False) == expected


    @pytest.mark.parametrize("value, unit, base, expected_value, expected_unit", BEST_FIT)
    def test_best_fit_unit(self, value, unit, base, expected_value, expected_unit):
        raw, fit = best_fit_unit(value, unit, base)
        assert fit == expected_unit
        assert raw == pytest.approx(expected_value)


    def test_pretty_fmt_bytes_defaults(self):
        assert pretty_fmt_bytes(1024 ** 2) == "1 MiB"
