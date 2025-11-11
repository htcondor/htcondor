import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import custom_ext_parser, make_headerlink_node, warn

KNOB_DEFS = []

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    global KNOB_DEFS
    # Create a new linkable target using the macro name
    knob, grouping = custom_ext_parser(text)
    targetid = knob
    targetnode = nodes.target('', knob, ids=[targetid], classes=["macro-def"])
    if grouping != "":
        grouping = grouping + " "

    if knob in KNOB_DEFS:
        docname = inliner.document.settings.env.docname
        warn(f"{docname}:{lineno} | '{knob}' configuration knob already defined!")
        textnode = nodes.Text(knob, " ")
        return [textnode], []
    else:
        KNOB_DEFS.append(knob)

    # Automatically include an index entry for macro definitions
    indexnode = addnodes.index()
    indexnode['entries'] = process_index_entry(f"pair: {knob}; {grouping}Configuration Options", targetid)
    set_role_source_info(inliner, lineno, indexnode)
    headerlink_node = make_headerlink_node(knob, options)

    return [indexnode, targetnode, headerlink_node], []

def setup(app):
    app.add_role("macro-def", macro_def_role)

