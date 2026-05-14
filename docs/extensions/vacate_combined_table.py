"""Generate vacate-code-combined-table.rst during Sphinx builds.

   This extension owns all logic for combining:
   1) the header row from vacate-code-table.rst,
   2) the first three columns from the data rows in hold-table.rst, and
   3) the data rows from vacate-code-table.rst.
"""

from __future__ import annotations

import os
import re
from typing import List

from generation import generate_rst

VACATE_TABLE_REL_PATH = "codes-other-values/vacate-code-table.rst"
HOLD_REASON_CODES_REL_PATH = "codes-other-values/hold-reason-codes.rst"


class VacateCombinedTableGenerator:
    """Create the combined vacate/hold grid table include file."""

    def __init__(self, docs_root: str) -> None:
        self.docs_root = docs_root
        self.out_path = generate_rst(docs_root, "vacate-code-combined-table.rst")
        self.vacate_table_path = os.path.join(docs_root, VACATE_TABLE_REL_PATH)
        hold_reason_codes_path = os.path.join(docs_root, HOLD_REASON_CODES_REL_PATH)
        self.hold_table_path = self._resolve_hold_table_path(hold_reason_codes_path)

    def _resolve_hold_table_path(self, hold_reason_codes_path: str) -> str:
        with open(hold_reason_codes_path, encoding="utf-8") as f:
            hold_reason_codes_text = f.read()

        include_match = re.search(
            r"^\s*\.\.\s+include::\s+(/codes-other-values/[^\s]+)\s*$",
            hold_reason_codes_text,
            re.MULTILINE,
        )
        if include_match:
            hold_table_rel = include_match.group(1).lstrip("/")
            return os.path.join(self.docs_root, hold_table_rel)

        return hold_reason_codes_path

    @staticmethod
    def _extract_grid_table_lines(text: str) -> List[str]:
        lines = []
        for line in text.splitlines():
            stripped = line.lstrip()
            if stripped.startswith("+") or stripped.startswith("|"):
                lines.append(stripped.rstrip())
        return lines

    @staticmethod
    def _split_grid_header_and_body(table_lines: List[str]) -> tuple[List[str], List[str]]:
        for i, line in enumerate(table_lines):
            if "=" in line and line.lstrip().startswith("+"):
                return table_lines[: i + 1], table_lines[i + 1 :]
        return table_lines, []

    @staticmethod
    def _parse_grid_widths(border_line: str) -> List[int]:
        parts = border_line.split("+")[1:-1]
        return [len(p) for p in parts]

    @staticmethod
    def _parse_grid_boundaries(border_line: str) -> List[int]:
        return [i for i, ch in enumerate(border_line) if ch == "+"]

    @staticmethod
    def _parse_grid_rows(body_lines: List[str], boundaries: List[int]) -> List[List[str]]:
        row_blocks = []
        current = []

        for line in body_lines:
            if line.startswith("|"):
                current.append(line.rstrip("\n"))
                continue

            if line.startswith("+") and current:
                row_blocks.append(current)
                current = []

        rows = []
        for block in row_blocks:
            n_cols = len(boundaries) - 1
            cols = [[] for _ in range(n_cols)]
            for content_line in block:
                for i in range(n_cols):
                    start = boundaries[i] + 1
                    end = boundaries[i + 1]
                    val = content_line[start:end] if end <= len(content_line) else ""
                    cols[i].append(val.strip())
            rows.append(["\n".join(col).rstrip() for col in cols])

        return rows

    @staticmethod
    def _render_grid_separator(indent: str, widths: List[int], fill: str = "-") -> str:
        return indent + "+" + "+".join([fill * w for w in widths]) + "+"

    @staticmethod
    def _render_grid_row(indent: str, widths: List[int], cells: List[str]) -> List[str]:
        normalized = []
        for c in cells:
            parts = c.split("\n") if c else [""]
            normalized.append(parts)

        out = []
        max_lines = max(len(c) for c in normalized)
        for i in range(max_lines):
            line = indent + "|"
            for col, width in enumerate(widths):
                text = normalized[col][i] if i < len(normalized[col]) else ""
                line += " " + text.ljust(width - 1) + "|"
            out.append(line)
        return out

    def generate(self) -> None:
        with open(self.vacate_table_path, encoding="utf-8") as f:
            vacate_text = f.read()
        with open(self.hold_table_path, encoding="utf-8") as f:
            hold_text = f.read()

        vacate_lines = self._extract_grid_table_lines(vacate_text)
        hold_lines = self._extract_grid_table_lines(hold_text)

        vacate_header, vacate_body = self._split_grid_header_and_body(vacate_lines)
        _, hold_body = self._split_grid_header_and_body(hold_lines)

        indent = vacate_header[0][: len(vacate_header[0]) - len(vacate_header[0].lstrip())]
        widths = self._parse_grid_widths(vacate_header[0])
        vacate_boundaries = self._parse_grid_boundaries(vacate_header[0])
        hold_boundaries = self._parse_grid_boundaries(hold_lines[0])

        hold_rows = self._parse_grid_rows(hold_body, hold_boundaries)
        hold_rows_3col = [row[:3] for row in hold_rows]
        vacate_rows = self._parse_grid_rows(vacate_body, vacate_boundaries)

        combined = list(vacate_header)
        for row in hold_rows_3col:
            combined.extend(self._render_grid_row(indent, widths, row))
            combined.append(self._render_grid_separator(indent, widths, "-"))
        for row in vacate_rows:
            combined.extend(self._render_grid_row(indent, widths, row))
            combined.append(self._render_grid_separator(indent, widths, "-"))

        content = (
            ".. This file is generated at build time by docs/extensions/vacate_combined_table.py.\n"
            ".. Do not edit manually.\n\n"
            ".. This file contains the complete Vacate Reason Codes table auto-generated by combining:\n"
            ".. 1) the header row from vacate-code-table.rst,\n"
            ".. 2) the first three columns from the data rows in hold-table.rst, and\n"
            ".. 3) the data rows from vacate-code-table.rst.\n\n"
            + "\n".join(combined)
            + "\n"
        )

        with open(self.out_path, "w", encoding="utf-8") as f:
            f.write(content)


def _on_builder_inited(app) -> None:
    generator = VacateCombinedTableGenerator(app.confdir)
    generator.generate()


def setup(app):
    app.connect("builder-inited", _on_builder_inited)
    return {
        "version": "1.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
