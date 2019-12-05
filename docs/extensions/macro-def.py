import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info


def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname

    # Create a new linkable target using the macro name
    targetid = text
    targetnode = nodes.target('', text, ids=[targetid], classes=["macro-def"])
    
    # Automatically include an index entry for macro definitions
    entries = process_index_entry(text, targetid)
    indexnode = addnodes.index()
    indexnode['entries'] = entries
    set_role_source_info(inliner, lineno, indexnode)

    return [indexnode, targetnode], []

def setup(app):
    app.add_role("macro-def", macro_def_role)

