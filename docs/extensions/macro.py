import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname
    macro_name = text
    ref_link = "href=\"../admin-manual/configuration-macros.html#" + str(macro_name) + "\""
    # Building only the manpages
    if os.environ.get('MANPAGES') == 'True':
        node = nodes.reference(rawtext, macro_name, refuri=ref_link, **options)
        return [node], []
    # If here then building the documentation
    macro_name_html = macro_name.replace("<", "&lt;").replace(">", "&gt;")
    #Create target id as 'macro_name-#' so when index references the macro call in a page it goes to that section
    targetid = '%s-%s' % (str(macro_name), inliner.document.settings.env.new_serialno('index'))
    #Set id so index successfully goes to that location in the web page
    node = nodes.raw("", "<a id=\"" + str(targetid) + "\" class=\"macro\" " + str(ref_link) + ">" + str(macro_name_html) + "</a>", format="html")

    # Automatically include an index entry for macro directive calls
    entries = process_index_entry(text, targetid)
    indexnode = addnodes.index()
    indexnode['entries'] = entries
    set_role_source_info(inliner, lineno, indexnode)

    return [indexnode, node], []

def setup(app):
    app.add_role("macro", macro_role)

