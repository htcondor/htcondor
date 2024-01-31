import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def tool_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    original_name, program_index = custom_ext_parser(text)
    program_name = original_name if original_name[:8] != "htcondor" else "htcondor"
    ref_link = f"href=\"{root_dir}/man-pages/{program_name}.html\""
    return make_ref_and_index_nodes(name, original_name, program_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    app.add_role("tool", tool_role)