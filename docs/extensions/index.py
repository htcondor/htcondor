import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, \
    set_role_source_info

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def index_role(typ, rawtext, text, lineno, inliner, options={}, content=[]):
    # type: (unicode, unicode, unicode, int, Inliner, Dict, List[unicode]) -> Tuple[List[nodes.Node], List[nodes.Node]]  # NOQA
    # create new reference target
    env = inliner.document.settings.env
    targetid = 'index-%s' % env.new_serialno('index')
    targetnode = nodes.target('', '', ids=[targetid])
    # split text and target in role content
    has_explicit_title, title, target = split_explicit_title(text)
    title = utils.unescape(title)
    target = utils.unescape(target)
    # if an explicit target is given, we can process it as a full entry
    if has_explicit_title:
        entries = process_index_entry(target, targetid)
    # otherwise we just create a "single" entry
    else:
        # but allow giving main entry
        main = ''
        if target.startswith('!'):
            target = target[1:]
            title = title[1:]
            main = 'main'
        entries = [('single', target, targetid, main, None)]
    indexnode = addnodes.index()
    indexnode['entries'] = entries
    set_role_source_info(inliner, lineno, indexnode)  # type: ignore
    textnode = nodes.Text(" ", " ")
    return [indexnode, targetnode, textnode], []

def setup(app):
    app.add_role("index", index_role, override=True)

