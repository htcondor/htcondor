import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import make_headerlink_node, warn

SUBMIT_CMD_DEFS = []

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def subcom_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    global SUBMIT_CMD_DEFS
    if text in SUBMIT_CMD_DEFS:
        docname = inliner.document.settings.env.docname
        warn(f"{docname} @ {lineno} | '{text}' submit command already defined!")
        textnode = nodes.Text(text, " ")
        return [textnode], []
    else:
        SUBMIT_CMD_DEFS.append(text)

    # Create a new linkable target using the subcom name
    targetid = text
    targetnode = nodes.target('', text, ids=[targetid], classes=["subcom-def"])

    headerlink_node = make_headerlink_node(str(text), options)

    # Automatically include an index entry for subcom definitions
    indexnode = addnodes.index()
    indexnode['entries'] = process_index_entry('pair: ' + text + '; Submit commands', targetid)
    set_role_source_info(inliner, lineno, indexnode)
    if os.environ.get('MANPAGES') == 'True':
        textnode = nodes.Text(text, " ")
        return [indexnode, targetnode, textnode], []
    else:
        textnode = nodes.Text(" ", " ")
        return [headerlink_node, indexnode, targetnode, textnode], []


def setup(app):
    app.add_role("subcom-def", subcom_def_role)

