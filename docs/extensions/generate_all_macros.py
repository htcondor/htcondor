
from __future__ import annotations

from pathlib import Path
from generation import generate_rst
from htc_helpers import get_all_defined_role, custom_ext_parser

def strip_macro_line_def(line: str):
    for role in get_all_defined_role("macro-def", line):
        macro, _ = custom_ext_parser(role)
        yield macro

def generate_all_macros_page(app) -> None:
    config_dir = Path(app.confdir) / "admin-manual" / "configuration"
    all_macros = generate_rst(app.confdir, "all_macros.rst")

    with open(all_macros, "w") as f:
        for item in config_dir.iterdir():
            if not item.is_file() or item.name in ["index.rst", "all.rst"] or item.suffix != ".rst":
                continue

            section = str(item.name[:-4]).replace("-", " ").replace("_", " ").title()
            underline = "-" * len(section)

            f.write(f"\n{section}\n")
            f.write(f"{underline}\n\n")

            with open(item, "r") as stream:
                in_entry = False
                for line in stream:
                    stripped = line.rstrip()

                    if not in_entry:
                        if ":macro-def:" in stripped:
                            first = True
                            for macro in strip_macro_line_def(stripped):
                                if not first:
                                    f.write("    .. faux-definition::\n\n")
                                f.write(f"{macro}\n")
                                first = False
                            in_entry = True
                    else:
                        if stripped == "" or stripped[0].isspace():
                            f.write(f"{stripped}\n")
                        else:
                            in_entry = False
                            if ":macro-def:" in stripped:
                                first = True
                                for macro in strip_macro_line_def(stripped):
                                    if not first:
                                        f.write("    .. faux-definition::\n\n")
                                    f.write(f"{macro}\n")
                                    first = False
                                in_entry = True


def setup(app):
    app.connect("builder-inited", generate_all_macros_page)
    return {
        "version": "1.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
