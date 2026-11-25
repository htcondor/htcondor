#!/usr/bin/python3

# Copyright 2026 HTCondor Team, Computer Sciences Department,
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

import enum
from typing import Tuple, Union

class BytesConversionBase(enum.IntEnum):
    """Conversion base enumeration for pretty byte printing"""
    METRIC = 1000
    IEC_BINARY = 1024

class BytesUnit(enum.IntEnum):
    """Common unit amounts for bytes"""
    BYTE = 0
    KILO = 1
    MEGA = 2
    GIGA = 3
    TERA = 4
    PETA = 5
    EXA = 6
    ZETTA = 7
    YOTTA = 8

    def unit(self, base: BytesConversionBase = BytesConversionBase.IEC_BINARY) -> str:
        """Return a formatted string of this specific byte unit with conversion base in mind"""
        modifier = ""
        if self.value > 0:
            modifier = self.name[0]
            if base == BytesConversionBase.IEC_BINARY:
                modifier += "i"
            elif self == BytesUnit.KILO:
                # SI kilo is a lowercase k
                modifier = modifier.lower()

        return f"{modifier}B"

    def convert(self, value: Union[int, float], to: "BytesUnit", base: BytesConversionBase = BytesConversionBase.IEC_BINARY) -> Union[int, float]:
        """Convert value of bytes for this current unit to another"""
        diff = self.value - to.value
        if diff > 0:
            # Larger unit -> smaller unit (e.g. KiB -> B): multiply
            return value * (base.value ** diff)
        elif diff < 0:
            # Smaller unit -> larger unit (e.g. B -> KiB): divide
            return value / (base.value ** -diff)
        return value

def best_fit_unit(value: Union[int, float], unit: "BytesUnit" = BytesUnit.BYTE, base: BytesConversionBase = BytesConversionBase.IEC_BINARY) -> Tuple[Union[int, float], BytesUnit]:
    """Return value converted to the largest unit that keeps it below the base, and that unit"""
    raw = unit.convert(value, BytesUnit.BYTE, base)

    final = BytesUnit.BYTE
    while abs(raw) >= base.value and final < BytesUnit.YOTTA:
        raw /= base.value
        final = BytesUnit(final + 1)

    return raw, final

def pretty_fmt_bytes(value: Union[int, float], unit: BytesUnit = BytesUnit.BYTE, base: BytesConversionBase = BytesConversionBase.IEC_BINARY, precision: int = 2, drop_zero_fraction: bool = True) -> str:
    """Format a byte amount as a human readable string in the largest fitting unit"""
    raw, final = best_fit_unit(value, unit, base)

    number = f"{raw:.{precision}f}"
    # Rounding can reach the base (e.g. 1023.999 KiB -> "1024.00"): use the next unit instead
    if abs(float(number)) >= base.value and final < BytesUnit.YOTTA:
        raw /= base.value
        final = BytesUnit(final + 1)
        number = f"{raw:.{precision}f}"

    # Optionally drop an all-zero fractional part (e.g. "2.00" -> "2")
    if drop_zero_fraction and precision > 0 and number.endswith("." + "0" * precision):
        number = number[:-(precision + 1)]

    return f"{number} {final.unit(base)}"
