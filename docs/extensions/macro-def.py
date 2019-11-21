import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname
    macro_name = text
    macro_name_html = macro_name.replace("<", "&lt;").replace(">", "&gt;")
    node = nodes.raw("", "<a name=\"" + str(macro_name) + "\"></a><b>" + str(macro_name_html) + "</b>", format="html")
    return [node], []

def setup(app):
    app.add_role("macro-def", macro_def_role)

