import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

CONFIG_KNOBS = []

def find_conf_knobs(dir: str):
    knobs = []
    definition_file = os.path.join(dir, "admin-manual", "configuration-macros.rst")
    with open(definition_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "macro-def" in line:
                begin = line.find("`") + 1
                end = line.rfind("`")
                knob = line[begin:end]
                if "[" in knob:
                    info = knob.find("[")
                    knob = knob[:info]
                if knob not in knobs:
                    knobs.append(knob)
    knobs.sort()
    return knobs

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    macro_name, macro_index = custom_ext_parser(text)
    if macro_name not in CONFIG_KNOBS:
        docname = inliner.document.settings.env.docname
        warn(f"{docname} @ {lineno} | Config knob '{macro_name}' not found in defined list. Either a typo or knob needs definition.")
    ref_link = f"href=\"{root_dir}/admin-manual/configuration-macros.html#" + str(macro_name) + "\""
    return make_ref_and_index_nodes(name, macro_name, macro_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    global CONFIG_KNOBS
    CONFIG_KNOBS = find_conf_knobs(app.srcdir)
    app.add_role("macro", macro_role)

